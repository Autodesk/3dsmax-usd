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

#include <MaxUsd.h>

namespace MAXUSD_NS_DEF {
namespace Listener {

/**
 * \brief Writes to the 3dsMax listener.
 * \param message The message to be written.
 * \param isError If true, output the text in red.
 */
void Write(const MCHAR* message, bool isError = false);

} // namespace Listener
} // namespace MAXUSD_NS_DEF