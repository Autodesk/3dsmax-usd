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

#include "ExplorerStyle.h"

#include <UFEUI/utils.h>

namespace UfeUi {

// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------
ExplorerStyle::ExplorerStyle(QStyle* style /*= nullptr*/)
    : QProxyStyle(style)
{
}

// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------
QSize ExplorerStyle::sizeFromContents(
    ContentsType        type,
    const QStyleOption* option,
    const QSize&        size,
    const QWidget*      widget) const
{
    if (type == QStyle::ContentsType::CT_MenuBar) {
        QSize result = QProxyStyle::sizeFromContents(type, option, size, widget);
        result.setHeight(qRound(UfeUi::Utils::dpiScale() * 23.5f));
        return result;
    } else if (type == QStyle::ContentsType::CT_MenuBarItem) {
        QSize result = QProxyStyle::sizeFromContents(type, option, size, widget);
        result.setHeight(qRound(UfeUi::Utils::dpiScale() * 22.5f));
        return result;
    }
    return QProxyStyle::sizeFromContents(type, option, size, widget);
}

} // namespace UfeUi
