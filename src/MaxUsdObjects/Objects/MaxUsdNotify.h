//
// Copyright 2025 Autodesk
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

#include <MaxUsd/Utilities/MaxSupportUtils.h>

#include <ref.h> // for REFMSG_USER

#ifdef IS_MAX2025_OR_GREATER
#include <notify.h>

class USDStageObject;

// No way to ensure custom notification codes are unique...but with any luck, it will be!
#define NOTIFY_SELECTION_HIGHLIGHT_ENABLED_CHANGED REFMSG_USER + 0x29415134
DEFINE_NOTIFY_CODE(NOTIFY_STAGE_LOAD_STATE_CHANGED, REFMSG_USER + 0x29415135, USDStageObject*)
DEFINE_NOTIFY_CODE(NOTIFY_STAGE_ANIM_PARAMETERS_CHANGED, REFMSG_USER + 0x29415136, USDStageObject*)
DEFINE_NOTIFY_CODE(NOTIFY_STAGE_CLICK_CREATE, REFMSG_USER + 0x29415137, USDStageObject*)

#else
// No way to ensure custom notification codes are unique...but with any luck, it will be!
#define NOTIFY_SELECTION_HIGHLIGHT_ENABLED_CHANGED REFMSG_USER + 0x29415134
#define NOTIFY_STAGE_LOAD_STATE_CHANGED            REFMSG_USER + 0x29415135
#define NOTIFY_STAGE_ANIM_PARAMETERS_CHANGED       REFMSG_USER + 0x29415136
#define NOTIFY_STAGE_CLICK_CREATE                  REFMSG_USER + 0x29415137
#endif
