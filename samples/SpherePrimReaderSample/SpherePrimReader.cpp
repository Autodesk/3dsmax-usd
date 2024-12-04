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
#include <MaxUsd/Translators/PrimReader.h>
#include <MaxUsd/Translators/PrimReaderRegistry.h>
#include <MaxUsd/Translators/ReadJobContext.h>
#include <MaxUsd/Translators/TranslatorPrim.h>
#include <MaxUsd/Translators/TranslatorUtils.h>
#include <MaxUsd/Translators/TranslatorXformable.h>
#include <MaxUsd/Utilities/TranslationUtils.h>

#include <pxr/usd/usdGeom/sphere.h>

#include <maxapi.h>

// Macro for the pixar namespace "pxr::"
PXR_NAMESPACE_USING_DIRECTIVE

/// Prim reader for importing a USD native sphere.
class SpherePrimReader : public MaxUsdPrimReader
{
public:
    SpherePrimReader(const UsdPrim& prim, MaxUsdReadJobContext& jobCtx);

    static MaxUsdPrimReader::ContextSupport
         CanImport(const MaxUsd::MaxSceneBuilderOptions&, const UsdPrim&);
    bool Read() override;
};

// This macro registers the Prim Reader, it's adding the SpherePrimReader class as a candidate when
// trying to import a UsdGeomSphere prim. The CanImport() method is responsible for defining what
// can be imported or not. It is also really important to set the project option "Remove
// unreferenced code and data" to NO, this could cause the Macro to be optimized out and the Reader
// to never be properly registered.

TF_REGISTRY_FUNCTION_WITH_TAG(MaxUsdPrimReaderRegistry, UsdGeomSphere)
{
    MaxUsdPrimReaderRegistry::Register<UsdGeomSphere>(
        SpherePrimReader::CanImport, [](const UsdPrim& prim, MaxUsdReadJobContext& context) {
            return std::make_shared<SpherePrimReader>(prim, context);
        });
}

/*
 * This Reader only deals with Sphere objects
 */

SpherePrimReader::SpherePrimReader(const UsdPrim& prim, MaxUsdReadJobContext& jobCtx)
    : MaxUsdPrimReader(prim, jobCtx)
{
}

MaxUsdPrimReader::ContextSupport
SpherePrimReader::CanImport(const MaxUsd::MaxSceneBuilderOptions&, const UsdPrim&)
{
    // the primreader registry applies a first filter based on prim types
    // this reader supports all UsdGeomSphere prims
    return MaxUsdPrimReader::ContextSupport::Supported;
}

/*
 * For this sample, we will demonstrate how to import
 */
bool SpherePrimReader::Read()
{
    const UsdPrim& usdPrim = GetUsdPrim();
    if (!usdPrim) {
        return false;
    }
    const UsdGeomSphere sphereSchema(usdPrim);
    if (!sphereSchema) {
        return false;
    }

    GeomObject* maxSphere = static_cast<GeomObject*>(
        GetCOREInterface()->CreateInstance(GEOMOBJECT_CLASS_ID, Class_ID(SPHERE_CLASS_ID, 0)));
    auto spherePb = maxSphere->GetParamBlock(0);

    // animated attribute
    if (!MaxUsdTranslatorUtil::ReadUsdAttribute(
            sphereSchema.GetRadiusAttr(),
            [&](const VtValue& radius, const UsdTimeCode&, const TimeValue time) {
                spherePb->SetValueByName(
                    _T("radius"), static_cast<float>(radius.Get<double>()), time);
                return true;
            },
            GetJobContext())) {
        // unable to properly set the radius of the sphere
        TF_WARN(
            "Unable to properly set the radius on '%s'.", usdPrim.GetName().GetString().c_str());
        return false;
    }

    INode* createdNode = MaxUsdTranslatorPrim::CreateAndRegisterNode(
        usdPrim, maxSphere, usdPrim.GetName(), GetJobContext());

    // read Xform attributes and convert those into 3ds Max transform values
    MaxUsdTranslatorXformable::Read(usdPrim, createdNode, GetJobContext());

    return true;
}