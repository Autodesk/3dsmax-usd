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
#include <MaxUsd/MaxUSDAPI.h>

#include <Qt/QmaxSpinBox.h>

#include <MaxUsd.h>

namespace MAXUSD_NS_DEF {

// MaxUsd QmaxDoubleSpinBox override to handle the context menu
// there is an issue with the 3ds Max DoubleSpinBox context menu
// which displays a 'Set to Minimum  RMB' menu item but resets the value
class MaxUSDAPI QmaxUsdDoubleSpinBox : public MaxSDK::QmaxDoubleSpinBox
{
    Q_OBJECT
public:
    explicit QmaxUsdDoubleSpinBox(QWidget* parent = nullptr);

protected:
    void mousePressEvent(QMouseEvent* event) override;
    void contextMenuEvent(QContextMenuEvent* event) override;
};

} // namespace MAXUSD_NS_DEF