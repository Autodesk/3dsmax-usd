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

#include "UsdTreeColumns.h"

#include "UfeUtils.h"

#include <UFEUI/treeItem.h>

#include <pxr/usd/usd/modelAPI.h>
#include <pxr/usd/usdGeom/imageable.h>

QVariant PurposeColumn::columnHeader(int role) const
{
    if (role == Qt::DisplayRole) {
        return QObject::tr("Purpose");
    }
    if (role == Qt::ToolTipRole) {
        return QObject::tr(
            "Purpose indicates render visibility intention. Blank/Default = no special purpose, "
            "Guide = helpers, Proxy = stand-ins, Render = final render quality.");
    }
    return {};
}

QVariant PurposeColumn::data(const UfeUi::TreeItem* item, int role) const
{
    if (role == Qt::DisplayRole) {
        const auto sceneItem = item->sceneItem();
        if (sceneItem) {
            const auto prim = MaxUsd::ufe::ufePathToPrim(sceneItem->path());

            if (prim.IsA<pxr::UsdGeomImageable>()) {
                const auto   purposeAttr = pxr::UsdGeomImageable(prim).GetPurposeAttr();
                pxr::TfToken purpose;
                purposeAttr.Get(&purpose);
                // The prim's cell is Blank when it has a default purpose
                return purpose == "default" ? QString()
                                            : QString::fromStdString(purpose.GetString());
            }
        }
    }
    return {};
}

QVariant KindColumn::columnHeader(int role) const
{
    if (role == Qt::DisplayRole) {
        return QObject::tr("Kind");
    }
    if (role == Qt::ToolTipRole) {
        return QObject::tr("Kind shows the prim category in a model hierarchy.");
    }
    return {};
}

QVariant KindColumn::data(const UfeUi::TreeItem* item, int role) const
{
    if (role == Qt::DisplayRole) {
        const auto sceneItem = item->sceneItem();
        if (sceneItem) {
            const auto   prim = MaxUsd::ufe::ufePathToPrim(sceneItem->path());
            pxr::TfToken kind;
            if (pxr::UsdModelAPI(prim).GetKind(&kind)) {
                return QString::fromStdString(kind.GetString());
            }
        }
    }
    return {};
}
