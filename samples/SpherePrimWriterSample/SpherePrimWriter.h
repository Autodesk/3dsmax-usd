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
#include <MaxUsd/Translators/PrimWriter.h>

#include <pxr/pxr.h>

/// Prim writer for exporting Sphere object to USD native sphere.
class SpherePrimWriter : public pxr::MaxUsdPrimWriter
{
public:
    SpherePrimWriter(const pxr::MaxUsdWriteJobContext& jobCtx, INode* node);

    static ContextSupport CanExport(INode* node, const MaxUsd::USDSceneBuilderOptions& exportArgs);
    pxr::TfToken          GetPrimType() override;
    bool                  Write(
                         pxr::UsdPrim&             targetPrim,
                         bool                      applyOffsetTransform,
                         const MaxUsd::ExportTime& timeFrame) override;

    virtual Interval GetValidityInterval(const TimeValue& time);
};