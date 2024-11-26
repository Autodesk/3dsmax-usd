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
#include "UiUtils.h"

#include <Qt/QmaxMainWindow.h>
#include <Qt/QmaxToolClips.h>

#include <GetCOREInterface.h>
#include <QtCore/QVariant.h>
#include <QtWidgets/QMessageBox>
#include <maxapi.h>

namespace MAXUSD_NS_DEF {
namespace Ui {

bool AskYesNoQuestion(const std::wstring& text, const std::wstring& caption)
{
    QMessageBox::StandardButton result = QMessageBox::question(
        GetCOREInterface()->GetQmaxMainWindow(),
        QString::fromStdWString(caption),
        QString::fromStdWString(text),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::Yes);
    return result == QMessageBox::StandardButton::Yes;
}

void DisableMaxToolClipsRecursively(QObject* object)
{
    MaxSDK::QmaxToolClips::disableToolClip(object);
    for (const auto& child : object->findChildren<QObject*>()) {
        MaxSDK::QmaxToolClips::disableToolClip(child);
    }
}

void IterateOverChildrenRecursively(
    QObject*                             parent,
    const std::function<void(QObject*)>& callback,
    bool                                 includingParent)
{
    if (parent) {
        if (includingParent) {
            callback(parent);
        }
        for (auto child : parent->children()) {
            IterateOverChildrenRecursively(child, callback, true);
        }
    }
}

void DisableMaxAcceleratorsOnFocus(QWidget* widget, bool disableMaxAccelerators)
{
#ifdef IS_MAX2023_OR_GREATER
    QtHelpers::DisableMaxAcceleratorsOnFocus(QWidget * widget, bool disableMaxAccelerators);
#else
    if (widget) {
        static constexpr char noMaxAccelerators[] = "NoMaxAccelerators";
        widget->setProperty(noMaxAccelerators, disableMaxAccelerators ? true : QVariant());
    }
#endif
}
} // namespace Ui
} // namespace MAXUSD_NS_DEF
