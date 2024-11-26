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
#pragma once

#include <pxr/imaging/hd/renderIndex.h>
#include <pxr/imaging/hd/sceneDelegate.h>
#include <pxr/pxr.h>
#include <pxr/usd/sdf/path.h>

PXR_NAMESPACE_OPEN_SCOPE

/**
 * \brief Hydra task controller for Nitrous.
 */
class HdMaxTaskController
{
public:
    HdMaxTaskController(HdRenderIndex* renderIndex, SdfPath const& controllerId);
    ~HdMaxTaskController();

    /// Return the render index this controller is bound to.
    HdRenderIndex* GetRenderIndex() { return _index; }

    /// Return the render index this controller is bound to.
    HdRenderIndex const* GetRenderIndex() const { return _index; }

    /// Return the controller's scene-graph id (prefixed to any
    /// scene graph objects it creates).
    SdfPath const& GetControllerId() const { return _controllerId; }

    /// -------------------------------------------------------
    /// Execution API

    /// Obtain the set of tasks managed by the task controller,
    /// for image generation. The tasks returned will be different
    /// based on current renderer state.
    HdTaskSharedPtrVector const GetRenderingTasks() const;

    /// -------------------------------------------------------
    /// Rendering API

    /// Set the collection to be rendered.
    void SetCollection(HdRprimCollection const& collection);

    /// Set the "view" opinion of the scenes render tags.
    /// The opinion is the base opinion for the entire scene.
    /// Individual tasks (such as the shadow task) may
    /// have a stronger opinion and override this opinion
    void SetRenderTags(TfTokenVector const& renderTags);

    const pxr::TfTokenVector& GetRenderTags() { return _renderTags; }

private:
    ///
    /// This class is not intended to be copied nor moved.
    ///
    HdMaxTaskController(HdMaxTaskController const&) = delete;
    HdMaxTaskController& operator=(HdMaxTaskController const&) = delete;
    HdMaxTaskController(HdMaxTaskController&&) = delete;
    HdMaxTaskController& operator=(HdMaxTaskController&&) = delete;

    HdRenderIndex* _index;
    SdfPath const  _controllerId;
    TfTokenVector  _renderTags;

    // Create taskController objects.
    void _CreateRenderGraph();

    // Helper functions.
    SdfPath _CreateRenderTask(const TfToken& materialTag);
    SdfPath _GetRenderTaskPath(TfToken const& materialTag) const;

    // A private scene delegate member variable backs the tasks this
    // controller generates. To keep _Delegate simple, the containing class
    // is responsible for marking things dirty.
    class _Delegate : public HdSceneDelegate
    {
    public:
        _Delegate(HdRenderIndex* parentIndex, SdfPath const& delegateID)
            : HdSceneDelegate(parentIndex, delegateID)
        {
        }
        virtual ~_Delegate() = default;

        // HdMaxTaskController set/get interface
        template <typename T>
        void SetParameter(SdfPath const& id, TfToken const& key, T const& value)
        {
            _valueCacheMap[id][key] = value;
        }
        template <typename T> T const& GetParameter(SdfPath const& id, TfToken const& key) const
        {
            VtValue     vParams;
            _ValueCache vCache;
            TF_VERIFY(
                TfMapLookup(_valueCacheMap, id, &vCache) && TfMapLookup(vCache, key, &vParams)
                && vParams.IsHolding<T>());
            return vParams.Get<T>();
        }
        bool HasParameter(SdfPath const& id, TfToken const& key) const
        {
            _ValueCache vCache;
            if (TfMapLookup(_valueCacheMap, id, &vCache) && vCache.count(key) > 0) {
                return true;
            }
            return false;
        }

        // HdSceneDelegate interface
        virtual VtValue       Get(SdfPath const& id, TfToken const& key);
        virtual VtValue       GetCameraParamValue(SdfPath const& id, TfToken const& key);
        virtual bool          IsEnabled(TfToken const& option) const;
        virtual TfTokenVector GetTaskRenderTags(SdfPath const& taskId);

    private:
        typedef TfHashMap<TfToken, VtValue, TfToken::HashFunctor> _ValueCache;
        typedef TfHashMap<SdfPath, _ValueCache, SdfPath::Hash>    _ValueCacheMap;
        _ValueCacheMap                                            _valueCacheMap;
    };
    _Delegate _delegate;

    // Generated tasks.
    SdfPathVector _renderTaskIds;
};

PXR_NAMESPACE_CLOSE_SCOPE
