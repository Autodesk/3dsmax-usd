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

#include "wrapPickItems.h"

#include <MaxUsdObjects/MaxUsdUfe/UfeUtils.h>

#include <UFEUI/StandardTreeColumns.h>
#include <UFEUI/Views/explorer.h>

#include <BoostPythonWrapper.h>

#include <usdUfe/ufe/Global.h>

#include <pxr/pxr.h>

#include <Qt/QmaxMainWindow.h>
#include <ufe/runTimeMgr.h>
#include <ufe/scene.h>
#include <ufe/selection.h>

#include <QtWidgets>
#include <boost/python/def.hpp>
#include <max.h>
#include <pybind11/pybind11.h>
#include <qapplication.h>

WrappingPickModeCallback::WrappingPickModeCallback(const UfeUi::Explorer::PickMode* pickMode)
    : QObject(nullptr)
    , _pickMode(pickMode)
{
}

WrappingPickModeCallback::~WrappingPickModeCallback() = default;

void WrappingPickModeCallback::selected(const Ufe::Path& path)
{
    if (_selection.append(Ufe::Hierarchy::createItem(path))) {
        Q_EMIT selectedSignal(path);
        Q_EMIT selectionChanged(_selection);
    }
}

void WrappingPickModeCallback::deSelected(const Ufe::Path& path)
{
    if (_selection.remove(Ufe::Hierarchy::createItem(path))) {
        Q_EMIT deSelectedSignal(path);
        Q_EMIT selectionChanged(_selection);
    }
}

void WrappingPickModeCallback::exited(bool userCancelled) { Q_EMIT exitedSignal(userCancelled); }

PXR_NAMESPACE_USING_DIRECTIVE

PyObject* _pickItems(pxr::UsdStagePtr stage, const std::string& dialogTitle = "")
{
    if (!stage) {
        return nullptr;
    }
    auto rootSceneItem = Ufe::Hierarchy::createItem(MaxUsd::ufe::getStagePath(stage));
    if (!rootSceneItem) {
        return nullptr;
    }

    QPointer<QDialog> dialog = new QDialog(GetCOREInterface()->GetQmaxMainWindow());
    if (dialogTitle.empty()) {
        const auto        layerNameWithExt = stage->GetRootLayer()->GetDisplayName();
        const size_t      lastIndex = layerNameWithExt.find_last_of(".");
        const std::string layerName = layerNameWithExt.substr(0, lastIndex);
        dialog->setWindowTitle(QString("Pick from %1").arg(QString::fromStdString(layerName)));
    } else {
        dialog->setWindowTitle(QString::fromStdString(dialogTitle));
    }

    dialog->setWindowFlags(Qt::Dialog | Qt::WindowCloseButtonHint);
    dialog->setMinimumSize(400, 600);
    dialog->setAttribute(Qt::WA_DeleteOnClose);

    dialog->setSizePolicy(QSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding));
    dialog->setLayout(new QVBoxLayout());

    UfeUi::TreeColumns columns { std::make_shared<NameColumn>("root", 0),
                                 std::make_shared<TypeColumn>(1) };

    const UfeUi::Explorer::ColorScheme colors = {
        // match item color in 3dsMax.
        QApplication::palette().color(QPalette::Inactive, QPalette::Button).name(),
        // match item selected color in 3dsMax.
        QApplication::palette().color(QPalette::Normal, QPalette::Light).name(),
        // match item selected/hovered color in 3dsMax
        QApplication::palette().color(QPalette::Inactive, QPalette::Light).name(),
    };

    const QString treeViewBranchAdjustStyle
        = QString { "	QTreeView, QTreeWidget{"
                    "	    show-decoration-selected: 1;"
                    "	}"
                    "	QTreeView:branch:hover {"
                    "		background-color: %1;"
                    "	}"
                    "	QTreeView:branch:selected {"
                    "		background-color: %2;"
                    "	}"
                    "	QTreeView:branch:selected:hover {"
                    "		background-color: %3;"
                    "	}"
                    "	QTreeView::branch:open{"
                    "		padding: 0.35em;"
                    "	}"
                    "	QTreeView::branch:closed{"
                    "		padding: 0.35em;"
                    "	}"
                    "	QTreeView::branch:open:has-children{"
                    "		image: url(:/ufe/Icons/branch_opened.png);"
                    "	}"
                    "	QTreeView::branch:closed:has-children {"
                    "		image: url(:/ufe/Icons/branch_closed.png);"
                    "	}" }
              .arg(colors.hover.name(), colors.selected.name(), colors.selectedHover.name());

    UfeUi::TypeFilter typeFilter;

    const auto handler = Ufe::RunTimeMgr::instance().hierarchyHandler(UsdUfe::getUsdRunTimeId());
    Ufe::Hierarchy::ChildFilter childFilter = handler->childFilter();

    auto explorer = new UfeUi::Explorer(
        rootSceneItem,
        columns,
        typeFilter,
        childFilter,
        false,
        treeViewBranchAdjustStyle,
        colors,
        dialog);
    dialog->layout()->addWidget(explorer);

    QPointer<QDialogButtonBox> buttonBox
        = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    dialog->layout()->addWidget(buttonBox);
    QObject::connect(buttonBox, &QDialogButtonBox::accepted, dialog, &QDialog::accept);
    QObject::connect(buttonBox, &QDialogButtonBox::rejected, dialog, &QDialog::reject);

    buttonBox->button(QDialogButtonBox::Ok)->setEnabled(false);

    auto pickmode = explorer->enterPickMode();
    if (!pickmode) {
        return nullptr;
    }
    auto cb = std::make_shared<WrappingPickModeCallback>(pickmode.get());
    pickmode->addCallback(cb);

    QObject::connect(
        cb.get(),
        &WrappingPickModeCallback::selectionChanged,
        buttonBox,
        [buttonBox](const Ufe::Selection& selection) {
            if (buttonBox) {
                if (auto btn = buttonBox->button(QDialogButtonBox::Ok)) {
                    btn->setEnabled(!selection.empty());
                    btn->update();
                }
            }
        });

    if (dialog->exec() == QDialog::Accepted) {
        auto selection = cb->selectedPrims();
        auto result = pybind11::cast(selection);
        return result.release().ptr();
    }
    return nullptr;
}

void wrapPickItems()
{
    pyboost::def(
        "PickItems",
        _pickItems,
        (pyboost::arg("stage"), pyboost::arg("dialogTitle") = ""),
        "Picks one or more items from the stage object");
}
