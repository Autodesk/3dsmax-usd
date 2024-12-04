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

enum TransformControllersParamMapID
{
    USDTransformControllerMapID_General
};

extern Class_ID USDXFORMABLECONTROLLER_CLASS_ID;

class USDXformableController : public USDBaseController
{
public:
    USDXformableController();

    // Controller overrides.
    Class_ID  ClassID() override;
    SClass_ID SuperClassID() override;
    void      GetValue(TimeValue t, void* val, Interval& valid, GetSetMethod method) override;
    void      GetClassName(MSTR& s, bool localized = true) const override;
    BOOL      PreventNodeDeletion() override;
    // From ReferenceTarget
    RefTargetHandle Clone(RemapDir& remap);

    // USDBaseController overrides
    bool        IsSourceObjectValid() const override;
    ClassDesc2* GetControllerClassDesc() const override;
    bool        UpdateSource(IParamBlock2* pb) override;

    /**
     * Returns the Xformable prim used as source for this transform controller.
     * @return The Xformable.
     */
    const pxr::UsdGeomXformable& GetXformable();

private:
    /// The Xformable driving the controller.
    pxr::UsdGeomXformable xformable;
};

/**
 * Base class for USD PRS controllers. PRS controllers can source their transforms
 * either from xformables, or attributes.
 * See https://help.autodesk.com/view/MAXDEV/2025/ENU/?guid=prs_controllers_and_node_transfo
 */
class USDPRSController : public USDXformableController
{
public:
    void EndEditParams(IObjParam* ip, ULONG flags, Animatable* next) override;

    bool IsSourceObjectValid() const override;
    bool UpdateSource(IParamBlock2* pb) override;

    /**
     * The source attribute for the controller. The returned attribute
     * is invalid if the controller is sourcing its transform from
     * a Xformable.
     * @return
     */
    const pxr::UsdAttribute& GetAttr() const;

private:
    /// The source attribute for the controller, left invalid
    /// if sourcing the transform from an xformable prim.
    pxr::UsdAttribute attribute;
};

extern Class_ID USDPOSITIONCONTROLLER_CLASS_ID;

class USDPositionController : public USDPRSController
{
public:
    USDPositionController();

    // Controller overrides.
    Class_ID  ClassID() override;
    SClass_ID SuperClassID() override;
    void      GetValue(TimeValue t, void* val, Interval& valid, GetSetMethod method) override;
    void      GetClassName(MSTR& s, bool localized = true) const override;

    // USDBaseController overrides
    ClassDesc2* GetControllerClassDesc() const override;
};

extern Class_ID USDSCALECONTROLLER_CLASS_ID;

class USDScaleController : public USDPRSController
{
public:
    USDScaleController();

    // Controller overrides.
    Class_ID  ClassID() override;
    SClass_ID SuperClassID() override;
    void      GetValue(TimeValue t, void* val, Interval& valid, GetSetMethod method) override;
    void      GetClassName(MSTR& s, bool localized = true) const override;

    // USDBaseController overrides
    ClassDesc2* GetControllerClassDesc() const override;
};

extern Class_ID USDROTATIONCONTROLLER_CLASS_ID;

class USDRotationController : public USDPRSController
{
public:
    USDRotationController();

    // Controller overrides.
    Class_ID  ClassID() override;
    SClass_ID SuperClassID() override;
    void      GetValue(TimeValue t, void* val, Interval& valid, GetSetMethod method) override;
    void      GetClassName(MSTR& s, bool localized = true) const override;

    // USDBaseController overrides
    ClassDesc2* GetControllerClassDesc() const override;
};
