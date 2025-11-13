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
    switch (name) {
    case IconName::Add: return MaxSDK::LoadMaxMultiResIcon(QString("Common/%1").arg("Plus"));
    case IconName::OpenFile: return MaxSDK::LoadMaxMultiResIcon(QString("Common/%1").arg("Folder"));
    case IconName::Delete: return MaxSDK::LoadMaxMultiResIcon(QString("Common/%1").arg("Delete"));
    case IconName::MoveUp: return MaxSDK::LoadMaxMultiResIcon(QString("Common/%1").arg("ArrowUp"));
    case IconName::MoveDown:
        return MaxSDK::LoadMaxMultiResIcon(QString("Common/%1").arg("ArrowDown"));

    default: return QIcon();
    }
}

int PreferenceApplicationHost::pm(const PixelMetric& metric) const
{
	const float scale = uiScale();
	switch (metric) {
	case PixelMetric::TinyPadding:
		return static_cast<int>(2 * scale);
	case PixelMetric::ResizableActiveAreaSize:
		return static_cast<int>(8 * scale);
	case PixelMetric::ResizableContentMargin:
		return static_cast<int>(4 * scale);
	case PixelMetric::ItemHeight:
		return static_cast<int>(24 * scale);
	default:
		return 0;
	}
}
