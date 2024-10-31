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

#include "ExportOptionsDialog.h"

#include <MaxUsd/Builders/JobContextRegistry.h>
#include <MaxUsd/Builders/SceneBuilderOptions.h>
#include <MaxUsd/Builders/USDSceneBuilderOptions.h>
#include <MaxUsd/Chaser/ExportChaser.h>
#include <MaxUsd/Chaser/ExportChaserRegistry.h>

#include <maxscript/mxsplugin/mxsCustomAttributes.h>

#include <boost/tokenizer.hpp>
#include <custattrib.h>
#include <icustattribcontainer.h>
#include <object.h>
#include <string.h>
using namespace std::string_literals;

// Data types supported by the UserDataExportChaserSample
enum class PropertyType : int
{
    USER_PROP = 1,  // "user"
    CUSTOM_DATA = 2 // "custom"
};

// The UserDataExportChaserSample class
// This Export Chaser sample adds Custom Data to the exported prims if the specified user
// property or custom attribute are found in the corresponding Node. The sample chaser
// acts with default data or using the arguments that may have been passed to the chaser.
class UserDataExportChaserSample : public pxr::MaxUsdExportChaser
{
public:
    // Constructor - can be customized for the Export Chaser requirements.
    // In the provided sample it receives the stage and Prims to Node map, which are
    // the minimal arguments for a functional chaser, and the chaser arguments, which are
    // not mandatory but can be used to parameterize the chaser.
    UserDataExportChaserSample(
        pxr::UsdStagePtr                                                      stage,
        const pxr::MaxUsdExportChaserRegistry::FactoryContext::PrimToNodeMap& primToNodeMap,
        const MaxUsd::USDSceneBuilderOptions::ChaserArgs&                     args,
        const pxr::VtDictionary&                                              jobContextOptions);

    // Processing that needs to run after the main 3ds Max USD export loop.
    bool PostExport() override;

private:
    // The exported stage
    pxr::UsdStagePtr stage;
    // Maps full USD prim paths to INodes
    const pxr::MaxUsdExportChaserRegistry::FactoryContext::PrimToNodeMap& primToNodeMap;
    // potentially user-customized jobContext options.
    const pxr::VtDictionary& jobContextOptions;

    // sample specific parameters
    // maps the data to be exported as Custom Data for the prims
    typedef std::map<PropertyType, std::set<std::string>> PropertyMap;
    PropertyMap                                           dataToExport;
};

// Add the using statement here to make the export chaser factory macro work
PXR_NAMESPACE_USING_DIRECTIVE
// Macro to register the Export Chaser. Defines a factory method for the chaser name. The 'ctx' will
// be of type MaxUsdExportChaserRegistry::FactoryContext. The method should return a
// MaxUsdExportChaser*. There are no guarantees about the lifetime of 'ctx'. It is also very
// important to set the project option "Remove unreferenced code and data" to NO, not doing so could
// cause the Macro to be optimized out and the Export Chaser to never be properly registered.
PXR_MAXUSD_DEFINE_EXPORT_CHASER_FACTORY(
    UserData,
    "User Data C++ DEMO",
    "Chaser to export user data along the exported USD prims",
    ctx)
{
    // Fetching the export chaser parameters
    // The chaser arguments are mapped using their registration chaser name (first arg of the macro)
    MaxUsd::USDSceneBuilderOptions::ChaserArgs myArgs;
    TfMapLookup(ctx.GetJobArgs().GetAllChaserArgs(), "UserData", &myArgs);
    const pxr::VtDictionary& jobContextOptions
        = ctx.GetJobArgs().GetJobContextOptions(TfToken("CustomDemoContext"));
    return new UserDataExportChaserSample(
        ctx.GetStage(), ctx.GetPrimToNodeMap(), myArgs, jobContextOptions);
}

UserDataExportChaserSample::UserDataExportChaserSample(
    pxr::UsdStagePtr                                                      stage,
    const pxr::MaxUsdExportChaserRegistry::FactoryContext::PrimToNodeMap& primToNodeMap,
    const MaxUsd::USDSceneBuilderOptions::ChaserArgs&                     args,
    const pxr::VtDictionary&                                              jobContextOptions)
    : stage(stage)
    , primToNodeMap(primToNodeMap)
    , jobContextOptions(jobContextOptions)
{
    // Default configuration
    dataToExport
        = { { PropertyType::USER_PROP, { "myUserProperty" } }, { PropertyType::CUSTOM_DATA, {} } };

    // Parsing the specific export chaser arguments
    boost::char_separator<char> sep(",");
    for (std::pair<std::string, std::string> item : args) {
        if (item.first == "user") {
            dataToExport[PropertyType::USER_PROP].clear();
            boost::tokenizer<boost::char_separator<char>> tokens(item.second, sep);
            for (auto name : tokens) {
                dataToExport[PropertyType::USER_PROP].insert(name);
            }
        } else if (item.first == "custom") {
            dataToExport[PropertyType::CUSTOM_DATA].clear();
            boost::tokenizer<boost::char_separator<char>> tokens(item.second, sep);
            for (auto name : tokens) {
                dataToExport[PropertyType::CUSTOM_DATA].insert(name);
            }
        } else {
            TF_WARN(
                "Wrong user data type ('%s') passed as argument to UserPropertyExportChaser",
                item.first.c_str());
        }
    }

    if (!jobContextOptions.empty()) {
        if (auto greetUser = jobContextOptions.GetValueAtPath({ "Greeting"s, "Greet User"s })) {
            if (greetUser->Get<bool>()) {
                std::string username;
                if (auto userNameV
                    = jobContextOptions.GetValueAtPath({ "Greeting"s, "User Name"s })) {
                    if (userNameV->CanCast<std::string>()) {
                        username = userNameV->Get<std::string>();
                    }
                }
                bool formal = false;
                if (auto formalV = jobContextOptions.GetValueAtPath({ "Greeting"s, "Formal"s })) {
                    if (formalV->CanCast<bool>()) {
                        formal = formalV->Get<bool>();
                    }
                }
                std::string option;
                if (auto optionAV = jobContextOptions.GetValueAtPath({ "Option A"s })) {
                    if (optionAV->CanCast<bool>() && optionAV->Get<bool>()) {
                        option = "Option A";
                    }
                }
                bool optionB = false;
                if (auto optionBV = jobContextOptions.GetValueAtPath({ "Option B"s })) {
                    if (optionBV->CanCast<bool>() && optionBV->Get<bool>()) {
                        option = "Option B";
                    }
                }

                std::string greeting;
                if (formal) {
                    time_t now = time(0);
                    tm*    localtm = localtime(&now);
                    greeting = (localtm->tm_hour > 12) ? "Good Afternoon" : "Good Morning";
                } else {
                    greeting = "Hi";
                }
                static constexpr const char* message = "{0} '{1}' - You have chosen {2}";
                MaxUsd::Log::Info(message, greeting.c_str(), username.c_str(), option.c_str());
            }
        }
    }
}

bool UserDataExportChaserSample::PostExport()
{
    // Cycle through the exported prim/node
    for (std::pair<SdfPath, INode*> exported : primToNodeMap) {
        pxr::UsdPrim prim = stage->GetPrimAtPath(exported.first);
        INode*       node = exported.second;

        // Are any of the specified user properties found in the Node
        for (std::string propName : dataToExport[PropertyType::USER_PROP]) {
            auto mxsPropName = MaxUsd::UsdStringToMaxString(propName);
            if (node->UserPropExists(mxsPropName)) {
                TSTR value;
                node->GetUserPropString(mxsPropName, value);

                if (_tcsicmp(value, _T("true")) == 0) {
                    prim.SetCustomDataByKey(pxr::TfToken(propName), pxr::VtValue(true));
                } else if (_tcsicmp(value, _T("false")) == 0) {
                    prim.SetCustomDataByKey(pxr::TfToken(propName), pxr::VtValue(false));
                } else {
                    // Note - to simplify the chaser sample implementation, the user property
                    // conversion to numerical values was not completed. It will be written as a
                    // simple string. The same chaser written in Python properly handles this
                    // situation however.
                    auto valueString = MaxUsd::MaxStringToUsdString(value);
                    // Strip the quoting marks from the string (if the property was originally a
                    // string)
                    if (*valueString.begin() == '\"' && *(valueString.end() - 1) == '\"') {
                        valueString.erase(valueString.begin());
                        valueString.erase(valueString.end() - 1);
                    }
                    prim.SetCustomDataByKey(pxr::TfToken(propName), pxr::VtValue(valueString));
                }
            }
        }

        // For the purpose of the sample, the chaser finds the custom attributes only on
        // the base level, not on the modifiers or materials of the node
        BaseObject*           obj = node->GetObjectRef();
        ICustAttribContainer* cc = obj->GetCustAttribContainer();
        if (!cc) {
            continue;
        }
        // Are any of the specified custom attributes found in the Node
        for (std::string propName : dataToExport[PropertyType::CUSTOM_DATA]) {
            auto mxsPropName = MaxUsd::UsdStringToMaxString(propName);
            for (int i = 0; i < cc->GetNumCustAttribs(); i++) {
                CustAttrib* ca = cc->GetCustAttrib(i);
                if (!ca) {
                    continue;
                }
                MSCustAttrib* msCA = (MSCustAttrib*)ca->GetInterface(I_SCRIPTEDCUSTATTRIB);
                if (!msCA) {
                    continue;
                }

                for (int j = 0; j < ca->NumParamBlocks(); ++j) {
                    IParamBlock2* pb2 = ca->GetParamBlock(j);
                    if (!pb2) {
                        continue;
                    }
                    ParamID paramId = MaxUsd::FindParamId(pb2, mxsPropName);
                    // If this specific attribute doesn't exist, we skip to look for it in the next
                    // PB
                    if (paramId == -1) {
                        continue;
                    }

                    // Found the attribute, extract the value and write the data
                    {
                        const ParamDef& paramDef = pb2->GetParamDef(paramId);
                        Interval        iv = FOREVER;
                        bool            hasFoundParam = false;
                        switch (paramDef.type) {
                        case TYPE_BOOL: {
                            int boolVal = 0;
                            hasFoundParam = pb2->GetValue(paramId, 0, boolVal, iv);
                            prim.SetCustomDataByKey(
                                pxr::TfToken(propName),
                                pxr::VtValue((boolVal == 0) ? false : true));
                            break;
                        }
                        case TYPE_INT: {
                            int strVal = false;
                            hasFoundParam = pb2->GetValue(paramId, 0, strVal, iv);
                            prim.SetCustomDataByKey(pxr::TfToken(propName), pxr::VtValue(strVal));
                            break;
                        }
                        case TYPE_FLOAT: {
                            float strVal = false;
                            hasFoundParam = pb2->GetValue(paramId, 0, strVal, iv);
                            prim.SetCustomDataByKey(pxr::TfToken(propName), pxr::VtValue(strVal));
                            break;
                        }
                        case TYPE_STRING: {
                            const wchar_t* strVal = L"";
                            hasFoundParam = pb2->GetValue(paramId, 0, strVal, iv);
                            prim.SetCustomDataByKey(
                                pxr::TfToken(propName),
                                pxr::VtValue(MaxUsd::MaxStringToUsdString(strVal)));
                            break;
                        }
                        default:
                            TF_WARN(
                                "Unsupported Custom Attribute type for '%s' in "
                                "UserPropertyExportChaser",
                                propName.c_str());
                            break;
                        }
                        // No need to search further, the attribute was found
                        break;
                    }
                }
            }
        }
    }
    return true;
}

REGISTER_EXPORT_JOB_CONTEXT_FCT(
    CustomDemoContext,
    "Custom Context C++ DEMO",
    "Custom plug-in configuration")
{
    VtDictionary extraArgs;
    extraArgs[MaxUsdSceneBuilderOptionsTokens->chaserNames]
        = VtValue(std::vector<VtValue> { VtValue(std::string("UserData")) });
    VtValue chaserArgUserProp(
        std::vector<VtValue> { VtValue(std::string("UserData")),
                               VtValue(std::string("user")),
                               VtValue(std::string("myUserFloatProperty,myUserProperty")) });
    VtValue chaserArgCustomProp(std::vector<VtValue> { VtValue(std::string("UserData")),
                                                       VtValue(std::string("custom")),
                                                       VtValue(std::string("inGame")) });
    extraArgs[MaxUsdSceneBuilderOptionsTokens->chaserArgs]
        = VtValue(std::vector<VtValue> { chaserArgUserProp, chaserArgCustomProp });

    return extraArgs;
}

TF_REGISTRY_FUNCTION(MaxUsdJobContextRegistry)
{
    MaxUsdJobContextRegistry::GetInstance().SetExportOptionsUI(
        "CustomDemoContext", &ExportOptionsDialog::ShowOptionsDialog);
}
