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
#pragma once
#include <MaxUsd.h>

#include <ufe/selection.h>

#include <QtWidgets/qwidget.h>

namespace MAXUSD_NS_DEF {

class QmaxUsdPythonWidgetPrivate;
class QmaxUsdPythonWidget : public QWidget
{
    Q_OBJECT

public:
    ~QmaxUsdPythonWidget() override;

    static QmaxUsdPythonWidget* create(
        const Ufe::Selection& selection,
        const std::string&    attributeName,
        const std::string&    pythonModuleDirectory,
        const std::string&    pythonModule,
        const std::string&    pythonEntryFunction,
        // TODO: passing additional parameters
        std::set<std::string>& handledAttributeNames);

protected:
    QmaxUsdPythonWidget(QWidget* parent = nullptr);

private:
    const std::unique_ptr<QmaxUsdPythonWidgetPrivate> d_ptr;
    Q_DECLARE_PRIVATE(QmaxUsdPythonWidget);
};

} // namespace MAXUSD_NS_DEF