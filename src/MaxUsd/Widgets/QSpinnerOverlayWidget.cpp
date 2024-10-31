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

namespace MAXUSD_NS_DEF {

QSpinnerOverlayWidget::QSpinnerOverlayWidget(QWidget* target)
    : QLabel { target }
{
    resizeEventFilter
        = std::make_unique<ResizeEventFilter>(target, [this]() { OnTargetResized(); });
    target->installEventFilter(resizeEventFilter.get());

    // Center the text within the bounds of the target, and make sure long text can wrap around its
    // width:
    setAlignment({ Qt::AlignCenter | Qt::AlignVCenter | Qt::TextWordWrap });
    setWordWrap(true);

    Hide();
}

void QSpinnerOverlayWidget::StartSpinning() { SetMode(MODE::SPINNER); }

void QSpinnerOverlayWidget::StartProgress() { SetMode(MODE::PROGRESS); }

void QSpinnerOverlayWidget::SetProgress(float currentProgress)
{
    spinnerWidget->SetProgress(currentProgress);
}

void QSpinnerOverlayWidget::ShowErrorMessage(const QString& message)
{
    SetMode(MODE::ERROR_TEXT, message);
}

bool QSpinnerOverlayWidget::ShowInformationMessage(const QString& message)
{
    if (mode == MODE::ERROR_TEXT) {
        return false;
    } else {
        SetMode(MODE::INFORMATION_TEXT, message);
        return true;
    }
}

void QSpinnerOverlayWidget::Hide(bool hideErrorMessage)
{
    if (!hideErrorMessage && mode == MODE::ERROR_TEXT) {
        // If an error was displayed, make sure it remains visible:
        return;
    }
    SetMode(MODE::OFF);
}

void QSpinnerOverlayWidget::SetMode(MODE mode, const QString& message)
{
    if (mode == MODE::SPINNER) {
        spinnerWidget->StartSpinning();
    } else if (mode == MODE::PROGRESS) {
        spinnerWidget->StartProgress();
    } else {
        spinnerWidget->Hide();
    }

    if (mode == MODE::ERROR_TEXT || mode == MODE::INFORMATION_TEXT) {
        QString formattedPayload = message;
        formattedPayload.replace("\n", "<br>");

        QColor textColor = mode == MODE::ERROR_TEXT ? errorColor : infoColor;
        setText(QString("<font style=\"color: %1;\">%2</font>")
                    .arg(textColor.name())
                    .arg(formattedPayload));
    } else {
        setText("");
    }

    setVisible(mode != MODE::OFF);

    this->mode = mode;
}

void QSpinnerOverlayWidget::OnTargetResized()
{
    QSize parentSize = parentWidget()->size();
    resize(parentSize);
    spinnerWidget->resize(parentSize);
}

} // namespace MAXUSD_NS_DEF
