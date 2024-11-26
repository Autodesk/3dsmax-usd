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
#include "HdMaxMesh.h"

#include "HdMaxInstancer.h"
#include "HdMaxMaterial.h"
#include "HdMaxRenderDelegate.h"

#include <RenderDelegate/DebugCodes.h>
#include <RenderDelegate/HdMaxColorMaterial.h>
#include <RenderDelegate/HdMaxDisplaySettings.h>
#include <RenderDelegate/MaxRenderGeometryFacade.h>
#include <RenderDelegate/SelectionRenderItem.h>

#include <MaxUsd/Utilities/MaterialUtils.h>
#include <MaxUsd/Utilities/TypeUtils.h>
#include <Maxusd/Utilities/TranslationUtils.h>

#include <pxr/imaging/hd/extComputationUtils.h>
#include <pxr/imaging/hd/material.h>
#include <pxr/imaging/hd/mesh.h>
#include <pxr/imaging/hd/meshTopology.h>
#include <pxr/imaging/hd/meshUtil.h>
#include <pxr/imaging/hd/smoothNormals.h>
#include <pxr/imaging/hd/vertexAdjacency.h>

#include <Graphics/CustomRenderItemHandle.h>
#include <Graphics/GeometryRenderItemHandle.h>
#include <Graphics/IConsolidationStrategy.h>
#include <Graphics/IVirtualDevice.h>
#include <Graphics/RenderItemHandleDecorator.h>
#include <Graphics/SimpleRenderGeometry.h>
#include <Graphics/StandardMaterialHandle.h>
#ifdef USD_VERSION_23_08
#include <pxr/imaging/hdSt/extCompCpuComputation.h>
#include <pxr/imaging/hdSt/extCompPrimvarBufferSource.h>
#else
#include <pxr/imaging/hd/extCompCpuComputation.h>
#include <pxr/imaging/hd/extCompPrimvarBufferSource.h>
#endif
#include <pxr/imaging/hd/extComputation.h>

#include <mutex>

// Hydra rendering is heavily multi-threaded. The Sync() method below is called from many threads -
// some calls related to the 3dsMax SDK, and the Graphics APIs must be protected with mutexes. Note
// that the Sync() function is reentrant, i.e. in some scenarios we can reenter this function on the
// same thread before exiting the first invocation - therefore we must use recursive mutexes, which
// allow this.
namespace {
std::recursive_mutex maxSdkMutex;
} // namespace

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

HdMaxMesh::HdMaxMesh(HdMaxRenderDelegate* delegate, SdfPath const& rPrimId, size_t renderDataId)
    : HdMesh(rPrimId)
    , renderDelegate(delegate)
{
}

HdDirtyBits HdMaxMesh::GetInitialDirtyBitsMask() const
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

PrimvarInfo* _GetPrimvarInfo(const PrimvarInfoMap& infoMap, const TfToken& token)
{
    const auto it = infoMap.find(token);
    if (it != infoMap.end()) {
        return it->second.get();
    }
    return nullptr;
}

bool _IsSharedVertexLayoutPossible(const PrimvarInfoMap& primvarInfo)
{
    for (const auto& it : primvarInfo) {
        const HdInterpolation interpolation = it.second->source.interpolation;
        if (interpolation == HdInterpolationUniform
            || interpolation == HdInterpolationFaceVarying) {
            return false;
        }
    }
    return true;
}

void HdMaxMesh::_UpdatePrimvarSources(
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

// Helper utility function to fill some primvar data to a vertex buffer.
template <class DEST_TYPE, class SRC_TYPE>
void _FillPrimvarData(
    DEST_TYPE*               vertexBuffer,
    size_t                   numVertices,
    const VtIntArray&        renderingToSceneFaceVtxIds,
    const pxr::SdfPath&      rprimId,
    const HdMeshTopology&    topology,
    const TfToken&           primvarName,
    const VtArray<SRC_TYPE>& primvarData,
    const HdInterpolation&   primvarInterpolation)
{
    switch (primvarInterpolation) {
    case HdInterpolationConstant:
        // Same value at every vertex.
        for (size_t v = 0; v < numVertices; ++v) {
            SRC_TYPE* pointer = reinterpret_cast<SRC_TYPE*>(&vertexBuffer[v]);
            *pointer = primvarData[0];
        }
        break;
    case HdInterpolationVarying:
    case HdInterpolationVertex:
        // One value per vertex.
        if (numVertices <= renderingToSceneFaceVtxIds.size()) {
            const size_t dataSize = primvarData.size();
            for (size_t v = 0; v < numVertices; ++v) {
                unsigned int index = renderingToSceneFaceVtxIds[v];
                if (index < dataSize) {
                    SRC_TYPE* pointer = reinterpret_cast<SRC_TYPE*>(&vertexBuffer[v]);
                    *pointer = primvarData[index];
                } else {
                    TF_DEBUG(HDMAX_DEBUG_MESH)
                        .Msg(
                            "Invalid Hydra prim '%s': "
                            "primvar %s has %u elements, while its topology "
                            "references face vertex index %u.\n",
                            rprimId.GetString(),
                            primvarName.GetText(),
                            dataSize,
                            index);
                }
            }
        } else {
            TF_CODING_ERROR(
                "Invalid Hydra prim '%s': "
                "requires %zu vertices, while the number of elements in "
                "renderingToSceneFaceVtxIds is %zu. Skipping primvar update.",
                rprimId.GetString(),
                numVertices,
                renderingToSceneFaceVtxIds.size());

            memset(vertexBuffer, 0, sizeof(DEST_TYPE) * numVertices);
        }
        break;
    case HdInterpolationUniform: {
        // One value per face.
        const VtIntArray& faceVertexCounts = topology.GetFaceVertexCounts();
        const size_t      numFaces = faceVertexCounts.size();
        if (numFaces <= primvarData.size()) {
            // The primvar has more data than needed, we issue a warning but
            // don't skip update. Truncate the buffer to the expected length.
            if (numFaces < primvarData.size()) {
                TF_DEBUG(HDMAX_DEBUG_MESH)
                    .Msg(
                        "Invalid Hydra prim '%s': "
                        "primvar %s has %zu elements, while its topology "
                        "references only up to element index %zu.\n",
                        rprimId.GetString(),
                        primvarName.GetText(),
                        primvarData.size(),
                        numFaces);
            }

            for (size_t f = 0, v = 0; f < numFaces; ++f) {
                const size_t faceVertexCount = faceVertexCounts[f];
                const size_t faceVertexEnd = v + faceVertexCount;
                for (; v < faceVertexEnd; ++v) {
                    SRC_TYPE* pointer = reinterpret_cast<SRC_TYPE*>(&vertexBuffer[v]);
                    *pointer = primvarData[f];
                }
            }
        } else {
            // The primvar has less data than needed. Issue warning and skip
            // update like what is done in HdStMesh.
            TF_DEBUG(HDMAX_DEBUG_MESH)
                .Msg(
                    "Invalid Hydra prim '%s': "
                    "primvar %s has only %zu elements, while its topology expects "
                    "at least %zu elements. Skipping primvar update.\n",
                    rprimId.GetString(),
                    primvarName.GetText(),
                    primvarData.size(),
                    numFaces);

            memset(vertexBuffer, 0, sizeof(DEST_TYPE) * numVertices);
        }
        break;
    }
    case HdInterpolationFaceVarying:
        // One value per face-vertex.

        // Unshared vertex layout is required for face-varying primvars, so we "flatten"
        // the data making sure each face vertex has a copy. In this case, the face vertex
        // indices will be a natural sequence [0-N].
        for (int i = 0; i < renderingToSceneFaceVtxIds.size(); ++i) {
            const auto dataSize = primvarData.size();
            const auto index = renderingToSceneFaceVtxIds[i];
            if (index <= dataSize) {
                SRC_TYPE* pointer = reinterpret_cast<SRC_TYPE*>(&vertexBuffer[i]);
                *pointer = primvarData[renderingToSceneFaceVtxIds[i]];
            } else {
                TF_DEBUG(HDMAX_DEBUG_MESH)
                    .Msg(
                        "Invalid Hydra prim '%s': "
                        "primvar %s has %u elements, while its topology "
                        "references face vertex index %u.\n",
                        rprimId.GetString(),
                        primvarName.GetText(),
                        dataSize,
                        index);
            }
        }
        break;
    default:
        TF_CODING_ERROR(
            "Invalid Hydra prim '%s': "
            "unimplemented interpolation %d for primvar %s",
            rprimId.GetString(),
            (int)primvarInterpolation,
            primvarName.GetText());
        break;
    }
}

// Depending on the primvar interpolation used, and the whether or not we can share vertices between
// faces, we might need to adjust the layout of the data that will end up in the nitrous vertex
// buffers. For example, for a constant interpolation, we need to set all the values (either for
// every vertex or every face-vertex if vertices are not shared) to the same thing. On the USD side,
// the primvar is an array with a single value, but on the Max side, we need to replicate the data.
void _adjustPrimvarDataLayout(
    pxr::SdfPath          primId,
    PrimvarInfo*          info,
    const TfToken&        primvarName,
    bool                  sharedVertexLayout,
    VtVec3fArray&         primvarData,
    const HdMeshTopology& topology)
{
    const VtIntArray& faceVertexIndices = topology.GetFaceVertexIndices();
    // If an unshared layout is required we need to adjust normals, to get one normals per-face
    // vertex. If we are using faceVarying interpolation, it is already the case.
    if (!sharedVertexLayout && info->source.interpolation != HdInterpolationFaceVarying) {

        const size_t numFaceVertexIndices = faceVertexIndices.size();
        VtVec3fArray unsharedVertexData(numFaceVertexIndices);
        _FillPrimvarData(
            unsharedVertexData.data(),
            numFaceVertexIndices,
            faceVertexIndices,
            primId,
            topology,
            primvarName,
            primvarData,
            info->source.interpolation);
        primvarData = unsharedVertexData;
    }
    // Shared layout and constant interpolation, adjust the buffer so we get the same value for
    // every vertex.
    else if (info->source.interpolation == HdInterpolationConstant) {
        VtVec3fArray constVertexData(topology.GetNumPoints());
        _FillPrimvarData(
            constVertexData.data(),
            topology.GetNumPoints(),
            faceVertexIndices,
            primId,
            topology,
            primvarName,
            primvarData,
            HdInterpolationConstant);
        primvarData = constVertexData;
    }
}

void HdMaxMesh::_LoadPoints(
    const pxr::SdfPath&   id,
    HdSceneDelegate*      delegate,
    const HdMeshTopology& topology)
{
    const auto info = _GetPrimvarInfo(primvarInfoMap, HdTokens->points);
    auto&      renderData = _GetRenderData();
    auto&      points = renderData.points;
    if (info && !info->source.data.IsEmpty() && info->source.data.CanCast<pxr::VtVec3fArray>()) {
        points = info->source.data.UncheckedGet<pxr::VtVec3fArray>();
    }

    // If there are no vertices available, we are done with this mesh.
    if (points.empty()) {
        return;
    }

    if (!sharedVertexLayout) {
        const VtIntArray& faceVertexIndices = topology.GetFaceVertexIndices();
        const size_t      numFaceVertexIndices = faceVertexIndices.size();
        VtVec3fArray      unsharedPoints(numFaceVertexIndices);
        _FillPrimvarData(
            unsharedPoints.data(),
            numFaceVertexIndices,
            faceVertexIndices,
            GetId(),
            topology,
            HdTokens->points,
            points,
            HdInterpolationFaceVarying);
        points = unsharedPoints;
    }
}

void HdMaxMesh::_LoadNormals(const pxr::SdfPath& id, HdSceneDelegate* delegate)
{
    const auto info = _GetPrimvarInfo(primvarInfoMap, HdTokens->normals);

    auto& renderData = _GetRenderData();

    auto& normals = renderData.normals;

    // Are normals explicitly defined?
    if (info && !info->source.data.IsEmpty() && info->source.data.CanCast<pxr::VtVec3fArray>()) {
        normals = info->source.data.UncheckedGet<pxr::VtVec3fArray>();
        _adjustPrimvarDataLayout(
            id, info, HdTokens->normals, sharedVertexLayout, normals, sourceTopology);
    }
    // Otherwise, compute the normals. These are smooth computed normals.
    else {
        Hd_VertexAdjacency adjacency;
        adjacency.BuildAdjacencyTable(&sourceTopology);

        const auto        pointsInfo = _GetPrimvarInfo(primvarInfoMap, HdTokens->points);
        pxr::VtVec3fArray sourcePoints;
        if (pointsInfo && !pointsInfo->source.data.IsEmpty()
            && pointsInfo->source.data.CanCast<pxr::VtVec3fArray>()) {
            sourcePoints = pointsInfo->source.data.UncheckedGet<pxr::VtVec3fArray>();
        }

        if (!sourcePoints.empty()) {
            auto computedNormals = Hd_SmoothNormals::ComputeSmoothNormals(
                &adjacency, int(sourcePoints.size()), sourcePoints.cdata());
            // The computed normals above, are vertex normals, if we need an unshared layout, we
            // need to make sure we have one normal value per face-vertex.
            if (!sharedVertexLayout) {
                const VtIntArray& renderingToSceneVertices = sourceTopology.GetFaceVertexIndices();
                normals.resize(renderData.points.size());
                for (int i = 0; i < normals.size(); ++i) {
                    normals[i] = computedNormals[renderingToSceneVertices[i]];
                }
            } else {
                normals = computedNormals;
            }
        }
    }
}

void HdMaxMesh::_LoadUvs(
    const pxr::SdfPath&         id,
    HdSceneDelegate*            delegate,
    const std::vector<TfToken>& uvPrimvars)
{
    auto& renderData = _GetRenderData();

    const auto numUvsChannels = uvPrimvars.size();

    auto fillWithPoints = [this, &renderData](pxr::VtVec3fArray& uvs) {
        auto& points = renderData.points;
        uvs.resize(points.size());
        std::copy(points.begin(), points.end(), uvs.begin());
    };

    // Guaranty at least one UV channel, to be used as fallback, if none defined.
    // We need a valid buffer - Use simple planar mapping similar to the usual 3dsmax defaults.
    if (numUvsChannels == 0) {
        renderData.uvs.resize(1);
        fillWithPoints(renderData.uvs[0].data);
        return;
    }

    renderData.uvs.resize(numUvsChannels);
    for (int i = 0; i < numUvsChannels; ++i) {
        auto& uvs = renderData.uvs[i].data;
        renderData.uvs[i].varname = uvPrimvars[i];

        const auto info = _GetPrimvarInfo(primvarInfoMap, uvPrimvars[i]);
        if (!info) {
            uvs.clear();
        } else {
            bool acceptedPrimvarType = true;
            if (info->source.data.CanCast<pxr::VtVec2fArray>()) {
                auto& data = info->source.data.UncheckedGet<pxr::VtVec2fArray>();
                uvs.resize(data.size());
                for (int i = 0; i < data.size(); ++i) {
                    uvs[i][0] = data[i][0];
                    uvs[i][1] = 1 - data[i][1]; // Adjust UV coordinate for Nitrous.
                    uvs[i][2] = 0;
                }
            } else if (info->source.data.CanCast<pxr::VtVec3fArray>()) {
                uvs = info->source.data.UncheckedGet<pxr::VtVec3fArray>();
                std::transform(uvs.begin(), uvs.end(), uvs.begin(), [](GfVec3f in) {
                    return GfVec3f(in[0], -in[1], in[2]);
                });
            } else if (info->source.data.CanCast<pxr::VtFloatArray>()) {
                const auto val = info->source.data.UncheckedGet<pxr::VtFloatArray>();
                uvs.resize(val.size());
                for (int idx = 0; idx < uvs.size(); ++idx) {
                    uvs[idx] = pxr::GfVec3f { val[idx], val[idx], val[idx] };
                }
            } else {
                TF_WARN("Unexpected primvar type.");
                acceptedPrimvarType = false;
            }

            if (acceptedPrimvarType) {
                _adjustPrimvarDataLayout(
                    id, info, uvPrimvars[i], sharedVertexLayout, uvs, sourceTopology);
            }
        }

        // If we need UVs, but get no data, fallback to planar mapping.
        if (uvs.empty()) {
            fillWithPoints(uvs);
        }
    }
}

void HdMaxMesh::_LoadDisplayColor(const pxr::SdfPath& id, HdSceneDelegate* delegate)
{
    auto&      renderData = _GetRenderData();
    auto&      displayColors = renderData.colors;
    const auto info = _GetPrimvarInfo(primvarInfoMap, HdTokens->displayColor);
    if (info && !info->source.data.IsEmpty() && info->source.data.CanCast<pxr::VtVec3fArray>()) {
        displayColors = info->source.data.UncheckedGet<pxr::VtVec3fArray>();
        _adjustPrimvarDataLayout(
            id, info, HdTokens->displayColor, sharedVertexLayout, displayColors, sourceTopology);
    }
}

HdMaxRenderData::SubsetRenderData
HdMaxMesh::_InitializeSubsetRenderData(const SdfPath& materialId, bool instanced, bool wireframe)
{
    std::lock_guard<std::recursive_mutex> maxLock(maxSdkMutex);

    HdMaxRenderData::SubsetRenderData renderData;
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

    renderData.geometry->SetPrimitiveType(
        wireframe ? MaxSDK::Graphics::PrimitiveLineList : MaxSDK::Graphics::PrimitiveTriangleList);

    const auto requiredStreams = HdMaxRenderData::GetRequiredStreams(wireframe);

    // TODO : Specifying tangents/binormals and all 4 UV channels is required to get good results
    // when the viewport is set to "high quality".
    renderData.geometry->SetStreamRequirement(requiredStreams);

    return renderData;
}

void HdMaxMesh::_UpdatePerMaterialRenderData(
    HdSceneDelegate* delegate,
    const SdfPath&   materialId,
    HdMaxRenderData& renderData,
    bool             instanced)
{
    const HdGeomSubsets& geomSubsets = sourceTopology.GetGeomSubsets();

    // If the UsdGeomSubsets do not cover all the faces in the mesh we need
    // to add an additional render item for those faces.
    size_t numFacesWithoutRenderItem = sourceTopology.GetNumFaces();

    // faceIdToMaterialSubset is used later to split the geometry into separate Nitrous meshes.
    faceIdToMaterialSubset.clear();
    faceIdToMaterialSubset.resize(sourceTopology.GetNumFaces(), materialId);

    // Things have changed, and we need to update the "per-material" render data, but let's try to
    // reuse what we already have as much as possible. Indeed, if we already have some nitrous
    // data generated for a material/mesh, we can reuse it. Anything we can't reuse has to be
    // deleted.

    // A map helping us keeping track of what we currently have, and will need.
    // Key -> The path of the material of this mesh subset.
    // Val -> A pair..
    //         Key : The render data.
    //         Val : True if we still need this render data, false otherwise.
    std::map<pxr::SdfPath, std::pair<HdMaxRenderData::SubsetRenderData, bool>> updatedSubsets;

    // Populate with the previous per-material data, initially we don't know if we will still need
    // these...
    for (const auto& data : renderData.shadedSubsets) {
        updatedSubsets.insert({ data.materiaId, { data, false } });
    }

    // Simple lambda to create or update a material subset render data.
    auto CreateOrUpdateMaterialSubset
        = [this, &updatedSubsets, &instanced](const pxr::SdfPath& matId) {
              // If we dont already have a subset for this material, create one.
              const auto it = updatedSubsets.find(matId);
              if (it == updatedSubsets.end()) {
                  auto item = _InitializeSubsetRenderData(matId, instanced, false);
                  updatedSubsets.insert({ matId, { item, true } });
              }
              // Otherwise just flag the one we have as still needed.
              else {
                  it->second.second = true;
              }
          };

    // Go through the new UsdGeomSubsets. On the 3dsMax side, we only need one subset per bound
    // material on the usd mesh. For example, if all UsdGeomSubsets share the same material, we can
    // only have one subset with the entire mesh. However, say all but one UsdGeomSubsets share the
    // same material, then we would need two subsets in Max.
    for (const auto& geomSubset : geomSubsets) {
        // Right now geom subsets only support face sets, but edge or vertex sets
        // are possible in the future.
        TF_VERIFY(geomSubset.type == HdGeomSubset::TypeFaceSet);
        if (geomSubset.type != HdGeomSubset::TypeFaceSet) {
            continue;
        }

        // There can be geom subsets on the object which are not material subsets. I've seen
        // familyName = "object" in usda files. If there is no materialId on the subset then
        // don't create a render item for it.
        if (SdfPath::EmptyPath() == geomSubset.materialId) {
            continue;
        }

        // The geomsubset materialId doesn't contain the delegate prefix.
        CreateOrUpdateMaterialSubset(geomSubset.materialId);

        // Update faceIdToMaterialSubset entries for this material.
        for (const auto& faceId : geomSubset.indices) {
            if (faceId >= faceIdToMaterialSubset.size()) {
                TF_VERIFY(faceId < faceIdToMaterialSubset.size());
                continue;
            }

            // We do not expect overlapping subsets, so at this point, the face should be assumed
            // bound to whatever material is bound at the mesh level.
            TF_VERIFY(materialId == faceIdToMaterialSubset[faceId]);
            faceIdToMaterialSubset[faceId] = geomSubset.materialId;
        }
        numFacesWithoutRenderItem -= geomSubset.indices.size();
    }

    TF_VERIFY(numFacesWithoutRenderItem >= 0);

    // If there are remaining faces that are not covered, create/update a subset for them (will use
    // the mesh material binding).
    if (numFacesWithoutRenderItem > 0) {
        CreateOrUpdateMaterialSubset(materialId);

        if (numFacesWithoutRenderItem == sourceTopology.GetNumFaces()) {
            // If there are no geom subsets that are material bind geom subsets, then we don't need
            // the _faceIdToGeomSubsetId mapping, we'll just create one item and use the full
            // topology for it.
            faceIdToMaterialSubset.clear();
            numFacesWithoutRenderItem = 0;
        }
    }

    // Finally, update the passed vector, to return the updated per-material render data to the
    // caller.
    renderData.shadedSubsets.clear();
    for (const auto& data : updatedSubsets) {
        // The subset render data is needed.
        if (data.second.second) {
            renderData.shadedSubsets.push_back(data.second.first);
        }
        // Not needed anymore, delete!
        // Delay the destruction of any render items so they are ref counted to 0 while on the main
        // thread. Indeed, it seems destroying render items is unsafe if not done from the main
        // thread. So just keep a reference to the render data for now.
        else {
            renderData.toDelete.push_back(data.second.first);
            this->renderDelegate
                ->RequestGC(); // request garbage collection to happen on the main thread.
        }
    }

    if (instanced) {
        renderData.instancer->SetSubsetCount(renderData.shadedSubsets.size());
    }
}

std::pair<TfToken, std::vector<TfToken>>
HdMaxMesh::_GetMaterialUvPrimvars(HdSceneDelegate* delegate, const SdfPath& materialId)
{
    if (materialId.IsEmpty()) {
        return {};
    }

    const auto it = materialToUvPrimvars.find(materialId);
    if (it != materialToUvPrimvars.end()) {
        return it->second;
    }
    const VtValue        vtMatResource = delegate->GetMaterialResource(materialId);
    std::vector<TfToken> uvPrimvars;

    TfToken diffuseColorUv;

    if (vtMatResource.IsHolding<HdMaterialNetworkMap>()) {
        const HdMaterialNetworkMap& networkMap = vtMatResource.UncheckedGet<HdMaterialNetworkMap>();
        HdMaterialNetwork           materialNetwork;
        TfMapLookup(networkMap.map, HdMaterialTerminalTokens->surface, &materialNetwork);

        const auto& maps = MaxUsd::MaterialUtils::USDPREVIEWSURFACE_MAPS;

        std::map<pxr::SdfPath, pxr::TfTokenVector> sdfPathToOutputsMap;

        // Build a map of sdfPaths to outputs for texture maps.
        // For example you might get an entry like : sdfPath -> [diffuseColor, opacity]
        for (const auto& rel : materialNetwork.relationships) {
            auto       outputName = rel.outputName.GetString();
            const auto it = std::find(maps.begin(), maps.end(), outputName);
            if (it == maps.end()) {
                continue;
            }
            sdfPathToOutputsMap[rel.inputId].push_back(rel.outputName);
        }

        pxr::TfHashMap<pxr::SdfPath, pxr::HdMaterialNode, pxr::SdfPath::Hash> pathToNode;
        for (const auto& node : materialNetwork.nodes) {
            pathToNode.insert({ node.path, node });
        }

        for (const auto& node : materialNetwork.nodes) {
            if (node.identifier != pxr::TfToken("UsdUVTexture")) {
                continue;
            }

            const auto textureMaps = sdfPathToOutputsMap.find(node.path);
            if (textureMaps == sdfPathToOutputsMap.end()) {
                // Not connected to anything?
                continue;
            }

            auto       outputs = textureMaps->second;
            const bool isDiffuseColorMap
                = std::find(outputs.begin(), outputs.end(), pxr::TfToken("diffuseColor"))
                != outputs.end();

            const auto primvar
                = MaxUsd::MaterialUtils::GetUsdUVTexturePrimvar(node, materialNetwork, pathToNode);
            if (!primvar.IsEmpty()) {
                if (isDiffuseColorMap) {
                    diffuseColorUv = primvar;
                }
                uvPrimvars.push_back(primvar);
            }
        }
    }
    materialToUvPrimvars.insert({ materialId, { diffuseColorUv, uvPrimvars } });
    return { diffuseColorUv, uvPrimvars };
}

HdMaxRenderData& HdMaxMesh::_GetRenderData() { return renderDelegate->GetRenderData(GetId()); }

bool HdMaxMesh::PrimvarIsRequired(const TfToken& primvar) const
{
    const TfTokenVector::const_iterator begin = requiredPrimvars.cbegin();
    const TfTokenVector::const_iterator end = requiredPrimvars.cend();
    return (std::find(begin, end, primvar) != end);
}

void HdMaxMesh::Sync(
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
    // There are other things we need to do if the topology is dirty, but those are handled later,
    // once we know what vertex layout we will need (shared or not - we figure this out from the
    // primvar interpolations schemes).
    bool topologyDirty = HdChangeTracker::IsTopologyDirty(*dirtyBits, id);

    if (topologyDirty) {
        auto getHdMaterialFromSubset = [](HdSceneDelegate*    sceneDelegate,
                                          const pxr::SdfPath& materialId) {
            return static_cast<HdMaxMaterial*>(
                sceneDelegate->GetRenderIndex().GetSprim(HdPrimTypeTokens->material, materialId));
        };

        // The topology has changed, unsubscribe from updates to the materials of the old topo...
        for (const auto& geomSubset : sourceTopology.GetGeomSubsets()) {
            if (!geomSubset.materialId.IsEmpty()) {
                auto material = getHdMaterialFromSubset(delegate, geomSubset.materialId);
                if (material) {
                    material->UnsubscribeFromMaterialUpdates(id);
                }
            }
        }

        sourceTopology = delegate->GetMeshTopology(id);

        renderData.sourceTopology = sourceTopology;
        renderData.sourceNumPoints = sourceTopology.GetNumPoints();
        renderData.sourceNumFaces = sourceTopology.GetNumFaces();

        // Subscribe to the new materials' updates.
        for (const auto& geomSubset : sourceTopology.GetGeomSubsets()) {
            if (!geomSubset.materialId.IsEmpty()) {
                auto material = getHdMaterialFromSubset(delegate, geomSubset.materialId);
                if (material) {
                    material->SubscribeForMaterialUpdates(id);
                }
            }
        }
    }

    // If the material assignment has changed, or at least some primvars are dirty. Need to
    // update/figure out what primvars we need to use for UVs.
    auto materialIdDirty = bool(*dirtyBits & HdChangeTracker::DirtyMaterialId);
    auto dirtyPrimvars = bool(*dirtyBits & HdChangeTracker::DirtyPrimvar);
    if (materialIdDirty || dirtyPrimvars) {
        allUvPrimvars.clear();
        auto addPrimvar = [this](const pxr::TfToken& pv) {
            if (std::find(allUvPrimvars.begin(), allUvPrimvars.end(), pv) == allUvPrimvars.end()) {
                allUvPrimvars.push_back(pv);
            }
        };

        // First find all the primvars actually used by the assigned materials. We definitely want
        // those.

        // Lambda, finds and adds the primvars used by a material.
        auto addMaterialUvs
            = [this, &renderData, &delegate, &addPrimvar](const SdfPath& materialId) {
                  TfToken              diffuseColorPrimvar;
                  std::vector<TfToken> materialPrimvars;

                  std::tie(diffuseColorPrimvar, materialPrimvars)
                      = _GetMaterialUvPrimvars(delegate, materialId);

                  if (!diffuseColorPrimvar.IsEmpty()) {
                      renderData.materialDiffuseColorUvPrimvars[materialId] = diffuseColorPrimvar;
                  }
                  for (const auto pv : materialPrimvars) {
                      addPrimvar(pv);
                  }
              };

        // Look at all the subset materials.
        for (const auto& subset : sourceTopology.GetGeomSubsets()) {
            addMaterialUvs(subset.materialId);
        }
        // Look at the prim material.
        const auto matId = delegate->GetMaterialId(id);
        addMaterialUvs(matId);

        // Also get the primvars which are explicitly mapped to a map channel, if requested.
        pxr::VtValue loadAllVal = renderDelegate->GetRenderSetting(
            pxr::TfToken("loadAllMappedPrimvars"), pxr::VtValue(false));
        if (loadAllVal.IsHolding<bool>()) {
            if (loadAllVal.Get<bool>()) {
                const auto& primvarOptions = renderDelegate->GetPrimvarMappingOptions();
                for (size_t i = 0; i < HdInterpolationCount; i++) {
                    const HdInterpolation           interp = static_cast<HdInterpolation>(i);
                    const HdPrimvarDescriptorVector primvars
                        = GetPrimvarDescriptors(delegate, interp);
                    for (const auto& pv : primvars) {
                        if (MaxUsd::PrimvarMappingOptions::invalidChannel
                            != primvarOptions.GetPrimvarChannelMapping(pv.name.GetString())) {
                            addPrimvar(pv.name);
                        }
                    }
                }
            }
        }
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

    bool normalsDirty = false;
    checkPrimvar(HdTokens->normals, normalsDirty, true);
    bool uvsDirty = false;

    for (const auto uvPrimvar : allUvPrimvars) {
        bool isDirty = false;
        checkPrimvar(
            uvPrimvar,
            isDirty,
            displaySettings.GetDisplayMode() == HdMaxDisplaySettings::USDPreviewSurface);
        uvsDirty |= isDirty;
    }

    bool displayColorDirty = false;
    checkPrimvar(
        HdTokens->displayColor,
        displayColorDirty,
        displaySettings.GetDisplayMode() == HdMaxDisplaySettings::USDPreviewSurface);
    auto pointsDirty = HdChangeTracker::IsPrimvarDirty(*dirtyBits, id, HdTokens->points);
    checkPrimvar(HdTokens->points, pointsDirty, true);

    if (needPrimvarSync) {
        // Update primvars, for now we only use the normals and one UV primvar.
        _UpdatePrimvarSources(delegate, *dirtyBits, requiredPrimvars);
    }

    auto newSharedLayoutPossible = _IsSharedVertexLayoutPossible(primvarInfoMap);
    if (newSharedLayoutPossible != sharedVertexLayout) {
        // The interpolation of a Primvar has changed, and the possibility of sharing vertices has
        // changed. We need to reload all the buffers, so we will consider everything as dirty for
        // this sync().
        pointsDirty = true;
        topologyDirty = true;
        normalsDirty = true;
        uvsDirty = true;
        displayColorDirty = true;
    }
    sharedVertexLayout = newSharedLayoutPossible;

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

    // If the topology has changed, we need to recompute the triangulation.
    if (topologyDirty) {
        const SdfPath materialId = delegate->GetMaterialId(id);
        bool          instanced = instancer != nullptr;

        // Shaded - need one render item for each material bound to the mesh.
        _UpdatePerMaterialRenderData(delegate, materialId, renderData, instanced);

        // Wireframe - one render item for the whole mesh (essentially treated as one subset with
        // everything). Only need to initialize the wireframe render data once.
        if (!renderData.wireframe.geometry) {
            renderData.wireframe = _InitializeSubsetRenderData({}, instanced, true);
        }

        // Flag transforms dirty as they will need to be re-applied on any newly created render
        // items.
        isTransformDirty = true;

        // Shared vertex layout, we can use the topology as-is.
        if (sharedVertexLayout) {
            renderingTopology = sourceTopology;
        } else {
            // Not sharing points, will re-index to a natural sequence [0-N].
            VtIntArray newFaceVertexIndices(sourceTopology.GetFaceVertexIndices().size());
            std::iota(newFaceVertexIndices.begin(), newFaceVertexIndices.end(), 0);

            renderingTopology = HdMeshTopology(
                sourceTopology.GetScheme(),
                sourceTopology.GetOrientation(),
                sourceTopology.GetFaceVertexCounts(),
                newFaceVertexIndices,
                sourceTopology.GetHoleIndices(),
                sourceTopology.GetRefineLevel());
        }

        HdMeshUtil meshUtil(&renderingTopology, GetId());
        triangulatedIndices.clear();
        VtIntArray trianglePrimitiveParams;
        meshUtil.ComputeTriangleIndices(&triangulatedIndices, &trianglePrimitiveParams, nullptr);

        pxr::TfHashMap<pxr::SdfPath, int, pxr::SdfPath::Hash> materialToSubsetIndex;
        // Shaded (one subset/render item per material)
        for (int i = 0; i < renderData.shadedSubsets.size(); ++i) {
            auto&        subsetItem = renderData.shadedSubsets[i];
            VtVec3iArray trianglesFaceVertexIndices; // for this item only!
            if (faceIdToMaterialSubset.size() == 0) {
                // If there is no mapping from face to render item then all the faces are on this
                // render item. VtArray has copy-on-write semantics so this is fast.
                trianglesFaceVertexIndices = triangulatedIndices;
            } else {
                for (size_t triangleId = 0; triangleId < triangulatedIndices.size(); ++triangleId) {
                    size_t faceId = HdMeshUtil::DecodeFaceIndexFromCoarseFaceParam(
                        trianglePrimitiveParams[triangleId]);

                    if (faceIdToMaterialSubset[faceId] == subsetItem.materiaId) {
                        trianglesFaceVertexIndices.push_back(triangulatedIndices[triangleId]);
                    }
                }
            }

            if (subsetItem.indices.size() != trianglesFaceVertexIndices.size()) {
                HdMaxChangeTracker::SetDirty(
                    subsetItem.dirtyBits, HdMaxChangeTracker::DirtyIndicesSize);
                HdMaxChangeTracker::SetDirty(
                    subsetItem.dirtyBits, HdMaxChangeTracker::DirtyIndices);
            } else if (!std::equal(
                           subsetItem.indices.cbegin(),
                           subsetItem.indices.cend(),
                           trianglesFaceVertexIndices.cbegin(),
                           trianglesFaceVertexIndices.cend())) {
                HdMaxChangeTracker::SetDirty(
                    subsetItem.dirtyBits, HdMaxChangeTracker::DirtyIndices);
            }

            subsetItem.indices = trianglesFaceVertexIndices;
            materialToSubsetIndex.insert({ subsetItem.materiaId, i });
        }

        // Wireframe - only need a single render item.
        {
            // In wireframe mode, we want to show the actual topology, not a triangulation.
            const VtIntArray& faceVertexIndices = renderingTopology.GetFaceVertexIndices();
            const VtIntArray& faceVertexCounts = renderingTopology.GetFaceVertexCounts();

            // Keep track of the current size of the index buffers. If they change, we must dirty
            // the render data appropriately.
            std::vector<size_t> previousWireIndicesSize;
            previousWireIndicesSize.reserve(renderData.shadedSubsets.size());
            for (auto& subset : renderData.shadedSubsets) {
                previousWireIndicesSize.push_back(subset.wireIndices.size());
                subset.wireIndices.clear();
            }

            // Build the segment indices from each face.
            int currIdx = 0;
            for (int i = 0; i < renderingTopology.GetNumFaces(); ++i) {
                auto& wireframeIndices
                    = renderData.shadedSubsets[materialToSubsetIndex[faceIdToMaterialSubset[i]]]
                          .wireIndices;
                auto faceVertexCount = faceVertexCounts[i];
                for (int j = 0; j < faceVertexCount; ++j) {
                    auto index = currIdx + j;
                    wireframeIndices.push_back(faceVertexIndices[index]);
                    wireframeIndices.push_back(
                        faceVertexIndices[((j + 1) % faceVertexCount) + currIdx]);
                }
                currIdx += faceVertexCount;
            }

            for (int i = 0; i < renderData.shadedSubsets.size(); ++i) {
                if (previousWireIndicesSize[i] != renderData.shadedSubsets[i].wireIndices.size()) {
                    HdMaxChangeTracker::SetDirty(
                        renderData.shadedSubsets[i].dirtyBits,
                        HdMaxChangeTracker::DirtyIndicesSize);
                    break;
                }
            }
        }

        // Typically we just flag all bits as not dirty at the end of the Sync() call.
        // However, in case no render items are created (no topology defined) at this timeCode,
        // we need to flag the topology as not dirty now, in case we return from the function
        // just below.
        *dirtyBits &= ~HdChangeTracker::DirtyTopology;
    }

    // If the topology did not produce any render items, we are done.
    if (renderData.shadedSubsets.empty()) {
        return;
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

    // If normals are flagged as dirty, update them.
    // Normals are computed unless already specified.
    if (normalsDirty || topologyDirty) {
        const auto previousNormalsSize = renderData.normals.size();
        _LoadNormals(id, delegate);
        const auto newNormalsSize = renderData.normals.size();

        renderData.SetAllSubsetRenderDataDirty(HdMaxChangeTracker::DirtyNormals);
        // Same size, can just update the buffer.
        if (previousNormalsSize != newNormalsSize) {
            renderData.SetAllSubsetRenderDataDirty(HdMaxChangeTracker::DirtyNormalsSize);
        }
    }

    if (uvsDirty) {
        const auto previousUvsSize = renderData.uvs.size();
        if (displaySettings.GetDisplayMode() == HdMaxDisplaySettings::USDPreviewSurface) {
            _LoadUvs(id, delegate, allUvPrimvars);
        } else {
            renderData.uvs.clear();
        }
        const auto newUvsSize = renderData.uvs.size();

        auto renderGeometry = renderData.shadedSubsets[0].geometry;

        // Same size, can just update the buffer.
        renderData.SetAllSubsetRenderDataDirty(HdMaxChangeTracker::DirtyUvs);
        if (previousUvsSize != newUvsSize) {
            renderData.SetAllSubsetRenderDataDirty(HdMaxChangeTracker::DirtyUvsSize);
        }
    }

    // Visibility - simply flag the render item as visible or not. Later on, this will control
    // whether the render item is considered for actual rendering.
    if (HdChangeTracker::IsVisibilityDirty(*dirtyBits, id)) {
        renderData.visible = delegate->GetVisible(id);
        *dirtyBits &= ~HdChangeTracker::DirtyVisibility;
        renderData.SetAllSubsetRenderDataDirty(HdMaxChangeTracker::DirtyVisibility);
    }

    // Handle changes to the display color. Create a 3dsmax material handle accordingly.
    if (displayColorDirty) {
        const auto previousDisplayColorSize = renderData.colors.size();
        if (displaySettings.GetDisplayMode() == HdMaxDisplaySettings::USDPreviewSurface) {
            _LoadDisplayColor(id, delegate);
        } else {
            renderData.colors.clear();
        }
        const auto newDisplayColorSize = renderData.colors.size();

        // Same size, can just update the buffer.
        renderData.SetAllSubsetRenderDataDirty(HdMaxChangeTracker::DirtyVertexColors);
        if (previousDisplayColorSize != newDisplayColorSize) {
            renderData.SetAllSubsetRenderDataDirty(HdMaxChangeTracker::DirtyVertexColorsSize);
        }

        // If we are using the display color as nitrous material, flag it as dirty.
        if (displaySettings.GetDisplayMode() == HdMaxDisplaySettings::USDDisplayColor) {
            renderData.SetAllSubsetRenderDataDirty(HdMaxChangeTracker::DirtyMaterial);
        }

        std::lock_guard<std::recursive_mutex> maxLock(maxSdkMutex);

        auto color = GfVec3f(0.8f, 0.8f, 0.8f);
        if (!renderData.colors.empty()) {
            color = renderData.colors[0];
        } else {
            auto displayColorAttr = delegate->Get(id, HdTokens->displayColor);
            if (displayColorAttr.IsArrayValued()) {
                const auto& colorArray = displayColorAttr.Get<VtVec3fArray>();
                color = colorArray[0];
            }
        }

        auto setDisplayColorMaterialHandle = [&color, &renderData, this](bool instanced) {
            auto& displayColorHandle = instanced ? renderData.instanceDisplayColorNitrousHandle
                                                 : renderData.displayColorNitrousHandle;
            displayColorHandle = HdMaxColorMaterial::Get(color, instanced);
        };

        // Always generate the non-instanced version, even when instanced, we might need it for
        // consolidation.
        setDisplayColorMaterialHandle(false);
        if (instancer) {
            // If the geometry is instanced, generate the instanced version of the displayColor
            // material.
            setDisplayColorMaterialHandle(true);
        }
    }

    // Update the rPrim's material id, and the material update subscriptions
    // accordingly.
    if (materialIdDirty) {
        const SdfPath  materialId = delegate->GetMaterialId(id);
        const SdfPath& origMaterialId = GetMaterialId();
        if (materialId != origMaterialId) {
            if (!origMaterialId.IsEmpty()) {
                pxr::HdMaxMaterial* material
                    = static_cast<pxr::HdMaxMaterial*>(delegate->GetRenderIndex().GetSprim(
                        HdPrimTypeTokens->material, origMaterialId));
                if (material) {
                    material->UnsubscribeFromMaterialUpdates(id);
                }
            }

            if (!materialId.IsEmpty()) {
                pxr::HdMaxMaterial* material = static_cast<pxr::HdMaxMaterial*>(
                    delegate->GetRenderIndex().GetSprim(HdPrimTypeTokens->material, materialId));
                if (material) {
                    material->SubscribeForMaterialUpdates(id);
                }
            }
        }
        SetMaterialId(materialId);

        // Using the usd preview surface as nitrous material, flag it as dirty.
        if (displaySettings.GetDisplayMode() == HdMaxDisplaySettings::USDPreviewSurface) {
            renderData.SetAllSubsetRenderDataDirty(HdMaxChangeTracker::DirtyMaterial);
        }
    }

    // Handles material and color changes on non-instanced geometry.
    if ((materialIdDirty || displayColorDirty) && !instancer) {
        std::lock_guard<std::recursive_mutex> maxLock(maxSdkMutex);
        for (auto& subsetItem : renderData.shadedSubsets) {
            // Setup the 3dsMax Material and its the viewport representation.
            // The Max material is what ends up being used for rendering, so it is always the
            // "best" we have. The viewport representation however, depends on viewport
            // display settings.

            MaxSDK::Graphics::BaseMaterialHandle usdPreviewSurfaceMaterialHandle;

            // 3dsMax Material : use the displayColor Material (diffuse color from vertex colors)
            // unless an actual material is defined.
            if (!subsetItem.materiaId.IsEmpty()) {
                auto materialCollection = renderDelegate->GetMaterialCollection();
                // Not built at this point.
                subsetItem.materialData
                    = materialCollection->AddMaterial(delegate, subsetItem.materiaId);
            }
        }
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
        // The final transform is the product of the mesh's transform and the instance's transform.
        auto meshTransform = delegate->GetTransform(id);
        for (auto& transform : transforms) {
            transform = meshTransform * transform;
        }

        auto newInstanceCount = transforms.size();

        auto extent = delegate->GetExtent(id);
        renderData.extent = extent;
        // Compute the total bounding box given all instances.
        renderData.boundingBox = MaxUsd::ComputeTotalExtent(extent, transforms);

        std::lock_guard<std::recursive_mutex> maxLock(maxSdkMutex);

        auto materialCollection = renderDelegate->GetMaterialCollection();
        for (auto& subsetItem : renderData.shadedSubsets) {
            if (subsetItem.materiaId.IsEmpty()) {
                continue;
            }
            subsetItem.materialData
                = materialCollection->AddMaterial(delegate, subsetItem.materiaId);
        }

        bool needFullRebuild = instancerIndicesDirty || topologyDirty || materialIdDirty;
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

HdDirtyBits HdMaxMesh::_PropagateDirtyBits(HdDirtyBits bits) const
{
    // When instance indices change, we also need to update our selection, as we directly rely
    // on the indices.
    if (bits & HdChangeTracker::DirtyInstanceIndex) {
        bits |= DirtySelectionHighlight;
    }

    return bits;
}

void HdMaxMesh::_InitRepr(TfToken const& reprToken, HdDirtyBits* dirtyBits)
{
    TF_UNUSED(dirtyBits);

    // Create an empty repr.
    _ReprVector::iterator it
        = std::find_if(_reprs.begin(), _reprs.end(), _ReprComparator(reprToken));
    if (it == _reprs.end()) {
        _reprs.emplace_back(reprToken, HdReprSharedPtr());
    }
}

void HdMaxMesh::Finalize(HdRenderParam* renderParam) { }

PXR_NAMESPACE_CLOSE_SCOPE

// Disable obscure warning : no definition for inline function : pxr::DefaultValueHolder
// Was not able to identify what is triggering this.
#pragma warning(disable : 4506)
