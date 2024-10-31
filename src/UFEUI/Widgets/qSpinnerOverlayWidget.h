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
#ifndef UFEUI_QSPINNER_OVERLAY_WIDGET_H
#define UFEUI_QSPINNER_OVERLAY_WIDGET_H

#include "qSpinnerWidget.h"
#include "resizeEventFilter.h"

#include <UFEUI/UFEUIAPI.h>

#include <QtWidgets/QLabel.h>
#include <memory>

namespace UfeUi {

/**
 * \brief Qt Widget to overlay a Spinner on top of a target Widget in order to convey information to the User.
 * \remarks Inspired from the behavior of the Shotgun progress indicator.
 */
class UFEUIAPI QSpinnerOverlayWidget : public QLabel
{
public:
    /**
     * \brief Constructor.
     * \param target A reference to the Widget on top of which this overlay should be displayed.
     */
    explicit QSpinnerOverlayWidget(QWidget* target);

    /**
     * \brief Show the overlay and start animating the Spinner.
     */
    void startSpinning();

    /**
     * \brief Show the overlay and display an animated progress arc representing the progress of an ongoing task.
     */
    void startProgress();

    /**
     * \brief Set the current progress of the ongoing task.
     * \param currentProgress Current progress of the task, in the [0.0;1.0] range.
     */
    void setProgress(float currentProgress);

    /**
     * \brief Display an error message to the User.
     * \param message The error message to display to the User (supporting HTML).
     */
    void showErrorMessage(const QString& message);

    /**
     * \brief Display an information message to the User.
     * \param message The information message to display to the User.
     * \return A flag indicating if the message was displayed to the User (supporting HTML).
     */
    bool showInformationMessage(const QString& message);

    /**
     * \brief Hide the overlay.
     * \param hideErrorMessage A flag indicating if the error message should also be hidden along with the overlay.
     */
    void hide(bool hideErrorMessage = true);

protected:
    /// Mode in which the Spinner can be:
    enum class MODE
    {
        OFF,     ///< Overlay is hidden.
        SPINNER, ///< Overlay is shown and its Spinner is in "spinner" mode, displaying a rotating
                 ///< arc.
        ERROR_TEXT,       ///< Overlay is shown, along with an error message.
        INFORMATION_TEXT, ///< Overlay is shown, along with an information message.
        PROGRESS, ///< Overlay is shown and its Spinner is in "progress" mode, displaying a arc.
        LAST      ///< Last item in the list of supported Modes.
    };

    /**
     * \brief Set the state of the overlay and its Spinner Widget.
     * \param mode The new Mode in which to set the Widget.
     * \param message A message to display to the User when the mode is either "ERROR_TEXT" or "INFORMATION_TEXT".
     */
    void setMode(MODE mode, const QString& message = "");

    /**
     * \brief Callback executed when the target Widget has been resized.
     */
    void onTargetResized();

protected:
    /// Current Mode of the Overlay:
    MODE _mode { MODE::OFF };

    /// Color of the error message text:
    QColor _errorColor { 255, 0, 0 };
    /// Color of the information message text:
    QColor _infoColor;

    /// Event filter used to handle notifications about the overlayed Widget being resized:
    std::unique_ptr<ResizeEventFilter> _resizeEventFilter;
    /// Spinner widget used to display information to the User about the progress of a task:
    std::unique_ptr<QSpinnerWidget> _spinnerWidget { std::make_unique<QSpinnerWidget>(this) };
};

} // namespace UfeUi

#endif // UFEUI_QSPINNER_OVERLAY_WIDGET_H
