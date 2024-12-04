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
#ifndef UFEUI_QSPINNER_WIDGET_H
#define UFEUI_QSPINNER_WIDGET_H

#include <QtCore/QTimer.h>
#include <QtGui/QPainter.h>
#include <QtWidgets/QWidget.h>

namespace UfeUi {

/**
 * \brief Widget to display a spinner or report progress about on ongoing task.
 * \remarks Inspired from the behavior of the Shotgun progress indicator.
 */
class QSpinnerWidget : public QWidget
{
public:
    /**
     * \brief Constructor.
     * \param parent A reference to the parent of the Spinner.
     */
    explicit QSpinnerWidget(QWidget* parent = nullptr);

    /**
     * \brief Start animating the Spinner.
     */
    void startSpinning();

    /**
     * \brief Show an animated progress arc representing the progress of an ongoing task.
     */
    void startProgress();

    /**
     * \brief Set the current progress of the ongoing task.
     * \param currentProgress Current progress of the task, in the [0.0;1.0] range.
     */
    void setProgress(float currentProgress);

    /**
     * \brief Hide the Spinner.
     */
    void hide();

protected:
    /**
     * \brief Draw a new frame for the Spinner.
     */
    void drawNewFrame();

    /**
     * \brief Draw an arc.
     * \param painter Painter with which to draw.
     * \param startAngle Angle at which to start drawing the arc (in degrees).
     * \param spanAngle Angle the arc covers (in degrees).
     */
    void drawOpenedCircle(QPainter& painter, float startAngle, float spanAngle);

    /**
     * \brief Draw the heartbeat cursor of the progress, to provide feedback to the User and avoid making it look like
     * the UI is frozen when the progress is not being updated.
     * \param painter Painter with which to draw.
     */
    void drawHeartBeat(QPainter& painter);

    /**
     * \brief Paint the Spinner widget on screen.
     * \param e Data associated to the Paint event.
     */
    void paintEvent(QPaintEvent* e);

protected:
    /// Mode in which the Spinner can be:
    enum class MODE
    {
        OFF,      ///< Spinner is off.
        SPINNER,  ///< Spinner is in "spinner" mode, displaying a rotating arc.
        PROGRESS, ///< Spinner is in "progress" mode, displaying a arc representing progress
                  ///< percentage.
        LAST      ///< Last item in the list of supported Modes.
    };

    /// Number of redraws to perform per second:
    static constexpr unsigned short UPDATES_PER_SECOND { 25 };
    /// Spinner dimension (in pixels):
    static constexpr size_t SPINNER_DIMENSION { 80 };

    /// Current Mode of the Spinner:
    MODE _mode { MODE::OFF };

    /// Base color of the Spinner:
    QColor _baseColor { 255, 255, 255 };

    /// Timer used to update the animation of the spinner:
    QTimer _timer { this };

    /// Current spinner angle:
    float _spinAngle { 0.0f };
    /// Target angle towards which to spin:
    float _spinAngleTo { 0.0f };
    /// Spinner angle at the time of the last paint:
    float _previousSpinAngleTo { 0.0f };

    /// Heartbeat counter indicating how many ticks occured during the last second, in order to draw
    /// the size of the heartbeat indicator:
    int _heartBeat { 0 };
};

} // namespace UfeUi

#endif // UFEUI_QSPINNER_WIDGET_H
