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
#include "HelperWriter.h"

#include <MaxUsd/Translators/primWriter.h>
#include <MaxUsd/Translators/writeJobContext.h>
#include <MaxUsd/Utilities/MaxSupportUtils.h>

#include <pxr/base/tf/token.h>
#include <pxr/pxr.h>
#include <pxr/usd/usdGeom/camera.h>

PXR_NAMESPACE_OPEN_SCOPE

MaxUsdHelperWriter::MaxUsdHelperWriter(const MaxUsdWriteJobContext& jobCtx, INode* node)
    : MaxUsdPrimWriter(jobCtx, node)
{
}

MaxUsdPrimWriter::ContextSupport
MaxUsdHelperWriter::CanExport(INode* node, const MaxUsd::USDSceneBuilderOptions& exportArgs)
{
    const auto object = node->EvalWorldState(exportArgs.GetResolvedTimeConfig().GetStartTime()).obj;
    if (object->ClassID() == Class_ID { POINTHELP_CLASS_ID, 0 }
        || object->CanConvertToType({ DUMMY_CLASS_ID, 0 })) {
        return ContextSupport::Fallback;
    }
    return ContextSupport::Unsupported;
}

TfToken MaxUsdHelperWriter::GetObjectPrimSuffix()
{
    const auto object = GetNode()->GetObjectRef();
    if (object->ClassID() == Class_ID { POINTHELP_CLASS_ID, 0 }) {
        return TfToken("Point");
    }
    return TfToken("Dummy");
}

bool MaxUsdHelperWriter::Write(
    UsdPrim&                  targetPrim,
    bool                      applyOffsetTransform,
    const MaxUsd::ExportTime& time)
{
    // No properties to write. We just want an Xform.
    return true;
}

Interval MaxUsdHelperWriter::GetValidityInterval(const TimeValue&)
{
    // We do not export any properties, and so nothing is animated, what we export is valid
    // "forever".
    return FOREVER;
}

PXR_NAMESPACE_CLOSE_SCOPE