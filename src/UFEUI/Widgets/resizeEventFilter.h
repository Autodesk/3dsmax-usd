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
#ifndef UFEUI_RESIZE_EVENT_FILTER_H
#define UFEUI_RESIZE_EVENT_FILTER_H

#include <QtCore/QEvent>
#include <QtCore/QObject>
#include <functional>

namespace UfeUi {

/**
 * \brief Qt event filter for Widget resize events.
 * \remarks This should be moved to an properly-supported Qt objects using "Q_OBJECT" and "signals" notation, once the
 * transition to newer 3ds Max version with Qt project settings is supported.
 */
class ResizeEventFilter : public QObject
{
public:
    /**
     * \brief Constructor.
     * \param target A reference to the Object whose resize event should be listened to.
     * \param onResize Callback to execute upon receiving a resize notification from the given target.
     */
    ResizeEventFilter(QObject* target, const std::function<void()>& onResize);

    /**
     * \brief Event handler executed upon receiving event notifications from the targeted Widget.
     * \param object A reference to the object emitting the event.
     * \param e Data about the event that was emitted.
     */
    bool eventFilter(QObject* object, QEvent* e) override;

protected:
    /// Callback to execute upon receiving a resize notification from the given target.
    std::function<void()> _onResize;
};

} // namespace UfeUi

#endif // UFEUI_RESIZE_EVENT_FILTER_H
