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
#include "QSpinnerOverlayWidget.h"

#include <QtWidgets/QApplication>

namespace UfeUi {

QSpinnerOverlayWidget::QSpinnerOverlayWidget(QWidget* target)
    : QLabel { target }
{
    _resizeEventFilter
        = std::make_unique<ResizeEventFilter>(target, [this]() { onTargetResized(); });
    target->installEventFilter(_resizeEventFilter.get());

    _infoColor = QApplication::palette().color(QPalette::Active, QPalette::WindowText);

    // Center the text within the bounds of the target, and make sure long text can wrap around its
    // width:
    setAlignment({ Qt::AlignCenter | Qt::AlignVCenter | Qt::TextWordWrap });
    setWordWrap(true);

    hide();
}

void QSpinnerOverlayWidget::startSpinning() { setMode(MODE::SPINNER); }

void QSpinnerOverlayWidget::startProgress() { setMode(MODE::PROGRESS); }

void QSpinnerOverlayWidget::setProgress(float currentProgress)
{
    _spinnerWidget->setProgress(currentProgress);
}

void QSpinnerOverlayWidget::showErrorMessage(const QString& message)
{
    setMode(MODE::ERROR_TEXT, message);
}

bool QSpinnerOverlayWidget::showInformationMessage(const QString& message)
{
    if (_mode == MODE::ERROR_TEXT) {
        return false;
    } else {
        setMode(MODE::INFORMATION_TEXT, message);
        return true;
    }
}

void QSpinnerOverlayWidget::hide(bool hideErrorMessage)
{
    if (!hideErrorMessage && _mode == MODE::ERROR_TEXT) {
        // If an error was displayed, make sure it remains visible:
        return;
    }
    setMode(MODE::OFF);
}

void QSpinnerOverlayWidget::setMode(MODE mode, const QString& message)
{
    if (mode == MODE::SPINNER) {
        _spinnerWidget->startSpinning();
    } else if (mode == MODE::PROGRESS) {
        _spinnerWidget->startProgress();
    } else {
        _spinnerWidget->hide();
    }

    if (mode == MODE::ERROR_TEXT || mode == MODE::INFORMATION_TEXT) {
        QString formattedPayload = message;
        formattedPayload.replace("\n", "<br>");

        QColor textColor = mode == MODE::ERROR_TEXT ? _errorColor : _infoColor;
        setText(QString("<font style=\"color: %1;\">%2</font>")
                    .arg(textColor.name())
                    .arg(formattedPayload));
    } else {
        setText("");
    }

    setVisible(mode != MODE::OFF);

    this->_mode = mode;
}

void QSpinnerOverlayWidget::onTargetResized()
{
    QSize parentSize = parentWidget()->size();
    resize(parentSize);
    _spinnerWidget->resize(parentSize);
}

} // namespace UfeUi
