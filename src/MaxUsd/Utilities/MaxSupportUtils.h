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

#if MAX_RELEASE >= 27900
#define IS_MAX_BETA
#endif

#if MAX_RELEASE >= 26900
#define IS_MAX2025_OR_GREATER
#endif

#if MAX_RELEASE >= 25900
#define IS_MAX2024_OR_GREATER
#endif

#if MAX_RELEASE >= 24900
#define IS_MAX2023_OR_GREATER
#endif

#if MAX_RELEASE >= 23900 && MAX_RELEASE < 24900
#define IS_MAX2022
#endif

#include <maxapi.h>
#ifdef IS_MAX2025_OR_GREATER
#include <notifyParams.h>
#endif
#include <MaxUsd/MaxUSDAPI.h>

#include <Graphics/BaseMaterialHandle.h>

#include <Materials/mtl.h>

namespace MaxSDKSupport {

MaxUSDAPI const MCHAR* GetString(const MSTR& str);
MaxUSDAPI const MCHAR* GetString(const MCHAR* str);

/** Callback function to handle 3ds Max's delete modifier event. */
void DeletedModifierNotifyHandler(void* param, NotifyInfo* info);

/**
 * Modifiers Can live in memory after deletion. This function will check if the modifier has been
 * deleted and if it's living in memory only.
 * @param mod modifier to be checked if was deleted.
 * @return true if the modifier has been deleted; false otherwise
 */
bool IsModifierDeleted(Modifier* mod);

#ifdef IS_MAX2025_OR_GREATER
using NotifyPostNodesCloned = ::NotifyPostNodesCloned;
inline INodeTab* GetClonedNodes(NotifyPostNodesCloned* cloneInfo) { return cloneInfo->clonedNodes; }

#else
struct NotifyPostNodesCloned
{
    INodeTab* srcNodes;
    INodeTab* dstNodes;
    CloneType cloneType;
};
inline INodeTab* GetClonedNodes(NotifyPostNodesCloned* cloneInfo) { return cloneInfo->dstNodes; }
#endif

/// Returns the version of 3ds Max in the format {major, update, hotfix, build}
MaxUSDAPI std::vector<int> GetMaxVersion();

namespace Graphics {
namespace MaterialConversionHelper {

MaxUSDAPI MaxSDK::Graphics::BaseMaterialHandle
          ConvertMaxToNitrousMaterial(Mtl& mtl, TimeValue t, bool /*realistic*/);

}
} // namespace Graphics
} // namespace MaxSDKSupport
