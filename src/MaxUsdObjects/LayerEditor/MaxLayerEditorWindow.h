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

#pragma once

#include "MaxSessionState.h"

#include <UsdLayerEditor/abstractLayerEditorWindow.h>
#include <UsdLayerEditor/layerEditorWindow.h>

#include <Qt/QmaxMainWindow.h>

#include <QtCore/QPointer>

/**
 * 3dsMax specific USD layer editor window class.
 */
class MaxLayerEditorWindow
    : public MaxSDK::QmaxMainWindow
    , public UsdLayerEditor::LayerEditorWindow
{
    Q_OBJECT
public:
    typedef MaxSDK::QmaxMainWindow PARENT_CLASS;

    MaxLayerEditorWindow(const char* panelName, QWidget* parent = nullptr, Qt::WindowFlags = {});
    ~MaxLayerEditorWindow() override;

    /**
     * Builds the Layer Editor UI.
     */
    void setupUi();

    // LayerEditorWindow overrides.
    std::string                   dccObjectName() const override;
    void                          selectDccObject(const char* objectPath) override;
    UsdLayerEditor::SessionState* getSessionState() override;
    QMainWindow*                  getMainWindow() override;

    /**
     * Responds to show context menu QT signal.
     * @param pos Position at which to create the context menu.
     */
    void onShowContextMenu(const QPoint& pos);

protected:
    MaxSessionState _sessionState;
};