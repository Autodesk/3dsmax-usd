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
#include "wrapInterval.h"
#include "wrapUSDSceneBuilderOptions.h"

#include <MaxUsd/Builders/USDSceneBuilderOptions.h>
#include <MaxUsd/Translators/PrimWriterRegistry.h>
#include <MaxUsd/Translators/WriteJobContext.h>

#include <pxr/base/tf/api.h>
#include <pxr/base/tf/pyContainerConversions.h>
#include <pxr/base/tf/pyEnum.h>
#include <pxr/base/tf/pyPolymorphic.h>
#include <pxr/base/tf/registryManager.h>

#include <Python.h>
#include <boost/python/class.hpp>
#include <boost/python/def.hpp>
#include <boost/python/make_constructor.hpp>
#include <boost/python/return_value_policy.hpp>
#include <boost/python/wrapper.hpp>

PXR_NAMESPACE_USING_DIRECTIVE

//----------------------------------------------------------------------------------------------------------------------
/// \brief  boost python binding for the MaxUsdPrimWriter
//----------------------------------------------------------------------------------------------------------------------
class PrimWriterWrapper
    : public MaxUsdPrimWriter
    , public TfPyPolymorphic<MaxUsdPrimWriter>
{
public:
    typedef PrimWriterWrapper This;

    PrimWriterWrapper(const MaxUsdWriteJobContext& jobCtx, INode* node)
        : MaxUsdPrimWriter(jobCtx, node)
    {
    }

    static std::shared_ptr<This> New(uintptr_t createdWrapper)
    {
        return *((std::shared_ptr<This>*)createdWrapper);
    }

    ~PrimWriterWrapper() override = default;

    const UsdStage& GetUsdStage() const { return *get_pointer(MaxUsdPrimWriter::GetUsdStage()); }

    const ULONG GetNodeHandle() const { return MaxUsdPrimWriter::GetNode()->GetHandle(); }

    bool
    default_Write(UsdPrim& targetPrim, bool applyOffsetTransform, const MaxUsd::ExportTime& time)
    {
        return MaxUsdPrimWriter::Write(targetPrim, applyOffsetTransform, time);
    }
    bool
    Write(UsdPrim& targetPrim, bool applyOffsetTransform, const MaxUsd::ExportTime& time) override
    {
        return this->template CallVirtual<>("Write", &This::default_Write)(
            targetPrim, applyOffsetTransform, time);
    }

    bool default_PostExport(UsdPrim& targetPrim)
    {
        return MaxUsdPrimWriter::PostExport(targetPrim);
    }
    bool PostExport(UsdPrim& targetPrim) override
    {
        return this->template CallVirtual<>("PostExport", &This::default_PostExport)(targetPrim);
    }

    TfToken default_GetPrimType() { return MaxUsdPrimWriter::GetPrimType(); }
    TfToken GetPrimType() override
    {
        return this->template CallVirtual<>("GetPrimType", &This::default_GetPrimType)();
    }

    std::string default_GetPrimName(const std::string& suggestedName)
    {
        return MaxUsdPrimWriter::GetPrimName(suggestedName);
    }
    std::string GetPrimName(const std::string& suggestedName) override
    {
        return this->template CallVirtual<>("GetPrimName", &This::default_GetPrimName)(
            suggestedName);
    }

    TfToken default_GetObjectPrimSuffix() { return MaxUsdPrimWriter::GetObjectPrimSuffix(); }
    TfToken GetObjectPrimSuffix() override
    {
        return this->template CallVirtual<>(
            "GetObjectPrimSuffix", &This::default_GetObjectPrimSuffix)();
    }

    bool default_HandlesObjectOffsetTransform()
    {
        return MaxUsdPrimWriter::HandlesObjectOffsetTransform();
    }
    bool HandlesObjectOffsetTransform() override
    {
        return this->template CallVirtual<>(
            "HandlesObjectOffsetTransform", &This::default_HandlesObjectOffsetTransform)();
    }

    IntervalWrapper default_GetValidityInterval(double frame)
    {
        return IntervalWrapper(
            MaxUsdPrimWriter::GetValidityInterval(MaxUsd::GetTimeValueFromFrame(frame)));
    }
    Interval GetValidityInterval(const TimeValue& time) override
    {
        return this->template CallVirtual<>(
            "GetValidityInterval",
            &This::default_GetValidityInterval)(MaxUsd::GetFrameFromTimeValue(time));
    }

    MaxUsd::XformSplitRequirement default_RequiresXformPrim()
    {
        return MaxUsdPrimWriter::RequiresXformPrim();
    }
    MaxUsd::XformSplitRequirement RequiresXformPrim() override
    {
        return this->template CallVirtual<>(
            "RequiresXformPrim", &This::default_RequiresXformPrim)();
    }

    MaxUsd::MaterialAssignRequirement default_RequiresMaterialAssignment()
    {
        return MaxUsdPrimWriter::RequiresMaterialAssignment();
    }
    MaxUsd::MaterialAssignRequirement RequiresMaterialAssignment() override
    {
        return this->template CallVirtual<>(
            "RequiresMaterialAssignment", &This::default_RequiresMaterialAssignment)();
    }

    MaxUsd::InstancingRequirement default_RequiresInstancing()
    {
        return MaxUsdPrimWriter::RequiresInstancing();
    }
    MaxUsd::InstancingRequirement RequiresInstancing() override
    {
        return this->template CallVirtual<>(
            "RequiresInstancing", &This::default_RequiresInstancing)();
    }

    //---------------------------------------------------------------------------------------------
    /// \brief  wraps a factory function that allows registering an updated Python class
    //---------------------------------------------------------------------------------------------
    class FactoryFnWrapper : public MaxUsdPythonObjectRegistry
    {
    public:
        // Instances of this class act as "function objects" that are fully compatible with the
        // std::function requested by MaxUsdPrimWriterRegistry::Register. These will create
        // python wrappers based on the latest class registered.
        MaxUsdPrimWriterSharedPtr operator()(const MaxUsdWriteJobContext& jobCtx, INode* node)
        {
            boost::python::object pyClass = GetPythonObject(_classIndex);
            auto                  sptr = std::make_shared<This>(jobCtx, node);
            TfPyLock              pyLock;
            boost::python::object instance = pyClass((uintptr_t)&sptr);
            boost::python::incref(instance.ptr());
            initialize_wrapper(instance.ptr(), sptr.get());
            return sptr;
        }

        // We can have multiple function objects, this one adapts the CanExport function:
        ContextSupport operator()(INode* node, const MaxUsd::USDSceneBuilderOptions& exportArgs)
        {
            boost::python::object pyClass = GetPythonObject(_classIndex);
            if (!pyClass) {
                // Prototype was unregistered
                return ContextSupport::Unsupported;
            }
            TfPyLock pyLock;

            std::string canExport = "CanExport";

            if (!PyObject_HasAttrString(pyClass.ptr(), canExport.c_str())) {
                MaxUsd::Log::Error(
                    "Registered python PrimWriter \"{}\" has no suitable CanExport(node, "
                    "exportArgs) method.",
                    boost::python::extract<std::string>(pyClass.attr("__name__"))());
                return ContextSupport::Unsupported;
            }

            boost::python::object CanExport = pyClass.attr("CanExport");
            PyObject*             callable = CanExport.ptr();
            try {
                auto res = boost::python::call<int>(
                    callable, node->GetHandle(), USDSceneBuilderOptionsWrapper(exportArgs));
                return ContextSupport(res);
            } catch (...) {
                MaxUsd::Log::Error("Unable to call the CanExport(node, exportArgs) method of the "
                                   "registered PrimWriter.");
            }
            return ContextSupport::Unsupported;
        }

        // Create a new wrapper for a Python class that is seen for the first time for a given
        // purpose. If we already have a registration for this purpose: update the class to
        // allow the previously issued factory function to use it.
        static FactoryFnWrapper
        Register(boost::python::object cl, const std::string& usdPrimWriterId, bool& updated)
        {
            size_t classIndex = RegisterPythonObject(cl, GetKey(cl, usdPrimWriterId));
            updated = classIndex == MaxUsdPythonObjectRegistry::UPDATED;
            // Return a new factory function:
            return FactoryFnWrapper { classIndex };
        }

        // Unregister a class for a given purpose. This will cause the associated factory
        // function to stop producing this Python class.
        static void Unregister(boost::python::object cl, const std::string& usdPrimWriterId)
        {
            UnregisterPythonObject(cl, GetKey(cl, usdPrimWriterId));
        }

    private:
        // Function object constructor. Requires only the index of the Python class to use.
        FactoryFnWrapper(size_t classIndex)
            : _classIndex(classIndex) {};

        size_t _classIndex;

        // Generates a unique key based on the name of the class, along with the class
        // purpose:
        static std::string GetKey(boost::python::object cl, const std::string& usdPrimWriterId)
        {
            return ClassName(cl) + "," + usdPrimWriterId + "," + ",PrimWriter";
        }
    };

    static void Register(boost::python::object cl, const TfToken& usdPrimWriterId)
    {
        bool             updated = false;
        FactoryFnWrapper fn = FactoryFnWrapper::Register(cl, usdPrimWriterId, updated);
        if (!updated) {
            MaxUsdPrimWriterRegistry::Register(usdPrimWriterId, fn, fn, true);
        }
    }

    static void Unregister(boost::python::object cl, const TfToken& usdPrimWriterId)
    {
        FactoryFnWrapper::Unregister(cl, usdPrimWriterId);
        MaxUsdPrimWriterRegistry::Unregister(usdPrimWriterId);
    }
};

namespace {

// This class is used to expose protected members of MaxUsdPrimWriter to Python
class PrimWriterAllowProtected : public MaxUsdPrimWriter
{
    typedef MaxUsdPrimWriter base_t;

public:
    USDSceneBuilderOptionsWrapper GetExportArgs() const
    {
        return USDSceneBuilderOptionsWrapper(base_t::GetExportArgs());
    }

    std::string GetFilename() const { return base_t::GetFilename(); }
};

USDSceneBuilderOptionsWrapper unprotect_GetExportArgs(MaxUsdPrimWriter& pw)
{
    return reinterpret_cast<PrimWriterAllowProtected&>(pw).GetExportArgs();
}

std::string unprotect_GetFilename(MaxUsdPrimWriter& pw)
{
    return reinterpret_cast<PrimWriterAllowProtected&>(pw).GetFilename();
}
} // namespace

TF_REGISTRY_FUNCTION(TfEnum)
{
    TF_ADD_ENUM_NAME(MaxUsdPrimWriter::ContextSupport::Supported, "Supported");
    TF_ADD_ENUM_NAME(MaxUsdPrimWriter::ContextSupport::Fallback, "Fallback");
    TF_ADD_ENUM_NAME(MaxUsdPrimWriter::ContextSupport::Unsupported, "Unsupported");

    TF_ADD_ENUM_NAME(MaxUsd::XformSplitRequirement::ForOffsetObjects, "ForOffsetObjects");
    TF_ADD_ENUM_NAME(MaxUsd::XformSplitRequirement::Always, "Always");
    TF_ADD_ENUM_NAME(MaxUsd::XformSplitRequirement::Never, "Never");

    TF_ADD_ENUM_NAME(MaxUsd::MaterialAssignRequirement::Default, "Default");
    TF_ADD_ENUM_NAME(MaxUsd::MaterialAssignRequirement::NoAssignment, "NoAssignment");

    TF_ADD_ENUM_NAME(MaxUsd::InstancingRequirement::Default, "Default");
    TF_ADD_ENUM_NAME(MaxUsd::InstancingRequirement::NoInstancing, "NoInstancing");
}

void wrapPrimWriter()
{
    boost::python::class_<PrimWriterWrapper, boost::noncopyable> c(
        "PrimWriter",
        "PrimWriter base class from which object/prim writers need to inherit from.\n"
        "The PrimWriter is only responsible for providing translation of the 3ds Max Object "
        "referenced by the received Node. It should therefor not attempt to handle instancing, "
        "material assignment, and the transform of the Node itself. Instancing is handled by the "
        "calling code - if an object is instanced across multiple nodes, the PrimWriter is only "
        "called once, on the first node referencing the instanced object. The required Xform prim "
        "hierarchy is already generated. Similarly, the Node's transform is applied by the calling "
        "code, on the UsdGeomXformable prim built by the PrimWriter, *after* it is run. If the USD "
        "prim is *not* a UsdGeomXformable, a warning is raised, but it doesn't prevent the export "
        "from continuing.",
        boost::python::no_init);

    boost::python::scope s(c);

    TfPyWrapEnum<MaxUsdPrimWriter::ContextSupport>();
    TfPyWrapEnum<MaxUsd::XformSplitRequirement>();
    TfPyWrapEnum<MaxUsd::MaterialAssignRequirement>();
    TfPyWrapEnum<MaxUsd::InstancingRequirement>();

    c.def("__init__", boost::python::make_constructor(&PrimWriterWrapper::New))
        .def(
            "Write",
            &PrimWriterWrapper::default_Write,
            (boost::python::args("self", "target_prim", "apply_offset_transform")),
            "Method for writing the prim's attribute for the given context. This is where the "
            "translation from the 3ds Max object to the USD prim happens.")
        .def(
            "PostExport",
            &PrimWriterWrapper::default_PostExport,
            (boost::python::args("self", "target_prim")),
            "Method called after all other prims have been written to the stage.")
        .def(
            "GetNodeHandle",
            &PrimWriterWrapper::GetNodeHandle,
            (boost::python::args("self")),
            "The handle of the node that will be exported by this prim writer.")
        .def(
            "GetPrimType",
            &PrimWriterWrapper::default_GetPrimType,
            (boost::python::args("self")),
            "The prim type you are writing to. For performance reasons, all prims get created "
            "ahead of time in a single `SdfChangeBlock`. This means the prim writers are not "
            "responsible for creating the prims. The type specified here is mostly a hint for that "
            "first creation pass and, if required, it can be overridden from the `Write()` method "
            "(by defining a prim at the same path with a different type). Unless you always force "
            "the creation of an Xform prim (see `RequireXformPrim()`), you should return an "
            "Xformable type here; otherwise, it is not possible to apply the node's transform onto "
            "the prim later (an error will be raised in this scenario).")
        .def(
            "GetPrimName",
            &PrimWriterWrapper::default_GetPrimName,
            (boost::python::args("self", "suggested_name")),
            "Returns the name that should be used for the prim. The base implementation should be "
            "sufficient in most cases, unless prim writers want to customize the prim's name. If "
            "so, it is their responsibility to ensure that the given name is unique amongst "
            "siblings. The `suggestedName` is what the base implementation uses, from the node's "
            "name, and uniqueness amongst siblings is ensured.")
        .def(
            "GetObjectPrimSuffix",
            &PrimWriterWrapper::default_GetObjectPrimSuffix,
            (boost::python::args("self")),
            "In a few scenarios, you need two prims to properly represent an INode. One for its "
            "transform, and one for the object it references (for example, in the case of a "
            "non-identity object offset transform, it must not inherit the transform, so you "
            "cannot use a single prim for the INode). When you do need to perform such a split, "
            "the object's prim has the same name as the node's prim, with an added suffix - the "
            "string returned here. Defaults to \"Object\".")
        .def(
            "GetValidityInterval",
            &PrimWriterWrapper::default_GetValidityInterval,
            (boost::python::args("self", "frame")),
            "Returns the validity interval of the exported prim at the given 3dsMax frame. In "
            "other words, until when what we export remains valid. This will guide the exporter in "
            "figuring out what frames need to be exported for this object. The default "
            "implementation return the validity interval of the object itself, which would "
            "basically mean that the Write(..) method would be called any time any property of the "
            "object changes.")
        .def(
            "HandlesObjectOffsetTransform",
            &PrimWriterWrapper::default_HandlesObjectOffsetTransform,
            (boost::python::arg("self")),
            "Choose whether to handle the object offset transform in the Write() manually or not.")
        .def(
            "RequiresXformPrim",
            &PrimWriterWrapper::default_RequiresXformPrim,
            (boost::python::arg("self")),
            "Returns the requirement to split the object from its transform in the scene.")
        .def(
            "RequiresMaterialAssignment",
            &PrimWriterWrapper::default_RequiresMaterialAssignment,
            (boost::python::arg("self")),
            "Returns the material assignment requirement for the object.")
        .def(
            "RequiresInstancing",
            &PrimWriterWrapper::default_RequiresInstancing,
            (boost::python::arg("self")),
            "Returns whether default instancing of the objects handled by the writer should be "
            "used.")
        .def(
            "GetExportArgs",
            &::unprotect_GetExportArgs,
            (boost::python::arg("self")),
            "Get the current global export args in effect.")
        .def(
            "GetFilename",
            &::unprotect_GetFilename,
            (boost::python::arg("self")),
            "Get the filename and path of where the stage is written on disk.")
        .def(
            "GetNodesToPrims",
            &MaxUsdPrimWriter::GetNodesToPrims,
            (boost::python::arg("self")),
            "Get a dictionary the nodes being exported and their respective prims.")
        .def(
            "GetUsdStage",
            &MaxUsdPrimWriter::GetUsdStage,
            boost::python::return_value_policy<boost::python::return_by_value>(),
            (boost::python::arg("self")),
            "Get the USD stage being written to.")
        .def(
            "Register",
            &PrimWriterWrapper::Register,
            (boost::python::args("derived_primwriter_class", "primwriter_given_name")),
            "Static method to register a PrimWriter into the PrimWriterRegistry.")
        .staticmethod("Register")
        .def("Unregister", &PrimWriterWrapper::Unregister)
        .staticmethod("Unregister");
}
