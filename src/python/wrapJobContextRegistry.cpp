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
#include <BoostPythonWrapper.h>
#include <MaxUsd/Builders/JobContextRegistry.h>
#include <MaxUsd/Translators/RegistryHelper.h>

#include <pxr/base/tf/pyPolymorphic.h>

#include <QtWidgets/qwidget.h>
#include <autodecref.h>
#include <boost/python.hpp>
#include <boost/python/args.hpp>
#include <sbkconverter.h>
#include <shiboken.h>
#ifdef USE_PYSIDE_6
#include <pyside6_qtwidgets_python.h>
#else
#include <pyside2_qtwidgets_python.h>
#endif
// these are needed for the conversion of the QWidget pointer to a Python object
PyTypeObject** SbkPySide_QtWidgetsTypes = nullptr;
SbkConverter** SbkPySide_QtWidgetsTypeConverters = nullptr;

PXR_NAMESPACE_USING_DIRECTIVE

//----------------------------------------------------------------------------------------------------------------------
/// \brief  boost python binding for the MaxUsdJobContextRegistry
//----------------------------------------------------------------------------------------------------------------------
class JobContextRegistry
{
public:
    static TfTokenVector ListJobContexts() { return MaxUsdJobContextRegistry::ListJobContexts(); }

    static pyboost::object GetJobContextInfo(const TfToken& jobContext)
    {
        MaxUsdJobContextRegistry::ContextInfo ctx
            = MaxUsdJobContextRegistry::GetInstance().GetJobContextInfo(jobContext);
        pyboost::dict dict;
        dict["jobContext"] = ctx.jobContext;
        dict["niceName"] = ctx.niceName;
        dict["exportDescription"] = ctx.exportDescription;
        // not exposing the associated export callbacks on purpose
        // the callbacks cannot be expressed usefully in Python
        // dict["exportEnablerCallback"] = ctx.exportEnablerCallback;
        // dict["exportOptionsCallback"] = ctx.exportOptionsCallback;
        dict["importDescription"] = ctx.importDescription;
        // not exposing the associated import callbacks on purpose
        // the callbacks cannot be expressed usefully in Python
        // dict["importEnablerCallback"] = ctx.importEnablerCallback;
        // dict["importOptionsCallback"] = ctx.importOptionsCallback;
        return std::move(dict);
    }

    static void RegisterImportJobContext(
        const std::string& jobContext,
        const std::string& niceName,
        const std::string& description,
        pyboost::object    enablerFct)
    {
        if (!PyCallable_Check(enablerFct.ptr())) {
            TF_CODING_ERROR(
                "Parameter enablerFct should be a callable function returning a dictionary.");
        }

        MaxUsdJobContextRegistry::GetInstance().RegisterImportJobContext(
            jobContext, niceName, description, [=]() { return callEnablerFn(enablerFct); }, true);
    }
    static void RegisterExportJobContext(
        const std::string& jobContext,
        const std::string& niceName,
        const std::string& description,
        pyboost::object    enablerFct)
    {
        if (!PyCallable_Check(enablerFct.ptr())) {
            TF_CODING_ERROR(
                "Parameter enablerFct should be a callable function returning a dictionary.");
        }

        MaxUsdJobContextRegistry::GetInstance().RegisterExportJobContext(
            jobContext, niceName, description, [=]() { return callEnablerFn(enablerFct); }, true);
    }

    static void SetImportOptionsUI(const std::string& jobContext, pyboost::object optionFct)
    {
        if (!PyCallable_Check(optionFct.ptr())) {
            TF_CODING_ERROR(
                "Parameter optionFct should be a callable function returning a dictionary.");
        }

        MaxUsdJobContextRegistry::GetInstance().SetImportOptionsUI(
            jobContext,
            [=](const std::string& jobContext, QWidget* parentUI, const VtDictionary& options) {
                return callOptionsFn(optionFct, jobContext, parentUI, options);
            },
            true);
    }

    static void SetExportOptionsUI(const std::string& jobContext, pyboost::object optionFct)
    {
        if (!PyCallable_Check(optionFct.ptr())) {
            TF_CODING_ERROR(
                "Parameter optionFct should be a callable function returning a dictionary.");
        }

        MaxUsdJobContextRegistry::GetInstance().SetExportOptionsUI(
            jobContext,
            [=](const std::string& jobContext, QWidget* parentUI, const VtDictionary& options) {
                return callOptionsFn(optionFct, jobContext, parentUI, options);
            },
            true);
    }

private:
    static VtDictionary callEnablerFn(pyboost::object fnc)
    {
        auto res = TfPyCall<VtDictionary>(fnc)();
        return res;
    }

    static VtDictionary callOptionsFn(
        pyboost::object     fnc,
        const std::string&  jobContext,
        QWidget*            parentUI,
        const VtDictionary& options)
    {
        PyGILState_STATE gstate = PyGILState_Ensure();

        PyObject* pyQtWidget = nullptr;
        if (parentUI) {
            if (!SbkPySide_QtWidgetsTypes) {
#ifdef USE_PYSIDE_6
                Shiboken::AutoDecRef requiredModule(Shiboken::Module::import("PySide6.QtWidgets"));
#else
                Shiboken::AutoDecRef requiredModule(Shiboken::Module::import("PySide2.QtWidgets"));
#endif
                if (!requiredModule.isNull()) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 8, 3)
                    SbkPySide_QtWidgetsTypes = &Shiboken::Module::getTypes(requiredModule)->type;
#else
                    SbkPySide_QtWidgetsTypes = Shiboken::Module::getTypes(requiredModule);
#endif
                    SbkPySide_QtWidgetsTypeConverters
                        = Shiboken::Module::getTypeConverters(requiredModule);
                }
            }
            if (SbkPySide_QtWidgetsTypes) {
                // Convert the QWidget pointer to a Python object
#ifdef USE_PYSIDE_6
#pragma warning(push)
#pragma warning(disable : 4996)
                pyQtWidget = Shiboken::Conversions::pointerToPython(
                    SbkPySide_QtWidgetsTypes[SBK_QWIDGET_IDX], parentUI);
#pragma warning(pop)
#else
                pyQtWidget = Shiboken::Conversions::pointerToPython(
                    reinterpret_cast<SbkObjectType*>(SbkPySide_QtWidgetsTypes[SBK_QWIDGET_IDX]),
                    parentUI);
#endif
            }
        }

        if (pyQtWidget == nullptr) {
            pyQtWidget = Py_None;
            Py_XINCREF(pyQtWidget);
        }
        auto res = TfPyCall<VtDictionary>(fnc)(jobContext, pyboost::handle<>(pyQtWidget), options);
        Py_XDECREF(pyQtWidget);

        PyGILState_Release(gstate);
        return res;
    }
};

//----------------------------------------------------------------------------------------------------------------------
void wrapJobContextRegistry()
{
    pyboost::class_<JobContextRegistry, noncopyable>("JobContextRegistry", pyboost::no_init)
        .def("ListJobContexts", &JobContextRegistry::ListJobContexts)
        .staticmethod("ListJobContexts")
        .def(
            "GetJobContextInfo",
            &JobContextRegistry::GetJobContextInfo,
            pyboost::arg("job_context_name"),
            "Get the JobContext information dictionary.")
        .staticmethod("GetJobContextInfo")
        .def(
            "RegisterExportJobContext",
            &JobContextRegistry::RegisterExportJobContext,
            pyboost::args(
                "job_context_name",
                "job_context_nice_name",
                "job_context_description",
                "job_context_function"),
            "Static method to register a JobContext function into the JobContextRegistry.")
        .staticmethod("RegisterExportJobContext")
        .def(
            "RegisterImportJobContext",
            &JobContextRegistry::RegisterImportJobContext,
            pyboost::args(
                "job_context_name",
                "job_context_nice_name",
                "job_context_description",
                "job_context_function"),
            "Static method to register a JobContext function into the JobContextRegistry.")
        .staticmethod("RegisterImportJobContext")
        .def(
            "SetImportOptionsUI",
            &JobContextRegistry::SetImportOptionsUI,
            pyboost::args("job_context_name", "job_context_function"),
            "Static method to register a JobContext option function into the JobContextRegistry.")
        .staticmethod("SetImportOptionsUI")
        .def(
            "SetExportOptionsUI",
            &JobContextRegistry::SetExportOptionsUI,
            pyboost::args("job_context_name", "job_context_function"),
            "Static method to register a JobContext option function into the JobContextRegistry.")
        .staticmethod("SetExportOptionsUI");
}
