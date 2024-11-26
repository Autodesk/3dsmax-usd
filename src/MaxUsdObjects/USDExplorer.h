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
#include <UFEUI/Views/explorer.h>

#include <ufe/hierarchy.h>

class USDStageObject;

class USDExplorer
{
public:
    static USDExplorer* Instance();

    /**
     * \brief Opens the USDExplorer.
     */
    void Open();

    /**
     * \brief Closes the USDExplorer.
     */
    void Close();

    /**
     * \brief Opens the given stage in the USD Explorer UI.
     * \param stage The USD Stage object to open in the USD explorer UI.
     */
    void OpenStage(USDStageObject* stageObject);

    /**
     * \brief Closes the given stage in the USD Explorer UI.
     * \param stage The USD Stage object to close in the USD explorer UI.
     */
    void CloseStage(USDStageObject* stageObject);

    /**
     * \brief Sets whether to show inactive prims in the treeview.
     * \param showInactive True to show inactive prims.
     */
    void SetShowInactivePrims(bool showInactive);

    /**
     * \brief Gets whether inactive prims are shown in the treeview.
     * \return True if inactive prims are shown.
     */
    bool ShowInactivePrims() const;

    /**
     * \brief If enabled, the explorer will make sure that the current selection is visible
     * by auto expanding ancestors of selected items, and scrolling to the first item in the
     * selection when it changes.
     * \return True if auto-expansion is enabled, false otherwise.
     */
    bool IsAutoExpandedToSelection();

    /**
     * \brief Sets selection auto-expansion. If enabled, the explorer will make sure that the
     * current selection is visible by auto expanding ancestors of selected items, and scrolling
     * to the first item in the selection when it changes.
     * \param autoExpandToSelection If true, auto-expansion is enabled.
     */
    void SetAutoExpandedToSelection(bool autoExpandToSelection);

    // Delete the copy/move constructors assignment operators.
    USDExplorer(const USDExplorer&) = delete;
    USDExplorer& operator=(const USDExplorer&) = delete;
    USDExplorer(USDExplorer&&) = delete;
    USDExplorer& operator=(USDExplorer&&) = delete;

    bool IsColumnHidden(int visualIdx);
    bool ToggleColumnHiddenState(int visualIdx);

    int GetManualColumnWidth(int visualIdx) const;

private:
    USDExplorer();

    static std::vector<UfeUi::Explorer*> AllStageExplorers();
    static UfeUi::Explorer*              ActiveStageExplorer();

    static std::unique_ptr<USDExplorer> instance;

    Ufe::Hierarchy::ChildFilter childFilter;
    bool                        autoExpandToSelection = false;

    struct TreeViewColumn
    {
        int  visualIdx;
        bool hidden;
        int  manualColumnWidth = -1;
    };
    typedef std::vector<TreeViewColumn> TreeViewColumsState;
    TreeViewColumsState                 treeViewColumnsState;
};
