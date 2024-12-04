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

#include "USDBaseController.h"

#include <control.h>
#include <iparamb2.h>
#include <qpointer.h>

class UsdControllerWidget;
class QDialog;

/**
 * Abstract USD controller for controllers driven by USD Attributes.
 */
class USDAttrController : public USDBaseController
{
public:
    USDAttrController() = default;
    ~USDAttrController() override;

    // Animatable overrides.
    void EditTrackParams(
        TimeValue           t,
        ParamDimensionBase* dim,
        const TCHAR*        pname,
        HWND                hParent,
        IObjParam*          ip,
        DWORD               flags) override;
    int TrackParamsType() override;

    // USDBaseController overrides.
    bool IsSourceObjectValid() const override;
    bool UpdateSource(IParamBlock2* pb) override;

    /**
     * Returns the value of the source attribute at the given time. Will take into account any
     * animation offsets defined on the stage object.
     * @param time The 3dsMax time at which to get the USD attribute value.
     * @return The attribute value.
     */
    pxr::VtValue GetAttrValue(const TimeValue& time) const;

    /**
     * Gets the USD attribute used as source for this controller.
     * @return The attribute.
     */
    const pxr::UsdAttribute& GetAttribute() const;

    /**
     * Attribute controllers share the same base UI, but may need to make adjustments to it.
     * By implementing this method, they can do so.
     * @param dialog The dialog showing the attribute controller widget.
     * @param controllerWidget The controller widget.
     */
    virtual void SetupDialog(
        const QPointer<QDialog>&             dialog,
        const QPointer<UsdControllerWidget>& controllerWidget)
        = 0;

private:
    /// The source attribute for the controller.
    pxr::UsdAttribute attribute;
    /// The dialog hosting the UI for the controller.
    QPointer<QDialog> dialog;
    /// The controller's UI widget (a QMaxParamBlockWidget)
    QPointer<UsdControllerWidget> controllerWidget;
};

/// Attribute controller to read into floats.
extern Class_ID USDFLOATCONTROLLER_CLASS_ID;
class USDFloatController : public USDAttrController
{
public:
    USDFloatController();
    Class_ID  ClassID() override;
    SClass_ID SuperClassID() override;

    void GetValue(TimeValue t, void* val, Interval& valid, GetSetMethod method) override;
    void GetClassName(MSTR& s, bool localized = true) const override;
    void SetupDialog(const QPointer<QDialog>& dialog, const QPointer<UsdControllerWidget>& widget)
        override;
    RefTargetHandle Clone(RemapDir& remap);
    ClassDesc2*     GetControllerClassDesc() const override;
};

/*
 * Attribute controller to read into Point3 values.
 * Note : Supports both 3 dimensional and 2 dimensional values.
 * For example a double2 value can be read into a Point3, and a 0 will be used for the third
 * component.
 * */
extern Class_ID USDPOINT3CONTROLLER_CLASS_ID;
class USDPoint3Controller : public USDAttrController
{
public:
    USDPoint3Controller();
    Class_ID  ClassID() override;
    SClass_ID SuperClassID() override;

    void GetValue(TimeValue t, void* val, Interval& valid, GetSetMethod method) override;
    void GetClassName(MSTR& s, bool localized = true) const override;
    void SetupDialog(const QPointer<QDialog>& dialog, const QPointer<UsdControllerWidget>& widget)
        override;
    RefTargetHandle Clone(RemapDir& remap);
    ClassDesc2*     GetControllerClassDesc() const override;
};

/*
 * Attribute controller to read into Point4 values.
 * Note : Supports both 4 dimensional and 3 dimensional values.
 * For example a double3 value can be read into a Point4, and a 0 will be used for the fourth
 * component.
 * */
extern Class_ID USDPOINT4CONTROLLER_CLASS_ID;
class USDPoint4Controller : public USDAttrController
{
public:
    USDPoint4Controller();
    Class_ID  ClassID() override;
    SClass_ID SuperClassID() override;

    void GetValue(TimeValue t, void* val, Interval& valid, GetSetMethod method) override;
    void GetClassName(MSTR& s, bool localized = true) const override;
    void SetupDialog(const QPointer<QDialog>& dialog, const QPointer<UsdControllerWidget>& widget)
        override;
    RefTargetHandle Clone(RemapDir& remap);
    ClassDesc2*     GetControllerClassDesc() const override;
};
