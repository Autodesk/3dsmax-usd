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

PXR_NAMESPACE_OPEN_SCOPE
/**
 * \brief A simple render pass for Hydra to Nitrous bridge.
 */
class HdMaxRenderPass final : public HdRenderPass
{
public:
    HdMaxRenderPass(HdRenderIndex* index, HdRprimCollection const& collection);

protected:
    void _Execute(
        HdRenderPassStateSharedPtr const& renderPassState,
        TfTokenVector const&              renderTags) override;

    void _MarkCollectionDirty() override { }

    /// Keep track of used render tags, to react to changes.
    TfTokenVector prevRenderTags;
    /// Keep track of the versioning for authored render tags so
    /// that we can react to changes.
    unsigned authoredTagsVer = 1;
};

PXR_NAMESPACE_CLOSE_SCOPE