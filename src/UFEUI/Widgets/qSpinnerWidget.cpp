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

#include <cmath>

namespace UfeUi {

QSpinnerWidget::QSpinnerWidget(QWidget* parent)
    : QWidget { parent }
{
    setVisible(false);

    connect(&_timer, &QTimer::timeout, this, &QSpinnerWidget::drawNewFrame);
}

void QSpinnerWidget::startSpinning()
{
    setVisible(true);

    _timer.start(40);
    _mode = MODE::SPINNER;
}

void QSpinnerWidget::startProgress()
{
    setVisible(true);

    _timer.start(1000 / UPDATES_PER_SECOND);
    _mode = MODE::PROGRESS;
    _spinAngle = 0.0f;
    _spinAngleTo = 0.0f;
}

void QSpinnerWidget::setProgress(float currentProgress)
{
    _spinAngle = std::max(_previousSpinAngleTo, _spinAngle);
    _spinAngleTo = 360.0f * currentProgress;

    repaint();
}

void QSpinnerWidget::hide()
{
    setVisible(false);

    _timer.stop();
    _mode = MODE::OFF;
}

void QSpinnerWidget::drawNewFrame()
{
    if (_mode == MODE::SPINNER) {
        _spinAngle += 1.0f;
        if (_spinAngle >= 90.0f) {
            _spinAngle = 0.0f;
        }
    } else if (_mode == MODE::PROGRESS) {
        // The progress attempts to maintain a smooth impression of the progress: instead of jumping
        // straight to the requested value, it will slide over to it.
        //
        // Sliding from 0.0 to 1.0 is done in a single second, so the sliding is quick to the eye.
        // If there are more than UPDATES_PER_SECOND steps, this sliding effect is actually not
        // visible since individual increments between steps will be smaller than 1.0 /
        // UPDATES_PER_SECOND of the circumference.
        _spinAngle = std::min(_spinAngleTo, _spinAngle + 360.0f / UPDATES_PER_SECOND);
        _heartBeat = ++_heartBeat % UPDATES_PER_SECOND;
    }

    repaint();
}

void QSpinnerWidget::drawOpenedCircle(QPainter& painter, float startAngle, float spanAngle)
{
    QPen pen(_baseColor);
    pen.setWidth(3);
    painter.setPen(pen);
    painter.translate(
        (painter.device()->width() - SPINNER_DIMENSION) / 2,
        (painter.device()->height() - SPINNER_DIMENSION) / 2);
    painter.drawArc(
        QRect(0, 0, SPINNER_DIMENSION, SPINNER_DIMENSION), startAngle * 16, spanAngle * 16);
}

void QSpinnerWidget::drawHeartBeat(QPainter& painter)
{
    float halfUpdate = UPDATES_PER_SECOND / 2.0f;
    float amplitude = (std::fabs(_heartBeat - halfUpdate) / halfUpdate) * 6.0f;
    float angle = (_spinAngle - 90.0f) * M_PI / 180.0f;
    float offset = (SPINNER_DIMENSION - amplitude) / 2.0f;

    QPen   pen(_baseColor);
    QBrush brush(_baseColor);
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
    if (_mode != MODE::OFF) {
        QPainter painter(this);
        painter.begin(this);
        painter.setRenderHint(QPainter::Antialiasing);

        if (_mode == MODE::SPINNER) {
            drawOpenedCircle(painter, -_spinAngle * 4, 340);
        } else if (_mode == MODE::PROGRESS) {
            drawOpenedCircle(painter, 90, -_spinAngle);
            drawHeartBeat(painter);
        }

        painter.end();
    }
}

} // namespace UfeUi
