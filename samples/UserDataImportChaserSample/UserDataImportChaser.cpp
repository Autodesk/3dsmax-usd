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
#include "CAMetaDataUtils.h"

#include <MaxUsd/Builders/JobContextRegistry.h>
#include <MaxUsd/Builders/SceneBuilderOptions.h>
#include <MaxUsd/Chaser/ImportChaser.h>
#include <MaxUsd/Chaser/ImportChaserRegistry.h>
#include <MaxUsd/MaxTokens.h>
#include <MaxUsd/Translators/ReadJobContext.h>
#include <MaxUsd/Translators/WriteJobContext.h>
#include <MaxUsd/Utilities/TranslationUtils.h>

#include <maxscript/mxsplugin/mxsCustomAttributes.h>

#include <boost/tokenizer.hpp>

PXR_NAMESPACE_USING_DIRECTIVE

enum UserDataImportChaserCodes
{
    UnableToAddMetadata,
    UnavailableParamBlock
};
TF_REGISTRY_FUNCTION(TfEnum)
{
    TF_ADD_ENUM_NAME(UnableToAddMetadata, "Unable to add USD Metadata to object.");
    TF_ADD_ENUM_NAME(
        UnavailableParamBlock,
        "Unable to get ParamBlock2 for IMetaDataManager/CustAttrib for object.");
};

enum class PropertyType : int
{
    USER_PROP = 1,  // "user"
    CUSTOM_DATA = 2 // "custom"
};

// The UserDataImportChaserSample class
// This Import Chaser sample reads customData from the imported prims and adds
// user defined properties or custom attributes to the corresponding node
// depending on the arguments. The sample chaser acts with default data or using
// the arguments that may have been passed to the chaser.
class UserDataImportChaserSample : public pxr::MaxUsdImportChaser
{
public:
    // Constructor - can be customized for the Import Chaser requirements.
    UserDataImportChaserSample(
        Usd_PrimFlagsPredicate&     returnPredicate,
        const MaxUsdReadJobContext& context,
        const fs::path&             filename);

    // Processing that needs to run after the main 3ds Max USD import loop.
    bool PostImport() override;

private:
    const MaxUsdReadJobContext context;
    const fs::path&            filename;

    typedef std::map<PropertyType, std::set<std::string>> PropertyMap;
    PropertyMap                                           dataToImport;
};

UserDataImportChaserSample::UserDataImportChaserSample(
    Usd_PrimFlagsPredicate&     returnPredicate,
    const MaxUsdReadJobContext& context,
    const fs::path&             filename)
    : context(context)
    , filename(filename)
{
    // Default configuration
    dataToImport
        = { { PropertyType::USER_PROP, { "myUserProperty" } }, { PropertyType::CUSTOM_DATA, {} } };

    // Parsing the specific import chaser arguments
    boost::char_separator<char> sep(",");

    for (const auto& args : context.GetArgs().GetAllChaserArgs()) {
        for (const auto& item : args.second) {
            if (item.first == "user") {
                dataToImport[PropertyType::USER_PROP].clear();
                boost::tokenizer<boost::char_separator<char>> tokens(item.second, sep);
                for (const auto& name : tokens) {
                    dataToImport[PropertyType::USER_PROP].insert(name);
                }
            } else if (item.first == "custom") {
                dataToImport[PropertyType::CUSTOM_DATA].clear();
                boost::tokenizer<boost::char_separator<char>> tokens(item.second, sep);
                for (const auto& name : tokens) {
                    dataToImport[PropertyType::CUSTOM_DATA].insert(name);
                }
            } else {
                TF_WARN(
                    "Wrong user data type ('%s') passed as argument to UserPropertyImportChaser",
                    item.first.c_str());
            }
        }
    }
}

bool UserDataImportChaserSample::PostImport()
{
    // Makes a prims to nodes map
    std::map<std::string, INode*> primsToNodes;
    {
        const auto& nodeMap = context.GetReferenceTargetRegistry();
        for (const std::pair<pxr::SdfPath, RefTargetHandle>& item : nodeMap) {
            INode* node = dynamic_cast<INode*>(item.second);
            if (node) {
                primsToNodes.insert({ item.first.GetString(), node });
            }
        }
    }

    // Go through the imported nodes / prims
    for (const auto& imported : primsToNodes) {
        pxr::UsdPrim prim = context.GetStage()->GetPrimAtPath(pxr::SdfPath(imported.first));
        INode*       node = imported.second;

        // User properties
        // Find and add the specified user property to the node as a user defined property.
        for (const auto& propName : dataToImport[PropertyType::USER_PROP]) {
            VtValue customData = prim.GetCustomDataByKey(pxr::TfToken(propName));
            if (!customData.IsEmpty()) {
                if (customData.GetTypeName() == "bool") {
                    node->SetUserPropBool(
                        MaxUsd::UsdStringToMaxString(propName).data(), customData.Get<bool>());
                } else if (customData.GetTypeName() == "int") {
                    node->SetUserPropInt(
                        MaxUsd::UsdStringToMaxString(propName).data(), customData.Get<int>());
                } else if (customData.GetTypeName() == "float") {
                    node->SetUserPropFloat(
                        MaxUsd::UsdStringToMaxString(propName).data(), customData.Get<float>());
                } else if (customData.GetTypeName() == "double") {
                    node->SetUserPropFloat(
                        MaxUsd::UsdStringToMaxString(propName).data(),
                        static_cast<float>(customData.Get<double>()));
                } else {
                    node->SetUserPropString(
                        MaxUsd::UsdStringToMaxString(propName).data(),
                        MaxUsd::UsdStringToMaxString(customData.Get<std::string>()));
                }
            }
        }

        // Custom Attributes
        IMetaDataManager* maxMetaDataManager = IMetaDataManager::GetInstance();

        // Contains the usd metadata names for defining our CustomAttributes and also the value to
        // set for each parameter
        std::vector<std::pair<caMetaData::UsdMetaDataTypeCA, caMetaData::ParameterValue>>
            customAttributesToAddAndUpdate;

        // Finds and adds the custom data property to the node as a custom attribute
        for (const auto& propName : dataToImport[PropertyType::CUSTOM_DATA]) {
            VtValue customData = prim.GetCustomDataByKey(pxr::TfToken(propName));
            if (!customData.IsEmpty()) {
                caMetaData::ParameterValue val;
                val.key = MaxUsd::UsdStringToMaxString(propName).data();

                if (customData.GetTypeName() == "bool") {
                    val.boolValue = customData.Get<bool>();
                    customAttributesToAddAndUpdate.push_back(
                        std::make_pair(caMetaData::UsdMetaDataTypeCA::CA_BOOL, val));
                } else if (customData.GetTypeName() == "int") {
                    val.intValue = customData.Get<int>();
                    customAttributesToAddAndUpdate.push_back(
                        std::make_pair(caMetaData::UsdMetaDataTypeCA::CA_INT, val));
                } else {
                    val.strValue
                        = MaxUsd::UsdStringToMaxString(customData.Get<std::string>()).data();
                    customAttributesToAddAndUpdate.push_back(
                        std::make_pair(caMetaData::UsdMetaDataTypeCA::CA_STR, val));
                }
            }
        }

        if (!customAttributesToAddAndUpdate.empty()) {
            IMetaDataManager::MetaDataID usdBuiltInMetaData
                = caMetaData::getOrDefineCABuiltInMetaData(customAttributesToAddAndUpdate);

            if (usdBuiltInMetaData != EmptyMetaDataID) {
                CustAttrib* usdCustomAttribute = maxMetaDataManager->AddMetaDataToAnimatable(
                    usdBuiltInMetaData, *(node->GetObjectRef()));
                if (usdCustomAttribute == nullptr) {
                    TF_ERROR(
                        UnableToAddMetadata,
                        "object '%s'",
                        MaxUsd::MaxStringToUsdString(node->GetObjectRef()->GetObjectName(false))
                            .c_str());
                    return false;
                }

                IParamBlock2* usdCustomAttributePb = usdCustomAttribute->GetParamBlock(0);
                if (usdCustomAttributePb == nullptr) {
                    TF_ERROR(
                        UnavailableParamBlock,
                        "object `%s",
                        MaxUsd::MaxStringToUsdString(node->GetObjectRef()->GetObjectName(false))
                            .c_str());
                    return false;
                }

                for (const auto& prop : customAttributesToAddAndUpdate) {
                    const auto& def = getCAMetaDataDef(prop.first, prop.second.key);
                    auto        val = prop.second;
                    const auto  stage = context.GetStage();
                    const auto  timeConfig = context.GetArgs().GetResolvedTimeConfig(stage);
                    const auto  startTime = MaxUsd::GetMaxTimeValueFromUsdTimeCode(
                        stage, timeConfig.GetStartTimeCode());
                    switch (def.usdMetaDataParamDef.m_dataType) {
                    case TYPE_STRING:
                        usdCustomAttributePb->SetValueByName(
                            def.usdMetaDataKey, val.strValue.c_str(), startTime);
                        break;
                    case TYPE_BOOL:
                        usdCustomAttributePb->SetValueByName(
                            def.usdMetaDataKey, val.boolValue, startTime);
                        break;
                    case TYPE_INT:
                        usdCustomAttributePb->SetValueByName(
                            def.usdMetaDataKey, val.intValue, startTime);
                        break;
                    default:
                        // Unhandled type that was not implemented
                        DbgAssert(_T("Unhandled customattribute data type"));
                        break;
                    }
                }
            }
        }
    }

    return true;
}

// clang-format off
REGISTER_IMPORT_JOB_CONTEXT_FCT(CustomImportContext, "Custom C++ Import Context", "Custom import plug-in configuration")
{
	// The following arguments are based on the variables contained in the 
	// custom data of the prims found in: ./SceneFiles/UserDataChaserSample.usda
	VtDictionary extraArgs;
	extraArgs[MaxUsdSceneBuilderOptionsTokens->chaserNames] = VtValue(std::vector<VtValue> { VtValue(std::string("UserDataImport")) } );
	VtValue chaserArgUserProp(std::vector<VtValue> {
									VtValue(std::string("UserDataImport")),
									VtValue(std::string("user")),
									VtValue(std::string("myUserFloatProperty,myUserProperty")) });
	VtValue chaserArgCustomProp(std::vector<VtValue> {
									VtValue(std::string("UserDataImport")),
									VtValue(std::string("custom")),
									VtValue(std::string("inGame,strVal")) });
	extraArgs[MaxUsdSceneBuilderOptionsTokens->chaserArgs] = VtValue(std::vector<VtValue>{ chaserArgUserProp, chaserArgCustomProp });
	
	return extraArgs;
}


// Macro to register the Import Chaser. Defines a factory method for the chaser name. The 'ctx' will
// be of type MaxUsdImportChaserRegistry::FactoryContext. The method should return a MaxUsdImportChaser*.
// There are no guarantees about the lifetime of 'ctx'.
// It is also very important to set the project option "Remove unreferenced code and data" to NO,
// not doing so could cause the Macro to be optimized out and the Import Chaser to never be properly registered.
PXR_MAXUSD_DEFINE_IMPORT_CHASER_FACTORY(UserDataImport, "Import Chaser C++ DEMO", "Custom plug-in configuration", ctx)
{
	return new UserDataImportChaserSample(
		*(new Usd_PrimFlagsPredicate()), 
		ctx.GetContext(), 
		ctx.GetFilename()
	);
}
// clang-format on