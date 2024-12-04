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
#include "testUfeRuntime.h"

#include "utils.h"

#include <stdexcept>
#include <unordered_map>

namespace UfeUiTest {

static const std::string notImplementedMsg = "Illegal call to unimplemented method.";

TestSceneItem::TestSceneItem(const Ufe::Path& p)
    : SceneItem(p)
{
}
TestSceneItem::~TestSceneItem() = default;

Ufe::UndoableCommandPtr TestSceneItem::clearMetadataCmd(const std::string& key) { return nullptr; }

Ufe::Value TestSceneItem::getGroupMetadata(const std::string& group, const std::string& key) const
{
    return {};
}

Ufe::UndoableCommandPtr TestSceneItem::setGroupMetadataCmd(
    const std::string& group,
    const std::string& key,
    const Ufe::Value&  value)
{
    return nullptr;
}

Ufe::UndoableCommandPtr
TestSceneItem::clearGroupMetadataCmd(const std::string& group, const std::string& key)
{
    return nullptr;
}

void        TestSceneItem::setHideable(bool hideable) { _hideable = hideable; }
bool        TestSceneItem::isHideable() { return _hideable; }
std::string TestSceneItem::nodeType() const { return "TestSceneItemType"; }

Ufe::Value TestSceneItem::getMetadata(const std::string&) const { return {}; }

Ufe::UndoableCommandPtr
TestSceneItem::setMetadataCmd(const std::string& key, const Ufe::Value& value)
{
    return nullptr;
}

void TestSceneItem::setMetadata(const std::string& key, const Ufe::Value& value) { }

TestHierarchy::TestHierarchy(const TestSceneItem::Ptr& item) { _item = item; }

Ufe::SceneItem::Ptr TestHierarchy::sceneItem() const { return _item; }

bool TestHierarchy::hasChildren() const { return !_childrenMap[_item->path()].empty(); }

Ufe::SceneItemList TestHierarchy::children() const
{
    Ufe::SceneItemList list;
    const auto&        children = _childrenMap[_item->path()];
    for (const auto& child : children) {
        list.push_back(createItem(child));
    }
    return list;
}

bool TestHierarchy::hasFilteredChildren(const ChildFilter&) const
{
    throw std::logic_error(notImplementedMsg);
}

Ufe::SceneItemList TestHierarchy::filteredChildren(const ChildFilter& filter) const
{
    if (filter.empty() || filter.front().name != "testFilter" || !filter.front().value) {
        return children();
    }
    // Arbitrarily filter out A2, B2, C2.
    Ufe::SceneItemList list;
    const auto&        children = _childrenMap[_item->path()];
    for (const auto& child : children) {
        if (child == A2 || child == B2 || child == C2) {
            continue;
        }
        list.push_back(createItem(child));
    }
    return list;
}

Ufe::SceneItem::Ptr TestHierarchy::parent() const { return createItem(_parentMap[_item->path()]); }

Ufe::SceneItem::Ptr TestHierarchy::defaultParent() const
{
    throw std::logic_error(notImplementedMsg);
}

Ufe::SceneItem::Ptr
TestHierarchy::insertChild(const Ufe::SceneItem::Ptr& child, const Ufe::SceneItem::Ptr& pos)
{
    throw std::logic_error(notImplementedMsg);
}

Ufe::InsertChildCommand::Ptr
TestHierarchy::insertChildCmd(const Ufe::SceneItem::Ptr& child, const Ufe::SceneItem::Ptr& pos)
{
    throw std::logic_error(notImplementedMsg);
}

Ufe::SceneItem::Ptr TestHierarchy::createGroup(const Ufe::PathComponent& name) const
{
    throw std::logic_error(notImplementedMsg);
}

Ufe::InsertChildCommand::Ptr TestHierarchy::createGroupCmd(const Ufe::PathComponent& name) const
{
    throw std::logic_error(notImplementedMsg);
}

Ufe::UndoableCommand::Ptr TestHierarchy::reorderCmd(const Ufe::SceneItemList& orderedList) const
{
    throw std::logic_error(notImplementedMsg);
}

Ufe::UndoableCommand::Ptr TestHierarchy::ungroupCmd() const
{
    throw std::logic_error(notImplementedMsg);
}

// Setup fake hierarchy like this :
//           - B1
//      - A1 - B2 - C1
// root - A2      - C2
//      - A3

const Ufe::Path TestHierarchy::root = getUfePath("/root");
const Ufe::Path TestHierarchy::A1 = getUfePath("/root/A1");
const Ufe::Path TestHierarchy::A2 = getUfePath("/root/A2");
const Ufe::Path TestHierarchy::A3 = getUfePath("/root/A3");
const Ufe::Path TestHierarchy::B1 = getUfePath("/root/A1/B1");
const Ufe::Path TestHierarchy::B2 = getUfePath("/root/A1/B2");
const Ufe::Path TestHierarchy::C1 = getUfePath("/root/A1/B2/C1");
const Ufe::Path TestHierarchy::C2 = getUfePath("/root/A1/B2/C2");

std::unordered_map<Ufe::Path, std::vector<Ufe::Path>> TestHierarchy::_childrenMap;
std::unordered_map<Ufe::Path, Ufe::Path>              TestHierarchy::_parentMap;

void TestHierarchy::resetTestHierarchy()
{
    _childrenMap.clear();
    _parentMap.clear();

    addChild(root, A1);
    addChild(root, A2);
    addChild(root, A3);

    addChild(A1, B1);
    addChild(A1, B2);

    addChild(B2, C1);
    addChild(B2, C2);
}

void TestHierarchy::addChild(const Ufe::Path& parent, const Ufe::Path& child)
{
    _childrenMap[parent].push_back(child);
    _parentMap[child] = parent;
}

void TestHierarchy::clearChildren(const Ufe::Path& parent)
{
    auto& children = _childrenMap[parent];
    for (const auto& child : children) {
        _parentMap.erase(child);
    }
    children.clear();
}

void TestHierarchy::removeChild(const Ufe::Path& child)
{
    const auto parent = _parentMap[child];
    auto&      children = _childrenMap[parent];

    const auto itc = std::find(children.begin(), children.end(), child);
    if (itc != children.end()) {
        children.erase(itc);
    }
    const auto itp = _parentMap.find(child);
    if (itp != _parentMap.end()) {
        _parentMap.erase(itp);
    }
}

Ufe::Hierarchy::ChildFilter TestHierarchy::childFilter()
{
    ChildFilter childFilter;
    childFilter.emplace_back("testFilter", "Test Filter Label", true);
    return childFilter;
}

std::unordered_map<Ufe::Path, bool> TestObject3d::visMap;

TestObject3d::TestObject3d(const TestSceneItem::Ptr& item) { _item = item; }

Ufe::SceneItem::Ptr TestObject3d::sceneItem() const { return _item; }

Ufe::BBox3d TestObject3d::boundingBox() const { return Ufe::BBox3d {}; }

bool TestObject3d::visibility() const
{
    const auto it = visMap.find(_item->path());
    if (it == visMap.end()) {
        return true;
    }
    return it->second;
}

void TestObject3d::setVisibility(bool vis) { visMap[_item->path()] = vis; }

class SetVisCommand : public Ufe::UndoableCommand
{
public:
    SetVisCommand(const Ufe::SceneItem::Ptr& object, bool vis)
        : _object(std::make_shared<TestObject3d>(std::dynamic_pointer_cast<TestSceneItem>(object)))
        , _vis(vis)
    {
    }

private:
    void execute() override { _object->setVisibility(_vis); }
    void undo() override { _object->setVisibility(!_vis); }
    void redo() override { _object->setVisibility(_vis); }

    TestObject3d::Ptr _object;
    bool              _vis;
};

Ufe::UndoableCommand::Ptr TestObject3d::setVisibleCmd(bool vis)
{
    return std::make_shared<SetVisCommand>(sceneItem(), vis);
}

void TestObject3d::clearVisMap() { visMap.clear(); }

TestObject3dHandler::Ptr TestObject3dHandler::create()
{
    return std::make_shared<TestObject3dHandler>();
}

Ufe::Object3d::Ptr TestObject3dHandler::object3d(const Ufe::SceneItem::Ptr& item) const
{
    TestSceneItem::Ptr testItem = std::dynamic_pointer_cast<TestSceneItem>(item);
    if (!testItem || !testItem->isHideable()) {
        return nullptr;
    }
    return std::make_shared<TestObject3d>(testItem);
}

TestHierarchyHandler::TestHierarchyHandler() { }

TestHierarchyHandler::~TestHierarchyHandler() { }

TestHierarchyHandler::Ptr TestHierarchyHandler::create()
{
    return std::make_shared<TestHierarchyHandler>();
}

Ufe::Hierarchy::Ptr TestHierarchyHandler::hierarchy(const Ufe::SceneItem::Ptr& item) const
{
    auto testItem = std::dynamic_pointer_cast<TestSceneItem>(item);
    if (!testItem) {
        return nullptr;
    }
    return std::make_shared<TestHierarchy>(testItem);
}

Ufe::SceneItem::Ptr TestHierarchyHandler::createItem(const Ufe::Path& path) const
{
    return std::make_shared<TestSceneItem>(path);
}

Ufe::Hierarchy::ChildFilter TestHierarchyHandler::childFilter() const { return {}; }

} // namespace UfeUiTest