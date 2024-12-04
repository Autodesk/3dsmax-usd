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
#include "wrapMaxSceneBuilderOptions.h"
#include "wrapReadJobContext.h"

#include <MaxUsd/Chaser/ImportChaser.h>
#include <MaxUsd/Chaser/ImportChaserRegistry.h>

#include <pxr/base/tf/pyPolymorphic.h>

#include <boost/python/class.hpp>
#include <boost/python/def.hpp>
#include <boost/python/make_constructor.hpp>
#include <boost/python/overloads.hpp>
#include <boost/python/wrapper.hpp>
#include <inode.h>

PXR_NAMESPACE_USING_DIRECTIVE

class ImportChaserRegistryFactoryContextWrapper : public MaxUsdImportChaserRegistry::FactoryContext
{
public:
    typedef ImportChaserRegistryFactoryContextWrapper  This;
    typedef MaxUsdImportChaserRegistry::FactoryContext base_t;

    ImportChaserRegistryFactoryContextWrapper() = default;
    ImportChaserRegistryFactoryContextWrapper(
        const MaxUsdImportChaserRegistry::FactoryContext& ctx);

    MaxUsdReadJobContextWrapper GetContext() const
    {
        return MaxUsdReadJobContextWrapper(base_t::GetContext());
    }

    MaxSceneBuilderOptionsWrapper GetJobArgs() const
    {
        return MaxSceneBuilderOptionsWrapper(base_t::GetJobArgs());
    }

    const std::string GetFilename() const { return base_t::GetFilename().string(); }

    boost::python::dict GetPrimsToNodeHandles() const { return primsToNodeHandles; }

private:
    boost::python::dict primsToNodeHandles;
};

ImportChaserRegistryFactoryContextWrapper::ImportChaserRegistryFactoryContextWrapper(
    const MaxUsdImportChaserRegistry::FactoryContext& ctx)
    : MaxUsdImportChaserRegistry::FactoryContext(
          *(new Usd_PrimFlagsPredicate()),
          ctx.GetContext(),
          ctx.GetFilename())
{
    const auto& nodeMap = ctx.GetContext().GetReferenceTargetRegistry();
    for (const std::pair<pxr::SdfPath, RefTargetHandle>& item : nodeMap) {
        INode* node = dynamic_cast<INode*>(item.second);
        if (node) {
            primsToNodeHandles[item.first] = node->GetHandle();
        }
    }
}

//----------------------------------------------------------------------------------------------------------------------
/// \brief  boost python binding for the MaxUsdImportChaser
//----------------------------------------------------------------------------------------------------------------------
class ImportChaserWrapper
    : public MaxUsdImportChaser
    , public TfPyPolymorphic<MaxUsdImportChaser>
{
public:
    typedef ImportChaserWrapper This;
    typedef MaxUsdImportChaser  base_t;

    static ImportChaserWrapper*
    New(const ImportChaserRegistryFactoryContextWrapper& factoryContext, uintptr_t createdWrapper)
    {
        return (ImportChaserWrapper*)createdWrapper;
    }

    virtual ~ImportChaserWrapper() = default;

    bool default_PostImport() { return base_t::PostImport(); }

    bool PostImport() override
    {
        return this->CallVirtual<>("PostImport", &This::default_PostImport)();
    }

    //---------------------------------------------------------------------------------------------
    /// \brief  wraps a factory function that allows registering an updated Python class
    //---------------------------------------------------------------------------------------------
    class FactoryFnWrapper : public MaxUsdPythonObjectRegistry
    {
    public:
        // Instances of this class act as "function objects" that are fully compatible with the
        // std::function requested by MaxUsdImportChaserRegistry::Register. These will create
        // python wrappers based on the latest Class registered.
        MaxUsdImportChaser*
        operator()(const MaxUsdImportChaserRegistry::FactoryContext& factoryContext)
        {
            boost::python::object pyClass = GetPythonObject(_classIndex);
            if (!pyClass) {
                // Prototype was unregistered
                return nullptr;
            }
            auto     chaser = new ImportChaserWrapper();
            TfPyLock pyLock;

            try {
                boost::python::object instance = pyClass(
                    ImportChaserRegistryFactoryContextWrapper(factoryContext), (uintptr_t)chaser);
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
        static MaxUsdImportChaserRegistry::FactoryFn
        Register(boost::python::object cl, const std::string& importChaserName)
        {
            size_t classIndex = RegisterPythonObject(cl, GetKey(cl, importChaserName));
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
        static void Unregister(boost::python::object cl, const std::string& importChaserName)
        {
            UnregisterPythonObject(cl, GetKey(cl, importChaserName));
        }

    private:
        // Function object constructor. Requires only the index of the Python class to use.
        FactoryFnWrapper(size_t classIndex)
            : _classIndex(classIndex) { };

        size_t _classIndex;

        // Generates a unique key based on the name of the class, along with the class
        // purpose:
        static std::string GetKey(boost::python::object cl, const std::string& importChaserName)
        {
            return ClassName(cl) + "," + importChaserName + "," + ",ImportChaser";
        }
    };

    static void Register(
        boost::python::object cl,
        const std::string&    importChaserName,
        const std::string&    niceName = {},
        const std::string&    description = {})
    {
        MaxUsdImportChaserRegistry::FactoryFn fn = FactoryFnWrapper::Register(cl, importChaserName);
        if (fn) {
            MaxUsdImportChaserRegistry::GetInstance().RegisterFactory(
                importChaserName,
                (niceName.empty() ? importChaserName
                                  : niceName), // default to chaser name if niceName is empty
                description,
                fn,
                true);
        }
    }

    static void Unregister(boost::python::object cl, const std::string& importChaserName)
    {
        FactoryFnWrapper::Unregister(cl, importChaserName);
    }
};

BOOST_PYTHON_FUNCTION_OVERLOADS(RegisterImport_overloads, ImportChaserWrapper::Register, 2, 4);

void wrapImportChaserRegistryFactoryContext()
{
    boost::python::class_<ImportChaserRegistryFactoryContextWrapper>(
        "MaxUsdImportChaserRegistryFactoryContext",
        "Holds data that can be accessed when constructing an ImportChaser object. This class "
        "allows the plugin\n"
        "code to only know about the context object during construction and only the data it needs "
        "to construct.",
        boost::python::no_init)
        .def(
            "GetContext",
            &ImportChaserRegistryFactoryContextWrapper::GetContext,
            (boost::python::arg("self")),
            "Returns the context.")
        .def(
            "GetStage",
            &ImportChaserRegistryFactoryContextWrapper::GetStage,
            (boost::python::arg("self")),
            "Get the imported USD stage.")
        .def(
            "GetFilename",
            &ImportChaserRegistryFactoryContextWrapper::GetFilename,
            boost::python::return_value_policy<boost::python::return_by_value>(),
            (boost::python::arg("self")),
            "Get the file name and path where the stage is written to on disk.")
        .def(
            "GetJobArgs",
            &ImportChaserRegistryFactoryContextWrapper::GetJobArgs,
            (boost::python::arg("self")),
            "Get the current global import args in effect.")
        .def(
            "GetPrimsToNodeHandles",
            &ImportChaserRegistryFactoryContextWrapper::GetPrimsToNodeHandles,
            (boost::python::arg("self")),
            "Returns a dictionary that maps the source USD prim paths to the imported MAXScript "
            "NodeHandles.");
}

void wrapImportChaser()
{
    typedef MaxUsdImportChaser This;

    boost::python::class_<ImportChaserWrapper, boost::noncopyable>(
        "ImportChaser",
        "ImportChaser base class from which import chasers need to inherit from. An ImportChaser "
        "instance is \n"
        "created at each import and called at the end of the import process. Chasers should not "
        "modify \n"
        "the structure of the USD file. Use this to make small changes or to add attributes, in a\n"
        "non-destructive way, to an imported stage.",
        boost::python::no_init)
        .def(
            "__init__",
            make_constructor(&ImportChaserWrapper::New),
            "Class constructor. Chasers should save necessary data when constructed. The "
            "constructor\n"
            "receives the Context. Save what you need from it so that you can make use of the "
            "information\n"
            "at the Chaser execution later.")
        .def(
            "PostImport",
            &This::PostImport,
            &ImportChaserWrapper::default_PostImport,
            (boost::python::arg("self")),
            "Method being called at the end of the standard import process.")
        .def(
            "Register",
            &ImportChaserWrapper::Register,
            RegisterImport_overloads(
                boost::python::args(
                    "imported_chaser_class", "import_chaser_name", "nice_name", "description"),
                "Static method to register an ImportChaser into the ChaserRegistry."))
        .staticmethod("Register")
        .def("Unregister", &ImportChaserWrapper::Unregister)
        .staticmethod("Unregister");
}
