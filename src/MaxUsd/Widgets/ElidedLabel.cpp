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

#include "ElidedLabel.h"

#include <QtGui/qpainter.h>
#include <QtWidgets/qstyleoption.h>

ElidedLabel::ElidedLabel(QWidget* parent)
    : QFrame(parent)
{
}

ElidedLabel::ElidedLabel(const QString& text, QWidget* parent)
    : QFrame(parent)
    , labelText(text)
{
}

void ElidedLabel::setText(const QString& text)
{
    labelText = text;
    update();
}

QString ElidedLabel::text() const { return labelText; }

Qt::Alignment ElidedLabel::alignment() const { return align; }

void ElidedLabel::setAlignment(Qt::Alignment alignment)
{
    align = alignment;
    update();
}

Qt::TextElideMode ElidedLabel::elideMode() const { return mode; }

void ElidedLabel::setElideMode(Qt::TextElideMode elideMode)
{
    mode = elideMode;
    update();
}

void ElidedLabel::setItalic(bool italic) { isItalic = italic; }

void ElidedLabel::paintEvent(QPaintEvent* event)
{
    QFrame::paintEvent(event);

    QStyleOption opt;
    opt.initFrom(this);

    QRect cr = contentsRect();

    // Draw standard text
    QPainter painter(this);
    if (isItalic) {
        QFont font(painter.font());
        font.setItalic(true);
        painter.setFont(font);
    }

    QString txt = painter.fontMetrics().elidedText(labelText, mode, cr.width());

    style()->drawItemText(
        &painter,
        cr,
        QStyle::visualAlignment(opt.direction, align),
        opt.palette,
        (opt.state & QStyle::State_Enabled) > 0,
        txt);
}
