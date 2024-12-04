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

#include "SubobjectManips.h"

#include <MaxUsdObjects/MaxUsdUfe/MaxUsdEditCommand.h>
#include <MaxUsdObjects/MaxUsdUfe/StageObjectMap.h>

#include <UFEUI/genericCommand.h>

#include <MaxUsd/Utilities/DiagnosticDelegate.h>
#include <MaxUsd/Utilities/TranslationUtils.h>
#include <MaxUsd/Utilities/TypeUtils.h>

XformableManip::XformableManip(
    const pxr::UsdGeomXformable& xformable,
    const pxr::UsdTimeCode&      timeCode)
{
    this->xformable = xformable;
    // The first thing we need to do is find a xformOp on the xformable prim that we can edit.
    // The op needs to be at the back of the ordered ops, as the "most local" transform.
    // If we dont find a suitable transform op there, we append our own.
    pxr::UsdGeomXformOp transformOp;
    bool                resetStack = false;
    auto                xformOps = xformable.GetOrderedXformOps(&resetStack);
    if (!xformOps.empty() && xformOps.back().GetOpType() == pxr::UsdGeomXformOp::TypeTransform) {
        transformOp = xformOps.back();
    } else {
        // Make sure the xform op full name is unique, add a suffix if need be.
        pxr::TfToken suffix = {};
        auto         nextCandidateName
            = pxr::UsdGeomXformOp::GetOpName(pxr::UsdGeomXformOp::TypeTransform, suffix);
        int count = 1;
        while (std::find_if(
                   xformOps.begin(),
                   xformOps.end(),
                   [&suffix, &nextCandidateName](const pxr::UsdGeomXformOp& op) {
                       return op.GetName() == nextCandidateName;
                       ;
                   })
               != xformOps.end()) {
            suffix = pxr::TfToken("t" + std::to_string(count));
            nextCandidateName
                = pxr::UsdGeomXformOp::GetOpName(pxr::UsdGeomXformOp::TypeTransform, suffix);
            count++;
        }
        transformOp = xformable.AddTransformOp(pxr::UsdGeomXformOp::PrecisionDouble, suffix);
        if (transformOp) {
            // Make sure the transform is the identity (transform could already exist even if not on
            // the stack).
            pxr::GfMatrix4d identity;
            identity.SetIdentity();
            transformOp.Set(identity);
        }
    }

    if (!transformOp.IsDefined()) {
        return;
    }

    pxr::GfMatrix4d currentEditOpMatrix;
    if (!transformOp.Get(&currentEditOpMatrix, timeCode)) {
        currentEditOpMatrix.SetIdentity();
    }

    xformOp = transformOp;
    initOpMatrix = currentEditOpMatrix;
    pivot = MaxUsd::GetPivotTransform(xformable, timeCode);
    initUsdWorldMatrix
        = pxr::UsdGeomImageable(xformable.GetPrim()).ComputeLocalToWorldTransform(timeCode);
    this->timeCode = timeCode;
}

void XformableManip::TransformInteractive(
    const pxr::GfMatrix4d& stageUnitAxis,
    const Matrix3&         partm,
    const Matrix3&         tmAxis,
    const Matrix3&         transform) const
{
    if (!xformOp.IsDefined()) {
        return;
    }

    const auto stageObject = StageObjectMap::GetInstance()->Get(xformable.GetPrim().GetStage());

    // A Pivot may be defined in the xformOp stack that we should use, however, in case of multiple
    // selection, we need to offset that pivot so that all selected object rotate around the same
    // point. In case of single selection, the pivotOffset here will evaluate to (0,0,0).
    const auto fullTransform = GetPivot() * initUsdWorldMatrix
        * stageObject->GetStageRootTransform() * MaxUsd::ToUsd(partm);
    const auto pivotOffset = MaxUsd::ToUsd(tmAxis.GetTrans()) - fullTransform.ExtractTranslation();
    auto       axis = tmAxis;
    axis.SetTrans(MaxUsd::ToMax(pivotOffset));

    auto worldRotAndScale = initUsdWorldMatrix * stageUnitAxis * MaxUsd::ToUsd(partm);
    worldRotAndScale.SetTranslateOnly({});

    auto usdMat = worldRotAndScale * MaxUsd::ToUsd(Inverse(axis)) * MaxUsd::ToUsd(transform)
        * MaxUsd::ToUsd(axis) * worldRotAndScale.GetInverse();

    auto newTransform = pivot.GetInverse() * usdMat * pivot * initOpMatrix;

    xformOp.Set(newTransform, pxr::UsdTimeCode::Default());
}

Ufe::UndoableCommand::Ptr XformableManip::BuildTransformCmd() const
{
    if (!xformOp.IsDefined()) {
        return nullptr;
    }

    // Our new transform is already set at the time we are editing at, however, set it again through
    // a command so that the entire transform can be undone. The value isn't really changing here,
    // so we aren't actually dirtying anything.
    pxr::GfMatrix4d newTransform;
    xformOp.Get(&newTransform, timeCode);
    auto callback = [xformOp = xformOp, initialMatrix = initOpMatrix, newTransform](
                        UfeUI::GenericCommand::Mode mode) {
        if (mode == UfeUI::GenericCommand::Mode::kUndo) {
            xformOp.Set(initialMatrix);

        } else if (mode == UfeUI::GenericCommand::Mode::kRedo) {
            xformOp.Set(newTransform);
        }
    };

    const auto name = "Transform USD Prim";

    const auto prim = xformable.GetPrim();
    const auto stageObject = StageObjectMap::GetInstance()->Get(prim.GetStage());
    const auto ufePath = MaxUsd::ufe::getUsdPrimUfePath(stageObject, prim.GetPath());

    return UfeUi::EditCommand::create(ufePath, UfeUI::GenericCommand::create(callback, name), name);
}

PointInstanceManip::PointInstanceManip(
    pxr::UsdGeomPointInstancer instancer,
    const std::vector<int>&    indices,
    const pxr::UsdTimeCode&    timeCode)
{
    // No pivot for point instances.
    this->instancer = instancer;
    this->timeCode = timeCode;
    this->indices = indices;
    pivot.SetIdentity();

    instancer.ComputeInstanceTransformsAtTime(
        &initOpTransforms, timeCode, timeCode, pxr::UsdGeomPointInstancer::ExcludeProtoXform);
    // Can happen if none of the PRS attributes are authored, assume identity.
    if (initOpTransforms.empty()) {
        pxr::GfMatrix4d identity;
        identity.SetIdentity();
        initOpTransforms.resize(instancer.GetInstanceCount(timeCode));
        std::fill(initOpTransforms.begin(), initOpTransforms.end(), identity);
    }

    const auto imageable = pxr::UsdGeomImageable { instancer.GetPrim() };
    auto       instancerWorldMatrix = imageable.ComputeLocalToWorldTransform(timeCode);

    const auto instanceCount = instancer.GetInstanceCount(timeCode);
    initUsdWorldMatrices.resize(instanceCount);
    for (const auto& idx : indices) {
        initUsdWorldMatrices[idx] = initOpTransforms[idx] * instancerWorldMatrix;
    }

    // Ensure that the prs attributes are created and well formed.

    auto positionsAttr = instancer.CreatePositionsAttr();

    pxr::VtVec3fArray positions;
    positionsAttr.Get(&positions, timeCode);
    initPositions = positions;
    if (positions.size() < instanceCount) {
        positions.resize(instanceCount);
        positionsAttr.Set(positions);
    }

    auto              scalesAttr = instancer.CreateScalesAttr();
    pxr::VtVec3fArray scales;
    scalesAttr.Get(&scales, timeCode);
    initScales = scales;
    if (scales.size() < instanceCount) {
        scales.resize(instanceCount);
        std::fill(scales.begin(), scales.end(), pxr::GfVec3f { 1.f, 1.f, 1.f });
        scalesAttr.Set(scales);
    }

    auto              orientationsAttr = instancer.CreateOrientationsAttr();
    pxr::VtQuathArray orientations;
    orientationsAttr.Get(&orientations, timeCode);
    initOrientations = orientations;
    if (orientations.size() < instanceCount) {
        orientations.resize(instanceCount);
        std::fill(orientations.begin(), orientations.end(), pxr::GfQuath::GetIdentity());
        orientationsAttr.Set(orientations);
    }
}

void PointInstanceManip::TransformInteractive(
    const pxr::GfMatrix4d& stageUnitAxis,
    const Matrix3&         partm,
    const Matrix3&         tmAxis,
    const Matrix3&         transform) const
{
    auto posAttr = instancer.GetPositionsAttr();
    auto oriAttr = instancer.GetOrientationsAttr();
    auto sclAttr = instancer.GetScalesAttr();

    // All attrs are expected at this point, unless we can't author on the prim at all.
    if (!posAttr.IsValid() || !oriAttr.IsValid() || !sclAttr.IsValid()) {
        return;
    }

    pxr::VtVec3fArray currentPositions;
    posAttr.Get(&currentPositions, timeCode);
    pxr::VtQuathArray currentOrientations;
    oriAttr.Get(&currentOrientations, timeCode);
    pxr::VtVec3fArray currentScales;
    sclAttr.Get(&currentScales, timeCode);

    const auto stageObject = StageObjectMap::GetInstance()->Get(instancer.GetPrim().GetStage());
    const auto stageRootTransform = stageObject->GetStageRootTransform();

    auto axis = tmAxis;
    for (const auto& idx : indices) {
        // In cases of multiple selection, we need to offset that pivot so that all selected object
        // rotate around the same point. In case of single selection, the pivotOffset here will
        // evaluate to (0,0,0).
        const auto fullTransform
            = initUsdWorldMatrices[idx] * stageRootTransform * MaxUsd::ToUsd(partm);
        const auto pivotOffset
            = MaxUsd::ToUsd(tmAxis.GetTrans()) - fullTransform.ExtractTranslation();
        axis.SetTrans(MaxUsd::ToMax(pivotOffset));

        auto worldRotAndScale = initUsdWorldMatrices[idx] * stageUnitAxis * MaxUsd::ToUsd(partm);
        worldRotAndScale.SetTranslateOnly({});

        auto usdMat = worldRotAndScale * MaxUsd::ToUsd(Inverse(axis)) * MaxUsd::ToUsd(transform)
            * MaxUsd::ToUsd(axis) * worldRotAndScale.GetInverse();

        auto newTransform = GetPivot().GetInverse() * usdMat * GetPivot() * initOpTransforms[idx];

        auto instanceTranslation = newTransform.ExtractTranslation();
        currentPositions[idx] = pxr::GfVec3f { instanceTranslation };

        const auto noScaleTransform = newTransform.RemoveScaleShear();
        const auto rotationQuat = pxr::GfQuath { noScaleTransform.ExtractRotationQuat() };
        currentOrientations[idx] = rotationQuat;

        pxr::GfMatrix4d rotR;
        pxr::GfVec3d    scale;
        pxr::GfMatrix4d rotU;
        pxr::GfVec3d    translate;
        pxr::GfMatrix4d project;
        newTransform.Factor(&rotR, &scale, &rotU, &translate, &project);
        currentScales[idx] = pxr::GfVec3f { scale };
    }

    posAttr.Set(currentPositions);
    oriAttr.Set(currentOrientations);
    sclAttr.Set(currentScales);
}

Ufe::UndoableCommand::Ptr PointInstanceManip::BuildTransformCmd() const
{
    auto posAttr = instancer.GetPositionsAttr();
    auto oriAttr = instancer.GetOrientationsAttr();
    auto sclAttr = instancer.GetScalesAttr();

    // All attrs are expected at this point, unless we can't author on the prim at all.
    if (!posAttr.IsValid() || !oriAttr.IsValid() || !sclAttr.IsValid()) {
        return nullptr;
    }

    pxr::VtVec3fArray newPos;
    posAttr.Get(&newPos);
    pxr::VtQuathArray newOri;
    oriAttr.Get(&newOri);
    pxr::VtVec3fArray newScl;
    sclAttr.Get(&newScl);

    auto callback = [posAttr,
                     iniPos = initPositions,
                     newPos,
                     oriAttr,
                     iniOri = initOrientations,
                     newOri,
                     sclAttr,
                     iniScl = initScales,
                     newScl](UfeUI::GenericCommand::Mode mode) {
        if (mode == UfeUI::GenericCommand::Mode::kUndo) {
            posAttr.Set(iniPos);
            oriAttr.Set(iniOri);
            sclAttr.Set(iniScl);
        } else if (mode == UfeUI::GenericCommand::Mode::kRedo) {
            posAttr.Set(newPos);
            oriAttr.Set(newOri);
            sclAttr.Set(newScl);
        }
    };

    const auto name = "Transform USD Point Instances";

    const auto prim = instancer.GetPrim();
    const auto stageObject = StageObjectMap::GetInstance()->Get(prim.GetStage());
    const auto ufePath = MaxUsd::ufe::getUsdPrimUfePath(stageObject, prim.GetPath());

    return UfeUi::EditCommand::create(ufePath, UfeUI::GenericCommand::create(callback, name), name);
}