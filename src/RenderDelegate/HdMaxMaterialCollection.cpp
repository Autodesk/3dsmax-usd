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

#include "HdMaxMaterialCollection.h"

#include "DLLEntry.h"
#include "HdMaxColorMaterial.h"
#include "resource.h"

#include <MaxUsd/MaxTokens.h>
#include <MaxUsd/Utilities/Logging.h>
#include <MaxUsd/Utilities/MaterialUtils.h>
#include <MaxUsd/Utilities/MathUtils.h>
#include <MaxUsd/Utilities/MaxSupportUtils.h>
#include <MaxUsd/Utilities/TranslationUtils.h>

#include <plugapi.h>

#ifdef IS_MAX2025_OR_GREATER
#include <Geom/trig.h>
#else
#include <trig.h>
#endif

#include <pxr/base/tf/diagnostic.h>
#include <pxr/imaging/hd/material.h>
#include <pxr/imaging/hd/mesh.h>
#include <pxr/imaging/hd/meshUtil.h>
#include <pxr/imaging/hd/smoothNormals.h>
#include <pxr/imaging/hd/vertexAdjacency.h>

#include <Graphics/TextureHandleUtility.h>

#include <IMultipleOutputChannel/IMultiOutputConsumer.h>
#include <MaxOSLInterface.h>

PXR_NAMESPACE_USING_DIRECTIVE

struct Transform2DData
{
    float  zRotation;
    Point2 translation;
    Point2 scale;

    Transform2DData()
        : zRotation(0.0f)
        , translation(Point2(0.0f, 0.0f))
        , scale(Point2(1.0f, 1.0f))
    {
    }

    bool operator==(const Transform2DData& config)
    {
        return zRotation == config.zRotation && translation == config.translation
            && scale == config.scale;
    }
};

const Class_ID HdMaxMaterialCollection::MaxUsdPreviewSurfaceMaterialClassID
    = Class_ID(0x6afa4933, 0x4787f1c7);

HdMaxMaterialCollection::MaterialData::MaterialData(
    const SdfPath&           materialId,
    const HdMaterialNetwork& materialNetwork)
    : id(materialId)
    , sourceNetwork(materialNetwork)
    , sourceNetworkToken(MaxUsd::MaterialUtils::ToXML(sourceNetwork))
{
}

HdMaxMaterialCollection::MaterialData::MaterialData(const SdfPath& materialId, Mtl* maxMaterial)
    : id(materialId)
{
    materialRef = std::make_shared<MaxUsd::MaterialRef>(maxMaterial);
};

void HdMaxMaterialCollection::MaterialData::UpdateSource(const HdMaterialNetwork& network)
{
    // Create a token for the network, to compare with what we had before. If the network did not
    // actually change, do nothing. USD will dirty materials for a bunch of reasons, sometimes,
    // there is nothing for us to do.
    const auto token = TfToken { MaxUsd::MaterialUtils::ToXML(network) };
    if (token == sourceNetworkToken) {
        return;
    }

    sourceNetwork = network;
    sourceNetworkToken = token;

    stdVpMaterialBuilt = false;
    maxUsdPreviewSurfaceMaterialBuilt = false;

    stdVpMaterial = {};
    stdVpInstancesMaterial = {};
    color = {};
    // Keep materialRef and stdVpMaterialRef, as the held Mtl* can be reused with updated
    // parameters.
}

void HdMaxMaterialCollection::MaterialData::Build(
    BitmapCache&                         bitmapCache,
    const MaxUsd::PrimvarMappingOptions& primvarMappingOptions,
    bool                                 standardViewport,
    bool                                 maxMaterial)
{
    const bool build3dsMaxMaterial = maxMaterial && !maxUsdPreviewSurfaceMaterialBuilt;
    const bool buildStdVpMaterial = standardViewport && !stdVpMaterialBuilt;

    if (!build3dsMaxMaterial && !buildStdVpMaterial) {
        return;
    }

    // Materials are built lazily at render time, we don't want to populate the undo stack.
    HoldSuspend holdSuspend;

    const auto& maps = build3dsMaxMaterial ? MaxUsd::MaterialUtils::USDPREVIEWSURFACE_MAPS
                                           : MaxUsd::MaterialUtils::USDPREVIEWSURFACE_STD_VP_MAPS;

    std::map<SdfPath, TfTokenVector> sdfPathToOutputsMap;

    // Build a map of sdfPaths to outputs for texture maps.
    // For example you might get an entry like : sdfPath -> [diffuseColor, opacity]
    for (const auto& rel : sourceNetwork.relationships) {
        const auto it = std::find(maps.begin(), maps.end(), rel.outputName);
        if (it == maps.end()) {
            continue;
        }
        sdfPathToOutputsMap[rel.inputId].push_back(rel.outputName);
    }

    // If building the full MaxUsdPreviewSurface material, all the work happens in this
    // paramblock, which will be initialized from the instance of the MaxUsdPreviewSurface
    // we will create.
    IParamBlock2* pb1 = nullptr;
    // Cache mapping between parameter names and their type from UsdPreviewSurface
    // scripted material ParamBlock. Only need to do this once.
    static std::unordered_map<std::string, ParamType2> paramNameToTypeMap;

    // If building the standard viewport representation of the material, there are two cases,
    // 1) The material is a simple color material, from the float3 value specified in the
    // UsdPreviewSurface material. 2) The material holds a simple diffuse texture map. Use a
    // physical material with only the base_color_map set. Hold off on creating the material, as in
    // the case of simple colors, we might reuse a previously created material for this color (A
    // simple nitrous StandardMaterialHandle).
    Texmap* stdVpDiffuseColorTexmap = nullptr;

    if (build3dsMaxMaterial) {
        Mtl* material = nullptr;
        if (!materialRef) {
            material = static_cast<Mtl*>(
                CreateInstance(MATERIAL_CLASS_ID, MaxUsdPreviewSurfaceMaterialClassID));

            // As soon as the material is assigned to the node, Max triggers some work, assuming it
            // will have to display the material. However, we manage/handle nitrous VP display
            // ourselves, so that work is useless, and sometimes quite costly. I found no way to
            // completely avoid this, but it is at least possible to greatly reduce the cost, by
            // disabling textures and setting up some other flags.
            material->SetMtlFlag(MTL_TEX_DISPLAY_ENABLED, FALSE);
            material->SetMtlFlag(MTL_SUB_DISPLAY_ENABLED, FALSE);
            material->SetMtlFlag(MTL_HW_TEX_ENABLED, FALSE);
            material->SetMtlFlag(MTL_HW_MAT_ENABLED, FALSE);

            // Material name doesn't change.
            const auto pathString = id.GetString();
            material->SetName(MSTR(MaxUsd::UsdStringToMaxString(pathString)));
            materialRef = std::make_shared<MaxUsd::MaterialRef>(material);
        } else {
            // Reuse the same material, but make sure to reset it completely before we set new
            // values. The new material may not define all the same properties as before.
            material = materialRef->GetAs<Mtl>();
            material->Reset();
        }

        // The MaxUsdPreviewSurface material settings are stored in ParamBlock 1.
        pb1 = material->GetParamBlock(1);

        // Get the mapping between parameter names and their type from the UsdPreviewSurface
        // scripted material ParamBlock.
        if (paramNameToTypeMap.empty()) {
            for (int i = 0; i < pb1->NumParams(); i++) {
                ParamID     paramID = pb1->IndextoID(i);
                std::string paramName
                    = MaxUsd::MaxStringToUsdString(pb1->GetLocalName(paramID).data());
                ParamType2 paramType = pb1->GetParameterType(paramID);
                paramNameToTypeMap[MaxUsd::UsdStringToLower(paramName)] = paramType;
            }
        }
    }

    TfHashMap<SdfPath, HdMaterialNode, SdfPath::Hash> pathToNode;

    // prefetch 2d transform data
    std::unordered_map<std::string, Transform2DData> transform2DPathToData;
    for (const auto& node : sourceNetwork.nodes) {
        pathToNode.insert({ node.path, node });

        if (node.identifier != TfToken("UsdTransform2d")) {
            continue;
        }

        const auto& transformParams = node.parameters;

        const auto nodePath = node.path.GetAsString();
        transform2DPathToData[nodePath] = Transform2DData();

        const auto transformRotationIt = transformParams.find(TfToken("rotation"));
        if (transformRotationIt != transformParams.end()
            && transformRotationIt->second.IsHolding<float>()) {
            transform2DPathToData[nodePath].zRotation = transformRotationIt->second.Get<float>();
        }

        const auto transformTranslationIt = transformParams.find(TfToken("translation"));
        if (transformTranslationIt != transformParams.end()
            && transformTranslationIt->second.IsHolding<GfVec2f>()) {
            GfVec2f transformTranslationValue = transformTranslationIt->second.Get<GfVec2f>();
            transform2DPathToData[nodePath].translation
                = Point2(transformTranslationValue[0], transformTranslationValue[1]);
        }

        const auto transformScaleIt = transformParams.find(TfToken("scale"));
        if (transformScaleIt != transformParams.end()
            && transformScaleIt->second.IsHolding<GfVec2f>()) {
            GfVec2f transformScaleValue = transformScaleIt->second.Get<GfVec2f>();
            transform2DPathToData[nodePath].scale
                = Point2(transformScaleValue[0], transformScaleValue[1]);
        }
    }

    for (const auto& node : sourceNetwork.nodes) {
        // Figure out texture map inputs from UsdUVTexture nodes.
        if (node.identifier == TfToken("UsdUVTexture")) {
            const auto textureMaps = sdfPathToOutputsMap.find(node.path);
            if (textureMaps == sdfPathToOutputsMap.end()) {
                continue;
            }

            const auto fileParamIt = node.parameters.find(TfToken("file"));
            if (fileParamIt == node.parameters.end()) {
                continue;
            }

            std::string wrapsStr, wraptStr;

            const auto wrapSIt = node.parameters.find(TfToken("wrapS"));
            if (wrapSIt != node.parameters.end()) {
                if (wrapSIt->second.CanCast<std::string>()) {
                    wrapsStr = VtValue::Cast<std::string>(wrapSIt->second).Get<std::string>();
                }
            }

            const auto wrapTIt = node.parameters.find(TfToken("wrapT"));
            if (wrapTIt != node.parameters.end()) {
                if (wrapTIt->second.CanCast<std::string>()) {
                    wraptStr = VtValue::Cast<std::string>(wrapTIt->second).Get<std::string>();
                }
            }

            const auto fileParam = fileParamIt->second;
            const auto subSetTexturePath = fileParam.Get<SdfAssetPath>().GetResolvedPath();

            // TODO : Add proper support for UDIMs. For now skip textures with a UDIM token.
            if (subSetTexturePath.find("<UDIM>") != std::string::npos) {
                continue;
            }

            // get associated transform data
            Transform2DData associatedTransformData = {};
            for (const auto& rel : sourceNetwork.relationships) {
                if (node.path == rel.outputId) {
                    const auto relInputPath = rel.inputId.GetAsString();
                    auto       t2dIt = transform2DPathToData.find(relInputPath);
                    if (t2dIt != transform2DPathToData.end()) {
                        associatedTransformData = transform2DPathToData[relInputPath];
                    }
                    break;
                }
            }

            // Default to 1
            int        uvChannel = 1;
            const auto primvar
                = MaxUsd::MaterialUtils::GetUsdUVTexturePrimvar(node, sourceNetwork, pathToNode);
            if (!primvar.IsEmpty()) {
                const auto channel
                    = primvarMappingOptions.GetPrimvarChannelMapping(primvar.GetString());
                if (channel != MaxUsd::PrimvarMappingOptions::invalidChannel) {
                    uvChannel = channel;
                }
            }

            // If using non-default texture transforms or wraps, use an OSLUberbitmap. Otherwise, we
            // use the regular bitmap. When converting to nitrous, the OSLUberbitmap is baked, and
            // this is very costly, so avoid using it if we can. THIS IS A PARTIAL SOLUTION : We
            // need to fully move away from the OSLUberbitmap if we can, and support transforms via
            // the regular bitmaps. The baking is costly but also causes issues, as if the user
            // changes the baking settings in the viewport, then the converted material falls out of
            // sync (baking happens at conversion time).
            const auto nonDefaultWrap
                = !wrapsStr.empty() && (wrapsStr != "repeat" || wraptStr != "repeat");
            const auto nonDefaultTransform = !(associatedTransformData == Transform2DData {});

            // USD model card textures generated from the USD scene delegate have a dummy path
            // "cardTexture". Their wrap is set to clamp, but the texture spans the entire card,
            // so we dont really need to treat it any different than "repeat". Avoid creating
            // costly OSL maps in this situation also.
            bool isDrawModeCardTexture = node.path == pxr::SdfPath("cardTexture");

            if ((nonDefaultWrap || nonDefaultTransform) && !isDrawModeCardTexture) {
                Texmap* oslTexMapMaterial
                    = static_cast<Texmap*>(CreateInstance(TEXMAP_CLASS_ID, OSLTex_CLASS_ID));
                auto pblock0 = oslTexMapMaterial->GetParamBlock(0);

#if MAX_RELEASE >= 26000
                auto oslPath = std::wstring(MaxSDKSupport::GetString(
                                                GetCOREInterface()->GetDir(APP_MAX_SYS_ROOT_DIR)))
                                   .append(L"OSL\\uberbitmap2.osl");
#else
                auto oslPath = std::wstring(MaxSDKSupport::GetString(
                                                GetCOREInterface()->GetDir(APP_MAX_SYS_ROOT_DIR)))
                                   .append(L"OSL\\uberbitmap.osl");
#endif
                // Set the OSL data
                pblock0->SetValueByName(_T( "OSLPath" ), MSTR(oslPath.c_str()), 0);
                pblock0->SetValueByName(_T( "OSLAutoUpdate" ), true, 0);

                // The OSLBitmap parameters are in the second param block
                auto pblock1 = oslTexMapMaterial->GetParamBlock(1);
                pblock1->SetValueByName(
                    _T("Filename"), MaxUsd::UsdStringToMaxString(subSetTexturePath), 0);

                // Map channel.
                pblock1->SetValueByName(_T( "UVSet" ), uvChannel, 0);

                // Set the WrapMode
                if (!wrapsStr.empty() && !wraptStr.empty() && wrapsStr != wraptStr) {
                    // warning here - No support for differing wrap modes
                    pblock1->SetValueByName(_T("WrapMode"), MSTR(L"periodic"), 0);
                } else if (!wrapsStr.empty() || !wraptStr.empty()) {
                    std::string usdWrapMode = !wrapsStr.empty() ? wrapsStr : wraptStr;
                    if (usdWrapMode == "black" || usdWrapMode == "clamp"
                        || usdWrapMode == "mirror") {
                        pblock1->SetValueByName(
                            _T("WrapMode"), MaxUsd::UsdStringToMaxString(usdWrapMode), 0);
                    } else if (usdWrapMode == "repeat") {
                        pblock1->SetValueByName(_T("WrapMode"), MSTR(L"periodic"), 0);
                    } else if (usdWrapMode == "useMetadata") {
                        // warning here - No support for wrap mode "useMetadata"
                        pblock1->SetValueByName(_T("WrapMode"), MSTR(L"periodic"), 0);
                    }
                } else {
                    pblock1->SetValueByName(_T("WrapMode"), MSTR(L"periodic"), 0);
                }

                if (associatedTransformData.translation != Point2(0.0f, 0.0f)) {
                    pblock1->SetValueByName(
                        _T("Offset"),
                        Point3(
                            -associatedTransformData.translation.x,
                            -associatedTransformData.translation.y,
                            0.0f),
                        0);
                }

                if (associatedTransformData.zRotation != 0.0f) {
                    pblock1->SetValueByName(_T("Rotate"), associatedTransformData.zRotation, 0);
                    pblock1->SetValueByName(_T("RotAxis"), Point3(0.0f, 0.0f, 1.0f), 0);
                    pblock1->SetValueByName(_T("RotCenter"), Point3(0.0f, 0.0f, 0.0f), 0);

                    Matrix3 rotMatrix = RotateZMatrix(-DegToRad(associatedTransformData.zRotation));

                    Point3   offsetVal;
                    Interval valid;
                    bool res = pblock1->GetValueByName<Point3>(_T("Offset"), 0, offsetVal, valid);

                    Point3 usdTranslation = Point3(offsetVal.x, offsetVal.y, 0.0f);

                    Point3 rotatedTranslation = usdTranslation * rotMatrix;
                    pblock1->SetValueByName(_T("Offset"), rotatedTranslation, 0);
                }

                if (associatedTransformData.scale != Point2(1.0f, 1.0f)) {
                    Point3   offsetVal;
                    Interval valid;
                    bool res = pblock1->GetValueByName<Point3>(_T("Offset"), 0, offsetVal, valid);
                    if (MaxUsd::MathUtils::IsAlmostZero(
                            associatedTransformData.scale.x - associatedTransformData.scale.y)) {
                        pblock1->SetValueByName(_T("Tiling"), Point3(1.0f, 1.0f, 1.0f), 0);
                        if (!MaxUsd::MathUtils::IsAlmostZero(associatedTransformData.scale.x)) {
                            pblock1->SetValueByName(
                                _T("Scale"), 1.0f / associatedTransformData.scale.x, 0);
                            pblock1->SetValueByName(
                                _T("Offset"),
                                Point3(
                                    offsetVal.x / associatedTransformData.scale.x,
                                    offsetVal.y / associatedTransformData.scale.y,
                                    0.0f),
                                0);
                        } else {
                            pblock1->SetValueByName(_T("Scale"), FLT_MAX, 0);
                            pblock1->SetValueByName(
                                _T("Offset"), Point3(FLT_MAX, FLT_MAX, 0.0f), 0);
                        }
                    } else {
                        if (!MaxUsd::MathUtils::IsAlmostZero(associatedTransformData.zRotation)) {
                            TF_WARN("Non uniform texture scaling with an applied rotation may "
                                    "result in incorrect "
                                    "texture mapping.");
                        }
                        pblock1->SetValueByName(_T("Scale"), 1.0f, 0);

                        Point3 offsetPoint, tilingPoint;
                        if (!MaxUsd::MathUtils::IsAlmostZero(associatedTransformData.scale.x)) {
                            tilingPoint.x = associatedTransformData.scale.x;
                            offsetPoint.x = offsetVal.x / associatedTransformData.scale.x;
                        } else {
                            tilingPoint.x = FLT_MAX;
                            offsetPoint.x = FLT_MAX;
                        }

                        if (!MaxUsd::MathUtils::IsAlmostZero(associatedTransformData.scale.y)) {
                            tilingPoint.y = associatedTransformData.scale.y;
                            offsetPoint.y = offsetVal.y / associatedTransformData.scale.y;
                        } else {
                            tilingPoint.y = FLT_MAX;
                            offsetPoint.y = FLT_MAX;
                        }
                        tilingPoint.z = 0.0f;
                        offsetPoint.z = 0.0f;
                        pblock1->SetValueByName(_T("Tiling"), tilingPoint, 0);
                        pblock1->SetValueByName(_T("Offset"), offsetPoint, 0);
                    }
                }

                for (const auto& map : textureMaps->second) {
                    if (build3dsMaxMaterial && pb1 != nullptr) {
                        const std::string& mapStr = map.GetString();

                        Texmap* multiOutputTexmap = static_cast<Texmap*>(
                            CreateInstance(TEXMAP_CLASS_ID, MULTIOUTPUTTOTEXMAP_CLASS_ID));
                        IMultiOutputConsumer* lMOConsumer = static_cast<IMultiOutputConsumer*>(
                            multiOutputTexmap->GetInterface(IMULTIOUTPUT_CONSUMER_INTERFACE));

                        // if the source map name has an associated input type, if it does
                        // connect the correct output of the OSLUberbitmap to the
                        // MULTIOUTPUTTOTEXMAP material (which in turn gets connected to the correct
                        // parameter in the UsdPreviewSurface scripted material)
                        auto paramTypeIt
                            = paramNameToTypeMap.find(MaxUsd::UsdStringToLower(mapStr));
                        if (paramTypeIt != paramNameToTypeMap.end()) {
                            auto paramType = paramTypeIt->second;
                            if (paramType == TYPE_FLOAT) {
                                lMOConsumer->SetOutputToInput(0, oslTexMapMaterial, 1);
                            } else if (paramType == TYPE_FRGBA) {
                                lMOConsumer->SetOutputToInput(0, oslTexMapMaterial, 0);
                            } else {
                                // unhandled parameter type
                            }
                        } else {
                            // unsupported source map
                        }
                        // connect MULTIOUTPUTTOTEXMAP material to the actual UsdPreviewSurface
                        // scripted material
                        const auto propertyName
                            = MaxUsd::UsdStringToMaxString(map.GetString() + std::string("_map"));
                        pb1->SetValueByName(propertyName, multiOutputTexmap, 0);
                    }
                    if (buildStdVpMaterial && map == MaxUsdUsdPreviewSurfaceTokens->diffuseColor) {
                        stdVpDiffuseColorTexmap = oslTexMapMaterial;
                    }
                }
            } else {
                for (const auto& map : textureMaps->second) {
                    // Lambda to retrieve or create the bitmap.
                    // Unfortunately we cannot reuse the same bitmap for the VP, any edits to the
                    // bitmap might break the nitrous handle we generate manually for the viewport.
                    auto getBitmap =
                        [&bitmapCache, &subSetTexturePath, &uvChannel, &textureMaps](bool forVp) {
                            BitmapTex* bitmapTexture = nullptr;
                            BitmapKey  key = { subSetTexturePath, uvChannel, forVp };
                            const auto it = bitmapCache.find(key);
                            if (it == bitmapCache.end()) {
                                bitmapTexture = NewDefaultBitmapTex();
                                bitmapTexture->SetMapName(
                                    MaxUsd::UsdStringToMaxString(subSetTexturePath).data());
                                // Use the USD texture prim path as name.
                                bitmapTexture->SetName(
                                    MaxUsd::UsdStringToMaxString(textureMaps->first.GetString()));
                                auto texRef = std::make_shared<MaxUsd::MaterialRef>(bitmapTexture);
                                bitmapCache.insert({ key, texRef });
                                bitmapTexture->GetUVGen()->SetMapChannel(uvChannel);
                            } else {
                                bitmapTexture = it->second->GetAs<BitmapTex>();
                                // Textures in the bitmap cache should not have any transforms
                                // applied or edits. If the user ever applies some, these are
                                // overriden on update as the bitmap is referencing USD data. This
                                // situation is only possible for textured referenced by the full
                                // material. The VP version cannot be edited by users.
                                if (!forVp) {
                                    bitmapTexture->GetUVGen()->Reset();
                                    bitmapTexture->GetUVGen()->SetMapChannel(uvChannel);
                                }
                            }
                            return bitmapTexture;
                        };

                    if (build3dsMaxMaterial) {
                        const auto propertyName
                            = MaxUsd::UsdStringToMaxString(map.GetString() + std::string("_map"));
                        pb1->SetValueByName(propertyName, getBitmap(false), 0);
                    }
                    if (buildStdVpMaterial && map == MaxUsdUsdPreviewSurfaceTokens->diffuseColor) {
                        stdVpDiffuseColorTexmap = getBitmap(true);
                    }
                }
            }
        }
        // Basic parameter inputs (simple values) from the UsdPreviewSurface node.
        else if (node.identifier == TfToken("UsdPreviewSurface")) {
            for (auto& param : node.parameters) {
                const auto& parameterToken = param.first;
                if (build3dsMaxMaterial) {
                    auto parameterName = MaxUsd::UsdStringToMaxString(parameterToken.GetString());
                    // int values : [useSpecularWorkflow]
                    if (param.second.IsHolding<int>()) {
                        auto intValue = param.second.Get<int>();
                        pb1->SetValueByName(parameterName, intValue, 0);
                    }
                    // float values : [roughness, clearcoat, clearcoatRoughness, opacity,
                    // opacityThreshold, ior, displacement, occlusion]
                    else if (param.second.IsHolding<float>()) {
                        auto floatValue = param.second.Get<float>();
                        pb1->SetValueByName(parameterName, floatValue, 0);
                    }
                    // float3 values = [diffuseColor, emissiveColor, specularColor, normal]
                    else if (param.second.IsHolding<GfVec3f>()) {
                        auto& float3Value = param.second.Get<GfVec3f>();

                        // Treat colors as colors in Max.
                        if (parameterName.EndsWith(L"Color")) {
                            Color color { float3Value[0], float3Value[1], float3Value[2] };
                            pb1->SetValueByName(parameterName, color, 0);
                        }
                        // normal as Point3.
                        else {
                            Point3 point { float3Value[0], float3Value[1], float3Value[2] };
                            pb1->SetValueByName(parameterName, point, 0);
                        }
                    }
                }
                if (buildStdVpMaterial) {
                    // For the standard viewport, only care about the diffuse color.
                    if (parameterToken == MaxUsdUsdPreviewSurfaceTokens->diffuseColor) {
                        if (param.second.IsHolding<GfVec3f>()) {
                            auto& float3Value = param.second.Get<GfVec3f>();
                            color = { float3Value[0], float3Value[1], float3Value[2] };
                            break;
                        }
                    }
                }
            }
        }
    }

    // Now we can figure out what we really need for the viewport material....
    if (buildStdVpMaterial) {
        // Has a diffuse texture? Create a simple physical material, which will be converted to
        // nitrous. This is faster than instanciating a Nitrous TextureHandle from the Bitmap, as
        // this causes an extra bake of the map. The shading is also a bit prettier...
        if (stdVpDiffuseColorTexmap) {
            Mtl* material = nullptr;
            if (!stdVpMaterialRef) {
                material = static_cast<Mtl*>(
                    CreateInstance(MATERIAL_CLASS_ID, PHYSICALMATERIAL_CLASS_ID));
                stdVpMaterialRef = std::make_shared<MaxUsd::MaterialRef>(material);
            } else {
                // Reuse the same material, just reset it.
                material = stdVpMaterialRef->GetAs<Mtl>();
                material->Reset();
            }
            material->GetParamBlock(0)->SetValueByName(
                L"base_color_map", stdVpDiffuseColorTexmap, 0);
            stdVpMaterial
                = MaxSDKSupport::Graphics::MaterialConversionHelper::ConvertMaxToNitrousMaterial(
                    *material, 0, false);
        } else {
            // Color only, we dont need a 3dsMax material at all. Typically this would already be
            // nullptr, but it could be that the USD data was modified in-memory (previously using a
            // texture map, and no longer). A Nitrous StandardMaterial will be created/reused from
            // the color we set previously.
            stdVpMaterialRef.reset();
        }
    }

    stdVpMaterialBuilt |= buildStdVpMaterial;
    maxUsdPreviewSurfaceMaterialBuilt |= build3dsMaxMaterial;
}

const HdMaterialNetwork& HdMaxMaterialCollection::MaterialData::GetSourceMaterialNetwork() const
{
    return sourceNetwork;
}

std::shared_ptr<MaxUsd::MaterialRef> HdMaxMaterialCollection::MaterialData::GetMaxMaterial() const
{
    return maxUsdPreviewSurfaceMaterialBuilt ? materialRef : nullptr;
}

MaxSDK::Graphics::BaseMaterialHandle&
HdMaxMaterialCollection::MaterialData::GetNitrousMaterial(bool forInstances)
{
    auto& handle = forInstances ? stdVpInstancesMaterial : stdVpMaterial;
    if (handle.IsValid()) {
        return handle;
    }

    // Diffuse color texture material.
    if (stdVpMaterialRef) {
        const auto material = stdVpMaterialRef->GetAs<Mtl>();
        if (!material) {
            DbgAssert(0 && "Viewport material uninitialized.");
            return handle;
        }
        handle = MaxSDKSupport::Graphics::MaterialConversionHelper::ConvertMaxToNitrousMaterial(
            *material, 0, false);
        return handle;
    }
    // Color only, check the cache and use a simple StandardMaterialHandle.
    handle = HdMaxColorMaterial::Get(color, forInstances);
    return handle;
}

bool HdMaxMaterialCollection::MaterialData::IsVpMaterialBuilt() const { return stdVpMaterialBuilt; }

MaxSDK::Graphics::BaseMaterialHandle
HdMaxMaterialCollection::MaterialData::ConvertToNitrous(Mtl* material)
{
    // Materials converted to Nitrous react poorly to modifications from the Material editor.
    // To avoid those issues, copy the material before the conversion.
    const auto copy = static_cast<Mtl*>(CloneRefHierarchy(materialRef->GetAs<Mtl>()));

    // We only support the standard viewport. Opacity/OpacityThreshold is implemented using OSL, the
    // conversion of this graph using realistic=false breaks, it is not expected to properly show
    // the opacity, but we should at least get an opaque result, but we just get a completely
    // invisible nitrous material. Workaround this problem by just disabling the opacity map in
    // standard mode.
    BitmapTex* noOpacityMap = nullptr;
    copy->GetParamBlock(1)->SetValueByName(L"opacity_map", noOpacityMap, 0);
    copy->GetParamBlock(1)->SetValueByName(L"opacity", 1.0f, 0);

    return MaxSDKSupport::Graphics::MaterialConversionHelper::ConvertMaxToNitrousMaterial(
        *copy, 0, false);
}

HdMaxMaterialCollection::MaterialDataPtr
HdMaxMaterialCollection::RegisterMaxMaterial(const SdfPath& materialId, Mtl* maxMaterial)
{
    const auto it = materials.find(materialId);
    if (it != materials.end()) {
        return it->second;
    }

    MaterialDataPtr materialData = std::make_shared<MaterialData>(materialId, maxMaterial);
    materials.insert({ materialId, materialData });

    return materialData;
}

HdMaxMaterialCollection::MaterialDataPtr
HdMaxMaterialCollection::AddMaterial(HdSceneDelegate* delegate, const SdfPath& materialId)
{
    const auto it = materials.find(materialId);
    if (it != materials.end()) {
        return it->second;
    }
    return UpdateMaterial(delegate, materialId);
}

void HdMaxMaterialCollection::RegisterMaxBitmap(BitmapTex* texture)
{
    auto      textureRef = std::make_shared<MaxUsd::MaterialRef>(texture);
    BitmapKey key { MaxUsd::MaxStringToUsdString(texture->GetMapName()),
                    texture->GetUVGen()->GetMapChannel(),
                    false };
    bitmapCache.insert({ key, textureRef });
}

HdMaxMaterialCollection::MaterialDataPtr
HdMaxMaterialCollection::UpdateMaterial(HdSceneDelegate* delegate, const SdfPath& materialId)
{
    const VtValue resource = delegate->GetMaterialResource(materialId);
    if (resource.IsHolding<HdMaterialNetworkMap>()) {
        const HdMaterialNetworkMap& networkMap = resource.UncheckedGet<HdMaterialNetworkMap>();
        HdMaterialNetwork           matNetwork;
        TfMapLookup(networkMap.map, HdMaterialTerminalTokens->surface, &matNetwork);

        auto it = materials.find(materialId);

        MaterialDataPtr materialData = nullptr;

        // Material doesn't exist yet.
        if (it == materials.end()) {
            materialData = std::make_shared<MaterialData>(materialId, matNetwork);
            materials.insert({ materialId, materialData });
        } else {
            it->second->UpdateSource(matNetwork);
            materialData = it->second;
        }

        return materialData;
    }
    return nullptr;
}

// Must be called from the main UI thread.
void HdMaxMaterialCollection::BuildMaterials(
    const MaxUsd::ProgressReporter&      progressReporter,
    const MaxUsd::PrimvarMappingOptions& primvarMappingOptions,
    bool                                 standardViewport,
    bool                                 maxMaterial)
{
    // Only show progress in the UI if we have materials that have to be built from scratch,
    // I.e. we have not generated any 3dsMax material for them yet.
    bool showProgressForMaxMaterials = false;
    bool showProgressForStdViewportMaterials = false;
    for (const auto& entry : materials) {
        // If we need to build full materials, prioritize showing progress for that.
        showProgressForMaxMaterials |= maxMaterial && entry.second->GetMaxMaterial() == nullptr;
        if (showProgressForMaxMaterials) {
            break;
        }
        showProgressForStdViewportMaterials
            |= standardViewport && !entry.second->IsVpMaterialBuilt();
    }

    const bool progressReporting
        = showProgressForMaxMaterials || showProgressForStdViewportMaterials;

    // Use a progressBar if new materials need to be built.
    if (showProgressForMaxMaterials) {
        progressReporter.Start(GetString(IDS_RENDERDELEGATE_BUILD_MATERIALS_PROGRESS_TITLE));
    } else if (showProgressForStdViewportMaterials) {
        progressReporter.Start(GetString(IDS_RENDERDELEGATE_BUILD_STD_VP_MATERIALS_PROGRESS_TITLE));
    }

    // Loop through all the materials, and build the Max/Nitrous representations.
    int currentMaterialIndex = 0;
    for (const auto& material : materials) {
        material.second->Build(bitmapCache, primvarMappingOptions, standardViewport, maxMaterial);
        if (progressReporting) {
            const auto currentPercentage = static_cast<double>(currentMaterialIndex++)
                / static_cast<double>(materials.size()) * 100.0;
            progressReporter.Update(static_cast<int>(currentPercentage));
        }
    }

    if (progressReporting) {
        progressReporter.End();
    }
}

std::shared_ptr<MaxUsd::MaterialRef> HdMaxMaterialCollection::GetDisplayColorMaterial()
{
    // Check if the Vertex displayColor was already created.
    if (displayColorMaterial) {
        return displayColorMaterial;
    }

    static Class_ID vcolClassID(VCOL_CLASS_ID, 0);

    Mtl*          mat = NewDefaultMaterial(_T("USD"));
    Texmap*       map = (Texmap*)CreateInstance(TEXMAP_CLASS_ID, vcolClassID);
    IParamBlock2* pb = mat->GetParamBlockByID(0);
    pb->SetValueByName(L"base_color_map_on", TRUE, 0);
    pb->SetValueByName(L"base_color_map", map, 0);

    mat->SetShininess(0, 0);
    mat->SetName(L"displayColor");

    displayColorMaterial = std::make_shared<MaxUsd::MaterialRef>(mat);
    return displayColorMaterial;
}

void HdMaxMaterialCollection::Clear()
{
    materials.clear();
    bitmapCache.clear();
}

void HdMaxMaterialCollection::RemoveMaterial(pxr::SdfPath const& path)
{
    materials.unsafe_erase(path);
}

// Disable obscure warning only occurring for 2022 (pixar usd version: 21.11):
// no definition for inline function : pxr::DefaultValueHolder Was not able to identify what is
// triggering this.
#pragma warning(disable : 4506)