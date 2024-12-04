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

#include <MaxUsd/Chaser/ExportChaser.h>
#include <MaxUsd/Chaser/ExportChaserRegistry.h>

#include <pxr/base/tf/pyPolymorphic.h>

#include <boost/python/class.hpp>
#include <boost/python/def.hpp>
#include <boost/python/make_constructor.hpp>
#include <boost/python/overloads.hpp>
#include <boost/python/wrapper.hpp>

PXR_NAMESPACE_USING_DIRECTIVE

class ExportChaserRegistryFactoryContextWrapper : public MaxUsdExportChaserRegistry::FactoryContext
{
public:
    typedef ExportChaserRegistryFactoryContextWrapper  This;
    typedef MaxUsdExportChaserRegistry::FactoryContext base_t;

    ExportChaserRegistryFactoryContextWrapper() = default;
    ExportChaserRegistryFactoryContextWrapper(
        const MaxUsdExportChaserRegistry::FactoryContext& ctx);

    boost::python::dict GetPrimsToNodeHandles() const { return primsToNodeHandles; }

    USDSceneBuilderOptionsWrapper GetJobArgs() const
    {
        return USDSceneBuilderOptionsWrapper(base_t::GetJobArgs());
    }

    const std::string GetFilename() const { return base_t::GetFilename().string(); }

private:
    boost::python::dict primsToNodeHandles;
};

ExportChaserRegistryFactoryContextWrapper::ExportChaserRegistryFactoryContextWrapper(
    const MaxUsdExportChaserRegistry::FactoryContext& ctx)
    : MaxUsdExportChaserRegistry::FactoryContext(
          ctx.GetStage(),
          ctx.GetPrimToNodeMap(),
          ctx.GetJobArgs(),
          ctx.GetFilename())
{
    for (const auto& item : ctx.GetPrimToNodeMap()) {
        primsToNodeHandles[item.first] = item.second->GetHandle();
    }
}

//----------------------------------------------------------------------------------------------------------------------
/// \brief  boost python binding for the MaxUsdExportChaser
//----------------------------------------------------------------------------------------------------------------------
class ExportChaserWrapper
    : public MaxUsdExportChaser
    , public TfPyPolymorphic<MaxUsdExportChaser>
{
public:
    typedef ExportChaserWrapper This;
    typedef MaxUsdExportChaser  base_t;

    static ExportChaserWrapper*
    New(const ExportChaserRegistryFactoryContextWrapper& factoryContext, uintptr_t createdWrapper)
    {
        return (ExportChaserWrapper*)createdWrapper;
    }

    virtual ~ExportChaserWrapper() = default;

    bool default_PostExport() { return base_t::PostExport(); }
    bool PostExport() override
    {
        return this->CallVirtual<>("PostExport", &This::default_PostExport)();
    }

    //---------------------------------------------------------------------------------------------
    /// \brief  wraps a factory function that allows registering an updated Python class
    //---------------------------------------------------------------------------------------------
    class FactoryFnWrapper : public MaxUsdPythonObjectRegistry
    {
    public:
        // Instances of this class act as "function objects" that are fully compatible with the
        // std::function requested by MaxUsdExportChaserRegistry::Register. These will create
        // python wrappers based on the latest Class registered.
        MaxUsdExportChaser*
        operator()(const MaxUsdExportChaserRegistry::FactoryContext& factoryContext)
        {
            boost::python::object pyClass = GetPythonObject(_classIndex);
            if (!pyClass) {
                // Prototype was unregistered
                return nullptr;
            }
            auto     chaser = new ExportChaserWrapper();
            TfPyLock pyLock;

            try {
                boost::python::object instance = pyClass(
                    ExportChaserRegistryFactoryContextWrapper(factoryContext), (uintptr_t)chaser);
                boost::python::incref(instance.ptr());
                initialize_wrapper(instance.ptr(), chaser);
                return chaser;
            } catch (...) {
                // something went wrong in the chaser constructor
                PyErr_Print();
                return nullptr;
            }
        }

        // Create a new wrapper for a Python class that is seen for the first time for a given
        // purpose. If we already have a registration for this purpose: update the class to
        // allow the previously issued factory function to use it.
        static MaxUsdExportChaserRegistry::FactoryFn
        Register(boost::python::object cl, const std::string& exportChaserName)
        {
            size_t classIndex = RegisterPythonObject(cl, GetKey(cl, exportChaserName));
            if (classIndex != MaxUsdPythonObjectRegistry::UPDATED) {
                // Return a new factory function:
                return FactoryFnWrapper { classIndex };
            } else {
                // We already registered a factory function for this purpose:
                return nullptr;
            }
        }

        // Unregister a class for a given purpose. This will cause the associated factory
        // function to stop producing this Python class.
        static void Unregister(boost::python::object cl, const std::string& exportChaserName)
        {
            UnregisterPythonObject(cl, GetKey(cl, exportChaserName));
        }

    private:
        // Function object constructor. Requires only the index of the Python class to use.
        FactoryFnWrapper(size_t classIndex)
            : _classIndex(classIndex) { };

        size_t _classIndex;

        // Generates a unique key based on the name of the class, along with the class
        // purpose:
        static std::string GetKey(boost::python::object cl, const std::string& exportChaserName)
        {
            return ClassName(cl) + "," + exportChaserName + "," + ",ExportChaser";
        }
    };

    static void Register(
        boost::python::object cl,
        const std::string&    exportChaserName,
        const std::string&    niceName = {},
        const std::string&    description = {})
    {
        MaxUsdExportChaserRegistry::FactoryFn fn = FactoryFnWrapper::Register(cl, exportChaserName);
        if (fn) {
            MaxUsdExportChaserRegistry::GetInstance().RegisterFactory(
                exportChaserName,
                (niceName.empty() ? exportChaserName
                                  : niceName), // default to chaser name if niceName is empty
                description,
                fn,
                true);
        }
    }

    static void Unregister(boost::python::object cl, const std::string& exportChaserName)
    {
        FactoryFnWrapper::Unregister(cl, exportChaserName);
    }
};

BOOST_PYTHON_FUNCTION_OVERLOADS(RegisterExport_overloads, ExportChaserWrapper::Register, 2, 4);

//----------------------------------------------------------------------------------------------------------------------
void wrapExportChaserRegistryFactoryContext()
{
    boost::python::class_<ExportChaserRegistryFactoryContextWrapper>(
        "MaxUsdExportChaserRegistryFactoryContext",
        "Holds data that can be accessed when constructing an ExportChaser object. This class "
        "allows the plugin\n"
        "code to only know about the context object during construction and only the data it needs "
        "to construct.",
        boost::python::no_init)
        .def(
            "GetStage",
            &MaxUsdExportChaserRegistry::FactoryContext::GetStage,
            (boost::python::arg("self")),
            "Get the USD stage being written to")
        .def(
            "GetPrimsToNodeHandles",
            &ExportChaserRegistryFactoryContextWrapper::GetPrimsToNodeHandles,
            (boost::python::arg("self")),
            "Returns a dictionary that maps the source USD prim paths to the imported MAXScript "
            "NodeHandles.")
        .def(
            "GetJobArgs",
            &ExportChaserRegistryFactoryContextWrapper::GetJobArgs,
            (boost::python::arg("self")),
            "Get the current global export args in effect.")
        .def(
            "GetFilename",
            &ExportChaserRegistryFactoryContextWrapper::GetFilename,
            boost::python::return_value_policy<boost::python::return_by_value>(),
            (boost::python::arg("self")),
            "Get the file name and path where the stage is written to on disk.");
}

//----------------------------------------------------------------------------------------------------------------------
void wrapExportChaser()
{
    typedef MaxUsdExportChaser This;

    boost::python::class_<ExportChaserWrapper, boost::noncopyable>(
        "ExportChaser",
        "ExportChaser base class from which export chasers need to inherit from. An ExportChaser "
        "instance is \n"
        "created at each export and called at the end of the export process. Chasers should not "
        "modify \n"
        "the structure of the USD file. Use this to make small changes or to add attributes, in a\n"
        "non-destructive way, to an exported stage.",
        boost::python::no_init)
        .def(
            "__init__",
            make_constructor(&ExportChaserWrapper::New),
            "Class constructor. Chasers should save necessary data when constructed. The "
            "constructor\n"
            "receives the Context. Save what you need from it so that you can make use of the "
            "information\n"
            "at the Chaser execution later.")
        .def(
            "PostExport",
            &This::PostExport,
            &ExportChaserWrapper::default_PostExport,
            (boost::python::arg("self")),
            "Method being called at the end of the standard export process.")
        .def(
            "Register",
            &ExportChaserWrapper::Register,
            RegisterExport_overloads(
                boost::python::args(
                    "export_chaser_class", "export_chaser_name", "nice_name", "description"),
                "Static method to register an ExportChaser into the ChaserRegistry."))
        .staticmethod("Register")
        .def("Unregister", &ExportChaserWrapper::Unregister)
        .staticmethod("Unregister");
}
