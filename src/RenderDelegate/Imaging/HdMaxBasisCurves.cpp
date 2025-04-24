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
#include "HdMaxBasisCurves.h"

#include "HdMaxInstancer.h"
#include "HdMaxRenderDelegate.h"

#include <RenderDelegate/MaxRenderGeometryFacade.h>
#include <RenderDelegate/SelectionRenderItem.h>

#include <Maxusd/Utilities/HydraUtils.h>
#include <Maxusd/Utilities/TranslationUtils.h>

#include <Graphics/CustomRenderItemHandle.h>
#include <Graphics/GeometryRenderItemHandle.h>
#ifdef USD_VERSION_23_08
#include <pxr/imaging/hdSt/extCompCpuComputation.h>
#include <pxr/imaging/hdSt/extCompPrimvarBufferSource.h>
#else
#include <pxr/imaging/hd/extCompCpuComputation.h>
#include <pxr/imaging/hd/extCompPrimvarBufferSource.h>
#endif
#include <pxr/imaging/hd/extComputation.h>

#include <mutex>

PXR_NAMESPACE_OPEN_SCOPE

// Moved some classes in 23.08.
#ifdef USD_VERSION_23_08
typedef HdStExtCompCpuComputationSharedPtr ExtCompCpuComputationSharedPtr;
typedef HdStExtCompCpuComputation          ExtCompCpuComputation;
typedef HdStExtCompPrimvarBufferSource     ExtCompPrimvarBufferSource;
#else
typedef HdExtCompCpuComputationSharedPtr ExtCompCpuComputationSharedPtr;
typedef HdExtCompCpuComputation          ExtCompCpuComputation;
typedef HdExtCompPrimvarBufferSource     ExtCompPrimvarBufferSource;
#endif

HdMaxBasisCurves::HdMaxBasisCurves(
    HdMaxRenderDelegate* delegate,
    SdfPath const&       rPrimId,
    size_t               renderDataId)
    : HdBasisCurves(rPrimId)
    , renderDelegate(delegate)
{
}

HdDirtyBits HdMaxBasisCurves::GetInitialDirtyBitsMask() const
{
    return HdChangeTracker::InitRepr | HdChangeTracker::DirtyCullStyle
        | HdChangeTracker::DirtyDoubleSided | HdChangeTracker::DirtyExtent
        | HdChangeTracker::DirtyNormals | HdChangeTracker::DirtyPoints
        | HdChangeTracker::DirtyPrimID | HdChangeTracker::DirtyPrimvar
        | HdChangeTracker::DirtyDisplayStyle | HdChangeTracker::DirtyRepr
        | HdChangeTracker::DirtyMaterialId | HdChangeTracker::DirtyTopology
        | HdChangeTracker::DirtyTransform | HdChangeTracker::DirtyVisibility
        | HdChangeTracker::DirtyInstancer | HdChangeTracker::DirtyInstanceIndex
        | HdChangeTracker::CustomBitsBegin;
}

PrimvarInfo* HdMaxBasisCurves::_GetPrimvarInfo(const PrimvarInfoMap& infoMap, const TfToken& token)
{
    const auto it = infoMap.find(token);
    if (it != infoMap.end()) {
        return it->second.get();
    }
    return nullptr;
}

void HdMaxBasisCurves::_UpdatePrimvarSources(
    HdSceneDelegate*     sceneDelegate,
    HdDirtyBits          dirtyBits,
    const TfTokenVector& requiredPrimvars)
{
    if (requiredPrimvars.empty()) {
        return;
    }

    const SdfPath& id = GetId();

    auto updatePrimvarInfo
        = [&](const TfToken& name, const VtValue& value, const HdInterpolation interpolation) {
              PrimvarInfo* info = _GetPrimvarInfo(primvarInfoMap, name);
              if (info) {
                  info->source.data = value;
                  info->source.interpolation = interpolation;
              } else {
                  primvarInfoMap[name]
                      = std::make_unique<PrimvarInfo>(PrimvarSource(value, interpolation));
              }
          };

    const TfTokenVector::const_iterator begin = requiredPrimvars.cbegin();
    const TfTokenVector::const_iterator end = requiredPrimvars.cend();

    // Inspired by HdStInstancer::_SyncPrimvars
    //
    // Get any required instanced primvars from the instancer. Get these before we get
    // any rprims primvars from the rprim itself. If both are present, the rprim's values override
    // the instancer's value.
    const SdfPath& instancerId = GetInstancerId();
    if (!instancerId.IsEmpty()) {
        HdPrimvarDescriptorVector instancerPrimvars
            = sceneDelegate->GetPrimvarDescriptors(instancerId, HdInterpolationInstance);
        for (const HdPrimvarDescriptor& pv : instancerPrimvars) {
            if (std::find(begin, end, pv.name) == end) {
                // Erase the unused primvar so we don't hold onto stale data.
                primvarInfoMap.erase(pv.name);
            } else {
                if (HdChangeTracker::IsPrimvarDirty(dirtyBits, instancerId, pv.name)) {
                    const VtValue value = sceneDelegate->Get(instancerId, pv.name);
                    updatePrimvarInfo(pv.name, value, HdInterpolationInstance);
                }
            }
        }
    }

    for (size_t i = 0; i < HdInterpolationCount; i++) {
        const HdInterpolation           interp = static_cast<HdInterpolation>(i);
        const HdPrimvarDescriptorVector primvars = GetPrimvarDescriptors(sceneDelegate, interp);

        for (const HdPrimvarDescriptor& pv : primvars) {
            if (std::find(begin, end, pv.name) == end) {
                // Erase the unused primvar so we don't hold onto stale data.
                primvarInfoMap.erase(pv.name);
            } else {
                if (HdChangeTracker::IsPrimvarDirty(dirtyBits, id, pv.name)) {
                    const VtValue value = GetPrimvar(sceneDelegate, pv.name);
                    updatePrimvarInfo(pv.name, value, interp);
                }
            }
        }
    }

    // Get the descriptors of computed primvars.
    HdExtComputationPrimvarDescriptorVector computedPrimvars
        = sceneDelegate->GetExtComputationPrimvarDescriptors(id, HdInterpolationVertex);
    const HdRenderIndex& renderIndex = sceneDelegate->GetRenderIndex();

    // At this point we've searched the primvars for the required primvars.
    // check to see if there are any HdExtComputation which should replace
    // of fill in for a missing primvar.
    for (const auto& primvarName : requiredPrimvars) {
        // Check if the primvar must be computed.
        auto result = std::find_if(
            computedPrimvars.begin(), computedPrimvars.end(), [&](const auto& compPrimvar) {
                return compPrimvar.name == primvarName;
            });
        if (result == computedPrimvars.end()) {
            continue;
        }

        HdExtComputationPrimvarDescriptor compPrimvar = *result;
        // Create the HdExtCompCpuComputation objects necessary to resolve the computation.
        HdExtComputation const* sourceComp
            = static_cast<HdExtComputation const*>(renderIndex.GetSprim(
                HdPrimTypeTokens->extComputation, compPrimvar.sourceComputationId));
        if (!sourceComp || sourceComp->GetElementCount() <= 0) {
            continue;
        }

        // This primvar must be computed.
        // The compPrimvar has the Id of the compute the data comes from, and the output
        // of the compute which contains the data.

        ExtCompCpuComputationSharedPtr cpuComputation;
        HdBufferSourceSharedPtrVector  sources;
        cpuComputation
            = ExtCompCpuComputation::CreateComputation(sceneDelegate, *sourceComp, &sources);

        // The last thing in source is the resolve of the computation that is our points.
        HdBufferSourceSharedPtr pointsSource(new ExtCompPrimvarBufferSource(
            compPrimvar.name,
            cpuComputation,
            compPrimvar.sourceComputationOutputName,
            compPrimvar.valueType));

        sources.push_back(pointsSource);

        // Resolve the computation.
        for (HdBufferSourceSharedPtr& source : sources) {
            source->Resolve();
        }

        const GfVec3f* points = static_cast<const GfVec3f*>(pointsSource->GetData());
        VtVec3fArray   vtPoints;
        vtPoints.resize(pointsSource->GetNumElements());
        std::copy(points, points + pointsSource->GetNumElements(), vtPoints.data());
        updatePrimvarInfo(primvarName, VtValue(vtPoints), HdInterpolationVertex);
    }
}

void HdMaxBasisCurves::_LoadPoints(
    const pxr::SdfPath&          id,
    HdSceneDelegate*             delegate,
    const HdBasisCurvesTopology& topology)
{
    const auto info = _GetPrimvarInfo(primvarInfoMap, HdTokens->points);
    auto&      renderData = _GetRenderData();
    auto&      points = renderData.points;
    if (info && !info->source.data.IsEmpty() && info->source.data.CanCast<pxr::VtVec3fArray>()) {
        points = info->source.data.UncheckedGet<pxr::VtVec3fArray>();
    }
}

HdMaxBasisCurvesRenderData::SubsetRenderData HdMaxBasisCurves::_InitializeSubsetRenderData(
    const SdfPath& materialId,
    bool           instanced,
    bool           wireframe)
{
    std::lock_guard<std::recursive_mutex> maxLock(MaxUsd::GetMaxSdkMutex());

    HdMaxBasisCurvesRenderData::SubsetRenderData renderData;
    renderData.materiaId = materialId;
    if (!instanced) {
        // Initialize 2 render items, one for regular display, and one for when we need to display
        // selection highlighting. One OR the other is used. The render item used for selection will
        // display both the geometry and the highlight.

        MaxSDK::Graphics::GeometryRenderItemHandle geometryRenderItem;
        geometryRenderItem.Initialize();
        auto simpleRenderGeometry = new MaxSDK::Graphics::SimpleRenderGeometry {};
        geometryRenderItem.SetRenderGeometry(simpleRenderGeometry);
        renderData.renderItem.Initialize(geometryRenderItem);
        renderData.renderItem.SetVisibilityGroup(
            wireframe ? MaxSDK::Graphics::RenderItemVisible_Wireframe
                      : MaxSDK::Graphics::RenderItemVisible_Shaded);

        MaxSDK::Graphics::CustomRenderItemHandle usdRenderItem;
        usdRenderItem.Initialize();
        const auto item = new SelectionRenderItem(
            static_cast<MaxSDK::Graphics::IRenderGeometryPtr>(simpleRenderGeometry), wireframe);
        usdRenderItem.SetCustomImplementation(item);
        renderData.selectionRenderItem.Initialize(usdRenderItem);
        renderData.selectionRenderItem.SetVisibilityGroup(
            wireframe ? MaxSDK::Graphics::RenderItemVisible_Wireframe
                      : MaxSDK::Graphics::RenderItemVisible_Shaded);

        renderData.geometry = std::make_unique<MaxRenderGeometryFacade>(simpleRenderGeometry);
    } else {
        // Again, initialize 2 render items for instances. However, we need to display both when
        // highlighting, the instanceSelectionRenderGeometry only carries the highlight.
        auto instanceRenderGeometry
            = new MaxRestrictedSDKSupport::Graphics::ViewportInstancing::InstanceDisplayGeometry {};
        auto instanceSelectionRenderGeometry
            = new MaxRestrictedSDKSupport::Graphics::ViewportInstancing::InstanceDisplayGeometry {};
        renderData.geometry = std::make_unique<MaxRenderGeometryFacade>(
            instanceRenderGeometry, instanceSelectionRenderGeometry);
    }

    renderData.geometry->SetPrimitiveType(MaxSDK::Graphics::PrimitiveLineList);

    const auto requiredStreams = HdMaxBasisCurvesRenderData::GetRequiredStreams();

    renderData.geometry->SetStreamRequirement(requiredStreams);

    return renderData;
}

HdMaxBasisCurvesRenderData& HdMaxBasisCurves::_GetRenderData()
{
    return renderDelegate->GetBasisCurvesRenderData(GetId());
}

bool HdMaxBasisCurves::PrimvarIsRequired(const TfToken& primvar) const
{
    const TfTokenVector::const_iterator begin = requiredPrimvars.cbegin();
    const TfTokenVector::const_iterator end = requiredPrimvars.cend();
    return (std::find(begin, end, primvar) != end);
}

void HdMaxBasisCurves::Sync(
    HdSceneDelegate* delegate,
    HdRenderParam*   renderParam,
    HdDirtyBits*     dirtyBits,
    TfToken const&   reprToken)
{
    const auto& id = GetId();
    auto&       displaySettings = renderDelegate->GetDisplaySettings();

    auto& renderData = _GetRenderData();
    if (!renderData.renderTagActive) {
        return;
    }

    // Update the topology.
    bool topologyDirty = HdChangeTracker::IsTopologyDirty(*dirtyBits, id);

    if (topologyDirty) {
        sourceTopology = delegate->GetBasisCurvesTopology(id);

        renderData.sourceTopology = sourceTopology;
        renderData.sourceNumPoints = sourceTopology.GetNumPoints();
    }

    bool needPrimvarSync = false;
    // Simple lambda to check/update the requirement for a given primvar, and the need to
    // synchronize it.
    auto checkPrimvar = [this, &id, &dirtyBits, &needPrimvarSync](
                            const TfToken primvarName, bool& dirtyFlag, bool condition) {
        if (HdChangeTracker::IsPrimvarDirty(*dirtyBits, id, primvarName)) {
            if (condition) {
                if (!PrimvarIsRequired(primvarName)) {
                    requiredPrimvars.push_back(primvarName);
                }
            } else {
                const auto it
                    = std::find(requiredPrimvars.begin(), requiredPrimvars.end(), primvarName);
                if (it != requiredPrimvars.end()) {
                    requiredPrimvars.erase(it);
                }
            }
            dirtyFlag = true;
            needPrimvarSync = true;
        }
    };

    auto pointsDirty = HdChangeTracker::IsPrimvarDirty(*dirtyBits, id, HdTokens->points);
    checkPrimvar(HdTokens->points, pointsDirty, true);

    if (needPrimvarSync) {
        _UpdatePrimvarSources(delegate, *dirtyBits, requiredPrimvars);
    }

    // Update the instancer.
    _UpdateInstancer(delegate, dirtyBits);
    const SdfPath& instancerId = delegate->GetInstancerId(GetId());

    const auto instancerDirty = HdChangeTracker::IsInstancerDirty(*dirtyBits, id);
    const auto instancerIndicesDirty = HdChangeTracker::IsInstanceIndexDirty(*dirtyBits, id);
    if (instancerDirty || instancerIndicesDirty) {
        instancer = delegate->GetRenderIndex().GetInstancer(instancerId);
    }

    auto isTransformDirty = HdChangeTracker::IsTransformDirty(*dirtyBits, id);
    auto isExtentDirty = HdChangeTracker::IsExtentDirty(*dirtyBits, id);

    // If the topology has changed, we need to recompute the indices.
    if (topologyDirty) {
        bool instanced = instancer != nullptr;

        // Render item to be used for shaded rendering
        if (!renderData.shadedCurve.geometry) {
            renderData.shadedCurve = _InitializeSubsetRenderData({}, instanced, false);
        }

        // Wireframe render item used for wireframe rendering
        if (!renderData.wireframeCurve.geometry) {
            renderData.wireframeCurve = _InitializeSubsetRenderData({}, instanced, true);
        }

        // Flag transforms dirty as they will need to be re-applied on any newly created render
        // items.
        isTransformDirty = true;

        renderingTopology = sourceTopology;

        const TfToken     type = renderingTopology.GetCurveType();
        const TfToken     basis = renderingTopology.GetCurveBasis();
        const TfToken     wrap = renderingTopology.GetCurveWrap();
        const VtIntArray& curveVertexCounts = renderingTopology.GetCurveVertexCounts();

        pxr::VtIntArray curveIndices;
        int             indexCount = 0;

        // Generate the indices for the curve.
        // The following is an example of how the indices are generated:
        // Curve Vertex Counts:
        //          [3, 4, 3]
        //  (i.e 3 curves, first has 3, second has 4, third has 3 vertices)
        // The indices are generated as follows for periodic wrap mode:
        //          [0,1, 1,2, 2,0,  3,4, 4,5, 5,6, 6,3,  7,8, 8,9, 9,7]
        //           i,j  i,j  i,j   i,j  i,j  i,j  i,j   i,j  i,j  i,j
        // The indices are generated as follows for non-periodic wrap mode:
        //          [0,1, 1,2,  3,4, 4,5, 5,6,  7,8, 8,9]
        //           i,j  i,j   i,j  i,j  i,j   i,j  i,j
        for (int count : curveVertexCounts) {
            int sentinel = (wrap == HdTokens->periodic ? count : count - 1);
            for (int startVal = indexCount; indexCount < startVal + sentinel; ++indexCount) {
                int i = indexCount;
                int j = (indexCount + 1) == (count + startVal) ? startVal : (indexCount + 1);
                curveIndices.push_back(i);
                curveIndices.push_back(j);
            }
            if (wrap != HdTokens->periodic)
                ++indexCount;
        }

        auto previousIndicesSize = renderData.shadedCurve.wireIndices.size();
        if (curveIndices.size() != previousIndicesSize) {
            HdMaxChangeTracker::SetDirty(
                renderData.shadedCurve.dirtyBits, HdMaxChangeTracker::DirtyIndicesSize);
            HdMaxChangeTracker::SetDirty(
                renderData.shadedCurve.dirtyBits, HdMaxChangeTracker::DirtyIndices);
        } else if (!std::equal(
                       renderData.shadedCurve.wireIndices.cbegin(),
                       renderData.shadedCurve.wireIndices.cend(),
                       curveIndices.cbegin(),
                       curveIndices.cend())) {
            HdMaxChangeTracker::SetDirty(
                renderData.shadedCurve.dirtyBits, HdMaxChangeTracker::DirtyIndices);
        }

        renderData.shadedCurve.wireIndices = curveIndices;
        renderData.wireframeCurve.wireIndices = curveIndices;

        // Just 1 subset for now for basis curves.
        renderData.instancer->SetSubsetCount(1);

        // Typically we just flag all bits as not dirty at the end of the Sync() call.
        // However, in case no render items are created (no topology defined) at this timeCode,
        // we need to flag the topology as not dirty now, in case we return from the function
        // just below.
        *dirtyBits &= ~HdChangeTracker::DirtyTopology;
    }

    // Update of the vertex position data if needed.
    if (pointsDirty) {
        const auto previousPointsSize = renderData.points.size();
        _LoadPoints(id, delegate, sourceTopology);
        const auto newPointsSize = renderData.points.size();

        renderData.SetAllSubsetRenderDataDirty(HdMaxChangeTracker::DirtyPoints);
        if (previousPointsSize != newPointsSize) {
            renderData.SetAllSubsetRenderDataDirty(HdMaxChangeTracker::DirtyPointsSize);
        }
    }

    // Visibility - simply flag the render item as visible or not. Later on, this will control
    // whether the render item is considered for actual rendering.
    if (HdChangeTracker::IsVisibilityDirty(*dirtyBits, id)) {
        renderData.visible = delegate->GetVisible(id);
        *dirtyBits &= ~HdChangeTracker::DirtyVisibility;
        renderData.SetAllSubsetRenderDataDirty(HdMaxChangeTracker::DirtyVisibility);
    }

    // Update the render item transforms. Instance transforms are handled separately.
    if (isTransformDirty && !instancer) {
        // We could set the transform on the render item right now, but this would require a lock,
        // and on scenes with many objects with animated transforms, this has a non-negligible cost.
        auto transform = delegate->GetTransform(id);

        if (renderData.transform != transform) {
            renderData.transform = transform;
            renderData.SetAllSubsetRenderDataDirty(HdMaxChangeTracker::DirtyTransforms);
        }
    }

    // Update the bounding box for the item.
    if ((isTransformDirty || isExtentDirty) && !instancer) {
        if (isExtentDirty) {
            renderData.extent = delegate->GetExtent(id);
        }
        // Compute the Prim's bounding box in world space.
        GfBBox3d box { renderData.extent };
        box.Transform(renderData.transform);
        renderData.boundingBox = box.ComputeAlignedBox();
    }

    auto dirtySelectionHighlight = bool(*dirtyBits & DirtySelectionHighlight);

    // For anything else than selection changes, we need to update instances.
    // Changes to selected instances are handled below on DirtySelectionHighlight.
    if (instancer && *dirtyBits != (HdChangeTracker::Varying | DirtySelectionHighlight)) {
        VtMatrix4dArray transforms
            = static_cast<HdMaxInstancer*>(instancer)->ComputeInstanceTransforms(id);
        // The final transform is the product of the basiscurves's transform and the instance's
        // transform.
        auto curveTransform = delegate->GetTransform(id);
        for (auto& transform : transforms) {
            transform = curveTransform * transform;
        }

        auto newInstanceCount = transforms.size();

        auto extent = delegate->GetExtent(id);
        renderData.extent = extent;
        // Compute the total bounding box given all instances.
        renderData.boundingBox = MaxUsd::ComputeTotalExtent(extent, transforms);

        std::lock_guard<std::recursive_mutex> maxLock(MaxUsd::GetMaxSdkMutex());

        bool needFullRebuild = instancerIndicesDirty || topologyDirty;
        renderData.SetAllSubsetRenderDataDirty(HdMaxChangeTracker::DirtyTransforms);
        if (instanceCount != newInstanceCount) {
            renderData.SetAllSubsetRenderDataDirty(HdMaxChangeTracker::DirtyTransformsSize);
        }

        renderData.instancer->RequestUpdate(needFullRebuild, transforms);
        instanceCount = newInstanceCount;
    }

    if (dirtySelectionHighlight) {
        // Start by clearing the current selection.
        renderData.selected = false;
        auto selStatus = renderDelegate->GetSelectionStatus(id);
        // For instances, need to check what instance indices are selected, and update the instancer
        // accordingly.
        if (instancer) {
            renderData.instancer->ResetSelection();

            if (selStatus) {
                renderData.selected = !selStatus->instanceIndices.empty();
                for (const auto& indexArray : selStatus->instanceIndices) {
                    for (const auto index : indexArray) {
                        renderData.instancer->Select(index);
                    }
                }
            }
            renderData.instancer->RequestSelectionDisplayUpdate(true);
        } else if (selStatus) {
            renderData.selected = selStatus->fullySelected;
        }
        renderData.SetAllSubsetRenderDataDirty(HdMaxChangeTracker::DirtySelectionHighlight);
    }

    // This clears all the non-custom dirty bits. This ensures that the rprim
    // doesn't have pending dirty bits that add it to the dirty list every
    // frame. GetInitialDirtyBitsMask sets certain dirty bits that aren't
    // reset (e.g. DirtyExtent, DirtyPrimID) that make this necessary.
    *dirtyBits &= ~HdChangeTracker::AllSceneDirtyBits;
    // Clear custom selection highlight bit.
    *dirtyBits &= ~DirtySelectionHighlight;
}

HdDirtyBits HdMaxBasisCurves::_PropagateDirtyBits(HdDirtyBits bits) const
{
    // When instance indices change, we also need to update our selection, as we directly rely
    // on the indices.
    if (bits & HdChangeTracker::DirtyInstanceIndex) {
        bits |= DirtySelectionHighlight;
    }

    return bits;
}

void HdMaxBasisCurves::_InitRepr(TfToken const& reprToken, HdDirtyBits* dirtyBits)
{
    TF_UNUSED(dirtyBits);

    // Create an empty repr.
    _ReprVector::iterator it
        = std::find_if(_reprs.begin(), _reprs.end(), _ReprComparator(reprToken));
    if (it == _reprs.end()) {
        _reprs.emplace_back(reprToken, HdReprSharedPtr());
    }
}

void HdMaxBasisCurves::Finalize(HdRenderParam* renderParam) { }

PXR_NAMESPACE_CLOSE_SCOPE

// Disable obscure warning : no definition for inline function : pxr::DefaultValueHolder
// Was not able to identify what is triggering this.
#pragma warning(disable : 4506)
