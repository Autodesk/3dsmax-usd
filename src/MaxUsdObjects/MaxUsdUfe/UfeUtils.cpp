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
#include "UfeUtils.h"

#include "MaxUfeUndoableCommandMgr.h"
#include "MaxUsdContextOpsHandler.h"
#include "MaxUsdEditCommand.h"
#include "MaxUsdHierarchyHandler.h"
#include "MaxUsdObject3dHandler.h"
#include "MaxUsdStagesSubject.h"
#include "MaxUsdUIInfoHandler.h"
#include "StageObjectMap.h"

#include <MaxUsdObjects/Objects/USDStageObject.h>

#include <UFEUI/editCommand.h>
#include <UFEUI/utils.h>

#include <MaxUsd/Utilities/TranslationUtils.h>

#include <usdUfe/ufe/Global.h>
#include <usdUfe/undo/UsdUndoManager.h>
#include <usdUfe/utils/loadRules.h>

#include <ufe/globalSelection.h>
#include <ufe/observableSelection.h>
#include <ufe/pathString.h>
#include <ufe/runTimeMgr.h>

#include <MaxUsd.h>
#include <max.h>

namespace MAXUSD_NS_DEF {
namespace ufe {

static const char usdSeparator = '/';

void initialize()
{
    UsdUfe::Handlers handlers;

    // Initialize the usdUfe runtime with the functions it needs, and a derived stage subject.
    UsdUfe::DCCFunctions functions;
    functions.stageAccessorFn = MaxUsd::ufe::getStage;
    functions.stagePathAccessorFn = MaxUsd::ufe::getStagePath;
    functions.ufePathToPrimFn = MaxUsd::ufe::ufePathToPrim;
    functions.timeAccessorFn = MaxUsd::ufe::getTime;
    functions.saveStageLoadRulesFn = MaxUsd::ufe::saveStageLoadRules;
    functions.isRootChildFn = MaxUsd::ufe::isRootChild;
    // Initialize the global UFE selection.
    Ufe::GlobalSelection::initializeInstance(std::make_shared<Ufe::ObservableSelection>());

    static MaxUsdStagesSubject::Ptr stagesSubject = MaxUsdStagesSubject::create();
    const auto usdRtId = UsdUfe::initialize(functions, handlers, stagesSubject);
    Ufe::PathString::registerPathComponentSeparator(usdRtId, usdSeparator);

    // Setup Max specific USD UFE implementations.

    // UI Info handler.
    const auto uiHandler = MaxUsd::ufe::MaxUsdUIInfoHandler::create();
    Ufe::RunTimeMgr::instance().setUIInfoHandler(usdRtId, uiHandler);
    // Object3d.
    const auto object3dHandler = MaxUsd::ufe::MaxUsdObject3dHandler::create();
    Ufe::RunTimeMgr::instance().setObject3dHandler(usdRtId, object3dHandler);
    // Hierarchy.
    const auto hierarchyHandler = MaxUsd::ufe::MaxUsdHierarchyHandler::create();
    Ufe::RunTimeMgr::instance().setHierarchyHandler(usdRtId, hierarchyHandler);
    // Context ops.
    const auto contextOpsHandler = MaxUsd::ufe::MaxUsdContextOpsHandler::create();
    Ufe::RunTimeMgr::instance().setContextOpsHandler(usdRtId, contextOpsHandler);

    // To catch scene notifications (objects added, removed, etc.) we need to subclass Ufe::Scene,
    // and tell Ufe to use an instance of this object.
    class MaxScene : public Ufe::Scene
    {
    };
    Ufe::Scene::initializeInstance(std::make_shared<MaxScene>());

    // Setup a 3dsmax specific command manager. To push UFE commands to the max
    // undo stack.
    Ufe::UndoableCommandMgr::initializeInstance(std::make_shared<MaxUfeUndoableCommandMgr>());

    // Setup edit commands. These wrap Ufe::UndoableCommand commands,
    // temporarily setting the target layer and triggering viewport redraws.
    auto create = [](const Ufe::Path&                 path,
                     const Ufe::UndoableCommand::Ptr& cmd,
                     const std::string&               cmdString) {
        return std::make_shared<MaxUsdEditCommand>(path, cmd, cmdString);
    };
    UfeUi::EditCommand::initializeCreator(create);

    // Configure DPI scaling for UFE widgets.
    UfeUi::Utils::setDpiScale(double(MaxSDK::GetUIScaleFactor()));
}

void finalize()
{
    Ufe::GlobalSelection::initializeInstance(nullptr);
    UsdUfe::finalize(true);
}

//------------------------------------------------------------------------------
// Utility Functions
//------------------------------------------------------------------------------
//
pxr::UsdStageWeakPtr getStage(const Ufe::Path& path)
{
    const auto usdStageObject = StageObjectMap::GetInstance()->Get(path);
    if (!usdStageObject) {
        return nullptr;
    }
    return usdStageObject->GetUSDStage();
}

Ufe::Path getStagePath(pxr::UsdStageWeakPtr stage)
{
    const auto object = StageObjectMap::GetInstance()->Get(stage);
    if (!object) {
        return Ufe::Path {};
    }
    return getUsdStageObjectPath(object);
}

pxr::UsdPrim ufePathToPrim(const Ufe::Path& path)
{
    // The first segment should map to a USDStageObject.
    // The path should generally look like this : /{Stage object GUID}/{usd path}
    // For point instances, the path looks like : /{Stage object GUID}/{usd path}/{instanceIdx}
    // If we just have the first segment, map to the usd pseudo-root prim.
    // More than 3 segments is not a legal path.
    const auto& segments = path.getSegments();
    if (segments.empty() || segments[0].empty() || segments.size() > 3) {
        return pxr::UsdPrim {};
    }

    // Find the Stage Object from the path, the object path is the first segment.
    const auto objectPath = Ufe::Path({ segments[0] });
    const auto object = StageObjectMap::GetInstance()->Get(objectPath);
    if (!object) {
        return pxr::UsdPrim {};
    }

    const pxr::UsdStageRefPtr stage = object->GetUSDStage();
    if (!stage) {
        return pxr::UsdPrim {};
    }

    if (segments.size() == 1u) {
        return stage->GetPseudoRoot();
    }

    // The USD path, is contained in the second segment.
    return stage->GetPrimAtPath(pxr::SdfPath(segments[1].string()));
}

Ufe::Path getUsdStageObjectPath(const USDStageObject* object)
{
    // Use the GUID of the stage object, to build the first segment.
    const auto stageObjectSegment
        = Ufe::PathSegment(object->GetGuid(), UsdUfe::getUsdRunTimeId(), usdSeparator);
    return Ufe::Path({ stageObjectSegment });
}

USDStageObject* getUsdStageObjectFromPath(const Ufe::Path& path)
{
    const auto segments = path.getSegments();
    if (segments.size() < 1) {
        return nullptr;
    }

    auto objectPath = Ufe::Path { { segments[0] } };
    return StageObjectMap::GetInstance()->Get(objectPath);
}

Ufe::Path getUsdPrimUfePath(USDStageObject* object, const pxr::SdfPath& primPath, int instanceIdx)
{
    const auto stage = object->GetUSDStage();
    if (!stage) {
        return {};
    }
    if (!stage->GetPrimAtPath(primPath).IsValid()) {
        return {};
    }
    const auto base = getUsdStageObjectPath(object);
    if (primPath.IsEmpty() || primPath.IsAbsoluteRootPath()) {
        return base;
    }
    auto segments = base.getSegments();
    segments.push_back(
        Ufe::PathSegment { primPath.GetString(), UsdUfe::getUsdRunTimeId(), usdSeparator });

    if (instanceIdx >= 0) {
        segments.push_back(Ufe::PathSegment {
            std::to_string(instanceIdx), UsdUfe::getUsdRunTimeId(), usdSeparator });
    }
    return Ufe::Path { segments };
}

bool isPointInstance(const Ufe::SceneItemPtr& item)
{
    const UsdUfe::UsdSceneItem::Ptr usdItem = std::dynamic_pointer_cast<UsdUfe::UsdSceneItem>(item);
    return usdItem && usdItem->isPointInstance();
}

pxr::UsdTimeCode getTime(const Ufe::Path& path)
{
    // If this path can resolve to stage object, consider the stage FPS/max FPS.
    // Find the Stage Object from the path, the object path is the first segment.
    const auto& segments = path.getSegments();
    const auto  stagePath = Ufe::Path({ segments[0] });
    const auto usdStageObject = StageObjectMap::GetInstance()->Get(stagePath);
    if (usdStageObject) {
        const auto currentTimeValue = GetCOREInterface()->GetTime();
        return usdStageObject->ResolveRenderTimeCode(currentTimeValue);
    }
    // Best effort.
    const auto frame = GetCOREInterface()->GetTime() / double(GetTicksPerFrame());
    return pxr::UsdTimeCode { frame };
}

void saveStageLoadRules(const PXR_NS::UsdStageRefPtr& stage)
{
    const auto usdStageObject = StageObjectMap::GetInstance()->Get(stage);
    if (!usdStageObject) {
        return;
    }
    return usdStageObject->SaveStageLoadRules();
}

bool isRootChild(const Ufe::Path& path)
{
    // When we have a single segment, it's the path of the Stage Object,
    // which will map to the USD pseudo-root. Building the hierarchy will
    // be handled by MaxUsdRootChildHierarchy.
    return path.nbSegments() == 1;
}

} // namespace ufe
} // namespace MAXUSD_NS_DEF
