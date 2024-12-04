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

#pragma once

#include <ufe/undoableCommand.h>

/**
 * Abstract class describing transform manipulations of USD entities.
 */
class SubObjectManip
{
public:
    SubObjectManip() = default;
    SubObjectManip(const SubObjectManip&) = delete;
    SubObjectManip& operator=(const SubObjectManip&) = delete;
    SubObjectManip(SubObjectManip&&) = delete;
    SubObjectManip& operator=(SubObjectManip&&) = delete;
    virtual ~SubObjectManip() = default;

    /**
     * Applies a transform on an object being manipulated interactively.
     * @param stageUnitAxis Unit/Axis conversion matrix for the USD Stage.
     * @param partm Parent transform (typically the node's transform)
     * @param tmAxis Axis transform (from gizmo)
     * @param transform The transform to be applied, a delta.
     */
    virtual void TransformInteractive(
        const pxr::GfMatrix4d& stageUnitAxis,
        const Matrix3&         partm,
        const Matrix3&         tmAxis,
        const Matrix3&         transform) const
        = 0;

    /**
     * Build's the transform command, meant to be called after interactive
     * transform completes. Command should represent the full transform (state
     * before any manipulation to the stage after interactive manipulation completes)
     * @return The command.
     */
    virtual Ufe::UndoableCommand::Ptr BuildTransformCmd() const = 0;

    const pxr::GfMatrix4d& GetPivot() const { return pivot; }

    const pxr::UsdTimeCode& GetTimeCode() const { return timeCode; }

protected:
    pxr::GfMatrix4d  pivot;
    pxr::UsdTimeCode timeCode;
};

/**
 * Performs manipulation of USD Xformable prims in subobject mode.
 */
class XformableManip : public SubObjectManip
{
public:
    XformableManip(const pxr::UsdGeomXformable& xformable, const pxr::UsdTimeCode& timeCode);

    XformableManip(const SubObjectManip&) = delete;
    XformableManip& operator=(const SubObjectManip&) = delete;
    XformableManip(XformableManip&&) = delete;
    XformableManip& operator=(XformableManip&&) = delete;
    ~XformableManip() override = default;

    void TransformInteractive(
        const pxr::GfMatrix4d& stageUnitAxis,
        const Matrix3&         partm,
        const Matrix3&         tmAxis,
        const Matrix3&         transform) const override;
    Ufe::UndoableCommand::Ptr BuildTransformCmd() const override;

protected:
    pxr::GfMatrix4d       initUsdWorldMatrix;
    pxr::UsdGeomXformOp   xformOp;
    pxr::GfMatrix4d       initOpMatrix;
    pxr::UsdGeomXformable xformable;
};

/**
 * Performs manipulation of USD Point instances in subobject mode.
 */
class PointInstanceManip : public SubObjectManip
{
public:
    PointInstanceManip(
        pxr::UsdGeomPointInstancer instancer,
        const std::vector<int>&    indices,
        const pxr::UsdTimeCode&    timeCode);

    PointInstanceManip(const SubObjectManip&) = delete;
    PointInstanceManip& operator=(const SubObjectManip&) = delete;
    PointInstanceManip(PointInstanceManip&&) = delete;
    PointInstanceManip& operator=(PointInstanceManip&&) = delete;
    ~PointInstanceManip() override = default;

    void TransformInteractive(
        const pxr::GfMatrix4d& stageUnitAxis,
        const Matrix3&         partm,
        const Matrix3&         tmAxis,
        const Matrix3&         transform) const override;
    Ufe::UndoableCommand::Ptr BuildTransformCmd() const override;

protected:
    pxr::UsdGeomPointInstancer instancer;
    std::vector<int>           indices;

    pxr::VtArray<pxr::GfMatrix4d> initOpTransforms;
    std::vector<pxr::GfMatrix4d>  initUsdWorldMatrices;
    pxr::VtArray<pxr::GfVec3f>    initPositions;
    pxr::VtArray<pxr::GfQuath>    initOrientations;
    pxr::VtArray<pxr::GfVec3f>    initScales;
};