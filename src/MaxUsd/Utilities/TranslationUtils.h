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

#include <MaxUsd/MaxUSDAPI.h>

#include <pxr/usd/usd/common.h>

#include <MaxUsd.h>
#include <inode.h>
#pragma warning(push)
#pragma warning(disable : 4275) // non dll-interface class 'boost::python::api::object' used as base
                                // for dll-interface struct 'boost::python::detail::list_base'
#include <pxr/usd/usdGeom/primvar.h>
#pragma warning(pop)
#include "TimeUtils.h"

#include <pxr/usd/usd/stageCache.h>
#include <pxr/usd/usdGeom/mesh.h>
#include <pxr/usd/usdUtils/stageCache.h>

// MAXX-63363: VS2019 v142: <experimental/filesystem> is deprecated and superseded by the C++17
// <filesystem> header
#if _MSVC_LANG > 201402L
#include <filesystem>
namespace fs = std::filesystem;
#else
#include <experimental/filesystem>
namespace fs = std::experimental::filesystem;
#endif

class IParamBlock2;
class ISkin;
class Modifier;

static const TSTR getModifierByClassScript = LR"(
	fn getModifierByClass obj modclass = (
		local foundMod = undefined
		local mods = obj.modifiers
		local mCount = mods.count
		if mCount > 0 then (
			for i = 1 to mCount while foundMod == undefined do (
				if classOf mods[i] == modclass then (
					foundMod = mods[i]
				)
			)
		)
		return foundMod
	)
)";

namespace MAXUSD_NS_DEF {

/**
 * \brief Creates a prim, or returns it for edition if already existing.
 * \tparam T The prim's class.
 * \param stage Stage on which to create or fetch the prim.
 * \param primPath The path of the prim.
 * \param token The type of the prim to create.
 * \return The prim.
 */
template <class T>
T FetchOrCreatePrim(const pxr::UsdStagePtr stage, const pxr::SdfPath& primPath, pxr::TfToken token)
{
    // Fetch or create the target prim. In a typical export scenario, a new prim will be defined
    // here.
    T prim = T(stage->GetPrimAtPath(primPath));
    if (!prim) {
        // Using the sdf apis as such instead of UsdStage::Define prim is much faster.
        {
            pxr::SdfChangeBlock changeBlock;
            const auto prim_spec = pxr::SdfCreatePrimInLayer(stage->GetRootLayer(), primPath);
            prim_spec->SetSpecifier(pxr::SdfSpecifierDef);
            prim_spec->SetTypeName(token);
        }
        prim = T(stage->GetPrimAtPath(primPath));
    }
    return prim;
}

/**
 * \brief Checks if a WSM is applied, and if a transform is required to properly represent it
 * on the USD prim exported from the given node. Unless the object is at the identity, we need
 * to bring back the mesh's points back into local space, so that once in the USD hierarchy, they
 * show up at the right location.
 * \param node The node to check for any applied World Space Modifier..
 * \param time The time at which to query the node's object.
 * \return True if a World Space Modifier is applied and the needs to be transformed to local space.
 */
MaxUSDAPI bool WsmRequiresTransformToLocalSpace(INode* node, const TimeValue& time);

/**
 * \brief Computes the Object-Offset Transform for a given node.
 * \param node The 3ds Max node referencing the object for which to get the offset transform.
 * \param time The Max time at which to get the transform.
 * \return The Object-Offset Transform.
 */
MaxUSDAPI Matrix3 GetMaxObjectOffsetTransform(INode* node);

/**
 * \brief Applies a node's object offset transform. Note that for geometric nodes with an active
 * World Space Modifier, the offset wil not be applied, as it is already considered by the points
 * which are now in world space.
 * \param node The node with the object-offset.
 * \param xformable The xformable onto which to apply the transform.
 * \param time The Max time at which to apply the transform.
 */
MaxUSDAPI void
ApplyObjectOffsetTransform(INode* node, pxr::UsdGeomXformable& xformable, const TimeValue& time);

/**
 * \brief Checks whether the given channel id is valid. In Max, there is a limit of 100
 * regular channels, plus hidden channels (MAP_ALPHA, and MAP_SHADING).
 * \param channel The channel Id to check.
 * \return True if the channel is valid, false otherwise.
 */
MaxUSDAPI bool IsValidChannel(int channel);

/**
 * \brief Create's a valid usd identifier (token) from a given string.
 * \param identifier The source identifier.
 * \param validIdentifier The created valid identifier.
 * \return True if the given string was itself valid, false otherwise.
 */
MaxUSDAPI bool GetValidIdentifier(const std::wstring& identifier, std::string& validIdentifier);

/**
 * \brief Returns the dimenstion for a given type.
 * \param type The type for which to get the dimension.
 * \return The dimension.
 */
MaxUSDAPI size_t GetTypeDimension(const pxr::SdfValueTypeName& type);

/**
 * \brief Validates that a USD attribute/primvar's data can be applied onto a given max mesh.
 * \param valueCount Length of the values array.
 * \param indices The data indices.
 * \param maxMesh The Max mesh against which to validate the mapped data.
 * \param interpolation The interpolation used for the attribute. The function will also make sure that
 * this interpolation is valid.
 * \param isIndexed Whether or not an index is used for the data. It would not be enough to only specify
 * an empty index array, as this is one of the things we must validate.
 * \return True if the mapped data can be applied to the mesh, false otherwise.
 */
MaxUSDAPI bool ValidateMappedDataForMesh(
    size_t                valueCount,
    const pxr::VtIntArray indices,
    const MNMesh&         maxMesh,
    const pxr::TfToken&   interpolation,
    bool                  isIndexed);

/**
 * \brief Checks if an attribute has been authored on a given Usd time code.
 * \param att Usd attribute to be checked if it has been authored.
 * \param timeCode Usd time used to check if the attribute has been authored.
 * \return true if the attribute has been authored on the given time code; false otherwise.
 */
MaxUSDAPI bool IsAttributeAuthored(const pxr::UsdAttribute& att, const pxr::UsdTimeCode& timeCode);

/**
 * \brief Attempts to figure out if a given path is a valid absolute path. This does not guarantee that the
 * path will be usable, only that is seems correctly formed. Note that this method considers UNC
 * paths valid absolute paths.
 * \param path The path to check.
 * \return True if the path is valid, false otherwise.
 */
MaxUSDAPI bool IsValidAbsolutePath(const fs::path& path);

/**
 * \brief Populate the INodeTab with all the nodes that can be considered instance of the given node in the USD
 * (including the node itself).
 * \param node The 3ds Max node for which to find all nodes who can be represented as instance of it
 * \param instanceNodes INodeTab in which to add nodes that can be represented as instances
 * \param eligibleNodes Set of nodes that can be used for instancing. If empty, all nodes will be considered eligible.
 * \return true if 2 nodes or more are returned in the INodeTab. (the node itself + another node)
 */
MaxUSDAPI bool FindInstanceableNodes(
    INode*                            node,
    INodeTab&                         instancesNode,
    const std::unordered_set<INode*>& eligibleNodes);

/**
 * \brief Goes down the object reference hierarchy to first object with modifier.
 * If none is found return the base object.
 * \param node node from which to find the first reference object that contain modifier
 * \return the first reference object with modifier applied or the base object.
 */
MaxUSDAPI Object* GetFirstDerivedObjectWithModifier(INode* node);

/**
 * \brief Utility class to ensure every name is unique.
 * To use, create an instance and call the GetName function with the desired name.
 * The instance will keep track of the used name and return a name with a numbered postfix if the
 *name is already used.
 **/
class UniqueNameGenerator
{
public:
    MaxUSDAPI std::string GetName(const std::string& name);
    MaxUSDAPI void        Reset();

private:
    std::unordered_set<std::string> existingNames;
    std::string                     GetNextName(const std::string& name);
};

/**
 * \brief RAII Scope guard for adding and remove a stage from the global stage cache.
 */
class StageCacheScopeGuard
{
public:
    /**
     * \brief Constructor, adds a stage to the global stage cache.
     * \param stage The stage.
     */
    StageCacheScopeGuard(const pxr::UsdStageRefPtr& stage)
    {
        // If the stage was already present in the global cache, we don't need to add it,
        // and we should not remove it in the destructor either. We want to leave the cache
        // in the same state we found it.
        auto& cache = pxr::UsdUtilsStageCache::Get();
        if (!cache.Contains(cache.GetId(stage))) {
            id = cache.Insert(stage);
        }
    }
    /**
     * \brief Destructor, removes the stage from the global stage cache.
     */
    ~StageCacheScopeGuard()
    {
        if (id.IsValid()) {
            pxr::UsdUtilsStageCache::Get().Erase(id);
        }
    }

private:
    pxr::UsdStageCache::Id id;
};

/**
 * \brief Returns the corresponding USD timecode to a max frame based on FPS ratios.
 * \param stage The stage from which to get the USD FPS.
 * \param maxFrame The max frame to be converted.
 * \return The converted max frame into USD time code based on FPS ratios.
 */
MaxUSDAPI pxr::UsdTimeCode
          GetUsdTimeCodeFromMaxFrame(const pxr::UsdStageWeakPtr& stage, const double maxFrame);

/**
 * \brief Returns the corresponding max frame to a USD timecode based on FPS ratios.
 * \param stage The stage from which to get the USD FPS.
 * \param usdTimeCode The USD time code to be converted.
 * \return The converted USD time code into max frame based on FPS ratios.
 */
MaxUSDAPI double
GetMaxFrameFromUsdTimeCode(const pxr::UsdStageWeakPtr& stage, const pxr::UsdTimeCode usdTimeCode);

/**
 * \brief Returns the corresponding max TimeValue from a USD TimeCode based on FPS ratios.
 * \param stage The stage from which to get the USD FPS.
 * \param usdTimeCode The USD time code to be converted.
 * \return The converted USD time code into max TimeValue based on FPS ratios.
 */
MaxUSDAPI TimeValue GetMaxTimeValueFromUsdTimeCode(
    const pxr::UsdStageWeakPtr& stage,
    const pxr::UsdTimeCode      usdTimeCode);

/**
 * \brief Returns the corresponding max frame to a USD timecode based on FPS ratios.
 * \param stage The stage from which to get the USD FPS.
 * \param usdTimeCode The USD time code to be convereted.
 * \return The converted USD time code into max frame based on FPS ratios.
 */
MaxUSDAPI float
GetMaxFrameFromUsdFrameTime(const pxr::UsdStageWeakPtr& stage, const pxr::UsdTimeCode usdFrame);

/**
 * \brief Returns the corresponding USD timecode from a Max time value.
 * \param stage The stage for which to get the time code (the definition of the USD time unit
 * is configurable per-stage).
 * \param timeValue The Max time value to convert.
 * \return The USD time code.
 */
MaxUSDAPI pxr::UsdTimeCode
          GetUsdTimeCodeFromMaxTime(const pxr::UsdStageWeakPtr& stage, const TimeValue& timeValue);

/**
 * \brief Returns the corresponding offset USD timecode from a Max time value, (custom) animation start frame
 * and optional custom animation length.
 * \param stage The stage for which to get the time code (the definition of the USD time unit
 * is configurable per-stage).
 * \param timeValue The Max time value to convert.
 * \param customAnimStartFrame The start frame of the animation.
 * \param customAnimationLength The length of the animation in frames.
 * \return The USD time code.
 */
MaxUSDAPI pxr::UsdTimeCode GetOffsetTimeCode(
    const pxr::UsdStageWeakPtr& stage,
    const TimeValue&            timeValue,
    const double                customAnimStartFrame,
    const double                customAnimationLength = 0);

/**
 * \brief Returns the USD timecode equivalent to the current time in 3dsMax.
 * \return The current USD timecode.
 */
MaxUSDAPI pxr::UsdTimeCode GetCurrentUsdTimeCode(const pxr::UsdStageWeakPtr& stage);

/**
 * \brief Gets the scaling factor from the given USD stage to the Max world.
 * \param stage The stage to scale from.
 * \return The scale factor from USD to Max.
 */
MaxUSDAPI double GetUsdToMaxScaleFactor(const pxr::UsdStageWeakPtr& stage);

/**
 * \brief Checks if the stage is using a 'Y' up axis. Helper method required to fix possible bad data
 * in the stage where the up axis is defined with lowercase characters (not valid comparison token).
 * \param stage The stage to for which the 'Y' up axis is verified.
 * \return True if the stage is using a 'Y' up axis, False otherwise.
 */
MaxUSDAPI bool IsStageUsingYUpAxis(const pxr::UsdStageWeakPtr& stage);

/**
 * \brief Returns the root transform we need to give a stage to adjust for a different
 * up-axis or unit setup.
 * \param stage The stage to get the root transform for.
 * \return The transform to adjust for the axis and units of the stage.
 */
MaxUSDAPI pxr::GfMatrix4d GetStageAxisAndUnitRootTransform(pxr::UsdStageWeakPtr stage);

/**
 * \brief Get the parameter Id for a given param name on a given ParamBlock
 * \param pb the param block for which to find the given parameter
 * \param name the name of the parameter for which to find the paramId
 * \return the parameter Id or -1 if the parameter is not found.
 */
MaxUSDAPI short FindParamId(IParamBlock2* pb2, const wchar_t* name);

/**
 * \brief Converts 3dsMax utf16 encoded wide string into a utf8 encoded std::string for USD
 * \param utf16EncodedWideString wide string coming from 3dsMax, expected to be utf16 encoded
 * \return utf8 encoded std::string
 */
MaxUSDAPI std::string MaxStringToUsdString(const wchar_t* utf16EncodedWideString);

/**
 * \brief Converts usd utf8 encoded string into a lowercase version of itself
 * \param utf8EncodedString utf8 encoding string
 * \return utf8EncodedString utf8 encoding string in lowercase
 */
MaxUSDAPI std::string UsdStringToLower(std::string utf8EncodedString);

/**
 * \brief Converts usd utf8 encoded string into a utf16 encoded wide string for 3dsMax
 * \param utf8EncodedString utf8 encoding string
 * \return utf16 encoded WStr
 */
MaxUSDAPI WStr UsdStringToMaxString(const std::string& utf8EncodedString);

/**
 * \brief Detects whether an std::string contains non-ascii characters.
 * \return true if a non-ascii character is detected.
 */
MaxUSDAPI bool HasUnicodeCharacter(const std::string& str);

/**
 * \brief Convert a 3ds Max object to USD, over a specified frame range, while respecting the object's validity
 * interval.
 * \param node The node carrying the object to convert.
 * \param convertFrame The conversion function to be used to convert a single frame.
 * \param usdTimeConfig The time configuration, specifying the frames we want converted.
 * \param objectValidityOverride An optional object validity evaluation; function returns the evaluated object validity
 * interval The default method returns a FOREVER validity interval
 */
MaxUSDAPI void ConvertFrames(
    INode*                                                                  node,
    std::function<void(Object*, const TimeValue&, const pxr::UsdTimeCode&)> convertFrame,
    const TimeConfig&                                                       usdTimeConfig,
    std::function<Interval(Object*, const TimeValue&)>                      objectValidityOverride
    = [](Object*, const TimeValue&) { return FOREVER; });

/**
 * \brief Returns a pair of vectors, specifying the 3dsMax timeValues and USD timeCodes we are exporting from
 * and to respectively, computed from the validity interval of the object referenced by the given
 * node. These are essentially "key frames" on the USD side. The export timeConfig is considered as
 * well (for example would not return frames outside of the supplied time range.)
 * \param node The 3dsMax node for which to get frames.
 * \param usdTimeConfig  The export time config.
 * \return A pair of arrays, the 3dsMax time values and associated USD time codes.
 */
MaxUSDAPI std::pair<pxr::VtArray<TimeValue>, pxr::VtArray<pxr::UsdTimeCode>>
          GetFramesFromValidityInterval(INode* node, const TimeConfig& usdTimeConfig);

/**
 * \brief Converts the 3ds Max time configuration returning the equivalent Usd TimeCode samples.
 * \param stage the usd stage for the 3ds Max time to Usd TimeCode conversion
 * \param timeConfig 3ds Max time configuration to get the time interval for the conversion
 * \return An array of UsdTimeCode with all UsdTimeCode in the interval
 */
MaxUSDAPI std::vector<pxr::UsdTimeCode>
GetUsdTimeSamplesForExport(const pxr::UsdStageWeakPtr& stage, const MaxUsd::TimeConfig& timeConfig);

/**
 * \brief Checks whether or not an object is a Bone.
 * \param object The object to check.
 * \return True if the object is a bone, false otherwise.
 */
MaxUSDAPI bool IsBoneObject(Object* object);

/**
 * \brief Returns the non-localized class name.
 * \param classDesc The class description from which to resolve the class name.
 * \return The non-localized class name
 */
MaxUSDAPI std::string GetNonLocalizedClassName(ClassDesc* classDesc);

/**
 * \brief Gets the rootmost node on the stage and attempts to transform it into a SkelRoot node.
 * Adapted from maya usd:
 * https://git.autodesk.com/maya3d/maya-usd/blob/dev/lib/mayaUsd/fileio/translators/skelBindingsProcessor.cpp
 * \param usdStage the stage to look for the SkelRoot prim
 * \param path The prim path to start the search
 * \param autoGenerate if true, this function will create a SkelRoot prim as the rootmost prim
 */
MaxUSDAPI pxr::SdfPath VerifyOrMakeSkelRoot(
    const pxr::UsdStagePtr& usdStage,
    const pxr::SdfPath&     path,
    const bool              autoGenerate = true);

/**
 * \brief Returns the first non-instance proxy ancestor prim of the given prim.
 * \param prim The prim to search from.
 * \return The first non-instance ancestor prim.
 */
MaxUSDAPI pxr::UsdPrim GetFirstNonInstanceProxyPrimAncestor(const pxr::UsdPrim& prim);

/**
 * \brief Returns the closes ancestor that is of the given kind, the prim itself included.
 * If no prim in the hierarchy matches the kind, returns an invalid prim.
 * \param prim The prim to search from.
 * \param kind The kind we are searching for.
 * \return The closest ancestor matching the kind (prim included).
 */
MaxUSDAPI pxr::UsdPrim
          GetPrimOrAncestorWithKind(const pxr::UsdPrim& prim, const pxr::TfToken& kind);

/**
 * \brief returns the path for the prim based on the given path and token.
 * If another prim already existed on the path, the conflict will be solved and a new prim will
 * be created.
 * \tparam T Prim type that will be used to create the prim if one doesn't exist. The class type
 * must define the function Define(stage,path).
 * \param usdStage USD stage to test the path on.
 * \param basePath Path to add the new prim as a child.
 * \return The newly created prim of the given template type.
 */
template <class T>
T VerifyOrMakePrimOfType(
    const pxr::UsdStagePtr& usdStage,
    const pxr::SdfPath&     basePath,
    const pxr::TfToken&     primName)
{
    UniqueNameGenerator subsetNameGenerator;
    std::string         currentPrimString = subsetNameGenerator.GetName(primName.GetString());
    pxr::SdfPath        currentPath = basePath.AppendElementString(currentPrimString);

    pxr::UsdPrim prim = usdStage->GetPrimAtPath(currentPath);
    while (prim && !prim.IsA<T>()) {
        currentPrimString = subsetNameGenerator.GetName(currentPrimString);
        currentPath = basePath.AppendElementString(currentPrimString);
        prim = usdStage->GetPrimAtPath(currentPath);
    }

    // if prim doesn't exist, create one and return
    if (!prim) {
        return T::Define(usdStage, currentPath);
    }

    return T(prim);
}

/**
 * \brief Create a path at basePath/primName, but with a uniqueness check. If a prim already exists at
 * that path, a number is added or incremented on the prim's name.
 * \param usdStage The stage in which we are creating the prim.
 * \param basePath The parent path of the prim we are creating.
 * \param primName The prim name (to be ensured unique).
 * \return The created unique path.
 */
inline pxr::SdfPath MakeUniquePrimPath(
    const pxr::UsdStagePtr& usdStage,
    const pxr::SdfPath&     basePath,
    const pxr::TfToken&     primName)
{
    UniqueNameGenerator subsetNameGenerator;
    std::string         currentPrimString = subsetNameGenerator.GetName(primName.GetString());
    pxr::SdfPath        currentPath = basePath.AppendElementString(currentPrimString);

    pxr::UsdPrim prim = usdStage->GetPrimAtPath(currentPath);
    while (prim) {
        currentPrimString = subsetNameGenerator.GetName(currentPrimString);
        currentPath = basePath.AppendElementString(currentPrimString);
        prim = usdStage->GetPrimAtPath(currentPath);
    }

    return currentPath;
}

/**
 * \brief Create a prim at basePath/primName, but with a uniqueness check. If a prim already exists at
 * that path, a number is added or incremented on the prim's name.
 * \tparam T The type of prim to create.
 * \param usdStage The stage in which we are creating the prim.
 * \param basePath The parent path of the prim we are creating.
 * \param primName The prim name (to be ensured unique).
 * \return The created prim.
 */
template <class T>
T MakeUniquePrimOfType(
    const pxr::UsdStagePtr& usdStage,
    const pxr::SdfPath&     basePath,
    const pxr::TfToken&     primName)
{
    pxr::SdfPath uniquePath = MakeUniquePrimPath(usdStage, basePath, primName);
    return T::Define(usdStage, uniquePath);
}

/**
 * \brief Looks for the rootmost Xform os SkelRoot prim on a given stage.
 * Adapted from maya usd
 * https://git.autodesk.com/maya3d/maya-usd/blob/dev/lib/mayaUsd/fileio/translators/skelBindingsProcessor.cpp
 * \param usdStage Stage to look for the root most Xform or SkelRoot prim
 * \param Path to where start the search
 * \return The found Xform or SkelRoot prim; returns a default prim if one can't be found
 */
MaxUSDAPI pxr::UsdPrim
          FindRootmostXformOrSkelRoot(pxr::UsdStagePtr usdStage, const pxr::SdfPath& path);

/**
 * \brief Creates a new layer in memory given the identifier and file format. If a layer already
 * exists with this identifier, it is cleared, and returned.
 * \param fileFormat The file format for the layer.
 * \param identifier The identifier for the layer.
 * \return The created or overwritten layer.
 */
MaxUSDAPI pxr::SdfLayerRefPtr
CreateOrOverwriteLayer(const pxr::SdfFileFormatConstPtr& fileFormat, const std::string& identifier);

/**
 * \brief Sets the usd Hidden metadata attribute on the given prim based on the given ParamBlock information.
 * \param usdCustomAttributePb ParamBlock containing the prim hidden data
 * \param translatedPrim prim where the metadata data should be set on
 * \return true if the Hidden metadata was set; false otherwise
 */
MaxUSDAPI bool
SetPrimHiddenFromCA(IParamBlock2* usdCustomAttributePb, pxr::UsdPrim& translatedPrim);

/**
 * \brief Sets the prim kind based on the ParamBlock information
 * \param usdCustomAttributePb ParamBlock with the prim's kind data
 * \param translatedPrim Prim to which the kind should be set on
 * \return true if the Kind metadata was set; false otherwise
 */
MaxUSDAPI bool SetPrimKindFromCA(IParamBlock2* usdCustomAttributePb, pxr::UsdPrim& translatedPrim);

/**
 * \brief Sets the prim Purpose based on the ParamBlock information
 * \param usdCustomAttributePb ParamBlock with the prim's purpose data
 * \param translatedPrim prim to set the Purpose metadata on
 * \return true if the Purpose metadata was set; false otherwise
 */
MaxUSDAPI bool
SetPrimPurposeFromCA(IParamBlock2* usdCustomAttributePb, pxr::UsdPrim& translatedPrim);

/**
 * \brief Gets all Max Skin modifiers interfaces from a given Max node.
 * \param node Max node to look for the skin modifier
 * \param enabledOnly If true, only return enabled skins.
 * \return vector containing all Max skin modifiers found
 */
MaxUSDAPI std::vector<ISkin*> GetMaxSkinModifiers(INode* node, bool enabledOnly = true);

/**
 * \brief Gets all Max Morphers modifiers interfaces from a given Max node.
 * \param node Max node to look for Morpher modifiers.
 * \param enabledOnly If true, only return enabled morphers.
 * \return vector containing all Max Morphers modifiers found
 */
MaxUSDAPI std::vector<Modifier*> GetMaxMorpherModifiers(INode* node, bool enabledOnly = true);

/**
 * \brief Gets all modifiers applied on a node.
 * \param node The node to get the modifiers from.
 * \param enabledOnly If true, only return modifiers which are currently enabled.
 * \return The modifiers.
 */
MaxUSDAPI std::vector<Modifier*> GetAllModifiers(INode* node, bool enabledOnly = true);

/**
 * \brief Simple struct to define a Prim, from its path and type.
 */
struct PrimDef
{
    pxr::SdfPath path;
    pxr::TfToken type;
};
typedef std::vector<MaxUsd::PrimDef>   PrimDefVector;
typedef std::shared_ptr<PrimDefVector> PrimDefVectorPtr;

/**
 * \brief When exporting 3dsMax nodes to USD Prims, we sometimes need use a separate
 * Xform prim to encode the node's transform (so one Xform prim + another prim for the object
 * itself). There are several scenarios where this is the case - a simple case is when
 * an offset is applied onto the node's object. Indeed, object offsets should not be inherited,
 * so we can't possibly use a single prim for the node (if we did, and the prim had children,
 * the object offset would be inherited, which is a problem). This enum encodes the requirement
 * to split a 3dsMax node into 2 prims (xform + object prim) from the object's perspective. Note
 * that even is "never" is returned, the xform may still be forced, for example if the object is
 * instanced.
 */
enum class XformSplitRequirement
{
    /// Always require a xform prim + object prim.
    Always,
    /// Requires a xform prim only if we had a non-identity object offset.
    ForOffsetObjects,
    /// Never require to generate an extra xform (at least for the object translation itself, other
    /// things may come into play, like instancing, which may eventually force an Xform)
    Never,
};

enum class BindTransformElement
{
    Mesh,
    Bone
};

/**
 * \brief When exporting a 3dsMax node to USD, this represent the need for a prim exported from a
 * 3dsMax node to be assigned a material.
 */
enum class MaterialAssignRequirement
{
    // Default material assignment.
    Default,
    // No material assignment. No material necessary, or material assignment handled differently.
    NoAssignment
};

/**
 * \brief When exporting a 3dsMax node to USD, this represent the need for instancing to be automatically
 * handled.
 */
enum class InstancingRequirement
{
    // Instancing handled automatically. Only the first instance will hit the Write() method.
    Default,
    // Instancing is not handled automatically, it is left to the prim writer to decided how to
    // handle instancing, or not..
    NoInstancing
};

/**
 * \brief Gets the a 3dsmax node transform as Pixar Usd transform representation.
 * \param sourceNode max node to get the transform from.
 * \param time the time to evaluate the sourceNode transform at.
 * \param YUp converts the sourceNode transform to use YUp axis.
 */
MaxUSDAPI pxr::GfMatrix4d GetNodeTransform(INode* sourceNode, TimeValue time, bool YUp);

/**
 * \brief Gets the mesh or bone transform used when bound to a skin modifier.
 * \param element Defines if the function is getting a mesh or a bone bind transform
 * \param node Max node containing the mesh or bone to be getting the transform from
 * \param skinMod the skin modifier that cached the bind transform for the given node
 * \param YUp if we should consider y or z axis to be the up
 * \param considerObjectOffset if true, the object offset will be baked into the geometry
 * \return the bind transform cached on the skin modifier for the passed node
 */
MaxUSDAPI pxr::GfMatrix4d GetBindTransform(
    BindTransformElement element,
    INode*               node,
    ISkin*               skinMod,
    bool                 YUp,
    bool                 considerObjectOffset);

/**
 * \brief Returns the translate pivot defined on the xformable's xformOp stack.
 * \param xformable The xformable to search in.
 * \param time Time at which we should fetch the pivot.
 * \return The translate pivot transform (identity matrix if no usable pivot was found).
 */
MaxUSDAPI pxr::GfMatrix4d
          GetPivotTransform(const pxr::UsdGeomXformable& xformable, const pxr::UsdTimeCode& time);

/**
 * \brief Callback helper class for enumerating Max node dependents with skin modifiers.
 * This class is a callback object for the ReferenceMaker::DoEnumDependentsImpl()
 * and ReferenceMaker::DoEnumDependents() methods. The proc() method is called by the system.
 */
class HasDependentSkinProc : public DependentEnumProc
{
public:
    ReferenceTarget*    node = nullptr;
    std::vector<ISkin*> foundSkinsMod;

    MaxUSDAPI HasDependentSkinProc(ReferenceTarget* target);

    MaxUSDAPI int proc(ReferenceMaker* rmaker) override;
};

class HasDependentMorpherProc : public DependentEnumProc
{
public:
    INode* node = nullptr;
    bool   hasDependentMorpher = false;

    MaxUSDAPI HasDependentMorpherProc(INode* target);

    MaxUSDAPI int proc(ReferenceMaker* rmaker) override;

private:
    bool IsMorpherDependentOnNode(INode* morpherModifierNode, INode* checkNode);
};

/**
 * \brief Returns the number of object currently in the 3dsmax Scene.
 * \return The object count.
 */
MaxUSDAPI int GetSceneObjectCount();

/**
 * \brief Resolves a token in a string, every occurrence of the token in the string will be replaced by the provided string.
 * \param str The string to resolve.
 * \param token The token to replace.
 * \param replacement The replacement string.
 * \return The resolved string.
 * */
MaxUSDAPI std::string
ResolveToken(const std::string& str, const std::string& token, const std::string& replacement);

/**
 * \brief Gets all nodes referencing a given object (more than one if instanced)
 * \param object The object for which to get the nodes
 * \return The nodes referencing the object.
 */
MaxUSDAPI INodeTab GetReferencingNodes(Object* object);

/**
 * \brief Computes the total extent of an object, taking into account all the transforms in the transform list.
 * \param extent The object's extent.
 * \param transformList The list of transforms to apply.
 * \return The total extent.
 */
MaxUSDAPI const pxr::GfRange3d
ComputeTotalExtent(const pxr::GfRange3d& extent, const pxr::VtMatrix4dArray& transformList);

} // namespace MAXUSD_NS_DEF
