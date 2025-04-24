//
// Copyright 2024 Autodesk
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
#pragma once
#include "QmaxUsdPythonWidget.h"

#include <autodecref.h>
#ifdef IS_MAX2022
#pragma warning(push)
#pragma warning(disable : 2220 4005)
#ifdef HAVE_SNPRINTF
#pragma push_macro("HAVE_SNPRINTF")
#define PUSHED_HAVE_SNPRINTF 1
#undef HAVE_SNPRINTF
#endif
#endif
#include <pybind11/pybind11.h>
#ifdef IS_MAX2022
#ifdef PUSHED_HAVE_SNPRINTF
#pragma pop_macro("HAVE_SNPRINTF")
#undef PUSHED_HAVE_SNPRINTF
#endif
#pragma warning(pop)
#endif

#include <sbkconverter.h>
#include <sbkstring.h>
#include <shiboken.h>
#ifdef USE_PYSIDE_6
#include <pyside6_qtwidgets_python.h>
#else
#include <pyside2_qtwidgets_python.h>
#endif

#include <boost/python.hpp>
#include <qfileinfo.h>
#include <string>

namespace MAXUSD_NS_DEF {

class QmaxUsdPythonWidgetPrivate
{
public:
    QmaxUsdPythonWidgetPrivate(QmaxUsdPythonWidget* q)
        : q_ptr(q)
#ifndef USE_PYSIDE6
        , _pythonObject(static_cast<PyObject*>(nullptr))
#endif
    {
    }

    ~QmaxUsdPythonWidgetPrivate()
    {
        if (_pythonWidget) {
            QMetaObject::invokeMethod(_pythonWidget, "cleanup");
            _pythonWidget->setParent(nullptr);
        }
        Shiboken::GilState gilState;
        _pythonObject.reset(nullptr);
    }

    Shiboken::AutoDecRef _pythonObject;
    QPointer<QWidget>    _pythonWidget;

private:
    // these are needed for the conversion between QWidget pointers and Python
    // objects
    static PyTypeObject** SbkPySide_QtWidgetsTypes;
    static SbkConverter** SbkPySide_QtWidgetsTypeConverters;

    static bool initTypes();

    QmaxUsdPythonWidget* q_ptr = nullptr;
    Q_DECLARE_PUBLIC(QmaxUsdPythonWidget);
};

PyTypeObject** QmaxUsdPythonWidgetPrivate::SbkPySide_QtWidgetsTypes = nullptr;
SbkConverter** QmaxUsdPythonWidgetPrivate::SbkPySide_QtWidgetsTypeConverters = nullptr;

bool QmaxUsdPythonWidgetPrivate::initTypes()
{
    if (!SbkPySide_QtWidgetsTypes) {

#ifdef USE_PYSIDE_6
        Shiboken::AutoDecRef requiredModule(Shiboken::Module::import("PySide6.QtWidgets"));
#else
        Shiboken::AutoDecRef requiredModule(Shiboken::Module::import("PySide2.QtWidgets"));
#endif

        if (!requiredModule.isNull()) {
            SbkPySide_QtWidgetsTypes = Shiboken::Module::getTypes(requiredModule);
            SbkPySide_QtWidgetsTypeConverters = Shiboken::Module::getTypeConverters(requiredModule);
        }
    }
    return SbkPySide_QtWidgetsTypes != nullptr;
}

QmaxUsdPythonWidget::QmaxUsdPythonWidget(QWidget* parent)
    : QWidget(parent)
    , d_ptr(new QmaxUsdPythonWidgetPrivate(this))
{
    setLayout(new QVBoxLayout());
    setContentsMargins(0, 0, 0, 0);

    // Prevent the widget from resizing its containing widget vertically what
    // could force the narrow columns in 3dsMax's command panel UI to expand.
    setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
}

QmaxUsdPythonWidget::~QmaxUsdPythonWidget() = default;

QmaxUsdPythonWidget* QmaxUsdPythonWidget::create(
    const Ufe::Selection&  selection,
    const std::string&     attributeName,
    const std::string&     pythonModuleDirectory,
    const std::string&     pythonModule,
    const std::string&     pythonEntryFunction,
    std::set<std::string>& handledAttributeNames)
{

    if (!Py_IsInitialized()) {
        Py_Initialize();
    }

    Shiboken::GilState gilState;

    if (!pythonModuleDirectory.empty()) {
        QFileInfo  fi(QString::fromStdString(pythonModuleDirectory));
        QByteArray realPythonModuleDirectory;
        if (fi.isDir()) {
            realPythonModuleDirectory = fi.absoluteFilePath().toUtf8();
        } else if (fi.isFile()) {
            realPythonModuleDirectory = fi.absolutePath().toUtf8();
        }

        if (!realPythonModuleDirectory.isEmpty()) {
            Shiboken::AutoDecRef sysModule(PyImport_ImportModule("sys"));
            Shiboken::AutoDecRef sysPath(PyObject_GetAttrString(sysModule, "path"));
            Shiboken::AutoDecRef pyModuleDir(Shiboken::String::fromCString(
                realPythonModuleDirectory.constData(), realPythonModuleDirectory.size()));

            Shiboken::AutoDecRef pCount(PyObject_GetAttrString(sysPath, "count"));
            if (pCount.isNull() || !PyCallable_Check(pCount)) {
                if (PyErr_Occurred()) {
                    PyErr_Print();
                }
                return nullptr;
            }

            Shiboken::AutoDecRef pValue(PyObject_CallObject(pCount, pyModuleDir));
            if (PyLong_AsSize_t(pValue) != 1) {
                Shiboken::AutoDecRef zero(PyLong_FromSize_t(0));
                int                  retval = PyList_Insert(sysPath, zero, pyModuleDir);
                if (retval == -1) {
                    PyErr_Print();
                    return nullptr;
                }
            }
        }
    }

    Shiboken::AutoDecRef pModule(PyImport_ImportModule(pythonModule.c_str()));
    if (pModule.isNull()) {
        PyErr_Print();
        return nullptr;
    }

    Shiboken::AutoDecRef pFunc(PyObject_GetAttrString(pModule, pythonEntryFunction.c_str()));
    if (pFunc.isNull() || !PyCallable_Check(pFunc)) {
        if (PyErr_Occurred()) {
            PyErr_Print();
        }
        return nullptr;
    }

    auto pySelection = pybind11::cast(selection);
    auto pyCollectionName = pybind11::cast(attributeName);
    pySelection.inc_ref();
    pyCollectionName.inc_ref();
    auto args = pybind11::make_tuple(pySelection, pyCollectionName);

    Shiboken::AutoDecRef pValue(PyObject_CallObject(pFunc, args.ptr()));
    if (pValue.isNull()) {
        PyErr_Print();
        return nullptr;
    }

    // for some reason we loose a ref with 'pValue' in the conversion below
    Py_INCREF(pValue.object());

    if (Shiboken::Object::isValid(pValue)) {

        auto result = new QmaxUsdPythonWidget();

        if (QmaxUsdPythonWidgetPrivate::initTypes()) {
            QWidget* pythonWidget = nullptr;
#ifdef USE_PYSIDE_6
            Shiboken::Conversions::pythonToCppPointer(
                QmaxUsdPythonWidgetPrivate::SbkPySide_QtWidgetsTypes[SBK_QWIDGET_IDX],
                pValue,
                &pythonWidget);
#else
            Shiboken::Conversions::pythonToCppPointer(
                reinterpret_cast<SbkObjectType*>(
                    QmaxUsdPythonWidgetPrivate::SbkPySide_QtWidgetsTypes[SBK_QWIDGET_IDX]),
                pValue,
                &pythonWidget);
#endif
            if (pythonWidget) {
                auto d = result->d_func();
#ifdef USE_PYSIDE_6
                d->_pythonObject.reset(pValue.release());
#else
                d->_pythonObject.reset(pValue);
                pValue.reset(nullptr);
#endif
                d->_pythonWidget = pythonWidget;
                result->layout()->addWidget(pythonWidget);
                return result;
            }
        }
        delete result;
    }
    return nullptr;
}

} // namespace MAXUSD_NS_DEF
