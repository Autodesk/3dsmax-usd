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
#pragma once
#include <pxr/imaging/hd/extComputation.h>

PXR_NAMESPACE_OPEN_SCOPE

// Simple derived class from HdExtComputation - we only need this to work around
// a concurrency issue in USD.
// See https://github.com/PixarAnimationStudios/USD/issues/1742
// The idea is to force pre-computation of the skinning transforms instead of
// having them lazily computed from HdMaxMesh::Sync(). Indeed, Sprims are sync'ed
// serially - so the call is safe here.
class HdMaxExtComputation : public pxr::HdExtComputation
{
public:
    HdMaxExtComputation(pxr::SdfPath const& id);

    ~HdMaxExtComputation() override = default;

    void Sync(
        pxr::HdSceneDelegate* sceneDelegate,
        pxr::HdRenderParam*   renderParam,
        pxr::HdDirtyBits*     dirtyBits) override;

private:
    bool _computeJointWorldInverseBindTransforms = true;
};

PXR_NAMESPACE_CLOSE_SCOPE