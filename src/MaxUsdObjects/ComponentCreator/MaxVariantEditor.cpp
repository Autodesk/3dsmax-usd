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
#include "MaxVariantEditor.h"

#include "UtilComponentCreator.h"

#include <MaxUsdObjects/MaxUsdUfe/UfeUtils.h>
#include <MaxUsdObjects/Objects/MaxUsdNotify.h>

#include <MaxUsd.h>
#include <MaxUsd/Interfaces/IUSDStageProvider.h>
#include <MaxUsd/Utilities/TranslationUtils.h>

#include <pxr/usd/sdf/layer.h>

#include <Qt/QMaxHelpers.h>
#include <Qt/QmaxDockWidget.h>
#include <ufe/globalSelection.h>
#include <ufe/observableSelection.h>
#include <ufe/observer.h>
#include <ufe/selection.h>
#include <ufe/selectionNotification.h>

#include <AdskUsdComponentCreator/Descriptions/ComponentDescription.h>
#include <AdskVariantEditor/BaseData.h>
#include <AdskVariantEditor/ComponentData.h>
#include <AdskVariantEditor/PrimData.h>
#include <AdskVariantEditor/VariantEditorWidget.h>
#include <GetCOREInterface.h>
#include <QtCore/QCoreApplication>
#include <QtCore/QFile>
#include <inode.h>
#include <max.h>
#include <notify.h>

#include <utility>
#include <vector>

namespace {
std::unique_ptr<MaxVariantEditor> g_instance;

// What the editor should display, and the stage it came from.
struct EditorTarget
{
    std::shared_ptr<AdskVariantEditor::BaseData> data;
    pxr::UsdStageRefPtr                          stage;
};

// Hides variant sets/variants the component creator uses to represent data outside any variant
// (a "" key in its variant_sets metadata), which would otherwise show up in the tree with a blank
// label. Remove once the component creator supports global component data natively and stops
// emitting those entries.
class FilteredBaseData : public AdskVariantEditor::BaseData
{
public:
    static AdskVariantEditor::BaseData::Ptr Wrap(const AdskVariantEditor::BaseData::Ptr& inner)
    {
        return inner ? std::make_shared<FilteredBaseData>(inner) : nullptr;
    }

    explicit FilteredBaseData(AdskVariantEditor::BaseData::Ptr inner)
        : inner(std::move(inner))
    {
        // NotifyChange()/RegisterChangeCallback() are non-virtual and operate on `this`, so
        // callbacks registered on this wrapper must be bridged from the wrapped object.
        forwardHandle = this->inner->RegisterChangeCallback([this] { NotifyChange(); });
    }

    bool CanBeSelected() const override { return inner->CanBeSelected(); }
    bool CanBeDefault() const override { return inner->CanBeDefault(); }
    bool CanBeTargeted() const override { return inner->CanBeTargeted(); }
    bool CanAddSelection() const override { return inner->CanAddSelection(); }
    bool IsVariant() const override { return inner->IsVariant(); }
    bool IsVariantSet() const override { return inner->IsVariantSet(); }
    bool IsEditable() const override { return inner->IsEditable(); }
    bool HasVariants() const override { return inner->HasVariants(); }
    bool HasVariantSets() const override { return inner->HasVariantSets(); }

    AdskVariantEditor::BaseData::Ptr GetParent() const override { return Wrap(inner->GetParent()); }
    bool IsSameComponent(const AdskVariantEditor::BaseData::Ptr& other) const override
    {
        return inner->IsSameComponent(Unwrap(other));
    }

    std::string GetName() const override { return inner->GetName(); }
    bool        SetName(const std::string& name) override { return inner->SetName(name); }

    bool IsSelected() const override { return inner->IsSelected(); }
    bool SetSelected(bool selected) override { return inner->SetSelected(selected); }
    bool IsDefault() const override { return inner->IsDefault(); }
    bool SetDefault(bool isDefault) override { return inner->SetDefault(isDefault); }
    bool IsTarget() const override { return inner->IsTarget(); }
    bool SetTarget(bool target) override { return inner->SetTarget(target); }
    bool AddSceneSelection(bool isReplacing, const std::string& purpose) override
    {
        return inner->AddSceneSelection(isReplacing, purpose);
    }

    std::vector<AdskVariantEditor::BaseData::Ptr> GetVariantSets() const override
    {
        return FilterAndWrap(inner->GetVariantSets());
    }
    void RefreshVariantSets() override { inner->RefreshVariantSets(); }
    AdskVariantEditor::BaseData::Ptr AddVariantSet(const std::string& name) override
    {
        return Wrap(inner->AddVariantSet(name));
    }
    bool RemoveVariantSet(const AdskVariantEditor::BaseData::Ptr& variantSet) override
    {
        return inner->RemoveVariantSet(Unwrap(variantSet));
    }

    std::vector<AdskVariantEditor::BaseData::Ptr> GetVariants() const override
    {
        return FilterAndWrap(inner->GetVariants());
    }
    AdskVariantEditor::BaseData::Ptr AddVariant(const std::string& name) override
    {
        return Wrap(inner->AddVariant(name));
    }
    bool RemoveVariant(const AdskVariantEditor::BaseData::Ptr& variant) override
    {
        return inner->RemoveVariant(Unwrap(variant));
    }

    AdskVariantEditor::ComponentDescription::Ptr GetComponentDescription() const override
    {
        return inner->GetComponentDescription();
    }
    AdskVariantEditor::VariantNesting GetVariantNesting() const override
    {
        return inner->GetVariantNesting();
    }

private:
    // Drops entries with an empty name (the component creator's "outside all variants" bucket),
    // wrapping the rest so nested empty entries stay hidden too.
    static std::vector<AdskVariantEditor::BaseData::Ptr> FilterAndWrap(
        const std::vector<AdskVariantEditor::BaseData::Ptr>& children)
    {
        std::vector<AdskVariantEditor::BaseData::Ptr> result;
        result.reserve(children.size());
        for (const auto& child : children) {
            if (child && !child->GetName().empty()) {
                result.push_back(Wrap(child));
            }
        }
        return result;
    }

    static AdskVariantEditor::BaseData::Ptr Unwrap(const AdskVariantEditor::BaseData::Ptr& data)
    {
        if (auto* filtered = dynamic_cast<FilteredBaseData*>(data.get())) {
            return filtered->inner;
        }
        return data;
    }

    AdskVariantEditor::BaseData::Ptr        inner;
    AdskVariantEditor::ChangeCallbackHandle forwardHandle;
};

// A component stage is shown as an editable component. Any other stage is shown through the prim
// carrying the variant sets, which the editor presents read-only apart from variant selection.
EditorTarget TargetFor(const pxr::UsdStageRefPtr& stage, const pxr::SdfPath& primPath)
{
    if (!stage) {
        return {};
    }
    if (MaxUsd::ComponentCreator::IsAdskUsdComponent(stage)) {
        if (auto desc
            = AdskUsdComponentCreator::ComponentDescription::CreateFromStageMetadata(stage)) {
            return { FilteredBaseData::Wrap(AdskVariantEditor::ComponentData::Create(desc)),
                      stage };
        }
        return {};
    }
    const pxr::UsdPrim prim
        = MaxUsd::ComponentCreator::FindPrimWithVariantSets(stage, primPath);
    if (!prim) {
        return {};
    }
    return { FilteredBaseData::Wrap(AdskVariantEditor::PrimData::Create(prim)), stage };
}

// The first USD prim in the current selection that resolves to something displayable, optionally
// restricted to one stage.
EditorTarget TargetForSelection(const pxr::UsdStageRefPtr& onlyStage = pxr::UsdStageRefPtr())
{
    const auto selection = Ufe::GlobalSelection::get();
    if (!selection) {
        return {};
    }
    for (const auto& item : *selection) {
        if (!item) {
            continue;
        }
        const pxr::UsdPrim prim = MaxUsd::ufe::ufePathToPrim(item->path());
        if (!prim) {
            continue;
        }
        const pxr::UsdStageRefPtr stage = MaxUsd::ufe::getStage(item->path());
        if (!stage || (onlyStage && stage != onlyStage)) {
            continue;
        }
        if (auto target = TargetFor(stage, prim.GetPath()); target.data) {
            return target;
        }
    }
    return {};
}

// The stage backing a Max node, when that node holds one.
pxr::UsdStageRefPtr StageForNode(INode* node)
{
    if (!node || !node->GetObjectRef()) {
        return {};
    }
    auto* provider = dynamic_cast<MaxUsd::IUSDStageProvider*>(
        node->GetObjectRef()->GetInterface(IUSDStageProvider_ID));
    if (!provider) {
        return {};
    }
    return provider->GetUSDStage();
}

// The first selected stage node with something displayable. Selecting a stage node carries no prim,
// so this only resolves for a component; prims come from the USD selection instead.
EditorTarget TargetForNodeSelection()
{
    for (INode* node : MaxUsd::GetSelectedNodes()) {
        if (auto target = TargetFor(StageForNode(node), pxr::SdfPath()); target.data) {
            return target;
        }
    }
    return {};
}

// Observes the USD selection so the editor follows it.
class VariantEditorSelectionObserver : public Ufe::Observer
{
public:
    explicit VariantEditorSelectionObserver(MaxVariantEditor* editor) : editor(editor) { }

    void operator()(const Ufe::Notification& notification) override
    {
        if (dynamic_cast<const Ufe::SelectionChanged*>(&notification)) {
            editor->HandleSelectionChanged();
        }
    }

private:
    MaxVariantEditor* editor;
};
} // namespace

bool MaxVariantEditor::IsCreated() { return g_instance != nullptr; }

MaxVariantEditor* MaxVariantEditor::Instance()
{
    if (!g_instance) {
        g_instance = std::unique_ptr<MaxVariantEditor>(new MaxVariantEditor());
    }
    return g_instance.get();
}

void MaxVariantEditor::Shutdown() { g_instance.reset(); }

MaxVariantEditor::MaxVariantEditor()
{
    RegisterNotification(OnNodePreDeleted, this, NOTIFY_SCENE_PRE_DELETED_NODE);
    RegisterNotification(OnSceneReset, this, NOTIFY_POST_SCENE_RESET);
    RegisterNotification(OnSelectionSetChanged, this, NOTIFY_SELECTIONSET_CHANGED);
}

MaxVariantEditor::~MaxVariantEditor()
{
    UnRegisterNotification(OnNodePreDeleted, this, NOTIFY_SCENE_PRE_DELETED_NODE);
    UnRegisterNotification(OnSceneReset, this, NOTIFY_POST_SCENE_RESET);
    UnRegisterNotification(OnSelectionSetChanged, this, NOTIFY_SELECTIONSET_CHANGED);

    if (selectionObserver) {
        if (const auto globalSelection = Ufe::GlobalSelection::get()) {
            globalSelection->removeObserver(selectionObserver);
        }
    }
}

void MaxVariantEditor::OnNodePreDeleted(void* param, NotifyInfo* info)
{
    auto* self = static_cast<MaxVariantEditor*>(param);
    if (!self || !self->currentStage) {
        return;
    }
    INode* node = GetNotifyParam<NOTIFY_SCENE_PRE_DELETED_NODE>(info);
    if (!node || !node->GetObjectRef()) {
        return;
    }
    auto* provider = dynamic_cast<MaxUsd::IUSDStageProvider*>(
        node->GetObjectRef()->GetInterface(IUSDStageProvider_ID));
    if (!provider) {
        return;
    }
    const pxr::UsdStageRefPtr stage = provider->GetUSDStage();
    if (!stage) {
        return;
    }
    // Clear only when the deleted node's stage is the exact one the editor is displaying.
    if (stage == self->currentStage) {
        self->Clear();
    }
}

void MaxVariantEditor::OnSceneReset(void* param, NotifyInfo* /*info*/)
{
    if (auto* self = static_cast<MaxVariantEditor*>(param)) {
        self->Clear();
    }
}

void MaxVariantEditor::OnSelectionSetChanged(void* param, NotifyInfo* /*info*/)
{
    if (auto* self = static_cast<MaxVariantEditor*>(param)) {
        self->HandleSelectionChanged();
    }
}

void MaxVariantEditor::EnsureDockWidget()
{
    if (dock) {
        return;
    }

    auto* maxMainWindow = GetCOREInterface()->GetQmaxMainWindow();

    dock = new MaxSDK::QmaxDockWidget(
        "USD Variant Manager", QObject::tr("USD Variant Manager"), maxMainWindow);
    dock->setAllowedAreas(Qt::AllDockWidgetAreas);
    dock->setProperty("QmaxDockMinMaximizable", true);

    widget = new AdskVariantEditor::VariantEditorWidget(nullptr, dock);
    dock->setWidget(widget);

    QtHelpers::DisableMaxAcceleratorsOnFocus(widget);

    const QSize defaultSize(MaxSDK::UIScaled(400), MaxSDK::UIScaled(600));
    if (!maxMainWindow->restoreDockWidget(dock)) {
        maxMainWindow->addDockWidget(Qt::RightDockWidgetArea, dock);
        dock->setHidden(true);
        dock->setFloating(true);
        dock->resize(defaultSize);
    }

    QObject::connect(
        dock, &MaxSDK::QmaxDockWidget::topLevelChanged, [defaultSize, dock = dock](bool topLevel) {
            if (topLevel) {
                dock->resize(defaultSize);
            }
        });

    if (const auto globalSelection = Ufe::GlobalSelection::get()) {
        selectionObserver = std::make_shared<VariantEditorSelectionObserver>(this);
        globalSelection->addObserver(selectionObserver);
    }
}

void MaxVariantEditor::HandleSelectionChanged()
{
    // Pinning is what stops the editor following the selection.
    if (pinned || !dock || !widget || !dock->isVisible()) {
        return;
    }
    // Prims come from the USD selection (viewport sub-object selection and the Explorer); a
    // selected stage node only carries a stage, so it is the fallback.
    auto target = TargetForSelection();
    if (!target.data) {
        target = TargetForNodeSelection();
    }
    // A selection with nothing to show leaves the current content alone, so selecting unrelated
    // scene elements does not wipe the editor.
    if (target.data) {
        widget->SetComponent(target.data);
        currentStage = target.stage;
    }
}

void MaxVariantEditor::Open()
{
    EnsureDockWidget();
    dock->show();
    dock->raise();
}

void MaxVariantEditor::OpenFor(const pxr::UsdStageRefPtr& stage)
{
    EnsureDockWidget();

    // A component shows itself; for any other stage the selection decides which prim's variants
    // are shown, so opening on a stage with nothing selected in it shows nothing.
    EditorTarget target = TargetFor(stage, pxr::SdfPath());
    if (!target.data) {
        target = TargetForSelection(stage);
    }
    if (target.data) {
        widget->SetComponent(target.data);
        currentStage = target.stage;
    }

    dock->show();
    dock->raise();
}

void MaxVariantEditor::NotifyComponentChanged(const pxr::UsdStageRefPtr& stage)
{
    if (!dock || !widget || pinned || !stage) {
        return;
    }
    NotifyComponentChanged(
        AdskUsdComponentCreator::ComponentDescription::CreateFromStageMetadata(stage));
}

void MaxVariantEditor::NotifyComponentChanged(
    const std::shared_ptr<AdskUsdComponentCreator::ComponentDescription>& desc)
{
    if (!dock || !widget || pinned || !desc || !currentStage) {
        return;
    }
    // Refresh only when the editor is displaying this component. Match on the root-layer
    // identifier, not the stage instance: a description resolves to its own stage, distinct
    // from the viewport stage stored in currentStage.
    if (currentStage->GetRootLayer()->GetIdentifier() != desc->GetRootLayerFilename()) {
        return;
    }
    widget->SetComponent(FilteredBaseData::Wrap(AdskVariantEditor::ComponentData::Create(desc)));
}

void MaxVariantEditor::RefreshCurrent()
{
    // Re-read the component currently displayed in the editor from its own stage.
    if (!dock || !widget || !currentStage) {
        return;
    }
    auto desc
        = AdskUsdComponentCreator::ComponentDescription::CreateFromStageMetadata(currentStage);
    if (desc) {
        widget->SetComponent(FilteredBaseData::Wrap(AdskVariantEditor::ComponentData::Create(desc)));
    }
}

void MaxVariantEditor::Clear()
{
    if (widget) {
        widget->SetComponent(nullptr);
    }
    currentStage = pxr::UsdStageRefPtr();
}

void MaxVariantEditor::RepointStage(
    const pxr::UsdStageRefPtr& oldStage,
    const pxr::UsdStageRefPtr& newStage)
{
    // Only repoint if the editor is currently displaying the old (relocated) stage. This is the
    // same component, just moved on disk, so refresh regardless of pin state. Does not show/raise
    // the dock -- preserves whether the editor was open or closed.
    if (!widget || !currentStage || currentStage != oldStage) {
        return;
    }
    currentStage = newStage;
    if (newStage && MaxUsd::ComponentCreator::IsAdskUsdComponent(newStage)) {
        auto desc
            = AdskUsdComponentCreator::ComponentDescription::CreateFromStageMetadata(newStage);
        if (desc) {
            widget->SetComponent(FilteredBaseData::Wrap(AdskVariantEditor::ComponentData::Create(desc)));
        }
    }
}

bool MaxVariantEditor::IsPinned() const { return pinned; }

void MaxVariantEditor::SetPinned(bool pinned) { this->pinned = pinned; }

pxr::UsdStageRefPtr MaxVariantEditor::GetCurrentStage() const { return currentStage; }
