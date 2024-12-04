//
// Copyright 2024 Autodesk
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

#include <control.h>
#include <iparamb2.h>

enum USDControllerParams
{
    USDControllerParams_USDStage,
    USDControllerParams_Path,
    USDControllerParams_PreventNodeDeletion
};

class USDBaseController : public Control
{

public:
    USDBaseController() = default;

    // Controller overrides.
    void            BeginEditParams(IObjParam* ip, ULONG flags, Animatable* prev) override;
    void            EndEditParams(IObjParam* ip, ULONG flags, Animatable* next) override;
    int             NumRefs() override;
    RefTargetHandle GetReference(int i) override;
    void            SetReference(int i, RefTargetHandle rtarg) override;
    int             NumParamBlocks() override;
    IParamBlock2*   GetParamBlock(int i) override;
    IParamBlock2*   GetParamBlockByID(BlockID id) override;
    void            Copy(Control* from) override { };
    void            SetValue(TimeValue t, void* val, int commit, GetSetMethod method) override { };

    /**
     * Whether the USD object being read by this controller is valid (I.e. a valid
     * object in a loaded USD stage).
     * @return True if the source is valid.
     */
    virtual bool IsSourceObjectValid() const = 0;

protected:
    // RefMaker override.
    RefResult NotifyRefChanged(
        const Interval& changeInt,
        RefTargetHandle hTarget,
        PartID&         partID,
        RefMessage      message,
        BOOL            propagate) override;

    /**
     * Returns the class desc of concrete implementations. This allows reusing some general setup
     * code for USD controllers.
     * @return The controller's class description.
     */
    virtual ClassDesc2* GetControllerClassDesc() const = 0;

    /**
     * Updates the USD source for this controller, from its parameters.
     * @param pb The paramblock to update from.
     * @return true if the source was changed from previous value, false otherwise.
     */
    virtual bool UpdateSource(IParamBlock2* pb) = 0;

    /// The param block for this controller.
    IParamBlock2* paramBlock = nullptr;
    /// The node owning the stage object.
    INode* stageNode = nullptr;
};

class USDBaseControllerClassDesc : public ClassDesc2
{
public:
    int          IsPublic() override;
    const TCHAR* Category() override;
    HINSTANCE    HInstance() override;
    bool         UseOnlyInternalNameForMAXScriptExposure() override;
};