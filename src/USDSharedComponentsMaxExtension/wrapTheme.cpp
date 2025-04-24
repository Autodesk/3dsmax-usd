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

#include <BoostPythonWrapper.h>

#include <pxr/pxr.h>

#include <MaxIcon.h>
#include <boost/python/def.hpp>
#include <qicon.h>
#include <qstring.h>
#include <shiboken.h>
#ifdef USE_PYSIDE_6
#include <pyside6_qtgui_python.h>
#else
#include <pyside2_qtgui_python.h>
#endif

// these are needed for the conversion between QtGui pointers and Python objects

PyTypeObject** SbkPySide_QtGuiTypes = nullptr;
SbkConverter** SbkPySide_QtGuiTypeConverters = nullptr;

bool initTypes()
{
    if (!SbkPySide_QtGuiTypes) {

#ifdef USE_PYSIDE_6
        Shiboken::AutoDecRef requiredModule(Shiboken::Module::import("PySide6.QtGui"));
#else
        Shiboken::AutoDecRef requiredModule(Shiboken::Module::import("PySide2.QtGui"));
#endif

        if (!requiredModule.isNull()) {
            SbkPySide_QtGuiTypes = Shiboken::Module::getTypes(requiredModule);
            SbkPySide_QtGuiTypeConverters = Shiboken::Module::getTypeConverters(requiredModule);
        }
    }
    return SbkPySide_QtGuiTypes != nullptr;
}

PXR_NAMESPACE_USING_DIRECTIVE

PyObject* _getMaxMultiResIcon(const std::string& name)
{
    PyObject* pyIcon = nullptr;
    auto icon = MaxSDK::LoadMaxMultiResIcon(QString("Common/%1").arg(QString::fromStdString(name)));
    if (initTypes()) {
#ifdef USE_PYSIDE_6
        pyIcon = Shiboken::Conversions::copyToPython(SbkPySide_QtGuiTypes[SBK_QICON_IDX], &icon);
#else
        pyIcon = Shiboken::Conversions::copyToPython(
            reinterpret_cast<SbkObjectType*>(SbkPySide_QtGuiTypes[SBK_QICON_IDX]), &icon);
#endif
    }
    return pyIcon;
}

std::string _getColorTheme()
{
    if (auto colorman = GetColorManager()) {
        return colorman->GetAppFrameColorTheme() == IColorManager::AppFrameColorTheme::kDarkTheme
            ? "dark"
            : "light";
    }
    return "dark";
}

float _getUIScaleFactor() { return MaxSDK::GetUIScaleFactor(); }

void wrapTheme()
{
    pyboost::def(
        "GetMaxMultiResIcon",
        _getMaxMultiResIcon,
        pyboost::args("name"),
        "Gets a QIcon for the given name");

    pyboost::def(
        "GetColorTheme", _getColorTheme, "Gets the currently selected color theme (light/dark)");

    pyboost::def(
        "GetUIScaleFactor", _getUIScaleFactor, "Gets the currently used UI scaling factor");
}
