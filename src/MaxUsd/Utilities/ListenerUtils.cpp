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
#include <MaxUsd/MaxUSDAPI.h>

#include <maxscript/maxscript.h>
#include <maxscript/util/listener.h>

#include <MaxUsd.h>

namespace MAXUSD_NS_DEF {
namespace Listener {

MaxUSDAPI void Write(const MCHAR* message, bool isError = false)
{
    if (nullptr == the_listener) {
        return;
    }
    const int oldStyle = the_listener->get_style();
    if (isError) {
        the_listener->set_style(LISTENER_STYLE_MESSAGE);
    }

    the_listener->edit_stream->puts(message);
    the_listener->edit_stream->puts(_T("\n"));
    the_listener->edit_stream->flush();

    if (isError) {
        the_listener->set_style(oldStyle);
    }
}
} // namespace Listener
} // namespace MAXUSD_NS_DEF