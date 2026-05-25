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
#include <MaxUsd/Utilities/OptionUtils.h>

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

QGeometryChangedDialog::QGeometryChangedDialog(QWidget* parent)
    : QDialog(parent)
{
}

void QGeometryChangedDialog::moveEvent(QMoveEvent* event)
{
    QDialog::moveEvent(event);
    Q_EMIT geometryChanged(geometry());
}

void QGeometryChangedDialog::resizeEvent(QResizeEvent* event)
{
    QDialog::resizeEvent(event);
    Q_EMIT geometryChanged(geometry());
}

PXR_NAMESPACE_USING_DIRECTIVE

PyObject* _pickItems(
    pxr::UsdStagePtr   stage,
    const std::string& dialogTitle = "",
    bool               hideRoot = false,
    bool               hideClassPrims = true)
{
    if (!stage) {
        return nullptr;
    }
    auto rootSceneItem = Ufe::Hierarchy::createItem(MaxUsd::ufe::getStagePath(stage));
    if (!rootSceneItem) {
        return nullptr;
    }

    QPointer<QGeometryChangedDialog> dialog
        = new QGeometryChangedDialog(GetCOREInterface()->GetQmaxMainWindow());
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

    UfeUi::TreeColumns columns { std::make_shared<NameColumn>("root", 0, false),
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

    const auto classPrimFilter = std::find_if(
        childFilter.begin(), childFilter.end(), [](const Ufe::ChildFilterFlag& filter) {
            return filter.name == "ClassPrims";
        });
    if (classPrimFilter == childFilter.end()) {
        DbgAssert(0 && _T("Usd Ufe ClassPrims child filter is not initalized."));
    } else {
        classPrimFilter->value = !hideClassPrims;
    }

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

    // Disable drag-and-drop to prevent reparenting in the prim picker.
    explorer->treeView()->setDragDropMode(QAbstractItemView::NoDragDrop);

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

    if (hideRoot) {
        auto rootIndex = explorer->treeView()->model()->index(0, 0);
        if (rootIndex.isValid() && rootIndex.data() == "root") {
            explorer->treeView()->setRootIndex(rootIndex);
        }
    }

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

    QRect savedGeometry = QRect(-1, -1, -1, -1);
    {
        VtDictionary dict;
        MaxUsd::OptionUtils::LoadUiOptions("USD PickItems", dict);
        auto           it = dict.find("Dialog Geometry");
        VtArray<float> val = { -1.0, -1.0, -1.0, -1.0 };
        if (it != dict.end()) {
            if (it->second.IsHolding<VtArray<float>>()) {
                val = it->second.GetWithDefault<VtArray<float>>(val);
            } else if (it->second.CanCast<VtArray<float>>()) {
                val = it->second.Cast<VtArray<float>>().GetWithDefault<VtArray<float>>(val);
            }
        }
        if (val.size() == 4 && val[2] >= 0.0f) {
            savedGeometry = QRect(
                MaxSDK::UIScaled(val[0]),
                MaxSDK::UIScaled(val[1]),
                MaxSDK::UIScaled(val[2]),
                MaxSDK::UIScaled(val[3]));
            dialog->setGeometry(savedGeometry);
        }
    }

    QRect dialogGeometry = savedGeometry;
    QObject::connect(
        dialog, &QGeometryChangedDialog::geometryChanged, [&dialogGeometry](const QRect& geometry) {
            dialogGeometry = geometry;
        });

    QObject::connect(dialog, &QDialog::finished, [&dialogGeometry, &savedGeometry]() {
        if (dialogGeometry != savedGeometry) {
            VtDictionary   dict;
            VtArray<float> val
                = { MaxSDK::UIUnScaled(static_cast<float>(dialogGeometry.left())),
                    MaxSDK::UIUnScaled(static_cast<float>(dialogGeometry.top())),
                    MaxSDK::UIUnScaled(static_cast<float>(dialogGeometry.width())),
                    MaxSDK::UIUnScaled(static_cast<float>(dialogGeometry.height())) };
            dict["Dialog Geometry"] = val;
            MaxUsd::OptionUtils::SaveUiOptions("USD PickItems", dict);
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
        (pyboost::arg("stage"),
         pyboost::arg("dialogTitle") = "",
         pyboost::arg("hideRoot") = true,
         pyboost::arg("hideClassPrims") = true),
        "Picks one or more items from the stage object");
}
