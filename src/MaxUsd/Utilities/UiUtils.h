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
#include <MaxUsd/MaxUSDAPI.h>

#include <MaxUsd.h>
#include <QtCore/QObject>

#ifdef IS_MAX2023_OR_GREATER
#include <Qt/QmaxHelpers.h>
#endif

namespace MAXUSD_NS_DEF {
namespace Ui {

/**
 * \brief Asks a yes/no question to the user via a QMessageBox.
 * \param text The question string.
 * \param caption Caption for the message box.
 * \return true if yes was selected, false otherwise.
 */
MaxUSDAPI bool AskYesNoQuestion(const std::wstring& text, const std::wstring& caption);

/**
 * \brief Disables 3dsmax's custom toolclips on an object and all its descendants.
 * \param object The object to disable tooltips on.
 */
MaxUSDAPI void DisableMaxToolClipsRecursively(QObject* object);

/** Iterates over all children of a QObject recursively and calls a callback on
 * each of them (and optionally also the object itself).
 * \param parent The parent object to iterate over.
 * \param callback The callback to call on each child.
 * \param includingParent If true, the callback will be called on the parent
 *        object as well. */
MaxUSDAPI void IterateOverChildrenRecursively(
    QObject*                             parent,
    const std::function<void(QObject*)>& callback,
    bool                                 includingParent = true);

/**
 * \brief Disable or not max accelerator on focus. Wraps the MaxSDK QtHelper equivalent,
 * to provide support for older max versions.
 * \param widget The widget on which to set the flag.
 * \param disableMaxAccelerators True to disable max accelerator.
 */
MaxUSDAPI void DisableMaxAcceleratorsOnFocus(QWidget* widget, bool disableMaxAccelerators);

} // namespace Ui
} // namespace MAXUSD_NS_DEF
