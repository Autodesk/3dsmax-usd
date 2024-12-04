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
#include "utils.h"

#include <UFEUI/treeItem.h>

#include <QtGui/QIcon.h>
#include <QtGui/QPalette.h>
#include <QtWidgets/QApplication.h>

namespace UfeUiTest {

Ufe::Path getUfePath(std::string path)
{
    auto rootSeg = Ufe::PathSegment(path, 1, separator);
    return Ufe::Path { { Ufe::PathSegment(path, 1, separator) } };
}

const QString        TestColumn::DefaultDisplayData = QString("Default");
const Qt::CheckState TestColumn::DefaultCheckStateData = Qt::CheckState::Checked;

QVariant TestColumn::columnHeader(int role) const
{
    if (role == Qt::DisplayRole) {
        return QString("TestColumnHeader")
            .append(QString::fromStdString(std::to_string(visualIndex())));
    }
    return {};
}

QVariant TestColumn::data(const UfeUi::TreeItem* treeItem, int role) const
{
    const auto it = _data.find(treeItem->sceneItem()->path());

    switch (role) {
    case Qt::CheckStateRole: {
        return it == _data.end() ? DefaultCheckStateData : it->second.second;
    }
    case Qt::DisplayRole: {
        auto str = it == _data.end() ? DefaultDisplayData : it->second.first;
        return str;
    }
    case Qt::ForegroundRole: {
        if (!treeItem->computedVisibility()) {
            return QApplication::palette().color(QPalette::Disabled, QPalette::WindowText);
        }
        return QVariant {};
    }
    }
    return QVariant {};
}

void TestColumn::flags(const UfeUi::TreeItem* treeItem, Qt::ItemFlags& flags)
{
    if (visualIndex() == 0) {
        flags.setFlag(Qt::ItemIsUserCheckable, true);
        flags.setFlag(Qt::ItemIsEnabled, false);
    } else if (visualIndex() == 1) {
        flags.setFlag(Qt::ItemIsUserCheckable, false);
        flags.setFlag(Qt::ItemIsEnabled, true);
    }
}

bool TestColumn::setData(const UfeUi::TreeItem* treeItem, const QVariant& value, int role)
{
    const Ufe::Path key = treeItem->sceneItem()->path();

    switch (role) {
    case Qt::CheckStateRole:
        _data[key].second = static_cast<Qt::CheckState>(value.toInt());
        return true;

    case Qt::DisplayRole: _data[key].first = value.toString(); return true;
    }
    return false;
}

std::shared_ptr<UfeUi::TreeModel> createEmptyModel()
{
    UfeUi::TreeColumns columns;
    return std::make_shared<UfeUi::TreeModel>(columns, nullptr);
}

std::shared_ptr<UfeUi::TreeItem> createTreeItem(UfeUi::TreeModel* model, const Ufe::Path& path)
{
    return std::make_shared<UfeUi::TreeItem>(model, Ufe::Hierarchy::createItem(path));
}

bool areIconsEqual(const QIcon& icon1, const QIcon& icon2)
{
    auto image1 = icon1.pixmap(16, 16).toImage();
    auto image2 = icon2.pixmap(16, 16).toImage();
    return image1 == image2;
};

} // namespace UfeUiTest