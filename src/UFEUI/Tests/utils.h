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

#include "testUfeRuntime.h"

#include <UFEUI/treeModel.h>

#include <ufe/sceneItem.h>

//! Test utilities.
namespace UfeUiTest {

static const char separator = '/';

//! Build a UFE path with a single segment, from a string.
Ufe::Path getUfePath(std::string path);

//! Creates an empty tree model.
std::shared_ptr<UfeUi::TreeModel> createEmptyModel();

//! Creates a treeItem from a model and ufe path.
std::shared_ptr<UfeUi::TreeItem> createTreeItem(UfeUi::TreeModel* model, const Ufe::Path& path);

//! Compare icons by baking to images, and comparing the results.
bool areIconsEqual(const QIcon& icon1, const QIcon& icon2);

//! Dummy subject, to be used when testing the Ufe explorer.
class TestSubject : public Ufe::Subject
{
};
static TestSubject testSubject;

//! Test tree column implementation.
class TestColumn : public UfeUi::TreeColumn
{
public:
    const static QString        DefaultDisplayData;
    const static Qt::CheckState DefaultCheckStateData;

    using TreeColumn::TreeColumn;

    QVariant columnHeader(int role) const override;
    QVariant data(const UfeUi::TreeItem*, int role) const override;
    void     flags(const UfeUi::TreeItem*, Qt::ItemFlags& flags) override;
    bool     setData(const UfeUi::TreeItem*, const QVariant& value, int role) override;

private:
    // Dummy backing model
    std::unordered_map<Ufe::Path, std::pair<QString, Qt::CheckState>> _data;
};
} // namespace UfeUiTest
;