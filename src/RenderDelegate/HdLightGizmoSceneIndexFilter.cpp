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
#include "pxr/pxr.h" // PXR_VERSION
#if PXR_VERSION >= 2311

#include "HdLightGizmoSceneIndexFilter.h"

#include <pxr/imaging/hd/containerDataSourceEditor.h>
#include <pxr/imaging/hd/extentSchema.h>
#include <pxr/imaging/hd/instancedBySchema.h>
#include <pxr/imaging/hd/meshSchema.h>
#include <pxr/imaging/hd/retainedDataSource.h>
#include <pxr/imaging/hd/tokens.h>
#include <pxr/usd/usdGeom/mesh.h>

#include <filesystem>
#include <stack>

PXR_NAMESPACE_OPEN_SCOPE

namespace {
std::string lightNameTemplate = "Light_%p";
} // namespace

HdLightGizmoSceneIndexFilter::HdLightGizmoSceneIndexFilter(
    const HdSceneIndexBaseRefPtr&           inputSceneIndex,
    const std::shared_ptr<GizmoMeshAccess>& meshAccess)
    : ParentClass(inputSceneIndex)
    , meshAccess(meshAccess)
{
    lightName = pxr::TfToken { pxr::TfMakeValidIdentifier(
        TfStringPrintf(lightNameTemplate.c_str(), this)) };

    // Generate the gizmos for all the lights initially present in the scene.
    std::stack<SdfPath> primPathsToTraverse({ SdfPath::AbsoluteRootPath() });
    while (!primPathsToTraverse.empty()) {
        SdfPath currPrimPath = primPathsToTraverse.top();
        primPathsToTraverse.pop();

        GenerateGizmo(currPrimPath, initialAddedPrims);

        for (const auto& childPath : _GetInputSceneIndex()->GetChildPrimPaths(currPrimPath)) {
            primPathsToTraverse.push(childPath);
        }
    }
}

bool HdLightGizmoSceneIndexFilter::GenerateGizmo(
    const SdfPath&                          primPath,
    HdSceneIndexObserver::AddedPrimEntries& newLights)
{
    const auto prim = _GetInputSceneIndex()->GetPrim(primPath);
    return GenerateGizmo(primPath, prim.primType, newLights);
}

bool HdLightGizmoSceneIndexFilter::GenerateGizmo(
    const SdfPath&                          primPath,
    const TfToken&                          type,
    HdSceneIndexObserver::AddedPrimEntries& addedPrims)
{
    if (!IsLight(type)) {
        return false;
    }

    // Build the data source for this light's gizmo.
    auto sourceLight = _GetInputSceneIndex()->GetPrim(primPath);
    auto dataSource = meshAccess->GetGizmoSource(sourceLight, primPath);
    lightGizmos[primPath] = dataSource;

    // Register a new prim - the filter will replace the light with a mesh, and move the light
    // under that mesh (so needing a new prim for the light).
    HdSceneIndexObserver::AddedPrimEntry addedPrim;
    addedPrim.primPath = primPath.AppendChild(lightName);
    addedPrim.primType = type;
    addedPrims.push_back(addedPrim);

    return true;
}

void HdLightGizmoSceneIndexFilter::NotifyInitialAddedPrims()
{
    _SendPrimsAdded(initialAddedPrims);
    initialAddedPrims.clear();
}

bool HdLightGizmoSceneIndexFilter::IsLight(const pxr::TfToken& type)
{
    return type == HdPrimTypeTokens->light || type == HdPrimTypeTokens->cylinderLight
        || type == HdPrimTypeTokens->rectLight || type == HdPrimTypeTokens->distantLight
        || type == HdPrimTypeTokens->sphereLight || type == HdPrimTypeTokens->diskLight
        || type == HdPrimTypeTokens->domeLight || type == HdPrimTypeTokens->meshLight
        || type == HdPrimTypeTokens->pluginLight || type == HdPrimTypeTokens->simpleLight;
}

HdSceneIndexPrim HdLightGizmoSceneIndexFilter::GetPrim(const SdfPath& primPath) const
{
    auto inputPrim = _GetInputSceneIndex()->GetPrim(primPath);

    // Replace lights with gizmo meshes.
    if (IsLight(inputPrim.primType)) {
        auto it = lightGizmos.find(primPath);
        if (it != lightGizmos.end()) {

            inputPrim.dataSource = it->second;
            inputPrim.primType = HdMeshSchemaTokens->mesh;
            return inputPrim;
        }
    }

    // This is the light that we moved under the gizmo, forward the original light's data source.
    if (primPath.GetName() == lightName) {
        auto baseLight = _GetInputSceneIndex()->GetPrim(primPath.GetParentPath());
        inputPrim.primType = baseLight.primType;
        inputPrim.dataSource = baseLight.dataSource;
        return inputPrim;
    }

    return inputPrim;
}

SdfPathVector HdLightGizmoSceneIndexFilter::GetChildPrimPaths(const SdfPath& primPath) const
{
    return _GetInputSceneIndex()->GetChildPrimPaths(primPath);
}

void HdLightGizmoSceneIndexFilter::_PrimsAdded(
    const HdSceneIndexBase&                       sender,
    const HdSceneIndexObserver::AddedPrimEntries& entries)
{
    HdSceneIndexObserver::AddedPrimEntries addedEntries;
    for (auto& entry : entries) {
        // If a light was added, we need to setup the gizmo - and notify a mesh instead at
        // the original light path.
        if (GenerateGizmo(entry.primPath, entry.primType, addedEntries)) {
            addedEntries.push_back({ entry.primPath, pxr::HdPrimTypeTokens->mesh });
            continue;
        }
        addedEntries.push_back(entry);
    }
    _SendPrimsAdded(addedEntries);
}

void HdLightGizmoSceneIndexFilter::_PrimsRemoved(
    const HdSceneIndexBase&                         sender,
    const HdSceneIndexObserver::RemovedPrimEntries& entries)
{
    _SendPrimsRemoved(entries);

    // Additionally need to remove any prims we created underneath...
    HdSceneIndexObserver::RemovedPrimEntries removedEntries;
    for (const auto& entry : entries) {
        if (lightGizmos.find(entry.primPath) != lightGizmos.end()) {
            lightGizmos.erase(entry.primPath);
            removedEntries.push_back(entry.primPath.AppendChild(lightName));
        }
    }
    _SendPrimsRemoved(removedEntries);
}

void HdLightGizmoSceneIndexFilter::_PrimsDirtied(
    const HdSceneIndexBase&                         sender,
    const HdSceneIndexObserver::DirtiedPrimEntries& entries)
{
    _SendPrimsDirtied(entries);

    HdSceneIndexObserver::DirtiedPrimEntries dirtiedEntries;
    for (const auto& entry : entries) {
        if (lightGizmos.find(entry.primPath) != lightGizmos.end()) {

            // Additionally need to dirty the prims we created underneath.
            auto lightChildEntry = entry;
            lightChildEntry.primPath = entry.primPath.AppendChild(lightName);
            dirtiedEntries.emplace_back(lightChildEntry);

            // Update the gizmo data source.
            auto sourceLight = _GetInputSceneIndex()->GetPrim(entry.primPath);
            auto dataSource = meshAccess->GetGizmoSource(sourceLight, entry.primPath);
            lightGizmos[entry.primPath] = dataSource;

            // Dirty the extents of the gizmo, we can rebuild the gizmo based on light parameters.
            auto extentEntry = entry;
            extentEntry.dirtyLocators = { HdExtentSchema::GetDefaultLocator() };
            dirtiedEntries.emplace_back(extentEntry);
        }
    }
    _SendPrimsDirtied(dirtiedEntries);
}

PXR_NAMESPACE_CLOSE_SCOPE

#endif