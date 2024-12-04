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
#include "TranslationUtils.h"

#include "MathUtils.h"
#include "MaxSupportUtils.h"
#include "MetaDataUtils.h"
#include "TypeUtils.h"

#include <pxr/usd/kind/registry.h>
#include <pxr/usd/usd/modelAPI.h>
#include <pxr/usd/usdGeom/metrics.h>
#include <pxr/usd/usdGeom/xform.h>
#include <pxr/usd/usdGeom/xformable.h>
#include <pxr/usd/usdSkel/root.h>
#include <pxr/usd/usdSkel/skeleton.h>
#include <pxr/usd/usdSkel/utils.h>

#include <maxscript/maxscript.h>

#include <XRef/iXrefItem.h>
#include <control.h>
#include <iInstanceMgr.h>
#include <iparamb2.h>
#include <iskin.h>
#include <max.h>
#include <modstack.h>
#include <path.h>

PXR_NAMESPACE_USING_DIRECTIVE

namespace MAXUSD_NS_DEF {

bool WsmRequiresTransformToLocalSpace(INode* node, const TimeValue& time)
{
    // Check's if a WSM exists.
    if (node->GetObjectRef() != node->GetObjOrWSMRef()) {
        // If a World Space Modifier is applied, the object's points are already transformed in
        // world space, and so the object transform is the identity. In this case, we would bake the
        // inverse of the node's transform into the geometry to move points back to local space, so
        // that within the USD hierarchy, where transforms are inherited, the overall transform of
        // each vertex is correct. Can be false positive if the object TM before WSM was at the
        // identity. Disregard this to avoid getting different results over animations, because it
        // is the identity transform, this wont have adverse effects.
        const auto afterWSM = node->GetObjTMAfterWSM(time);
        return MaxUsd::MathUtils::IsIdentity(afterWSM);
    }
    return false;
}

Matrix3 GetMaxObjectOffsetTransform(INode* node)
{
    // The following implementation is directly from the Max docs.
    Matrix3 transform = Matrix3::Identity;
    Point3  pos = node->GetObjOffsetPos();
    transform.PreTranslate(pos);
    Quat quat = node->GetObjOffsetRot();
    PreRotateMatrix(transform, quat);
    ScaleValue scaleValue = node->GetObjOffsetScale();
    ApplyScaling(transform, scaleValue);
    return transform;
}

void ApplyObjectOffsetTransform(
    INode*                 node,
    pxr::UsdGeomXformable& xformable,
    const TimeValue&       time)
{
    // If a WSM is applied, the offset is already considered for the geometry's points,
    // which are now in world space.
    if (WsmRequiresTransformToLocalSpace(node, time)) {
        return;
    }
    const auto objectTransform = MaxUsd::GetMaxObjectOffsetTransform(node);
    if (!MaxUsd::MathUtils::IsIdentity(objectTransform)) {
        bool                resetsXformStack = false;
        pxr::UsdGeomXformOp usdGeomXFormOp;
        size_t              nbOfOps = xformable.GetOrderedXformOps(&resetsXformStack).size();
        usdGeomXFormOp = xformable.AddXformOp(
            pxr::UsdGeomXformOp::TypeTransform,
            pxr::UsdGeomXformOp::PrecisionDouble,
            nbOfOps > 0 ? pxr::TfToken("t" + std::to_string(nbOfOps)) : pxr::TfToken());
        usdGeomXFormOp.Set(MaxUsd::ToUsd(objectTransform));
    }
}

bool IsValidChannel(int channel) { return channel >= -NUM_HIDDENMAPS && channel < MAX_MESHMAPS; }

bool GetValidIdentifier(const std::wstring& identifier, std::string& validIdentifier)
{
    const auto bytes = MaxUsd::MaxStringToUsdString(identifier.c_str());
    validIdentifier = pxr::TfMakeValidIdentifier(bytes);
    return wcscmp(identifier.c_str(), MaxUsd::UsdStringToMaxString(validIdentifier.c_str()).data())
        == 0;
}

size_t GetTypeDimension(const pxr::SdfValueTypeName& type)
{
    size_t dimension = 1;
    auto   dimensions = type.GetDimensions();
    if (dimensions.size > 0) {
        dimension = dimensions.d[0];
    }
    return dimension;
}

bool ValidateMappedDataForMesh(
    size_t                valueCount,
    const pxr::VtIntArray indices,
    const MNMesh&         maxMesh,
    const pxr::TfToken&   interpolation,
    bool                  isIndexed)
{
    if (!pxr::UsdGeomPrimvar::IsValidInterpolation(interpolation)) {
        return false;
    }

    // Data requirements will vary depending on the interpolation used.
    int minValueCount = 1; // Constant interpolation.
    if (interpolation == pxr::UsdGeomTokens->vertex
        || interpolation == pxr::UsdGeomTokens->varying) {
        minValueCount = maxMesh.VNum();
    } else if (interpolation == pxr::UsdGeomTokens->uniform) {
        minValueCount = maxMesh.FNum();
    } else if (interpolation == pxr::UsdGeomTokens->faceVarying) {
        minValueCount = 0;
        for (int i = 0; i < maxMesh.FNum(); i++) {
            minValueCount += maxMesh.F(i)->deg;
        }
    }

    if (!isIndexed) {
        return valueCount >= minValueCount;
    }

    int minIndexCount = minValueCount;
    minValueCount = 1;

    // Indexed...
    if (indices.size() < minIndexCount || valueCount < minValueCount) {
        return false;
    }
    return std::all_of(indices.begin(), indices.end(), [&valueCount](int idx) {
        return idx >= 0 && idx < valueCount;
    });
}

bool IsAttributeAuthored(const pxr::UsdAttribute& att, const pxr::UsdTimeCode& timeCode)
{
    std::vector<double> samples(att.GetNumTimeSamples());
    att.GetTimeSamples(&samples);
    return std::find(samples.begin(), samples.end(), timeCode) != samples.end();
}

bool IsValidAbsolutePath(const fs::path& path)
{
    if (!path.is_absolute() || !path.has_filename() || !path.has_extension()) {
        return false;
    }
    auto maxPath = MaxSDK::Util::Path(path.c_str());
    // Need to resolve the path before validating the path length.
    maxPath = maxPath.GetResolvedAbsolutePath();
    if (maxPath.GetString().Length() > MAX_PATH) {
        return false;
    }
    return maxPath.IsLegal();
}

bool FindInstanceableNodes(
    INode*                            node,
    INodeTab&                         instancesNode,
    const std::unordered_set<INode*>& eligibleNodes)
{
    INodeTab      nodeTabs;
    IInstanceMgr* instanceMgr = IInstanceMgr::GetInstanceMgr();
    instanceMgr->GetInstances(*node, nodeTabs);

    // The resulting nodeTabs will contains the node itself.
    if (nodeTabs.Count() < 2) {
        return false;
    }

    // Find the last derived object with modifier applied to it
    Object* refObjectPtr1 = GetFirstDerivedObjectWithModifier(node);

    if (refObjectPtr1 == nullptr ||
        // Don't consider object with space warp for instancing.
        refObjectPtr1->ClassID() == Class_ID(WSM_DERIVOB_CLASS_ID, 0) ||
        // Don't consider helpers without geometry for instancing either, no point in instancing
        // Xforms on the USD side.
        (refObjectPtr1->SuperClassID() == HELPER_CLASS_ID
         && !refObjectPtr1->CanConvertToType({ TRIOBJ_CLASS_ID, 0 }))) {
        return false;
    }

    // Look at every other related nodes to find the ones where the last derived node with modifier
    // is the same.
    for (int i = 0; i < nodeTabs.Count(); i++) {
        const auto node = nodeTabs[i];
        if (!eligibleNodes.empty() && eligibleNodes.find(node) == eligibleNodes.end()) {
            continue;
        }

        Object* refObjectPtr2 = GetFirstDerivedObjectWithModifier(node);

        // Don't consider object with space warp for instancing
        if (refObjectPtr2 == nullptr
            || refObjectPtr2->ClassID() == Class_ID(WSM_DERIVOB_CLASS_ID, 0)) {
            continue;
        }

        if (refObjectPtr1 == refObjectPtr2) {
            instancesNode.Append(1, &nodeTabs[i]);
        }
    }

    return instancesNode.Count() > 1 ? true : false;
}

Object* GetFirstDerivedObjectWithModifier(INode* node)
{
    Object* objectPtr = node->GetObjOrWSMRef();
    while (objectPtr && objectPtr->SuperClassID() == GEN_DERIVOB_CLASS_ID) {
        IDerivedObject* derivedObjectPtr = static_cast<IDerivedObject*>(objectPtr);
        int             nbModifier = derivedObjectPtr->NumModifiers();
        if (nbModifier > 0) {
            break;
        }
        objectPtr = derivedObjectPtr->GetObjRef();
    }
    return objectPtr;
}

std::string UniqueNameGenerator::GetName(const std::string& name)
{
    if (existingNames.find(name) == existingNames.end()) {
        existingNames.insert(name);
        return name;
    }

    std::string newName = GetNextName(name);
    while (existingNames.find(newName) != existingNames.end()) {
        newName = GetNextName(newName);
    }

    existingNames.insert(newName);
    return newName;
}

std::string UniqueNameGenerator::GetNextName(const std::string& name)
{
    // Look for a number at the end of the string to be incremented...
    bool     foundSuffix = false;
    uint64_t oldSuffixInt = 0;
    if (!name.empty()) {
        int      index = static_cast<int>(name.size()) - 1;
        uint64_t factor = 1;
        while (index >= 0 && _istdigit(name[index])) {
            oldSuffixInt += factor * std::stoi(std::string(1, name[index--]));
            factor *= 10;
            foundSuffix = true;
        }
    }

    // No number suffix found, simply, append "1"
    if (!foundSuffix) {
        return (name + "1");
    }

    // Replace with the new incremented suffix.
    const auto oldSuffix = std::to_string(oldSuffixInt);
    auto       suffixPos = name.rfind(oldSuffix);
    // If the new suffix is bigger, check if we have a preceding zero that we should use,
    // so that we bump 009 to 010 and not 0010.
    const auto newSuffix = std::to_string(oldSuffixInt + 1);
    if (newSuffix.size() > oldSuffix.size() && name[suffixPos - 1] == '0') {
        suffixPos--;
    }
    auto newName = name.substr(0, suffixPos) + newSuffix;
    return newName;
}

float GetMaxFrameFromUsdFrameTime(
    const pxr::UsdStageWeakPtr& stage,
    const pxr::UsdTimeCode      usdFrameTimeCode)
{
    return static_cast<float>(
        usdFrameTimeCode.GetValue() / (GetFrameRate() * stage->GetTimeCodesPerSecond()));
}

double
GetMaxFrameFromUsdTimeCode(const pxr::UsdStageWeakPtr& stage, const pxr::UsdTimeCode usdTimeCode)
{
    auto stageFPS = stage->GetTimeCodesPerSecond();
    auto maxFPS = GetFrameRate();

    return usdTimeCode.GetValue() * (maxFPS / stageFPS);
}

TimeValue GetMaxTimeValueFromUsdTimeCode(
    const pxr::UsdStageWeakPtr& stage,
    const pxr::UsdTimeCode      usdTimeCode)
{
    const auto timeCodePerSec = stage->GetTimeCodesPerSecond();
    return MaxUsd::GetTimeValueFromFrame(GetFrameRate() * usdTimeCode.GetValue() / timeCodePerSec);
}

pxr::UsdTimeCode
GetUsdTimeCodeFromMaxFrame(const pxr::UsdStageWeakPtr& stage, const double maxFrame)
{
    auto stageFPS = stage->GetTimeCodesPerSecond();
    auto maxFPS = GetFrameRate();

    return maxFrame * (stageFPS / maxFPS);
}

pxr::UsdTimeCode
GetUsdTimeCodeFromMaxTime(const pxr::UsdStageWeakPtr& stage, const TimeValue& timeValue)
{
    auto secs = TicksToSec(timeValue);
    auto tcPerSec = stage->GetTimeCodesPerSecond();
    return pxr::UsdTimeCode(secs * tcPerSec);
}

pxr::UsdTimeCode GetOffsetTimeCode(
    const pxr::UsdStageWeakPtr& stage,
    const TimeValue&            timeValue,
    const double                customAnimStartFrame,
    const double                customAnimationLength)
{
    auto stageStartCode = stage->GetStartTimeCode();
    auto stageEndCode = stage->GetEndTimeCode();

    auto sourceAnimLength = stageEndCode - stageStartCode;
    auto maxAnimLength = MaxUsd::GetMaxFrameFromUsdTimeCode(stage, sourceAnimLength);

    auto usdAnimRenderScaler = 1.0;
    if (customAnimationLength != 0 && customAnimationLength != maxAnimLength) {
        usdAnimRenderScaler = maxAnimLength / customAnimationLength;
    }

    auto tcPerSec = stage->GetTimeCodesPerSecond();
    auto animStartTimeInTicks = customAnimStartFrame * GetTicksPerFrame();

    auto usdAnimRenderOffsetInTimecodes
        = stageStartCode - GetUsdTimeCodeFromMaxFrame(stage, customAnimStartFrame).GetValue();
    auto timeSinceAnimStartedInTicks = timeValue - animStartTimeInTicks;

    auto timeBeforeAnimStarted = TicksToSec(static_cast<TimeValue>(animStartTimeInTicks));
    auto usdTimeCode = pxr::UsdTimeCode(
        timeBeforeAnimStarted * tcPerSec
        + (TicksToSec(static_cast<TimeValue>(timeSinceAnimStartedInTicks * usdAnimRenderScaler))
           * tcPerSec)
        + usdAnimRenderOffsetInTimecodes);

    return usdTimeCode;
}

pxr::UsdTimeCode GetCurrentUsdTimeCode(const pxr::UsdStageWeakPtr& stage)
{
    const auto frame = GetCOREInterface()->GetTime() / double(GetTicksPerFrame());
    return GetUsdTimeCodeFromMaxFrame(stage, frame);
}

bool IsStageUsingYUpAxis(const pxr::UsdStageWeakPtr& stage)
{
    if (!stage) {
        return false;
    }
    const auto upAxis = pxr::UsdGeomGetStageUpAxis(stage);
    if (upAxis == pxr::UsdGeomTokens->y || upAxis == pxr::UsdGeomTokens->z) {
        return upAxis == pxr::UsdGeomTokens->y;
    } else {
        const char* upAxisValue = upAxis.data();
        return std::toupper(upAxisValue[0]) == pxr::UsdGeomTokens->y.data()[0];
    }
}

pxr::GfMatrix4d GetStageAxisAndUnitRootTransform(pxr::UsdStageWeakPtr stage)
{
    pxr::GfMatrix4d rootTransform;
    if (!stage) {
        rootTransform.SetIdentity();
        return rootTransform;
    }

    const auto rescaleFactor = MaxUsd::GetUsdToMaxScaleFactor(stage);
    rootTransform.SetScale(rescaleFactor);

    if (IsStageUsingYUpAxis(stage)) {
        MaxUsd::MathUtils::ModifyTransformYToZUp(rootTransform);
    }
    return rootTransform;
}

double GetUsdToMaxScaleFactor(const pxr::UsdStageWeakPtr& stage)
{
    const double maxUnitsPerMeter = GetSystemUnitScale(UNITS_METERS);
    const double usdUnitsPerMeter = pxr::UsdGeomGetStageMetersPerUnit(stage);
    const double rescaleFactor = usdUnitsPerMeter / maxUnitsPerMeter;
    return rescaleFactor;
}

void UniqueNameGenerator::Reset() { existingNames.clear(); }

short FindParamId(IParamBlock2* pb2, const wchar_t* name)
{
    ParamBlockDesc2* pbDesc = pb2->GetDesc();
    int              paramIndex = pbDesc->NameToIndex(name);
    return pbDesc->IndextoID(paramIndex);
}

std::string MaxStringToUsdString(const wchar_t* utf16EncodedWideString)
{
    std::string utf8EncodedStr = UTF8Str::FromMCHAR(utf16EncodedWideString).data();
    return utf8EncodedStr;
}

std::string UsdStringToLower(std::string utf8EncodedString)
{
    std::transform(
        utf8EncodedString.begin(),
        utf8EncodedString.end(),
        utf8EncodedString.begin(),
        [](unsigned char c) { return std::tolower(c); });
    return utf8EncodedString;
};

WStr UsdStringToMaxString(const std::string& utf8EncodedString)
{
    // Build a WStr. The returned WStr will be UTF16 encoded.
    return TSTR::FromUTF8(utf8EncodedString.c_str());
}

bool HasUnicodeCharacter(const std::string& str)
{
    // An ASCII character uses only the lower 7 bits of a char (values 0-127).
    // A non-ASCII Unicode character encoded in UTF-8 uses char elements that all have the upper bit
    // set.
    for (const auto c : str) {
        if (static_cast<unsigned char>(c) > 127) {
            return true;
        }
    }
    return false;
}

void ConvertFrames(
    INode*                                                                  node,
    std::function<void(Object*, const TimeValue&, const pxr::UsdTimeCode&)> convertFrame,
    const TimeConfig&                                                       usdTimeConfig,
    std::function<Interval(Object*, const TimeValue&)>                      objectValidityOverride)
{
    const TimeValue startTime = usdTimeConfig.GetStartTime();
    const TimeValue endTime = usdTimeConfig.GetEndTime();
    const int       timeStep = usdTimeConfig.GetTimeStep();

    for (TimeValue timeVal = startTime; timeVal <= endTime;) {
        pxr::UsdTimeCode usdTimeCode;
        auto             objectWS = node->EvalWorldState(timeVal);
        // also call any additional object validity evaluation method defined by the caller
        // the default additional method returns a FOREVER validity interval
        Interval additionalObjectEvaluationValidity = objectValidityOverride(objectWS.obj, timeVal);

        // If the object is not animated, we only need to convert a single frame, at the default
        // timeCode.
        if (!usdTimeConfig.IsAnimated()) {
            usdTimeCode = pxr::UsdTimeCode::Default();
            convertFrame(objectWS.obj, timeVal, usdTimeCode);
            break;
        }

        // Use the object validity interval to avoid exporting frames needlessly.
        Interval validity = objectWS.Validity(timeVal);
        // combine the validity intervals - the intersection of the intervals
        validity &= additionalObjectEvaluationValidity;
        const TimeValue lastValidTime = validity.End();

        // The first frame we export should be the last from the validity interval applicable
        // at usdTimeConfig.startFrame
        if (timeVal == startTime && timeVal != lastValidTime && lastValidTime < endTime) {
            timeVal = lastValidTime;
            continue;
        }

        usdTimeCode = pxr::UsdTimeCode(double(timeVal) / double(GetTicksPerFrame()));
        convertFrame(objectWS.obj, timeVal, usdTimeCode);

        // The next frame we are interested in, is the last frame where the object is
        // still valid in this state, or the first frame of the next validity interval, if
        // we were already at the end of an interval.
        if (lastValidTime != timeVal) {
            // If the validity interval goes beyond the frame range we are interested in,
            // we don't need to specify another frame, as the last frame we set is still valid.
            if (lastValidTime > endTime) {
                break;
            }
            timeVal = lastValidTime;
        } else {
            if (timeVal == endTime) {
                break;
            }
            // Calculate the next frame to convert, make sure the endFrame is converted.
            timeVal = std::min(timeVal + timeStep, endTime);
        }
    }
}

std::pair<pxr::VtArray<TimeValue>, pxr::VtArray<pxr::UsdTimeCode>>
GetFramesFromValidityInterval(INode* node, const TimeConfig& usdTimeConfig)
{
    pxr::VtArray<TimeValue>        maxTimes;
    pxr::VtArray<pxr::UsdTimeCode> usdTimes;

    auto getFrames
        = [&maxTimes,
           &usdTimes](Object* obj, const TimeValue& timeVal, const pxr::UsdTimeCode& usdTimeCode) {
              maxTimes.push_back(timeVal);
              usdTimes.push_back(usdTimeCode);
          };

    ConvertFrames(node, getFrames, usdTimeConfig);
    return { maxTimes, usdTimes };
}

std::vector<pxr::UsdTimeCode>
GetUsdTimeSamplesForExport(const pxr::UsdStageWeakPtr& stage, const MaxUsd::TimeConfig& timeConfig)
{
    std::vector<pxr::UsdTimeCode> timeSamples;

    const int       timeStep = timeConfig.GetTimeStep();
    const TimeValue startTime = timeConfig.GetStartTime();
    const TimeValue endTime = timeConfig.GetEndTime();
    for (TimeValue timeVal = startTime; timeVal <= endTime;) {
        timeSamples.emplace_back(GetUsdTimeCodeFromMaxTime(stage, timeVal));
        if (timeVal == endTime) {
            break;
        }
        timeVal = std::min(timeVal + timeStep, endTime);
    }

    return timeSamples;
}

bool IsBoneObject(Object* object)
{
    const auto classId = object->ClassID();
    return classId == Class_ID(0x008a63c0, 0x00000000) || // Bone
        classId == Class_ID(0x28bf6e8d, 0x2ecca840) ||    // BoneGeometry
        classId == Class_ID(0x56ae72e5, 0x389b6659) ||    // CATPArent
        classId == Class_ID(0x2e6a0c09, 0x43d5c9c0) ||    // CATBone
        classId == Class_ID(0x00009125, 0x00000000) ||    // Biped_object
        classId == Class_ID(0x73dc4833, 0x65c93caa);      // HubObject
}

std::vector<Modifier*> GetMaxMorpherModifiers(INode* node, bool enabledOnly)
{
    static const auto ClassID_Morpher = Class_ID(0x17bb6854, 0xa5cba2a3);

    std::vector<Modifier*> morphers;
    std::vector<Modifier*> allModifiers = GetAllModifiers(node, enabledOnly);

    for (Modifier* m : allModifiers) {
        if ((m->ClassID() != ClassID_Morpher) || (!m->IsEnabled() && enabledOnly)) {
            continue;
        }

        morphers.emplace_back(m);
    }

    return morphers;
}

std::string GetNonLocalizedClassName(ClassDesc* classDesc)
{
#if MAX_VERSION_MAJOR >= 24
    return MaxUsd::MaxStringToUsdString(classDesc->NonLocalizedClassName());
#else
    return MaxUsd::MaxStringToUsdString(classDesc->ClassName());
#endif
}

pxr::SdfPath VerifyOrMakeSkelRoot(
    const pxr::UsdStagePtr& usdStage,
    const pxr::SdfPath&     path,
    const bool              autoGenerate)
{
    // Only try to auto-rename to SkelRoot if we're not already a descendant of one.
    // Otherwise, verify that the user tagged it in a sane way.
    if (UsdSkelRoot root = UsdSkelRoot::Find(usdStage->GetPrimAtPath(path))) {
        // Verify that the SkelRoot isn't nested in another SkelRoot.
        // This is necessary because UsdSkel doesn't handle nested skel roots
        // very well currently; this restriction may be loosened in the future.
        if (UsdSkelRoot root2 = UsdSkelRoot::Find(root.GetPrim().GetParent())) {
            Log::Error(
                "The SkelRoot {} is nested inside another "
                "SkelRoot {}. This might cause unexpected behavior.",
                root.GetPath().GetText(),
                root2.GetPath().GetText());
            return SdfPath();
        }

        return root.GetPath();
    }

    if (autoGenerate) {
        // If auto-generating the SkelRoot, find the rootmost UsdGeomXform and turn
        // it into a SkelRoot.
        // XXX: It might be good to also consider model hierarchy here, and not
        // go past our ancestor component when trying to generate the SkelRoot.
        // (Example: in a scene with /World, /World/Char_1, /World/Char_2, we
        // might want SkelRoots to stop at Char_1 and Char_2.) Unfortunately,
        // the current structure precludes us from accessing model hierarchy
        // here.
        if (UsdPrim root = FindRootmostXformOrSkelRoot(usdStage, path)) {
            UsdSkelRoot::Define(usdStage, root.GetPath());
            return root.GetPath();
        }

        if (path.IsRootPrimPath()) {
            Log::Error(
                "The prim {} is a root prim, so it has no ancestors that"
                " that can be converted to a SkelRoot. (USD requires that skinned meshes"
                " and skeletons be encapsulated under a SkelRoot.)",
                path.GetText());
        } else {
            Log::Error(
                "Could not find an ancestor of the prim {} "
                "that can be converted to a SkelRoot. (USD requires that skinned meshes"
                " and skeletons be encapsulated under a SkelRoot.)",
                path.GetText());
        }
    }

    return SdfPath();
}

UsdPrim FindRootmostXformOrSkelRoot(UsdStagePtr stage, const SdfPath& path)
{
    UsdPrim currentPrim = stage->GetPrimAtPath(path);
    UsdPrim rootmost;
    while (currentPrim) {
        if (currentPrim.IsA<UsdGeomXform>()) {
            rootmost = currentPrim;
        } else if (currentPrim.IsA<UsdSkelRoot>()) {
            rootmost = currentPrim;
        }
        currentPrim = currentPrim.GetParent();
    }

    return rootmost;
}

pxr::UsdPrim GetFirstNonInstanceProxyPrimAncestor(const pxr::UsdPrim& prim)
{
    pxr::UsdPrim nonProxyPrim = prim;

    // Loop up ancestors of selected prim until we find a "non-instance proxy" prim
    if (nonProxyPrim.IsInstanceProxy()) {
        while (nonProxyPrim.IsInstanceProxy()) {
            nonProxyPrim = nonProxyPrim.GetParent();
        }
    }

    return nonProxyPrim;
}

pxr::UsdPrim GetPrimOrAncestorWithKind(const pxr::UsdPrim& prim, const pxr::TfToken& kind)
{
    pxr::UsdPrim iterPrim = prim;
    pxr::TfToken primKind;

    while (iterPrim) {
        if (pxr::UsdModelAPI(iterPrim).GetKind(&primKind)
            && pxr::KindRegistry::IsA(primKind, kind)) {
            break;
        }

        iterPrim = iterPrim.GetParent();
    }

    return iterPrim;
}

pxr::SdfLayerRefPtr
CreateOrOverwriteLayer(const pxr::SdfFileFormatConstPtr& fileFormat, const std::string& identifier)
{
    // The layer could already be in memory...(previous version loaded in a stage)
    pxr::SdfLayerRefPtr matLayer = pxr::SdfLayer::Find(identifier);

    if (matLayer) {
        // If so, clear it - it will be overridden.
        matLayer->Clear();
        return matLayer;
    }

    return pxr::SdfLayer::New(fileFormat, identifier);
}

bool SetPrimHiddenFromCA(IParamBlock2* usdCustomAttributePb, pxr::UsdPrim& translatedPrim)
{
    // usd_hidden
    MetaData::ParameterValue hiddenVal;
    bool hasHiddenCA = GetUsdMetaDataValue(usdCustomAttributePb, MetaData::Hidden, 0, hiddenVal);
    if (hasHiddenCA) {
        return translatedPrim.SetHidden(hiddenVal.boolValue);
    }
    return false;
}

bool SetPrimKindFromCA(IParamBlock2* usdCustomAttributePb, pxr::UsdPrim& translatedPrim)
{
    // usd_kind
    MetaData::ParameterValue kindVal;
    bool hasKindCA = GetUsdMetaDataValue(usdCustomAttributePb, MetaData::Kind, 0, kindVal);
    if (hasKindCA && !kindVal.strValue.empty()) {
        std::string sKind(MaxStringToUsdString(kindVal.strValue.c_str()));
        TfToken     kind(sKind);

        if (!KindRegistry::HasKind(kind)) {
            Log::Warn(
                L"Unknown kind={0} detected but will still be exported for object: {1}",
                kindVal.strValue,
                std::wstring(
                    translatedPrim.GetName().GetString().begin(),
                    translatedPrim.GetName().GetString().end()));
        }
        return UsdModelAPI(translatedPrim).SetKind(kind);
    }
    return false;
}

bool SetPrimPurposeFromCA(IParamBlock2* usdCustomAttributePb, UsdPrim& translatedPrim)
{
    // usd_purpose
    MetaData::ParameterValue purposeVal;
    bool hasPurposeCA = GetUsdMetaDataValue(usdCustomAttributePb, MetaData::Purpose, 0, purposeVal);
    if (hasPurposeCA && !purposeVal.strValue.empty()) {
        std::string sPurpose(MaxStringToUsdString(purposeVal.strValue.c_str()));
        TfToken     purpose(sPurpose);
        if (purpose != UsdGeomTokens->default_) {
            UsdAttribute purposeAttr = UsdGeomImageable(translatedPrim).CreatePurposeAttr();
            return purposeAttr.Set(purpose);
        }
    }
    return false;
}

std::vector<ISkin*> GetMaxSkinModifiers(INode* node, bool enabledOnly)
{
    std::vector<ISkin*> skins;
    const auto&         allMods = GetAllModifiers(node, enabledOnly);
    for (const auto& mod : allMods) {
        if (auto* skin = static_cast<ISkin*>(mod->GetInterface(I_SKIN))) {
            skins.emplace_back(skin);
        }
    }
    return skins;
}

// From maxwrapr.cpp
ReferenceTarget* _FindFirstNonXRefTarg(IXRefItem* pXRefItem)
{
    ReferenceTarget* pFirstNonXRefTarg = nullptr;
    while (pXRefItem) {
        pFirstNonXRefTarg = pXRefItem->GetSrcItem(false);
        pXRefItem = nullptr;
        if (pFirstNonXRefTarg) {
            pXRefItem = static_cast<IXRefItem*>(pFirstNonXRefTarg->GetInterface(IID_XREF_ITEM));
        }
    }
    return pFirstNonXRefTarg;
}

// From maxmods.cpp
Object* _GetNonXRefObject(Object* obj)
{
    IXRefItem* pXRefItem
        = obj ? static_cast<IXRefItem*>(obj->GetInterface(IID_XREF_ITEM)) : nullptr;
    Object* pFirstNonXRef = nullptr;
    if (pXRefItem) {
        pFirstNonXRef = dynamic_cast<Object*>(_FindFirstNonXRefTarg(pXRefItem));
    }
    return pFirstNonXRef ? pFirstNonXRef : obj;
}

// Implementation heavily inspired from maxscript $.modifiers[...] (maxmods.cpp / _get_modifier())
std::vector<Modifier*> GetAllModifiers(INode* node, bool enabledOnly)
{
    std::vector<Modifier*> allModifiers;

    SClass_ID       superClass;
    IDerivedObject* derObject;

    // First,  world-space modifiers.
    if ((derObject = node->GetWSMDerivedObject()) != nullptr) {
        for (int i = 0; i < derObject->NumModifiers(); i++) {
            const auto mod = derObject->GetModifier(i);
            if (!mod->IsEnabled() && enabledOnly) {
                continue;
            }
            allModifiers.push_back(mod);
        }
    }

    // Next, get object-space modifiers.
    Object* obj = _GetNonXRefObject(node->GetObjectRef());
    if ((superClass = obj->SuperClassID()) == GEN_DERIVOB_CLASS_ID) {
        while (superClass == GEN_DERIVOB_CLASS_ID) {
            derObject = static_cast<IDerivedObject*>(obj);
            for (int i = 0; i < derObject->NumModifiers(); i++) {
                const auto mod = derObject->GetModifier(i);
                if (!mod->IsEnabled() && enabledOnly) {
                    continue;
                }
                allModifiers.push_back(mod);
            }
            obj = _GetNonXRefObject(derObject->GetObjRef());
            superClass = obj ? obj->SuperClassID() : 0;
        }
    }
    return allModifiers;
}

pxr::GfMatrix4d GetNodeTransform(INode* sourceNode, TimeValue time, bool YUp)
{
    const auto      nodeTransform = sourceNode->GetNodeTM(time);
    pxr::GfMatrix4d objectTransformUsd = MaxUsd::ToUsd(nodeTransform);
    MaxUsd::MathUtils::RoundMatrixValues(objectTransformUsd, std::numeric_limits<float>::digits10);

    if (YUp) {
        MaxUsd::MathUtils::ModifyTransformZToYUp(objectTransformUsd);
    }
    return objectTransformUsd;
}

pxr::GfMatrix4d GetBindTransform(
    MaxUsd::BindTransformElement element,
    INode*                       node,
    ISkin*                       skinMod,
    bool                         YUp,
    bool                         considerObjectOffset)
{
    pxr::GfMatrix4d bindTransformUsd;
    bindTransformUsd.SetIdentity();

    if (!skinMod || !node) {
        return bindTransformUsd;
    }

    Matrix3 nodeBindTransform;

    // GetBoneInitTM and GetSkinInitTM returns the same error value, in case they fail
    switch (element) {
    case BindTransformElement::Bone: {
        if (skinMod->GetBoneInitTM(node, nodeBindTransform) == SKIN_INVALID_NODE_PTR) {
            return bindTransformUsd;
        }
        break;
    }
    case BindTransformElement::Mesh: {
        if (skinMod->GetSkinInitTM(node, nodeBindTransform) == SKIN_INVALID_NODE_PTR) {
            return bindTransformUsd;
        }
        if (!considerObjectOffset) {
            nodeBindTransform = MaxUsd::GetMaxObjectOffsetTransform(node) * nodeBindTransform;
        }
        break;
    }
    default: {
        return bindTransformUsd;
    }
    }

    bindTransformUsd = ToUsd(nodeBindTransform);

    MathUtils::RoundMatrixValues(bindTransformUsd, std::numeric_limits<float>::digits10);
    MathUtils::FixNonUniformScaling(bindTransformUsd);

    if (YUp) {
        MathUtils::ModifyTransformZToYUp(bindTransformUsd);
    }

    return bindTransformUsd;
}

pxr::GfMatrix4d
GetPivotTransform(const pxr::UsdGeomXformable& xformable, const pxr::UsdTimeCode& time)
{
    // We only consider simple pivots, more specialized pivots are not supported (rotate / scale
    // pivots).
    pxr::GfMatrix4d pivotTransform;
    pivotTransform.SetIdentity();

    bool       pivotFound = false;
    bool       pivotInverseFound = false;
    const auto pivotToken = pxr::TfToken("xformOp:translate:pivot");
    const auto pivotInverseToken = pxr::TfToken("!invert!xformOp:translate:pivot");

    bool resetStack = false;
    auto xformOps = xformable.GetOrderedXformOps(&resetStack);

    for (size_t i = 0; i < xformOps.size(); ++i) {
        const pxr::UsdGeomXformOp& xformop = xformOps[i];
        if (xformop.GetOpName() == pivotToken) {
            pivotFound = true;
            pivotTransform = xformop.GetOpTransform(time);
            if (pivotInverseFound) {
                break;
            }
        }
        // Make sure the pivot and its inverse are present.
        // Otherwise we cant use the pivot as a pivot, as it will actually contribute
        // to the fully composed transform.
        else if (xformop.GetOpName() == pivotInverseToken) {
            pivotInverseFound = true;
            if (pivotFound) {
                break;
            }
        }
    }

    if (!pivotFound || !pivotInverseFound) {
        return pivotTransform;
    }
    return pivotTransform;
}

HasDependentSkinProc::HasDependentSkinProc(ReferenceTarget* node) { this->node = node; }

int HasDependentSkinProc::proc(ReferenceMaker* rmaker)
{
    if (rmaker != node) {
        if (auto skin = static_cast<ISkin*>(rmaker->GetInterface(I_SKIN))) {
            if (MaxSDKSupport::IsModifierDeleted(static_cast<Modifier*>(rmaker))) {
                return DEP_ENUM_CONTINUE;
            }
            // Make sure the node is an actual bone. Nodes may have dependent
            // skins for other reasons.
            for (int i = 0; i < skin->GetNumBones(); ++i) {
                if (node == skin->GetBone(i)) {
                    foundSkinsMod.emplace_back(skin);
                    return DEP_ENUM_CONTINUE;
                }
            }
        }
    }
    return DEP_ENUM_CONTINUE;
}

HasDependentMorpherProc::HasDependentMorpherProc(INode* target) { this->node = target; }

int HasDependentMorpherProc::proc(ReferenceMaker* rmaker)
{
    static const auto ClassID_Morpher = Class_ID(0x17bb6854, 0xa5cba2a3);
    auto              rmakerClassId = rmaker->ClassID();

    if (rmaker == node) {
        return DEP_ENUM_CONTINUE;
    }

    if (IsMorpherDependentOnNode(dynamic_cast<INode*>(rmaker), node)) {
        // morpher depends on the node, stop the search
        hasDependentMorpher = true;
        return DEP_ENUM_HALT;
    }

    return DEP_ENUM_CONTINUE;
}

bool HasDependentMorpherProc::IsMorpherDependentOnNode(INode* morpherModifierNode, INode* checkNode)
{
    static const TSTR isMorpherDependentScript = LR"(
		fn isMorpherDependent originalNodeHandle targetNodeHandle = (
			local isDependent = false

			local originalNode = maxOps.getNodeByHandle originalNodeHandle
			local targetNode = maxOps.getNodeByHandle targetNodeHandle

			modi = (getModifierByClass originalNode Morpher)

			if iskindof modi Modifier and IsValidMorpherMod modi do
			(
				local numberOfChannels = (WM3_NumberOfChannels modi)
				for channel = 1 to numberOfChannels do
				(
					numberOfProgressiveMorphers = (WM3_NumberOfProgressiveMorphs modi channel)
					for progressiveMorpher = 1 to numberOfProgressiveMorphers do
					(
						local progMorphNode = (WM3_GetProgressiveMorphNode modi channel progressiveMorpher)
						if progMorphNode != undefined and progMorphNode == targetNode do
						(
							isDependent = true
						)
					)
				)
			)

			return isDependent
		)
		isMorpherDependent )";

    if (!morpherModifierNode || !checkNode) {
        return false;
    }

    FPValue            rvalue;
    std::wstringstream ss;
    ss << getModifierByClassScript << isMorpherDependentScript << morpherModifierNode->GetHandle()
       << L" ";
    ss << checkNode->GetHandle() << L'\n' << L'\0';
    return ExecuteMAXScriptScript(
               ss.str().c_str(), static_cast<MAXScript::ScriptSource>(3), false, &rvalue)
        && rvalue.type == TYPE_BOOL && rvalue.b;
}

int GetSceneObjectCount()
{
    FPValue rvalue;
    BOOL    executeReturn = FALSE;
    executeReturn = ExecuteMAXScriptScript(
        L"objects.count", MAXScript::ScriptSource::NonEmbedded, false, &rvalue);
    return executeReturn ? rvalue.i : 0;
}

std::string
ResolveToken(const std::string& str, const std::string& token, const std::string& replacement)
{
    std::string result = str;
    size_t      pos = result.find(token);
    while (pos != std::string::npos) {
        result.replace(pos, token.length(), replacement);
        pos = result.find(token, pos + replacement.length());
    }
    return result;
}

INodeTab GetReferencingNodes(Object* object)
{
    ULONG handle = 0;
    object->NotifyDependents(FOREVER, (PartID)&handle, REFMSG_GET_NODE_HANDLE);
    INode* firstNode = GetCOREInterface()->GetINodeByHandle(handle);
    if (!firstNode) {
        return {};
    }
    INodeTab nodes;
    IInstanceMgr::GetInstanceMgr()->GetInstances(*firstNode, nodes);
    return nodes;
}

const pxr::GfRange3d
ComputeTotalExtent(const pxr::GfRange3d& extent, const pxr::VtMatrix4dArray& transformList)
{
    pxr::GfRange3d totalExtent;
    if (!extent.IsEmpty()) {
        for (const auto& transform : transformList) {
            auto transformedMin = transform.Transform(extent.GetMin());
            auto transformedMax = transform.Transform(extent.GetMax());

            // After the transform, need to make sure all components are actual the mins/maxs
            pxr::GfVec3d min = { std::min(transformedMin[0], transformedMax[0]),
                                 std::min(transformedMin[1], transformedMax[1]),
                                 std::min(transformedMin[2], transformedMax[2]) };
            pxr::GfVec3d max = { std::max(transformedMin[0], transformedMax[0]),
                                 std::max(transformedMin[1], transformedMax[1]),
                                 std::max(transformedMin[2], transformedMax[2]) };
            totalExtent.ExtendBy({ min, max });
        }
    }

    return totalExtent;
}

} // namespace MAXUSD_NS_DEF
