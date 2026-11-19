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
#pragma once

#include <AdskUsdComponentCreator/Infos/CreatorInfo.h>
#include <QWidget>
#include <memory>

class QTreeWidget;

// Read-only tree preview of the component's prim hierarchy (component structure report).
class ComponentPrimHierarchyRollup : public QWidget
{
public:
    explicit ComponentPrimHierarchyRollup(QWidget* parent = nullptr);
    void SetFromInfo(const AdskUsdComponentCreator::CreatorInfo::Ptr& info);

private:
    QTreeWidget* tree = nullptr;
};

// Read-only tree preview of the component's on-disk file structure (file structure report).
class ComponentFileStructureRollup : public QWidget
{
public:
    explicit ComponentFileStructureRollup(QWidget* parent = nullptr);
    void SetFromInfo(const AdskUsdComponentCreator::CreatorInfo::Ptr& info);

private:
    QTreeWidget* tree = nullptr;
};
