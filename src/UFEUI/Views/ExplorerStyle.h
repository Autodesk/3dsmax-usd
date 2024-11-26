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

#ifndef UFEUI_EXPLORER_STYLE
#define UFEUI_EXPLORER_STYLE

#include <qproxystyle.h>

namespace UfeUi {

class ExplorerStyle : public QProxyStyle
{
    Q_OBJECT

public:
    ExplorerStyle(QStyle* style = nullptr);

    QSize sizeFromContents(
        ContentsType        type,
        const QStyleOption* option,
        const QSize&        size,
        const QWidget*      widget) const override;
};

}; // namespace UfeUi

#endif // UFEUI_EXPLORER_STYLE