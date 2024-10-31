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
#ifndef UFEUI_EXPLORER_SEARCH_THREAD
#define UFEUI_EXPLORER_SEARCH_THREAD

#include "ItemSearch.h"
#include "TreeColumn.h"
#include "TreeModel.h"
#include "UFEUIAPI.h"

#include <QtCore/QThread.h>

namespace UfeUi {

class TreeModel;

/**
 * \brief Thread used to identify specific UFE items within a UFE subtree.
 */
class UFEUIAPI ExplorerSearchThread : public QThread
{
public:
    /**
     * \brief Constructor.
     * \param rootItem The root Ufe scene item we are searching from.
     * \param columns Columns in for the explorer.
     * \param searchFilter The search filter against which to try and match UFE item in the Scene.
     * \param TypeFilter The Type filtering config. Include or exclude item types by name.
     * \param childFilter Ufe Hierarchy child filter, filters item when traversing the
     * hierarchy. Used by the runtime hierarchy implementation.
     * \param parent A reference to the parent of the thread.
     */
    explicit ExplorerSearchThread(
        const Ufe::SceneItem::Ptr&         rootItem,
        const TreeColumns&                 columns,
        const std::string&                 searchFilter,
        const TypeFilter&                  typeFilter,
        const Ufe::Hierarchy::ChildFilter& childFilter,
        QObject*                           parent = nullptr);

    /**
     * \brief Consume the TreeModel built from the results of the search performed within the UFE subtree.
     * \return The TreeModel built from the results of the search performed within the UFE subtree.
     */
    std::unique_ptr<TreeModel> consumeResults();

protected:
    /**
     * \brief Perform the search in the Qt thread.
     */
    void run() override;

    Ufe::SceneItem::Ptr _rootItem;
    TreeColumns         _columns;

    /// Search filter against which to try and match UFE items in the scene:
    std::string _searchFilter;
    /// Type fitltering configuration :
    TypeFilter _typeFilter;
    /// Hierarchy child filter, used to filter children when traversing.
    Ufe::Hierarchy::ChildFilter _childFilter;
    /// Reference to the TreeModel built from the search performed within the UFE scene:
    std::unique_ptr<TreeModel> _results;
};

} // namespace UfeUi

#endif // UFEUI_EXPLORER_SEARCH_THREAD
