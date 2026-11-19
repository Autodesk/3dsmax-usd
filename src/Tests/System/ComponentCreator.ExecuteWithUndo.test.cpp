//
// Copyright 2026 Autodesk
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

#include "ComponentCreatorTestHelpers.h"
#include "TestHelpers.h"

#include <MaxUsdObjects/Objects/USDStageObject.h>

#include <MaxUsd/Utilities/ScopeGuard.h>

#include <pxr/usd/usd/stage.h>

#include <AdskUsdComponentCreator/Commands/AddVariantSetCommand.h>
#include <AdskUsdComponentCreator/Descriptions/ComponentDescription.h>
#include <cstddef>

using namespace CCTest;
namespace cc = AdskUsdComponentCreator;

namespace {
// Top-level variant-set count read back from the component's composed stage metadata.
std::size_t VariantSetCount(const pxr::UsdStageRefPtr& stage)
{
    auto desc = cc::ComponentDescription::CreateFromStageMetadata(stage);
    return desc ? desc->GetVariantSets().size() : 0;
}
} // namespace

// A ComponentCommand run through MaxHost::ExecuteWithUndo must undo/redo with 3ds Max's
// native undo. AddVariantSet is a representative WantsAutomaticUndo command; adding one
// at top level bumps the variant-set count, and undo/redo must revert/restore it.
TEST(ComponentCreatorExecuteWithUndo, AddVariantSetIsUndoableAndRedoable)
{
    const auto resetGuard = MaxUsd::MakeScopeGuard([]() {}, []() { QuickReset(); });

    USDStageObject* comp = CreateComponentFromCube(L"UndoVarSetBase");
    ASSERT_NE(comp, nullptr);
    const auto stage = comp->GetUSDStage();
    ASSERT_TRUE(stage);

    const std::size_t before = VariantSetCount(stage);

    auto desc = cc::ComponentDescription::CreateFromStageMetadata(stage);
    ASSERT_TRUE(desc);

    // Runs through MaxHost::ExecuteWithUndo (the path under refactor).
    auto cmd = cc::AddVariantSetCommand::ExecuteWithUndo(desc, {}, "UndoVarSet");
    ASSERT_TRUE(cmd);
    ASSERT_TRUE(cmd->IsSuccessful());
    ASSERT_EQ(VariantSetCount(stage), before + 1) << DumpStage(stage);

    MaxUndo();
    EXPECT_EQ(VariantSetCount(stage), before) << "undo must remove the added variant set\n"
                                              << DumpStage(stage);

    MaxRedo();
    EXPECT_EQ(VariantSetCount(stage), before + 1) << "redo must restore the added variant set\n"
                                                  << DumpStage(stage);
}
