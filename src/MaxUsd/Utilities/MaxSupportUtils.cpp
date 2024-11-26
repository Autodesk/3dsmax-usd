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
#include "MaxSupportUtils.h"

#include <Graphics/MaterialConversionHelper.h>

#include <modstack.h>
#include <notify.h>
#include <ref.h>

/*
 * This struct became available only on later 3ds Max versions.
 */
#if MAX_RELEASE >= 23900 && MAX_RELEASE < 25900
struct NotifyModAddDelParam
{
    INode*      node;
    Modifier*   mod;
    ModContext* mc;
};
#endif

/**
 * Struct to store information about deleted modifiers.
 * In 3ds Max, modifiers stay in memory after deletion. However, it's when checking for dependencies
 * of a node, if a modifier has been deleted or not. This can be an issue for Writers that will need
 * to find information on certain modifiers (SkeletonWriter, for example). In this case, it's
 * necessary to keep a reference to the modifier that was deleted and the derived object that it
 * was. The object will be deleted for when the 3ds Max scene is changing or when clearing the undo
 * stack.
 */
class DeletedModifierInfo : public ReferenceMaker
{
public:
    DeletedModifierInfo() = default;
    ~DeletedModifierInfo() override
    {
        HoldSuspend hs;
        if (this->mod) {
            DeleteReference(0);
        }
        if (this->obj) {
            DeleteReference(1);
        }
    }

    Modifier*       mod = nullptr; // modifier reference that was deleted
    IDerivedObject* obj = nullptr;
    bool            postDelete
        = false; // flag used to know if the post delete modifier event has been triggered

protected:
    // Override the ReferenceMaker

    RefResult NotifyRefChanged(
        const Interval& changeInt,
        RefTargetHandle hTarget,
        PartID&         partID,
        RefMessage      message,
        BOOL            propagate) override;

    int NumRefs() override { return 2; }

    RefTargetHandle GetReference(int i) override
    {
        switch (i) {
        case 0: return mod;
        case 1: return obj;
        default: return nullptr;
        }
    }

    void SetReference(int i, RefTargetHandle rtarg) override
    {
        // Pause the undo system to avoid creating undo steps for this operation
        HoldSuspend hs;
        switch (i) {
        case 0: mod = dynamic_cast<Modifier*>(rtarg); break;
        case 1: obj = dynamic_cast<IDerivedObject*>(rtarg); break;
        default: break;
        }
    }

    // Makes weak reference
    BOOL IsRealDependency(ReferenceTarget* rtarg) override { return false; }
};

std::unordered_map<Modifier*, DeletedModifierInfo> deletedModifiers;

RefResult DeletedModifierInfo::NotifyRefChanged(
    const Interval& changeInt,
    RefTargetHandle hTarget,
    PartID&         partID,
    RefMessage      message,
    BOOL            propagate)
{
    switch (message) {
    case REFMSG_TARGET_DELETED: {
        // When the target being deleted is the modifier, then it can just be removed from the map.
        // When the target is the derived object, that means the modifier is already gone the
        // referenced needs to be cleared
        if (hTarget == this->mod) {
            this->mod = nullptr;
            deletedModifiers.erase(static_cast<Modifier*>(hTarget));
            return REF_STOP;
        } else if (hTarget == this->obj) {
            // This will nullify the reference maker to this object, also removing from 3ds Max
            this->obj = nullptr;
        }
        break;
    }
    case REFMSG_MODIFIER_ADDED: {
        if (postDelete) {
            // For the case that the modifier that was deleted has been re-added,
            // remove it from the map
            if ((ReferenceTarget*)partID == this->mod) {
                deletedModifiers.erase(this->mod);
                return REF_STOP;
            }
        }
        break;
    }
    default: break;
    }

    return REF_DONTCARE;
}

namespace MaxSDKSupport {

const MCHAR* GetString(const MSTR& str) { return str.data(); }

const MCHAR* GetString(const MCHAR* str) { return str; }

bool IsModifierDeleted(Modifier* mod)
{
    return deletedModifiers.find(mod) != deletedModifiers.end();
}

void DeletedModifierNotifyHandler(void* param, NotifyInfo* info)
{
    if (!info) {
        return;
    }

    // After the NOTIFY_PRE_MODIFIER_DELETED event, one *ADDED_MODIFIER* event is also triggered.
    // To work around this, it's possible to use the NOTIFY_POST_MODIFIER_DELETED event. However,
    // this event does not contain the derived object information that is needed to properly handle
    // the modifier deletion. So, making use of both events it's possible to cache the required
    // information to handle this case.
    switch (info->intcode) {
    case NOTIFY_PRE_MODIFIER_DELETED: {
#ifdef IS_MAX2025_OR_GREATER
        NotifyModAddDelParam* data = GetNotifyParam<NOTIFY_PRE_MODIFIER_DELETED>(info);
#else
        NotifyModAddDelParam* data = static_cast<NotifyModAddDelParam*>(info->callParam);
#endif
        if (data->mod) {
            IDerivedObject* obj = nullptr;
            int             idx = 0;
            data->mod->GetIDerivedObject(data->mc, obj, idx);
            auto& deletedModInfo = deletedModifiers[data->mod];

            // Prevents those operations going into the undo stack
            HoldSuspend hs;

            deletedModInfo.postDelete = false;
            deletedModInfo.ReplaceReference(0, data->mod);
            deletedModInfo.ReplaceReference(1, obj);
        }
        break;
    }
    case NOTIFY_POST_MODIFIER_DELETED: {
#ifdef IS_MAX2025_OR_GREATER
        NotifyModAddDelParam* data = GetNotifyParam<NOTIFY_POST_MODIFIER_DELETED>(info);
#else
        NotifyModAddDelParam* data = static_cast<NotifyModAddDelParam*>(info->callParam);
#endif
        if (data->mod) {
            auto it = deletedModifiers.find(data->mod);
            if (it != deletedModifiers.end()) {
                it->second.postDelete = true;
            }
        }
        break;
    }
    default: break;
    }
}

std::vector<int> GetMaxVersion()
{
    std::vector<int> version;

    const TSTR buildNumberString = MaxSDK::Util::GetMaxBuildNumber();
    int        majorVersion = 0, updateVersion = 0, hotFix = 0, buildNumber = 0;
    if (!buildNumberString.isNull()) {
        _stscanf(
            buildNumberString,
            _T("%d.%d.%d.%d"),
            &majorVersion,
            &updateVersion,
            &hotFix,
            &buildNumber);
        version.emplace_back(majorVersion);
        version.emplace_back(updateVersion);
        version.emplace_back(hotFix);
        version.emplace_back(buildNumber);
    }

    return version;
}

MaxSDK::Graphics::BaseMaterialHandle
Graphics::MaterialConversionHelper::ConvertMaxToNitrousMaterial(
    Mtl&      mtl,
    TimeValue t,
    bool      realistic)
{
#ifdef IS_MAX2023_OR_GREATER
    return MaxSDK::Graphics::MaterialConversionHelper::ConvertMaxToNitrousMaterial(
        mtl, t, realistic);
#endif

#ifdef IS_MAX2022
    return MaxSDK::Graphics::MaterialConversionHelper::ConvertMaxToNitrousMaterial(
        mtl,
        t,
        MaxSDK::Graphics::MaterialConversionHelper::MaterialStyles::MaterialStyle_MaterialDecide);
#endif
}

} // namespace MaxSDKSupport