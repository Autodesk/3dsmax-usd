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

#include <MaxUsd/Translators/primWriter.h>

PXR_NAMESPACE_OPEN_SCOPE

class MaxUsdMeshWriter : public MaxUsdPrimWriter
{
public:
    static ContextSupport CanExport(INode* node, const MaxUsd::USDSceneBuilderOptions& exportArgs);

    MaxUsdMeshWriter(const MaxUsdWriteJobContext& jobCtx, INode* node);

    bool
    Write(UsdPrim& targetPrim, bool applyOffsetTransform, const MaxUsd::ExportTime& time) override;

    MaxUsd::XformSplitRequirement RequiresXformPrim() override;

    bool HandlesObjectOffsetTransform() override { return true; };

    TfToken GetObjectPrimSuffix() override { return TfToken("Shape"); };

    TfToken GetPrimType() override { return MaxUsdPrimTypeTokens->Mesh; };

    WStr GetWriterName() override { return L"Mesh writer"; };
};

PXR_NAMESPACE_CLOSE_SCOPE
