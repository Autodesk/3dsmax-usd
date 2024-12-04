//
// Copyright 2023 Autodesk
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
#include "pythonObjectRegistry.h"
#include "wrapUSDSceneBuilderOptions.h"

#include <MaxUsd/Builders/USDSceneBuilderOptions.h>
#include <MaxUsd/Translators/ShaderWriter.h>
#include <MaxUsd/Translators/ShaderWriterRegistry.h>
#include <MaxUsd/Translators/WriteJobContext.h>

#include <pxr/base/tf/api.h>
#include <pxr/base/tf/pyContainerConversions.h>
#include <pxr/base/tf/pyEnum.h>
#include <pxr/base/tf/pyPolymorphic.h>
#include <pxr/base/tf/registryManager.h>

#include <maxscript/kernel/value.h>
#include <maxscript/maxwrapper/mxsobjects.h>

#include <boost/python/class.hpp>
#include <boost/python/def.hpp>
#include <boost/python/make_constructor.hpp>
#include <boost/python/return_internal_reference.hpp>
#include <boost/python/return_value_policy.hpp>

PXR_NAMESPACE_USING_DIRECTIVE

//----------------------------------------------------------------------------------------------------------------------
/// \brief  boost python binding for the MaxUsdShaderWriter
//----------------------------------------------------------------------------------------------------------------------
template <typename T = MaxUsdShaderWriter>
class ShaderWriterWrapper
    : public T
    , public TfPyPolymorphic<MaxUsdShaderWriter>
{
public:
    typedef ShaderWriterWrapper This;
    typedef T                   base_t;

    ShaderWriterWrapper(Mtl* material, const SdfPath& usdPath, MaxUsdWriteJobContext& jobCtx)
        : T(material, usdPath, jobCtx)
        , materialValue(nullptr)
    {
    }

    static std::shared_ptr<This> New(uintptr_t createdWrapper)
    {
        return *((std::shared_ptr<This>*)createdWrapper);
    }

    virtual ~ShaderWriterWrapper() = default;

    const UsdStage& GetUsdStage() const { return *get_pointer(base_t::GetUsdStage()); }

    // This is the pattern inspired from USD/pxr/base/tf/wrapTestTfPython.cpp
    // It would have been simpler to call 'this->CallVirtual<>("Write",
    // &base_t::default_Write)();' But it is not allowed Instead of having to create a
    // function 'default_Write(...)' to call the base class when there is no Python implementation.
    void default_Write() { base_t::Write(); }
    void Write() override { this->template CallVirtual<>("Write", &This::default_Write)(); }

    bool default_HasMaterialDependencies() const { return base_t::HasMaterialDependencies(); }
    bool HasMaterialDependencies() const override
    {
        return this->template CallVirtual<>(
            "HasMaterialDependencies", &This::default_HasMaterialDependencies)();
    };

    void default_GetSubMtlDependencies(std::vector<Mtl*>& subMtl) const
    {
        base_t::GetSubMtlDependencies(subMtl);
    }
    void GetSubMtlDependencies(std::vector<Mtl*>& subMtl) const
    {
        PyGILState_STATE gstate = PyGILState_Ensure();

        Override o = this->GetOverride("GetSubMtlDependencies");
        if (o) {
            auto result = std::function<boost::python::list()>(TfPyCall<boost::python::list>(o))();
            for (int i = 0; i < len(result); ++i) {
                subMtl.push_back(dynamic_cast<Mtl*>(
                    Animatable::GetAnimByHandle(boost::python::extract<INT_PTR>(result[i]))));
            }
        } else {
            This::default_GetSubMtlDependencies(subMtl);
        }

        PyGILState_Release(gstate);
    }

    /// \brief Method called after all materials are exported
    void default_PostWrite() { base_t::PostWrite(); }
    void PostWrite() override
    {
        this->template CallVirtual<>("PostWrite", &This::default_PostWrite)();
    };

    INT_PTR GetMaterialAnimHandle()
    {
        if (!materialValue) {
            materialValue = MAXClass::make_wrapper_for(base_t::GetMaterial());
        }
        if (materialValue)
            return Animatable::GetHandleByAnim(materialValue->to_reftarg());
        return 0;
    }

    //---------------------------------------------------------------------------------------------
    /// \brief  wraps a factory function that allows registering an updated Python class
    //---------------------------------------------------------------------------------------------
    class FactoryFnWrapper : public MaxUsdPythonObjectRegistry
    {
    public:
        // Instances of this class act as "function objects" that are fully compatible with the
        // std::function requested by MaxUsdShaderWriterRegistry::Register. These will create
        // python wrappers based on the latest class registered.
        MaxUsdShaderWriterSharedPtr
        operator()(Mtl* material, const SdfPath& usdPath, MaxUsdWriteJobContext& jobCtx)
        {
            boost::python::object pyClass = GetPythonObject(_classIndex);
            auto                  sptr = std::make_shared<This>(material, usdPath, jobCtx);
            TfPyLock              pyLock;
            boost::python::object instance = pyClass((uintptr_t)&sptr);
            boost::python::incref(instance.ptr());
            initialize_wrapper(instance.ptr(), sptr.get());
            return sptr;
        }

        // We can have multiple function objects, this one adapts the CanExport function:
        MaxUsdShaderWriter::ContextSupport
        operator()(const MaxUsd::USDSceneBuilderOptions& exportArgs)
        {
            boost::python::object pyClass = GetPythonObject(_classIndex);
            if (!pyClass) {
                // Prototype was unregistered
                return MaxUsdShaderWriter::ContextSupport::Unsupported;
            }

            TfPyLock pyLock;

            std::string canExport = "CanExport";
            if (!PyObject_HasAttrString(pyClass.ptr(), canExport.c_str())) {
                MaxUsd::Log::Error(
                    "Registered python ShaderWriter \"{}\" has no suitable CanExport(node, "
                    "exportArgs) method.",
                    boost::python::extract<std::string>(pyClass.attr("__name__"))());
                return MaxUsdShaderWriter::ContextSupport::Unsupported;
            }

            boost::python::object CanExport = pyClass.attr(canExport.c_str());
            PyObject*             callable = CanExport.ptr();
            try {
                auto res
                    = boost::python::call<int>(callable, USDSceneBuilderOptionsWrapper(exportArgs));
                return MaxUsdShaderWriter::ContextSupport(res);
            } catch (...) {
                MaxUsd::Log::Error("Unable to call the CanExport(exportArgs) method of the "
                                   "registered ShaderWriter.");
            }
            return MaxUsdShaderWriter::ContextSupport::Unsupported;
        }

        // Declaring if the material(s) the Writer is registered for are target agnostic.
        // A target agnostic material is a material that can be exported to any target, and doesn't
        // need to be exported to each specific target.
        bool operator()()
        {
            boost::python::object pyClass = GetPythonObject(_classIndex);
            if (!pyClass) {
                // Prototype was unregistered
                return false;
            }

            TfPyLock pyLock;

            std::string isMaterialTargetAgnostic = "IsMaterialTargetAgnostic";
            // Registered python ShaderWriter has no suitable GetTargetAgnosticMaterials() method.
            if (!PyObject_HasAttrString(pyClass.ptr(), isMaterialTargetAgnostic.c_str())) {
                return false;
            }

            boost::python::object IsMaterialTargetAgnostic
                = pyClass.attr(isMaterialTargetAgnostic.c_str());
            PyObject* callable = IsMaterialTargetAgnostic.ptr();
            try {
                auto res = boost::python::call<bool>(callable);
                return res;
            } catch (...) {
                MaxUsd::Log::Error("Unable to call the IsMaterialTargetAgnostic() method of the "
                                   "registered ShaderWriter.");
            }
            return false;
        }

        // Create a new wrapper for a Python class that is seen for the first time for a given
        // purpose. If we already have a registration for this purpose: update the class to
        // allow the previously issued factory function to use it.
        static FactoryFnWrapper
        Register(boost::python::object cl, const std::string& usdShaderId, bool& updated)
        {
            size_t classIndex = RegisterPythonObject(cl, GetKey(cl, usdShaderId));
            updated = classIndex == MaxUsdPythonObjectRegistry::UPDATED;
            // Return a new factory function:
            return FactoryFnWrapper { classIndex };
        }

        // Unregister a class for a given purpose. This will cause the associated factory
        // function to stop producing this Python class.
        static void Unregister(boost::python::object cl, const std::string& usdShaderId)
        {
            UnregisterPythonObject(cl, GetKey(cl, usdShaderId));
        }

    private:
        // Function object constructor. Requires only the index of the Python class to use.
        FactoryFnWrapper(size_t classIndex)
            : _classIndex(classIndex) { };

        size_t _classIndex;

        // Generates a unique key based on the name of the class, along with the class
        // purpose:
        static std::string GetKey(boost::python::object cl, const std::string& usdShaderId)
        {
            return ClassName(cl) + "," + usdShaderId + "," + ",ShaderWriter";
        }
    };

    static void Register(boost::python::object cl, const TfToken& usdShaderId)
    {
        bool             updated = false;
        FactoryFnWrapper fn = FactoryFnWrapper::Register(cl, usdShaderId, updated);
        if (!updated) {
            MaxUsdShaderWriterRegistry::Register(usdShaderId, fn, fn, fn, true);
        }
    }

    static void Unregister(boost::python::object cl, const TfToken& usdShaderId)
    {
        FactoryFnWrapper::Unregister(cl, usdShaderId);
    }

protected:
    Value* materialValue;
};

namespace {

// This class is used to expose protected members of MaxUsdShaderWriter to Python
class ShaderWriterAllowProtected : public MaxUsdShaderWriter
{
    typedef MaxUsdShaderWriter base_t;

public:
    void SetUsdPrim(const UsdPrim& usdPrim) { base_t::SetUsdPrim(usdPrim); }
    USDSceneBuilderOptionsWrapper GetExportArgs() const
    {
        return USDSceneBuilderOptionsWrapper(base_t::GetExportArgs());
    }
    boost::python::dict GetMaterialsToPrimsMap() const
    {
        boost::python::dict materialsToPrims;
        for (const auto& matToPrim : base_t::GetMaterialsToPrimsMap()) {
            materialsToPrims[Animatable::GetHandleByAnim(matToPrim.first)] = matToPrim.second;
        }
        return materialsToPrims;
    }
};

void unprotect_SetUsdPrim(MaxUsdShaderWriter& pw, const UsdPrim& prim)
{
    reinterpret_cast<ShaderWriterAllowProtected&>(pw).SetUsdPrim(prim);
}
USDSceneBuilderOptionsWrapper unprotect_GetExportArgs(MaxUsdShaderWriter& pw)
{
    return reinterpret_cast<ShaderWriterAllowProtected&>(pw).GetExportArgs();
}
boost::python::dict unprotect_GetMaterialsToPrimsMap(MaxUsdShaderWriter& pw)
{
    return reinterpret_cast<ShaderWriterAllowProtected&>(pw).GetMaterialsToPrimsMap();
}
} // namespace
//----------------------------------------------------------------------------------------------------------------------

TF_REGISTRY_FUNCTION(TfEnum)
{
    TF_ADD_ENUM_NAME(MaxUsdShaderWriter::ContextSupport::Supported, "Supported");
    TF_ADD_ENUM_NAME(MaxUsdShaderWriter::ContextSupport::Fallback, "Fallback");
    TF_ADD_ENUM_NAME(MaxUsdShaderWriter::ContextSupport::Unsupported, "Unsupported");
}

void wrapShaderWriter()
{
    boost::python::class_<ShaderWriterWrapper<>, boost::noncopyable> c(
        "ShaderWriter",
        "ShaderWriter base class from which material writers need to inherit from. \n"
        "A ShaderWriter instance is created for each material needing translation.",
        boost::python::no_init);

    boost::python::scope s(c);

    TfPyWrapEnum<MaxUsdShaderWriter::ContextSupport>();

    c.def("__init__", make_constructor(&ShaderWriterWrapper<>::New))
        .def(
            "Write",
            &ShaderWriterWrapper<>::Write,
            &ShaderWriterWrapper<>::default_Write,
            (boost::python::arg("self")),
            "Method called to properly export the material.")
        .def(
            "HasMaterialDependencies",
            &ShaderWriterWrapper<>::HasMaterialDependencies,
            &ShaderWriterWrapper<>::default_HasMaterialDependencies,
            (boost::python::arg("self")),
            "Reports whether the ShaderWriter needs additional dependent materials to be exported.")
        .def(
            "PostWrite",
            &ShaderWriterWrapper<>::PostWrite,
            &ShaderWriterWrapper<>::default_PostWrite,
            (boost::python::arg("self")),
            "Method called after all materials are exported.")
        .def(
            "GetMaterial",
            &ShaderWriterWrapper<>::GetMaterialAnimHandle,
            (boost::python::arg("self")),
            "Get the MAXScript AnimHandle on the material being exported.")
        .def(
            "GetSubMtlDependencies",
            &ShaderWriterWrapper<>::GetSubMtlDependencies,
            &ShaderWriterWrapper<>::default_GetSubMtlDependencies,
            boost::python::arg("self"),
            "Retrieve the dependent materials")
        .def(
            "GetUsdPrim",
            &ShaderWriterWrapper<>::GetUsdPrim,
            boost::python::return_internal_reference<>(),
            (boost::python::arg("self")),
            "Get the USD prim being written to.")
        .def(
            "SetUsdPrim",
            &::unprotect_SetUsdPrim,
            (boost::python::arg("self")),
            "Set the USD Shade prim.")
        .def(
            "GetExportArgs",
            &::unprotect_GetExportArgs,
            (boost::python::arg("self")),
            "Get the current global export args in effect.")
        .def(
            "GetMaterialsToPrimsMap",
            &::unprotect_GetMaterialsToPrimsMap,
            (boost::python::arg("self")),
            "Gets the current map of exported materials (handle) and their paths.")
        .def(
            "GetUsdStage",
            &MaxUsdShaderWriter::GetUsdStage,
            boost::python::return_value_policy<boost::python::return_by_value>(),
            (boost::python::arg("self")),
            "Get the USD stage being written to.")
        .def(
            "GetUsdPath",
            &MaxUsdShaderWriter::GetUsdPath,
            boost::python::return_value_policy<boost::python::return_by_value>(),
            (boost::python::arg("self")),
            "Get the USD prim destination.")
        .def(
            "GetFilename",
            &MaxUsdShaderWriter::GetFilename,
            boost::python::return_value_policy<boost::python::return_by_value>(),
            (boost::python::arg("self")),
            "Get the filename and path of where the stage is written on disk.")
        .def(
            "IsUSDZFile",
            &MaxUsdShaderWriter::IsUSDZFile,
            boost::python::return_value_policy<boost::python::return_by_value>(),
            (boost::python::arg("self")),
            "Get whether the file to be exported is a USDZ file.")
        .def(
            "Register",
            &ShaderWriterWrapper<>::Register,
            (boost::python::args("shader_writer_class", "material_non_localized_name")),
            "Static method to register an ShaderWriter into the ShaderWriterRegistry.")
        .staticmethod("Register")
        .def("Unregister", &ShaderWriterWrapper<>::Unregister)
        .staticmethod("Unregister");
}
