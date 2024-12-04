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
#include "treeItem.h"

#include <Ufe/object3d.h>

namespace UfeUi {

TreeItem::TreeItem(TreeModel* model, const Ufe::SceneItem::Ptr& sceneItem)
    : _parent(nullptr)
    , _item(sceneItem)
    , _model(model)
    , _uniqueId(hash(this))
{
}

TreeItem::~TreeItem() { _model->_treeItemMap.remove(_uniqueId); }

TreeItem* TreeItem::appendChild(const Ufe::SceneItem::Ptr& sceneItem)
{
    const auto row = static_cast<int>(_children.size());
    _children.emplace_back(std::make_unique<TreeItem>(_model, sceneItem));
    // In CPP < 17, emplace_back doesnt return a reference to the inserted element.
    const auto& treeItem = _children.back();

    treeItem->setParent(this);
    const auto uniqueId = treeItem->uniqueId();
    const auto idx = _model->createIndex(row, 0, uniqueId);
    _model->_treeItemMap.insert(uniqueId, { idx, treeItem.get() });
    return treeItem.get();
}

TreeItem* TreeItem::child(int row) const
{
    if (row < 0 || row >= _children.size()) {
        return nullptr;
    }
    return _children.at(row).get();
}

size_t TreeItem::childCount() const { return _children.size(); }

TreeItem* TreeItem::parentItem() const { return _parent; }

size_t TreeItem::uniqueId() const { return _uniqueId; }

bool TreeItem::computedVisibility() const
{
    bool hidden = false;

    // Do we have a cache of the disabled stage?
    if (_visCache != VisStateCache::None) {
        return _visCache == VisStateCache::Visible;
    }

    // Is the parent hidden?
    if (_parent && !_parent->computedVisibility()) {
        _visCache = VisStateCache::Hidden;
        return false;
    }

    // Ancestors are not hidden, query the authored visibility.
    if (const auto object3d = Ufe::Object3d::object3d(sceneItem())) {
        hidden = !object3d->visibility();
    }

    this->_visCache = hidden ? VisStateCache::Hidden : VisStateCache::Visible;
    return !hidden;
}

void TreeItem::clearChildren() { _children.clear(); }

void TreeItem::removeChild(TreeItem* item)
{
    const auto it
        = std::find_if(_children.begin(), _children.end(), [&item](const TreeItem::Ptr& child) {
              return child->uniqueId() == item->uniqueId();
          });
    if (it != _children.end()) {
        _children.erase(it);
    }
}

void _findDescendantsRecursive(
    const TreeItem*                      item,
    std::function<bool(const TreeItem*)> predicate,
    std::vector<TreeItem*>&              found)
{
    for (int i = 0; i < item->childCount(); ++i) {
        const auto& child = item->child(i);
        if (predicate(child)) {
            found.push_back(child);

            _findDescendantsRecursive(child, predicate, found);
        }
    }
}

TreeItem::TreeItems
TreeItem::findDescendants(const std::function<bool(const TreeItem*)>& predicate) const
{
    TreeItems found;
    _findDescendantsRecursive(this, predicate, found);
    return found;
}

void TreeItem::clearStateCache()
{
    _visCache = VisStateCache::None;
    for (const auto& child : _children) {
        child->clearStateCache();
    }
}

size_t TreeItem::hash(const TreeItem* item)
{
    if (!item) {
        return 0;
    }
    const auto sceneItem = item->sceneItem();
    if (!sceneItem) {
        return 0;
    }
    return sceneItem->path().hash();
}

void TreeItem::setParent(TreeItem* parent) { _parent = parent; }

int TreeItem::row() const
{
    if (_parent) {
        const auto it = std::find_if(
            _parent->_children.begin(),
            _parent->_children.end(),
            [this](const TreeItem::Ptr& child) { return child->uniqueId() == uniqueId(); });
        // Should never happen...
        if (it == _parent->_children.end()) {
            return 0;
        }
        return static_cast<int>(std::distance(_parent->_children.begin(), it));
    }
    return 0;
}

} // namespace UfeUi
