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

#include <pxr/imaging/hd/renderPass.h>
#include <pxr/imaging/hd/task.h>

PXR_NAMESPACE_OPEN_SCOPE

class HdMaxRenderTask : public HdTask
{
public:
    HdMaxRenderTask(HdSceneDelegate* delegate, SdfPath id)
        : HdTask(id)
    {
    }

    // HdTask overrides.
    void Sync(HdSceneDelegate* delegate, HdTaskContext* ctx, HdDirtyBits* dirtyBits) override;
    void Prepare(HdTaskContext* ctx, HdRenderIndex* renderIndex) override;
    void Execute(HdTaskContext* ctx) override;
    const TfTokenVector& GetRenderTags() const override;

private:
    HdRenderPassSharedPtr      pass;
    HdRenderPassStateSharedPtr renderPassState;
    TfTokenVector              renderTags;
};

PXR_NAMESPACE_CLOSE_SCOPE
