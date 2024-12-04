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
#include "CameraWriter.h"
#include "HelperWriter.h"
#include "MeshWriter.h"
#include "PhotometricLightWriter.h"
#include "ShapeWriter.h"
#include "SkeletonWriter.h"
#include "SkinMorpherWriter.h"
#include "StageWriter.h"
#include "SunPositionerWriter.h"

#include <MaxUsd/Translators/PrimWriterRegistry.h>

PXR_NAMESPACE_USING_DIRECTIVE

#define MAXUSD_REGISTER_BASEWRITER(writerClass)                 \
    MaxUsdPrimWriterRegistry::RegisterBaseWriter(               \
        [](const MaxUsdWriteJobContext& jobCtx, INode* node) {  \
            return std::make_shared<writerClass>(jobCtx, node); \
        },                                                      \
        &writerClass::CanExport);

// register all the base writers all at once and keep them ordered
TF_REGISTRY_FUNCTION(MaxUsdPrimWriterRegistry)
{
    MAXUSD_REGISTER_BASEWRITER(MaxUsdStageWriter)
    MAXUSD_REGISTER_BASEWRITER(MaxUsdSkeletonWriter)
    MAXUSD_REGISTER_BASEWRITER(MaxUsdSkinMorpherWriter)
    MAXUSD_REGISTER_BASEWRITER(MaxUsdShapeWriter)
    MAXUSD_REGISTER_BASEWRITER(MaxUsdMeshWriter)
    MAXUSD_REGISTER_BASEWRITER(MaxUsdCameraWriter)
    MAXUSD_REGISTER_BASEWRITER(MaxUsdPhotometricLightWriter)
    MAXUSD_REGISTER_BASEWRITER(MaxUsdSunPositionerWriter)
    MAXUSD_REGISTER_BASEWRITER(MaxUsdHelperWriter)
}