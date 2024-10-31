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

#include "utils.h"

#include <ufe/hierarchy.h>
#include <ufe/hierarchyHandler.h>
#include <ufe/object3dHandler.h>
#include <ufe/sceneItem.h>
#include <ufe/types.h>

#include <unordered_map>

namespace UfeUiTest {

/* Dummy UFE runtime implementation */

class TestSceneItem : public Ufe::SceneItem
{
public:
    using Ptr = std::shared_ptr<TestSceneItem>;

    TestSceneItem(const Ufe::Path& p);
    ~TestSceneItem() override;

    std::string nodeType() const override;
    Ufe::Value  getMetadata(const std::string&) const override;
    Ufe::UndoableCommandPtr
         setMetadataCmd(const std::string& key, const Ufe::Value& value) override;
    void setMetadata(const std::string& key, const Ufe::Value& value) override;
    Ufe::UndoableCommandPtr clearMetadataCmd(const std::string& key = "") override;
    Ufe::Value getGroupMetadata(const std::string& group, const std::string& key) const override;
    Ufe::UndoableCommandPtr setGroupMetadataCmd(
        const std::string& group,
        const std::string& key,
        const Ufe::Value&  value) override;
    Ufe::UndoableCommandPtr
    clearGroupMetadataCmd(const std::string& group, const std::string& key = "") override;

    // For testing.
    void setHideable(bool hideable);
    bool isHideable();

private:
    bool _hideable = true;
};

class TestObject3d : public Ufe::Object3d
{
public:
    using Ptr = std::shared_ptr<TestObject3d>;

    TestObject3d(const TestSceneItem::Ptr& item);
    Ufe::SceneItem::Ptr       sceneItem() const override;
    Ufe::BBox3d               boundingBox() const override;
    bool                      visibility() const override;
    void                      setVisibility(bool vis) override;
    Ufe::UndoableCommand::Ptr setVisibleCmd(bool vis) override;

    // Resetting the backing state.
    static void clearVisMap();

private:
    TestSceneItem::Ptr _item = nullptr;

    // Fake a backing runtime, keeping track of vis/hidden items.
    static std::unordered_map<Ufe::Path, bool> visMap;
};

class TestHierarchy : public Ufe::Hierarchy
{
public:
    TestHierarchy(const TestSceneItem::Ptr& item);

    Ufe::SceneItem::Ptr sceneItem() const override;
    bool                hasChildren() const override;
    Ufe::SceneItemList  children() const override;
    bool                hasFilteredChildren(const ChildFilter&) const override;
    Ufe::SceneItemList  filteredChildren(const ChildFilter&) const override;
    Ufe::SceneItem::Ptr parent() const override;
    Ufe::SceneItem::Ptr defaultParent() const override;
    Ufe::SceneItem::Ptr
    insertChild(const Ufe::SceneItem::Ptr& child, const Ufe::SceneItem::Ptr& pos) override;
    Ufe::InsertChildCommand::Ptr
    insertChildCmd(const Ufe::SceneItem::Ptr& child, const Ufe::SceneItem::Ptr& pos) override;
    Ufe::SceneItem::Ptr          createGroup(const Ufe::PathComponent& name) const override;
    Ufe::InsertChildCommand::Ptr createGroupCmd(const Ufe::PathComponent& name) const override;
    Ufe::UndoableCommand::Ptr    reorderCmd(const Ufe::SceneItemList& orderedList) const override;
    Ufe::UndoableCommand::Ptr    ungroupCmd() const override;

    // Manipulate fake runtime hierarchy.
    static void                        resetTestHierarchy();
    static void                        addChild(const Ufe::Path& parent, const Ufe::Path& child);
    static void                        clearChildren(const Ufe::Path& parent);
    static void                        removeChild(const Ufe::Path& child);
    static Ufe::Hierarchy::ChildFilter childFilter();

    static const Ufe::Path root;
    static const Ufe::Path A1;
    static const Ufe::Path A2;
    static const Ufe::Path A3;
    static const Ufe::Path B1;
    static const Ufe::Path B2;
    static const Ufe::Path C1;
    static const Ufe::Path C2;

private:
    TestSceneItem::Ptr _item = nullptr;

    // Fake backing model.
    static std::unordered_map<Ufe::Path, std::vector<Ufe::Path>> _childrenMap;
    static std::unordered_map<Ufe::Path, Ufe::Path>              _parentMap;
};

class TestHierarchyHandler : public Ufe::HierarchyHandler
{
public:
    typedef std::shared_ptr<TestHierarchyHandler> Ptr;

    TestHierarchyHandler();
    ~TestHierarchyHandler() override;

    static TestHierarchyHandler::Ptr create();

    Ufe::Hierarchy::Ptr         hierarchy(const Ufe::SceneItem::Ptr& item) const override;
    Ufe::SceneItem::Ptr         createItem(const Ufe::Path& path) const override;
    Ufe::Hierarchy::ChildFilter childFilter() const override;
};

class TestObject3dHandler : public Ufe::Object3dHandler
{
public:
    static TestObject3dHandler::Ptr create();

    Ufe::Object3d::Ptr object3d(const Ufe::SceneItem::Ptr& item) const override;
};

} // namespace UfeUiTest