//
// Copyright 2024 Autodesk
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

#include "MaxLayerEditorWindow.h"

#include "maxSessionState.h"

#include <MaxUsdObjects/MaxUsdUfe/StageObjectMap.h>

#include <UsdLayerEditor/layerEditorWidget.h>
#include <UsdLayerEditor/layerEditorWindow.h>
#include <UsdLayerEditor/layerTreeModel.h>
#include <UsdLayerEditor/layerTreeView.h>
#include <UsdLayerEditor/sessionState.h>

#include <ufe/path.h>
#include <ufe/pathString.h>

#include <QtCore/QPointer>
#include <vector>

using namespace UsdLayerEditor;

MaxLayerEditorWindow::MaxLayerEditorWindow(
    const char*     panelName,
    QWidget*        parent,
    Qt::WindowFlags flags)
    : PARENT_CLASS(parent, flags)
    , UsdLayerEditor::LayerEditorWindow(panelName)
{
    setupUi();

    // The Layer editor uses the Highlight role in way not used in 3dsMax. Adjust
    // the palette to get the usual 3dsMax look.
    QPalette palette = QApplication::palette();
    palette.setColor(
        QPalette::Highlight, QApplication::palette().color(QPalette::Normal, QPalette::Light));
    for (const auto& child : this->findChildren<QWidget*>()) {
        child->setPalette(palette);
    }
}

MaxLayerEditorWindow::~MaxLayerEditorWindow() { }

void MaxLayerEditorWindow::setupUi()
{
    LayerTreeModel::suspendUsdNotices(false);
    _layerEditor = new LayerEditorWidget(_sessionState, this);
    QObject::connect(
        treeView(),
        &QWidget::customContextMenuRequested,
        this,
        &MaxLayerEditorWindow::onShowContextMenu);
    setCentralWidget(_layerEditor);
}

std::string MaxLayerEditorWindow::dccObjectName() const
{
    return _sessionState.stageEntry()._displayName;
}

void MaxLayerEditorWindow::selectDccObject(const char* objectPath)
{
    SessionState::StageEntry entry;

    // The object path is in the form /{Stage Object GUID}
    const auto ufePath = Ufe::PathString::path(objectPath);
    const auto object = StageObjectMap::GetInstance()->Get(ufePath);

    if (_sessionState.getStageEntry(&entry, object)) {
        if (entry._stage != nullptr) {
            _sessionState.setStageEntry(entry);
        }
    }
}

SessionState* MaxLayerEditorWindow::getSessionState() { return &_sessionState; }

QMainWindow* MaxLayerEditorWindow::getMainWindow() { return this; }

void MaxLayerEditorWindow::onShowContextMenu(const QPoint& pos) { buildContextMenu(pos); }
