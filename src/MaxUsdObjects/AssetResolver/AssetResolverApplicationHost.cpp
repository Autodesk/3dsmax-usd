//
// Copyright 2025 Autodesk
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

#include "AssetResolverApplicationHost.h"

#ifdef ADSK_ASSET_RESOLVER_ENABLED

#include <maxicon.h>
#include <winutil.h>

#include <pxr/base/tf/notice.h>
#include <pxr/base/tf/type.h>

AssetResolverApplicationHost* AssetResolverApplicationHost::s_instance = nullptr;

AssetResolverApplicationHost::AssetResolverApplicationHost(QObject* parent)
    : Adsk::ApplicationHost(parent)
{
    Adsk::ApplicationHost::injectInstance(this);

    // Register a observer to redraw the views when the context data change is completed, which
    // allows the viewport to update the display of assets when the resolver settings are changed
    pxr::TfNotice::Register(pxr::TfCreateWeakPtr(this), &AssetResolverApplicationHost::RefreshViewports);
}

void AssetResolverApplicationHost::CreateInstance(QObject* parent)
{
    if (!s_instance) {
        s_instance = new AssetResolverApplicationHost(parent);
    }
}

float AssetResolverApplicationHost::uiScale() const { return MaxSDK::GetUIScaleFactor(); }

QIcon AssetResolverApplicationHost::icon(const IconName& name) const
{
    auto          colorman = GetColorManager();
    const QString theme = colorman
        ? colorman->GetAppFrameColorTheme() == IColorManager::AppFrameColorTheme::kDarkTheme
            ? "dark"
            : "light"
        : "dark";
    switch (name) {
    case IconName::Add: {
        QIcon icon;
        icon.addFile(QString(":/AdskAssetResolver/icons/%1/add_100.png").arg(theme));
        icon.addFile(QString(":/AdskAssetResolver/icons/%1/add_150.png").arg(theme));
        icon.addFile(QString(":/AdskAssetResolver/icons/%1/add_200.png").arg(theme));
        return icon;
    }
    case IconName::AddFolder: {
        QIcon icon;
        icon.addFile(QString(":/AdskAssetResolver/icons/%1/addFolder_100.png").arg(theme));
        icon.addFile(QString(":/AdskAssetResolver/icons/%1/addFolder_150.png").arg(theme));
        icon.addFile(QString(":/AdskAssetResolver/icons/%1/addFolder_200.png").arg(theme));
        return icon;
    }
    case IconName::OpenFile: {
        QIcon icon;
        icon.addFile(QString(":/AdskAssetResolver/icons/%1/browse_100.png").arg(theme));
        icon.addFile(QString(":/AdskAssetResolver/icons/%1/browse_150.png").arg(theme));
        icon.addFile(QString(":/AdskAssetResolver/icons/%1/browse_200.png").arg(theme));
        return icon;
    }
    case IconName::Delete: {
        QIcon icon;
        icon.addFile(QString(":/AdskAssetResolver/icons/%1/trash_100.png").arg(theme));
        icon.addFile(QString(":/AdskAssetResolver/icons/%1/trash_150.png").arg(theme));
        icon.addFile(QString(":/AdskAssetResolver/icons/%1/trash_200.png").arg(theme));
        return icon;
    }
    case IconName::MoveUp: {
        QIcon icon;
        icon.addFile(QString(":/AdskAssetResolver/icons/%1/moveUp_100.png").arg(theme));
        icon.addFile(QString(":/AdskAssetResolver/icons/%1/moveUp_150.png").arg(theme));
        icon.addFile(QString(":/AdskAssetResolver/icons/%1/moveUp_200.png").arg(theme));
        return icon;
    }
    case IconName::MoveDown: {
        QIcon icon;
        icon.addFile(QString(":/AdskAssetResolver/icons/%1/moveDown_100.png").arg(theme));
        icon.addFile(QString(":/AdskAssetResolver/icons/%1/moveDown_150.png").arg(theme));
        icon.addFile(QString(":/AdskAssetResolver/icons/%1/moveDown_200.png").arg(theme));
        return icon;
    }
    case IconName::Gripper: {
        QIcon icon;
        icon.addFile(QString(":/AdskAssetResolver/icons/%1/gripper_100.png").arg(theme));
        icon.addFile(QString(":/AdskAssetResolver/icons/%1/gripper_150.png").arg(theme));
        icon.addFile(QString(":/AdskAssetResolver/icons/%1/gripper_200.png").arg(theme));
        return icon;
    }
    case IconName::GripperActive: {
        QIcon icon;
        icon.addFile(QString(":/AdskAssetResolver/icons/%1/gripper_active_100.png").arg(theme));
        icon.addFile(QString(":/AdskAssetResolver/icons/%1/gripper_active_150.png").arg(theme));
        icon.addFile(QString(":/AdskAssetResolver/icons/%1/gripper_active_200.png").arg(theme));
        return icon;
    }
    case IconName::Lock: {
        QIcon icon;
        icon.addFile(QString(":/AdskAssetResolver/icons/%1/lock_100.png").arg(theme));
        icon.addFile(QString(":/AdskAssetResolver/icons/%1/lock_150.png").arg(theme));
        icon.addFile(QString(":/AdskAssetResolver/icons/%1/lock_200.png").arg(theme));
        return icon;
    }
    case IconName::Unresolved: {
        QIcon icon;
        icon.addFile(QString(":/AdskAssetResolver/icons/%1/unresolved_100.png").arg(theme));
        icon.addFile(QString(":/AdskAssetResolver/icons/%1/unresolved_150.png").arg(theme));
        icon.addFile(QString(":/AdskAssetResolver/icons/%1/unresolved_200.png").arg(theme));
        return icon;
    }
    case IconName::Resolved: {
        QIcon icon;
        icon.addFile(QString(":/AdskAssetResolver/icons/%1/resolved_100.png").arg(theme));
        icon.addFile(QString(":/AdskAssetResolver/icons/%1/resolved_150.png").arg(theme));
        icon.addFile(QString(":/AdskAssetResolver/icons/%1/resolved_200.png").arg(theme));
        return icon;
    }
    case IconName::UnresolvedPreview: {
        QIcon icon;
        icon.addFile(QString(":/AdskAssetResolver/icons/%1/unresolved_preview_100.png").arg(theme));
        icon.addFile(QString(":/AdskAssetResolver/icons/%1/unresolved_preview_150.png").arg(theme));
        icon.addFile(QString(":/AdskAssetResolver/icons/%1/unresolved_preview_200.png").arg(theme));
        return icon;
    }
    case IconName::ResolvedPreview: {
        QIcon icon;
        icon.addFile(QString(":/AdskAssetResolver/icons/%1/resolved_preview_100.png").arg(theme));
        icon.addFile(QString(":/AdskAssetResolver/icons/%1/resolved_preview_150.png").arg(theme));
        icon.addFile(QString(":/AdskAssetResolver/icons/%1/resolved_preview_200.png").arg(theme));
        return icon;
    }
    case IconName::ToggleOff: {
        QIcon icon;
        icon.addFile(QString(":/AdskAssetResolver/icons/%1/toggle_off_100.png").arg(theme));
        icon.addFile(QString(":/AdskAssetResolver/icons/%1/toggle_off_150.png").arg(theme));
        icon.addFile(QString(":/AdskAssetResolver/icons/%1/toggle_off_200.png").arg(theme));
        return icon;
    }
    case IconName::ToggleOn: {
        QIcon icon;
        icon.addFile(QString(":/AdskAssetResolver/icons/%1/toggle_on_100.png").arg(theme));
        icon.addFile(QString(":/AdskAssetResolver/icons/%1/toggle_on_150.png").arg(theme));
        icon.addFile(QString(":/AdskAssetResolver/icons/%1/toggle_on_200.png").arg(theme));
        return icon;
    }
    case IconName::Refresh: {
        QIcon icon;
        icon.addFile(QString(":/AdskAssetResolver/icons/%1/refresh_100.png").arg(theme));
        icon.addFile(QString(":/AdskAssetResolver/icons/%1/refresh_150.png").arg(theme));
        icon.addFile(QString(":/AdskAssetResolver/icons/%1/refresh_200.png").arg(theme));
        return icon;
    }
    case IconName::RefreshHover: {
        QIcon icon;
        icon.addFile(QString(":/AdskAssetResolver/icons/%1/refresh_hover_100.png").arg(theme));
        icon.addFile(QString(":/AdskAssetResolver/icons/%1/refresh_hover_150.png").arg(theme));
        icon.addFile(QString(":/AdskAssetResolver/icons/%1/refresh_hover_200.png").arg(theme));
        return icon;
    }
    default: return QIcon();
    }
}

int AssetResolverApplicationHost::pm(const PixelMetric& metric) const
{
    return static_cast<int>(uiScale() * ApplicationHost::pm(metric));
}

void AssetResolverApplicationHost::RefreshViewports(const Adsk::ArContextDataChangeCompleted&)
{
    GetCOREInterface()->RedrawViews(GetCOREInterface()->GetTime());
}

#endif // ADSK_ASSET_RESOLVER_ENABLED