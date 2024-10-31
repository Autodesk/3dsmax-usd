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
#include "PythonObjectRegistry.h"
#include "wrapMaxSceneBuilderOptions.h"
#include "wrapReadJobContext.h"

#include <MaxUsd/Translators/PrimReader.h>
#include <MaxUsd/Translators/PrimReaderRegistry.h>
#include <MaxUsd/Translators/RegistryHelper.h>
#include <MaxUsd/Translators/ShaderReader.h>
#include <MaxUsd/Translators/ShaderReaderRegistry.h>
#include <MaxUsd/Translators/ShadingModeImporter.h>
#include <MaxUsd/Translators/TranslatorXformable.h>

#include <pxr/base/tf/pyContainerConversions.h>
#include <pxr/base/tf/pyEnum.h>
#include <pxr/base/tf/pyPolymorphic.h>
#include <pxr/base/tf/pyResultConversions.h>

#include <boost/python/class.hpp>
#include <boost/python/def.hpp>
#include <boost/python/make_constructor.hpp>
#include <boost/python/overloads.hpp>
#include <boost/python/pure_virtual.hpp>
#include <boost/python/return_internal_reference.hpp>
#include <boost/python/return_value_policy.hpp>
#include <boost/python/wrapper.hpp>
#include <inode.h>

PXR_NAMESPACE_USING_DIRECTIVE

//----------------------------------------------------------------------------------------------------------------------
/// \brief  boost python binding for the MaxUsdPrimReader
//----------------------------------------------------------------------------------------------------------------------
template <typename T = MaxUsdPrimReader>
class PrimReaderWrapper
    : public T
    , public TfPyPolymorphic<MaxUsdPrimReader>
{
public:
    typedef PrimReaderWrapper This;
    typedef T                 base_t;

    PrimReaderWrapper(const UsdPrim& prim, MaxUsdReadJobContext& args)
        : T(prim, args)
    {
    }

    static std::shared_ptr<This> New(uintptr_t createdWrapper)
    {
        return *((std::shared_ptr<This>*)createdWrapper);
    }

    virtual ~PrimReaderWrapper() = default;

    bool Read() override { return this->template CallPureVirtual<bool>("Read")(); }

    bool default_HasPostReadSubtree() const { return base_t::HasPostReadSubtree(); }
    bool HasPostReadSubtree() const override
    {
        return this->template CallVirtual<bool>(
            "HasPostReadSubtree", &This::default_HasPostReadSubtree)();
    }

    void default_PostReadSubtree() { base_t::PostReadSubtree(); }
    void PostReadSubtree() override
    {
        this->template CallVirtual<>("PostReadSubtree", &This::default_PostReadSubtree)();
    }

    void default_InstanceCreated(const UsdPrim& prim, INT_PTR instance)
    {
        base_t::InstanceCreated(
            prim, dynamic_cast<INode*>(MaxUsdReadJobContextWrapper::GetReferenceTarget(instance)));
    }

    void InstanceCreatedWrapped(const UsdPrim& prim, INT_PTR instance)
    {
        this->template CallVirtual<>("InstanceCreated", &This::default_InstanceCreated)(
            prim, instance);
    }
    void InstanceCreated(const UsdPrim& prim, INode* instance) override
    {
        InstanceCreatedWrapped(prim, MaxUsdReadJobContextWrapper::GetAnimHandle(instance));
    }

    // Must be declared for python to be able to call the protected function _GetArgs in
    // MaxUsdPrimReader
    const UsdPrim&                GetUsdPrim() { return base_t::GetUsdPrim(); }
    MaxSceneBuilderOptionsWrapper GetImportArgs()
    {
        return MaxSceneBuilderOptionsWrapper(base_t::GetArgs());
    }

    MaxUsdReadJobContextWrapper GetJobContext() const
    {
        return MaxUsdReadJobContextWrapper(base_t::GetJobContext());
    }

    // helper method to call MaxUsdTranslatorXformable::Read from Python
    // \param correction Any correction to apply on the UsdPrim transform to properly position the
    // 3ds Max node (identity by default)
    void ReadXformable(boost::python::list correction = boost::python::list())
    {
        auto refTarget
            = base_t::GetJobContext().GetMaxRefTargetHandle(base_t::GetUsdPrim().GetPath(), false);
        if (refTarget) {
            // identity matrix
            Matrix3 correctionMatrix;
            if (!correction.is_none() && len(correction) == 4) {
                Point3 v[4];
                for (int i = 0; i < 4; ++i) {
                    const boost::python::list& row
                        = boost::python::extract<boost::python::list>(correction[i]);
                    if (len(row) != 3) {
                        MaxUsd::Log::Warn("Malformed correction matrix. Defaulting to Identity.");
                        break;
                    }
                    for (int j = 0; j < 3; ++j) {
                        v[i][j] = boost::python::extract<float>(row[j]);
                    }
                }
                correctionMatrix.Set(v[0], v[1], v[2], v[3]);
            }

            MaxUsdTranslatorXformable::Read(
                base_t::GetUsdPrim(),
                static_cast<INode*>(refTarget),
                base_t::GetJobContext(),
                correctionMatrix);
        }
    }

    //---------------------------------------------------------------------------------------------
    /// \brief  wraps a factory function that allows registering an updated Python class
    //---------------------------------------------------------------------------------------------
    class FactoryFnWrapper : public MaxUsdPythonObjectRegistry
    {
    public:
        // Instances of this class act as "function objects" that are fully compatible with the
        // std::function requested by MaxUsdPrimReaderRegistry::Register. These will create
        // python wrappers based on the latest class registered.
        MaxUsdPrimReaderSharedPtr operator()(const UsdPrim& prim, MaxUsdReadJobContext& jobCtx)
        {
            boost::python::object pyClass = GetPythonObject(_classIndex);
            if (!pyClass) {
                // Prototype was unregistered
                return nullptr;
            }
            auto                  sptr = std::make_shared<This>(prim, jobCtx);
            TfPyLock              pyLock;
            boost::python::object instance = pyClass((uintptr_t)&sptr);
            boost::python::incref(instance.ptr());
            initialize_wrapper(instance.ptr(), sptr.get());
            return sptr;
        }

        // We can have multiple function objects, this one adapts the CanImport function:
        MaxUsdPrimReader::ContextSupport
        operator()(const MaxUsd::MaxSceneBuilderOptions& args, const UsdPrim& importPrim)
        {
            boost::python::object pyClass = GetPythonObject(_classIndex);
            if (!pyClass) {
                // Prototype was unregistered
                return MaxUsdPrimReader::ContextSupport::Unsupported;
            }
            TfPyLock pyLock;
            if (PyObject_HasAttrString(pyClass.ptr(), "CanImport"))

            {
                boost::python::object CanImport = pyClass.attr("CanImport");
                PyObject*             callable = CanImport.ptr();
                try {
                    auto res = boost::python::call<int>(
                        callable, MaxSceneBuilderOptionsWrapper(args), importPrim);
                    return MaxUsdPrimReader::ContextSupport(res);
                } catch (...) {
                    MaxUsd::Log::Error("Unable to call the CanImport(importArgs, importPrim) "
                                       "method of the registered PrimReader.");
                }
            }
            return MaxUsdPrimReader::CanImport(args, importPrim);
        }

        // Create a new wrapper for a Python class that is seen for the first time for a given
        // purpose. If we already have a registration for this purpose: update the class to
        // allow the previously issued factory function to use it.
        static FactoryFnWrapper
        Register(boost::python::object cl, const std::string& typeName, bool& updated)
        {
            size_t classIndex = RegisterPythonObject(cl, GetKey(cl, typeName));
            updated = classIndex == MaxUsdPythonObjectRegistry::UPDATED;
            // Return a new factory function:
            return FactoryFnWrapper { classIndex };
        }

        // Unregister a class for a given purpose. This will cause the associated factory
        // function to stop producing this Python class.
        static void Unregister(boost::python::object cl, const std::string& typeName)
        {
            UnregisterPythonObject(cl, GetKey(cl, typeName));
        }

    private:
        // Function object constructor. Requires only the index of the Python class to use.
        FactoryFnWrapper(size_t classIndex)
            : _classIndex(classIndex) {};

        size_t _classIndex;

        // Generates a unique key based on the name of the class, along with the class
        // purpose:
        static std::string GetKey(boost::python::object cl, const std::string& typeName)
        {
            return ClassName(cl) + "," + typeName + "," + ",PrimReader";
        }
    };

    static void Register(boost::python::object cl, const std::string& typeName)
    {
        bool             updated = false;
        FactoryFnWrapper fn = FactoryFnWrapper::Register(cl, typeName, updated);
        if (!updated) {
            auto type = TfType::FindByName(typeName);
            MaxUsdPrimReaderRegistry::Register(type, fn, fn, true);
        }
    }

    static void Unregister(boost::python::object cl, const std::string& typeName)
    {
        FactoryFnWrapper::Unregister(cl, typeName);
    }
};

//----------------------------------------------------------------------------------------------------------------------
/// \brief  boost python binding for the MaxUsdShaderReader
//----------------------------------------------------------------------------------------------------------------------
class ShaderReaderWrapper : public PrimReaderWrapper<MaxUsdShaderReader>
{
public:
    typedef ShaderReaderWrapper This;
    typedef MaxUsdShaderReader  base_t;

    ShaderReaderWrapper(const UsdPrim& prim, MaxUsdReadJobContext& args)
        : PrimReaderWrapper<MaxUsdShaderReader>(prim, args)
    {
    }

    static std::shared_ptr<This> New(uintptr_t createdWrapper)
    {
        return *((std::shared_ptr<This>*)createdWrapper);
    }

    virtual ~ShaderReaderWrapper() { _downstreamReader = nullptr; }

    const UsdPrim&                GetUsdPrim() { return base_t::GetUsdPrim(); }
    MaxSceneBuilderOptionsWrapper GetImportArgs()
    {
        return MaxSceneBuilderOptionsWrapper(base_t::GetArgs());
    }

    INT_PTR default_GetCreatedMaterial(
        const MaxUsdShadingModeImportContext& context,
        const UsdPrim&                        prim) const
    {
        return MaxUsdReadJobContextWrapper::GetAnimHandle(
            base_t::GetCreatedMaterial(context, prim));
    }
    INT_PTR GetCreatedMaterialAnimHandle(
        const MaxUsdShadingModeImportContext& context,
        const UsdPrim&                        prim) const
    {
        return this->CallVirtual<INT_PTR>(
            "GetCreatedMaterialAnimHandle", &This::default_GetCreatedMaterial)(context, prim);
    }

    void RegisterCreatedMaterial(const SdfPath& path, INT_PTR animHandle)
    {
        ReferenceTarget* ref = MaxUsdReadJobContextWrapper::GetReferenceTarget(animHandle);
        if (ref) {
            base_t::GetJobContext().RegisterNewMaxRefTargetHandle(path, ref);
        }
    }

    //---------------------------------------------------------------------------------------------
    /// \brief  wraps a factory function that allows registering an updated Python class
    //---------------------------------------------------------------------------------------------
    class FactoryFnWrapper : public MaxUsdPythonObjectRegistry
    {
    public:
        // Instances of this class act as "function objects" that are fully compatible with the
        // std::function requested by MaxUsdShaderReaderRegistry::Register. These will create
        // python wrappers based on the latest class registered.
        MaxUsdPrimReaderSharedPtr operator()(const UsdPrim& prim, MaxUsdReadJobContext& jobCtx)
        {
            boost::python::object pyClass = GetPythonObject(_classIndex);
            if (!pyClass) {
                // Prototype was unregistered
                return nullptr;
            }
            auto                  sptr = std::make_shared<This>(prim, jobCtx);
            TfPyLock              pyLock;
            boost::python::object instance = pyClass((uintptr_t)&sptr);
            boost::python::incref(instance.ptr());
            initialize_wrapper(instance.ptr(), sptr.get());
            return sptr;
        }

        // We can have multiple function objects, this one adapts the CanImport function:
        MaxUsdShaderReader::ContextSupport operator()(const MaxUsd::MaxSceneBuilderOptions& args)
        {
            boost::python::object pyClass = GetPythonObject(_classIndex);
            if (!pyClass) {
                // Prototype was unregistered
                return MaxUsdShaderReader::ContextSupport::Unsupported;
            }
            TfPyLock pyLock;
            if (PyObject_HasAttrString(pyClass.ptr(), "CanImport"))

            {
                boost::python::object CanImport = pyClass.attr("CanImport");
                PyObject*             callable = CanImport.ptr();
                try {
                    auto res
                        = boost::python::call<int>(callable, MaxSceneBuilderOptionsWrapper(args));
                    return MaxUsdShaderReader::ContextSupport(res);
                } catch (...) {
                    MaxUsd::Log::Error("Unable to call the CanImport(importArgs) method of the "
                                       "registered ShaderReader.");
                }
            }
            return MaxUsdShaderReader::CanImport(args);
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
            : _classIndex(classIndex) {};

        size_t _classIndex;

        // Generates a unique key based on the name of the class, along with the class
        // purpose:
        static std::string GetKey(boost::python::object cl, const std::string& usdShaderId)
        {
            return ClassName(cl) + "," + usdShaderId + "," + ",ShaderReader";
        }
    };

    static void Register(boost::python::object cl, const TfToken& usdShaderId)
    {
        bool             updated = false;
        FactoryFnWrapper fn = FactoryFnWrapper::Register(cl, usdShaderId, updated);
        if (!updated) {
            MaxUsdShaderReaderRegistry::Register(usdShaderId, fn, fn, true);
        }
    }

    static void Unregister(boost::python::object cl, const TfToken& usdShaderId)
    {
        FactoryFnWrapper::Unregister(cl, usdShaderId);
    }

    std::shared_ptr<MaxUsdShaderReader> _downstreamReader;
};

//----------------------------------------------------------------------------------------------------------------------

TF_REGISTRY_FUNCTION(TfEnum)
{
    TF_ADD_ENUM_NAME(MaxUsdPrimReader::ContextSupport::Supported, "Supported");
    TF_ADD_ENUM_NAME(MaxUsdPrimReader::ContextSupport::Fallback, "Fallback");
    TF_ADD_ENUM_NAME(MaxUsdPrimReader::ContextSupport::Unsupported, "Unsupported");
}

BOOST_PYTHON_MEMBER_FUNCTION_OVERLOADS(
    readXformable_overload,
    PrimReaderWrapper::ReadXformable,
    0,
    1)

void wrapPrimReader()
{
    using namespace boost::python;
    typedef MaxUsdPrimReader This;

    class_<PrimReaderWrapper<>, boost::noncopyable> c("PrimReader", no_init);

    scope s(c);
    TfPyWrapEnum<MaxUsdPrimReader::ContextSupport>();

    c.def("__init__", make_constructor(&PrimReaderWrapper<>::New))
        .def(
            "Read",
            pure_virtual(&MaxUsdPrimReader::Read),
            (boost::python::arg("self")),
            "Method called to import a USD prim.")
        .def(
            "HasPostReadSubtree",
            &This::HasPostReadSubtree,
            &PrimReaderWrapper<>::default_HasPostReadSubtree,
            (boost::python::arg("self")),
            "Specifies whether this prim reader specifies a PostReadSubtree step.")
        .def(
            "PostReadSubtree",
            &This::PostReadSubtree,
            &PrimReaderWrapper<>::default_PostReadSubtree,
            (boost::python::arg("self")),
            "An additional import step that runs after all descendants of this prim have been "
            "processed.")
        .def(
            "InstanceCreated",
            &PrimReaderWrapper<>::InstanceCreatedWrapped,
            &PrimReaderWrapper<>::default_InstanceCreated,
            (boost::python::args("self", "prim", "anim_handle")),
            "Method called when a 3ds Max instance is created (cloned) from a Node which "
            "originally was created using this reader instance.")
        .def(
            "GetUsdPrim",
            &PrimReaderWrapper<>::GetUsdPrim,
            return_internal_reference<>(),
            (boost::python::arg("self")),
            "Get the UsdPrim on which the reader is acting.")
        .def(
            "GetArgs",
            &PrimReaderWrapper<>::GetImportArgs,
            (boost::python::arg("self")),
            "Get the current import arguments in effect.")
        .def(
            "GetJobContext",
            &PrimReaderWrapper<>::GetJobContext,
            (boost::python::arg("self")),
            "Get the read job context in effect.")

        .def(
            "ReadXformable",
            &PrimReaderWrapper<>::ReadXformable,
            readXformable_overload(
                (boost::python::arg("correction_matrix") = boost::python::object()),
                "Reads xform attributes from xformable and converts them into 3ds Max transform "
                "values. The `correction_matrix` is a correction ([[U],[V],[N],[T]] see Matrix3) "
                "to apply on the UsdPrim transform to properly position the 3ds Max node when "
                "there is an orientation difference between the two worlds."))
        .def(
            "Register",
            &PrimReaderWrapper<>::Register,
            (boost::python::args("prim_reader_class", "usd_prim_id")),
            "Static method to register an PrimReader into the PrimReaderRegistry.")
        .staticmethod("Register")
        .def("Unregister", &PrimReaderWrapper<>::Unregister)
        .staticmethod("Unregister");
}

//----------------------------------------------------------------------------------------------------------------------
void wrapShaderReader()
{
    using namespace boost::python;
    typedef MaxUsdShaderReader This;

    class_<ShaderReaderWrapper, bases<PrimReaderWrapper<>>, boost::noncopyable> c(
        "ShaderReader",
        "Base class for USD prim readers that import USD shader prims as 3ds Max materials.\n"
        "A ShaderReader instance is created for each material needing translation.",
        no_init);

    scope s(c);

    c.def("__init__", make_constructor(&ShaderReaderWrapper::New))
        .def(
            "Read",
            pure_virtual(&MaxUsdPrimReader::Read),
            (boost::python::arg("self")),
            "Method called to properly import the material.")
        .def(
            "GetCreatedMaterial",
            &ShaderReaderWrapper::GetCreatedMaterialAnimHandle,
            &ShaderReaderWrapper::default_GetCreatedMaterial,
            (boost::python::args("context", "prim")),
            "Get the MAXScript AnimHandle on the material created for the given Prim.")
        .def(
            "RegisterCreatedMaterial",
            &ShaderReaderWrapper::RegisterCreatedMaterial,
            (boost::python::args("self", "path", "anim_handle")),
            "Record 3ds Max animHandle as being created for the prim path")
        .def(
            "GetUsdPrim",
            &ShaderReaderWrapper::GetUsdPrim,
            return_internal_reference<>(),
            (boost::python::arg("self")),
            "Get the UsdPrim on which the reader is acting.")
        .def(
            "GetArgs",
            &ShaderReaderWrapper::GetImportArgs,
            (boost::python::arg("self")),
            "Get the current import arguments in effect.")
        .def(
            "Register",
            &ShaderReaderWrapper::Register,
            (boost::python::args("shader_reader_class", "usd_shader_id")),
            "Static method to register an ShaderReader into the ShaderReaderRegistry.")
        .staticmethod("Register")
        .def("Unregister", &ShaderReaderWrapper::Unregister)
        .staticmethod("Unregister");
}