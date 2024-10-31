//
// Copyright 2023 Autodesk
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

#include "TabWidget.h"

#include <qevent.h>
#include <qpainter.h>

namespace UfeUi {

// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------
TabWidget::TabWidget(QWidget* parent)
    : QTabWidget(parent)
{
}

// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------
void TabWidget::setPlaceHolderText(const QString& placeHolderText)
{
    m_PlaceHolderText = placeHolderText;
}

// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------
void TabWidget::paintEvent(QPaintEvent* event)
{
    QTabWidget::paintEvent(event);
    if (!count()) {
        QPainter painter(this);
        painter.drawText(rect(), Qt::AlignCenter | Qt::TextWordWrap, m_PlaceHolderText);
    }
}

} // namespace UfeUi