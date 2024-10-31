//
// Copyright 2016 Pixar
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
// © 2023 Autodesk, Inc. All rights reserved.
//

#include "HdMaxTaskController.h"

#include "HdMaxRenderTask.h"

#include <pxr/imaging/hd/renderDelegate.h>
#include <pxr/imaging/hd/sceneDelegate.h>

PXR_NAMESPACE_OPEN_SCOPE

TF_DEFINE_PRIVATE_TOKENS(
    _tokens,
    // global camera
    (camera)(renderTags));

VtValue HdMaxTaskController::_Delegate::Get(SdfPath const& id, TfToken const& key)
{
    _ValueCache* vcache = TfMapLookupPtr(_valueCacheMap, id);
    VtValue      ret;
    if (vcache && TfMapLookup(*vcache, key, &ret)) {
        return ret;
    }
    TF_CODING_ERROR("%s:%s doesn't exist in the value cache\n", id.GetText(), key.GetText());
    return VtValue();
}

VtValue HdMaxTaskController::_Delegate::GetCameraParamValue(SdfPath const& id, TfToken const& key)
{
    return Get(id, key);
}

bool HdMaxTaskController::_Delegate::IsEnabled(TfToken const& option) const
{
    return HdSceneDelegate::IsEnabled(option);
}

TfTokenVector HdMaxTaskController::_Delegate::GetTaskRenderTags(SdfPath const& taskId)
{
    if (HasParameter(taskId, _tokens->renderTags)) {
        return GetParameter<TfTokenVector>(taskId, _tokens->renderTags);
    }
    return TfTokenVector();
}

HdMaxTaskController::HdMaxTaskController(HdRenderIndex* renderIndex, SdfPath const& controllerId)
    : _index(renderIndex)
    , _controllerId(controllerId)
    , _delegate(renderIndex, controllerId)
{
    _CreateRenderGraph();
}

HdMaxTaskController::~HdMaxTaskController()
{
    for (auto const& id : _renderTaskIds) {
        GetRenderIndex()->RemoveTask(id);
    }
}

void HdMaxTaskController::_CreateRenderGraph()
{
    _renderTaskIds.push_back(_CreateRenderTask(TfToken()));
}

SdfPath HdMaxTaskController::_CreateRenderTask(const TfToken& materialTag)
{
    auto taskId = _GetRenderTaskPath(materialTag);
    GetRenderIndex()->InsertTask<pxr::HdMaxRenderTask>(&_delegate, taskId);

    HdRprimCollection collection(
        HdTokens->geometry,
        HdReprSelector(HdReprTokens->smoothHull),
        /*forcedRepr*/ false,
        materialTag);
    collection.SetRootPath(SdfPath::AbsoluteRootPath());

    // Create an initial set of render tags in case the user doesn't set any
    TfTokenVector renderTags = { HdTokens->geometry };

    _delegate.SetParameter(taskId, HdTokens->collection, collection);
    _delegate.SetParameter(taskId, HdTokens->renderTags, renderTags);

    return taskId;
}

SdfPath HdMaxTaskController::_GetRenderTaskPath(TfToken const& materialTag) const
{
    std::string str = TfStringPrintf("renderTask_%s", materialTag.GetText());
    std::replace(str.begin(), str.end(), ':', '_');
    return GetControllerId().AppendChild(TfToken(str));
}

HdTaskSharedPtrVector const HdMaxTaskController::GetRenderingTasks() const
{
    HdTaskSharedPtrVector tasks;

    // The set of tasks we can run, in order, is:
    // See _CreateRenderGraph for more details.
    for (auto const& id : _renderTaskIds) {
        tasks.push_back(GetRenderIndex()->GetTask(id));
    }

    return tasks;
}

void HdMaxTaskController::SetCollection(HdRprimCollection const& collection)
{
    // XXX For now we assume the application calling to set a new
    //     collection does not know or setup the material tags and does not
    //     split up the collection according to material tags.
    //     In order to ignore materialTags when comparing collections we need
    //     to copy the old tag into the new collection. Since the provided
    //     collection is const, we need to make a not-ideal copy.
    HdRprimCollection newCollection = collection;

    for (SdfPath const& renderTaskId : _renderTaskIds) {
        HdRprimCollection oldCollection
            = _delegate.GetParameter<HdRprimCollection>(renderTaskId, HdTokens->collection);

        TfToken const& oldMaterialTag = oldCollection.GetMaterialTag();
        newCollection.SetMaterialTag(oldMaterialTag);

        if (oldCollection == newCollection) {
            continue;
        }

        _delegate.SetParameter(renderTaskId, HdTokens->collection, newCollection);
        GetRenderIndex()->GetChangeTracker().MarkTaskDirty(
            renderTaskId, HdChangeTracker::DirtyCollection);
    }
}

void HdMaxTaskController::SetRenderTags(TfTokenVector const& renderTags)
{
    _renderTags = renderTags;

    HdChangeTracker& tracker = GetRenderIndex()->GetChangeTracker();

    for (SdfPath const& renderTaskId : _renderTaskIds) {
        if (_delegate.GetTaskRenderTags(renderTaskId) != renderTags) {
            _delegate.SetParameter(renderTaskId, _tokens->renderTags, renderTags);
            tracker.MarkTaskDirty(renderTaskId, HdChangeTracker::DirtyRenderTags);
        }
    }
}

PXR_NAMESPACE_CLOSE_SCOPE
