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
#include <MaxUsd/Builders/JobContextRegistry.h>
#include <MaxUsd/Builders/SceneBuilderOptions.h>
#include <MaxUsd/Chaser/ImportChaser.h>
#include <MaxUsd/Chaser/ImportChaserRegistry.h>

#include <pxr/usd/usd/primFlags.h>

#include <maxscript/mxsplugin/mxsCustomAttributes.h>

#include <boost/tokenizer.hpp>
#include <custattrib.h>
#include <icustattribcontainer.h>
#include <object.h>

// Add the using statement here to make the import chaser factory macro work
PXR_NAMESPACE_USING_DIRECTIVE

// The ImportChaserSample class that logs a simple message after an import
class ImportChaserSample : public pxr::MaxUsdImportChaser
{
public:
    // constructor - can be customized for the Import Chaser requirements.
    ImportChaserSample(
        Usd_PrimFlagsPredicate&     returnPredicate,
        const MaxUsdReadJobContext& context,
        const fs::path&             filename);

    // processing that needs to run after the main 3ds Max USD import loop.
    bool PostImport() override;

private:
    // the imported stage
    const MaxUsdReadJobContext context;
    const fs::path&            filename;
};

// Macro to register the Import Chaser. Defines a factory method for the chaser name. The 'ctx' will
// be of type MaxUsdImportChaserRegistry::FactoryContext. The method should return a
// MaxUsdImportChaser*. There are no guarantees about the lifetime of 'ctx'. It is also very
// important to set the project option "Remove unreferenced code and data" to NO, not doing so could
// cause the Macro to be optimized out and the Import Chaser to never be properly registered.
PXR_MAXUSD_DEFINE_IMPORT_CHASER_FACTORY(
    ImportLog,
    "Import Chaser C++ DEMO",
    "Import chaser that logs a message ",
    ctx)
{
    return new ImportChaserSample(
        *(new Usd_PrimFlagsPredicate()), ctx.GetContext(), ctx.GetFilename());
}

ImportChaserSample::ImportChaserSample(
    Usd_PrimFlagsPredicate&     returnPredicate,
    const MaxUsdReadJobContext& context,
    const fs::path&             filename)
    : context(context)
    , filename(filename)
{
}

bool ImportChaserSample::PostImport()
{
    maxUsd::Log::Info("Stage imported successfully from '{0}'.\n", filename.string());
    return true;
}

REGISTER_IMPORT_JOB_CONTEXT_FCT(
    CustomImportContext,
    "Custom Import Context",
    "Custom import plug-in configuration")
{
    VtDictionary extraArgs;
    extraArgs[MaxUsdSceneBuilderOptionsTokens->chaser]
        = VtValue(std::vector<VtValue> { VtValue(std::string("ImportLog")) });
    VtValue chaserArgUserProp(
        std::vector<VtValue> { VtValue(std::string("ImportLog")),
                               VtValue(std::string("user")),
                               VtValue(std::string("myUserFloatProperty,myUserProperty")) });
    VtValue chaserArgCustomProp(std::vector<VtValue> { VtValue(std::string("ImportLog")),
                                                       VtValue(std::string("custom")),
                                                       VtValue(std::string("inGame")) });
    extraArgs[MaxUsdSceneBuilderOptionsTokens->chaserArgs]
        = VtValue(std::vector<VtValue> { chaserArgUserProp, chaserArgCustomProp });

    return extraArgs;
}