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

#include "PreferenceApplicationHost.h"

#include <maxicon.h>
#include <winutil.h>

#ifdef IS_MAX2026_OR_GREATER

PreferenceApplicationHost* PreferenceApplicationHost::s_instance = nullptr;

PreferenceApplicationHost::PreferenceApplicationHost(QObject* parent)
    : Adsk::ApplicationHost(parent)
{
    Adsk::ApplicationHost::injectInstance(this);
}

void PreferenceApplicationHost::CreateInstance(QObject* parent)
{
    if (!s_instance) {
        s_instance = new PreferenceApplicationHost(parent);
    }
}

float PreferenceApplicationHost::uiScale() const { return MaxSDK::GetUIScaleFactor(); }

QIcon PreferenceApplicationHost::icon(const IconName& name) const
{
    auto          colorman = GetColorManager();
    const QString theme = colorman
        ? colorman->GetAppFrameColorTheme() == IColorManager::AppFrameColorTheme::kDarkTheme
            ? "dark"
            : "light"
        : "dark";
    switch (name) {
    case IconName::Add: return MaxSDK::LoadMaxMultiResIcon("Common/Plus");
    case IconName::AddFolder: {
        QIcon icon;
        icon.addFile(QString(":/AdskAssetResolver/icons/%1/addFolder_100.png").arg(theme));
        icon.addFile(QString(":/AdskAssetResolver/icons/%1/addFolder_150.png").arg(theme));
        icon.addFile(QString(":/AdskAssetResolver/icons/%1/addFolder_200.png").arg(theme));
        return icon;
    }
    case IconName::OpenFile: return MaxSDK::LoadMaxMultiResIcon("Common/Folder");
    case IconName::Delete: return MaxSDK::LoadMaxMultiResIcon("Common/Delete");
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
    default: return QIcon();
    }
}

int PreferenceApplicationHost::pm(const PixelMetric& metric) const
{
    return static_cast<int>(uiScale() * ApplicationHost::pm(metric));
}

#endif // IS_MAX2026_OR_GREATER