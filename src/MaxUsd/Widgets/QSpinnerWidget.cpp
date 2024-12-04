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
#include "QSpinnerWidget.h"

namespace MAXUSD_NS_DEF {

QSpinnerWidget::QSpinnerWidget(QWidget* parent)
    : QWidget { parent }
{
    setVisible(false);

    connect(&timer, &QTimer::timeout, this, &QSpinnerWidget::DrawNewFrame);
}

void QSpinnerWidget::StartSpinning()
{
    setVisible(true);

    timer.start(40);
    mode = MODE::SPINNER;
}

void QSpinnerWidget::StartProgress()
{
    setVisible(true);

    timer.start(1000 / UPDATES_PER_SECOND);
    mode = MODE::PROGRESS;
    spinAngle = 0.0f;
    spinAngleTo = 0.0f;
}

void QSpinnerWidget::SetProgress(float currentProgress)
{
    spinAngle = std::max(previousSpinAngleTo, spinAngle);
    spinAngleTo = 360.0f * currentProgress;

    repaint();
}

void QSpinnerWidget::Hide()
{
    setVisible(false);

    timer.stop();
    mode = MODE::OFF;
}

void QSpinnerWidget::DrawNewFrame()
{
    if (mode == MODE::SPINNER) {
        spinAngle += 1.0f;
        if (spinAngle >= 90.0f) {
            spinAngle = 0.0f;
        }
    } else if (mode == MODE::PROGRESS) {
        // The progress attempts to maintain a smooth impression of the progress: instead of jumping
        // straight to the requested value, it will slide over to it.
        //
        // Sliding from 0.0 to 1.0 is done in a single second, so the sliding is quick to the eye.
        // If there are more than UPDATES_PER_SECOND steps, this sliding effect is actually not
        // visible since individual increments between steps will be smaller than 1.0 /
        // UPDATES_PER_SECOND of the circumference.
        spinAngle = std::min(spinAngleTo, spinAngle + 360.0f / UPDATES_PER_SECOND);
        heartBeat = ++heartBeat % UPDATES_PER_SECOND;
    }

    repaint();
}

void QSpinnerWidget::DrawOpenedCircle(QPainter& painter, float startAngle, float spanAngle)
{
    QPen pen(baseColor);
    pen.setWidth(3);
    painter.setPen(pen);
    painter.translate(
        static_cast<qreal>((painter.device()->width() - SPINNER_DIMENSION) / 2),
        static_cast<qreal>((painter.device()->height() - SPINNER_DIMENSION) / 2));
    painter.drawArc(
        QRect(0, 0, SPINNER_DIMENSION, SPINNER_DIMENSION),
        static_cast<int>(startAngle * 16),
        static_cast<int>(spanAngle * 16));
}

void QSpinnerWidget::DrawHeartBeat(QPainter& painter)
{
    float halfUpdate = UPDATES_PER_SECOND / 2.0f;
    float amplitude = (std::fabs(heartBeat - halfUpdate) / halfUpdate) * 6.0f;
    float angle = (spinAngle - 90.0f) * static_cast<float>(M_PI) / 180.0f;
    float offset = (SPINNER_DIMENSION - amplitude) / 2.0f;

    QPen   pen(baseColor);
    QBrush brush(baseColor);
    pen.setWidth(1);
    painter.setPen(pen);
    painter.setBrush(brush);
    painter.drawEllipse(QRectF(
        std::cosf(angle) * (SPINNER_DIMENSION / 2.0f) + offset,
        std::sinf(angle) * (SPINNER_DIMENSION / 2.0f) + offset,
        amplitude,
        amplitude));
}

void QSpinnerWidget::paintEvent(QPaintEvent* e)
{
    if (mode != MODE::OFF) {
        QPainter painter(this);
        painter.begin(this);
        painter.setRenderHint(QPainter::Antialiasing);

        if (mode == MODE::SPINNER) {
            DrawOpenedCircle(painter, -spinAngle * 4, 340);
        } else if (mode == MODE::PROGRESS) {
            DrawOpenedCircle(painter, 90, -spinAngle);
            DrawHeartBeat(painter);
        }

        painter.end();
    }
}

} // namespace MAXUSD_NS_DEF
