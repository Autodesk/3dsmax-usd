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

#include <BoostPythonWrapper.h>

#include <autodecref.h>
#include <pybind11/pybind11.h>
#include <sbkconverter.h>
#include <sbkstring.h>
#include <shiboken.h>
#ifdef USE_PYSIDE_6
#include <pyside6_qtwidgets_python.h>
#else
#include <pyside2_qtwidgets_python.h>
#endif

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
            if (const auto mo = _pythonWidget->metaObject()) {
                int idx = mo->indexOfMethod(QMetaObject::normalizedSignature("cleanup()"));
                if (idx != -1) {
                    mo->method(idx).invoke(_pythonWidget, Qt::AutoConnection);
                }
            }
            _pythonWidget->setParent(nullptr);
        }
        Shiboken::GilState gilState;
        _pythonObject.reset(nullptr);
    }

    Shiboken::AutoDecRef _pythonObject;
    QPointer<QWidget>    _pythonWidget;

private:
    // needed for the conversion between QWidget pointers and Python objects
    static PyTypeObject** SbkPySide_QtWidgetsTypes;
    static bool           initTypes();

    QmaxUsdPythonWidget* q_ptr = nullptr;
    Q_DECLARE_PUBLIC(QmaxUsdPythonWidget);
};

PyTypeObject** QmaxUsdPythonWidgetPrivate::SbkPySide_QtWidgetsTypes = nullptr;

bool QmaxUsdPythonWidgetPrivate::initTypes()
{
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

    return embed(pValue);
}

QmaxUsdPythonWidget* QmaxUsdPythonWidget::embed(PyObject* pySideWidget, QWidget* parent)
{
    if (!Py_IsInitialized()) {
        Py_Initialize();
    }

    Shiboken::GilState gilState;

    // Wrapping the PyObject* in a pybind11 object that increases and decreases
    // the reference automatically
    auto pySideWidgetObj = pybind11::reinterpret_borrow<pybind11::object>(pySideWidget);
    if (!pySideWidgetObj || pySideWidgetObj.is_none()) {
        return nullptr;
    }

    if (Shiboken::Object::isValid(pySideWidget)) {

        auto result = new QmaxUsdPythonWidget();

        if (QmaxUsdPythonWidgetPrivate::initTypes()) {
            QWidget* pythonWidget = nullptr;
#ifdef USE_PYSIDE_6
#pragma warning(push)
#pragma warning(disable : 4996)
            Shiboken::Conversions::pythonToCppPointer(
                QmaxUsdPythonWidgetPrivate::SbkPySide_QtWidgetsTypes[SBK_QWIDGET_IDX],
                pySideWidget,
                &pythonWidget);
#pragma warning(pop)
#else
            Shiboken::Conversions::pythonToCppPointer(
                reinterpret_cast<SbkObjectType*>(
                    QmaxUsdPythonWidgetPrivate::SbkPySide_QtWidgetsTypes[SBK_QWIDGET_IDX]),
                pySideWidget,
                &pythonWidget);
#endif
            if (pythonWidget) {
                auto d = result->d_func();
                d->_pythonObject.reset(pySideWidgetObj.release().ptr());
                d->_pythonWidget = pythonWidget;
                result->layout()->addWidget(pythonWidget);

                // pass the object name of the embedded widget to the parent
                // widget.
                result->setObjectName(pythonWidget->objectName());
                return result;
            }
        }
        delete result;
    }
    return nullptr;
}

} // namespace MAXUSD_NS_DEF
