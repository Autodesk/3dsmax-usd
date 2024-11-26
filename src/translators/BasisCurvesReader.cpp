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
#include <MaxUsd/CurveConversion/CurveConverter.h>
#include <MaxUsd/Translators/PrimReader.h>
#include <MaxUsd/Translators/PrimReaderRegistry.h>
#include <MaxUsd/Translators/TranslatorMorpher.h>
#include <MaxUsd/Translators/TranslatorPrim.h>
#include <MaxUsd/Translators/TranslatorSkel.h>
#include <MaxUsd/Translators/TranslatorUtils.h>
#include <MaxUsd/Translators/TranslatorXformable.h>
#include <MaxUsd/Utilities/TranslationUtils.h>

#include <pxr/base/tf/diagnostic.h>
#include <pxr/usd/usdGeom/basisCurves.h>

#include <inode.h>
#include <linshape.h>
#include <simpshp.h>
#include <simpspl.h>
#include <splshape.h>

PXR_NAMESPACE_OPEN_SCOPE

class MaxUsdBasisCurvesReader : public MaxUsdPrimReader
{
public:
    MaxUsdBasisCurvesReader(const UsdPrim& prim, MaxUsdReadJobContext& jobCtx)
        : MaxUsdPrimReader(prim, jobCtx)
    {
    }

    ~MaxUsdBasisCurvesReader() = default;

    bool Read() override;
};

TF_REGISTRY_FUNCTION_WITH_TAG(MaxUsdPrimReaderRegistry, UsdGeomBasisCurves)
{
    MaxUsdPrimReaderRegistry::Register<UsdGeomBasisCurves>(
        [](const UsdPrim& prim, MaxUsdReadJobContext& jobCtx) {
            return std::make_shared<MaxUsdBasisCurvesReader>(prim, jobCtx);
        });
}

bool MaxUsdBasisCurvesReader::Read()
{
    const auto               prim = GetUsdPrim();
    const UsdGeomBasisCurves basisCurvesPrim(prim);

    SplineShape* shapeObj
        = (SplineShape*)GetCOREInterface()->CreateInstance(SHAPE_CLASS_ID, splineShapeClassID);

    const auto timeConfig = GetArgs().GetResolvedTimeConfig(prim.GetStage());
    const auto startTimeCode = timeConfig.GetStartTimeCode();

    size_t numberOfSplinesCreated
        = MaxUsd::CurveConverter::ConvertToSplineShape(basisCurvesPrim, *shapeObj, startTimeCode);

    if (numberOfSplinesCreated > 0) {
        auto createdNode = MaxUsdTranslatorPrim::CreateAndRegisterNode(
            prim, shapeObj, prim.GetName(), GetJobContext());

        // position the node
        MaxUsdTranslatorXformable::Read(prim, createdNode, GetJobContext());
    }

    return true;
}

PXR_NAMESPACE_CLOSE_SCOPE