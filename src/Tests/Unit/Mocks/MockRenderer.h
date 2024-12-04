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

#include <Rendering/Renderer.h>

class MockRenderer : public Renderer
{
protected:
    /// This ClassID is associated to the renderer in the BTT tool
    Class_ID RendererClassID {};

public:
    /**
     * \brief Constructor.
     * \param classID ID of the Mock Renderer.
     */
    explicit MockRenderer(const Class_ID& classID = Class_ID(0x00000000, 0x00000000))
        : RendererClassID { classID }
    {
    }

    Class_ID ClassID() override { return RendererClassID; }

public:
    /**
     * The following members inherited from the Interface interface are not
     * implemented. Their return values should not be considered, and can cause
     * undefined side-effects.
     */
    int Open(
        INode* /*scene*/,
        INode* /*vnode*/,
        ViewParams* /*viewPar*/,
        RendParams& /*rp*/,
        HWND /*hwnd*/,
        DefaultLight* /*defaultLights*/ = NULL,
        int /*numDefLights*/ = 0,
        RendProgressCallback* /*prog*/ = NULL)
    {
        return 0;
    }
    int Render(
        TimeValue /*t*/,
        Bitmap* /*tobm*/,
        FrameRendParams& /*frp*/,
        HWND /*hwnd*/,
        RendProgressCallback* /*prog*/ = NULL,
        ViewParams* /*viewPar*/ = NULL)
    {
        return 0;
    }
    void Close(HWND /*hwnd*/, RendProgressCallback* /*prog*/ = NULL) { }
    bool ApplyRenderEffects(TimeValue /*t*/, Bitmap* /*pBitmap*/, bool /*updateDisplay*/ = true)
    {
        return false;
    }
    RendParamDlg* CreateParamDialog(IRendParams* /*ir*/, BOOL /*prog*/ = FALSE) { return nullptr; }
    void          ResetParams() { }
    int           GetAAFilterSupport() { return 0; }
    bool          SupportsTexureBaking() { return false; }
    bool          SupportsCustomRenderPresets() { return false; }
    int           RenderPresetsFileVersion() { return -1; }
    BOOL          RenderPresetsIsCompatible(int /*version*/) { return false; }
    const MCHAR*  RenderPresetsMapIndexToCategory(int /*catIndex*/) { return NULL; }
    int           RenderPresetsMapCategoryToIndex(const MCHAR* /*category*/) { return 0; }
    int  RenderPresetsPreSave(ITargetedIO* /*root*/, BitArray /*saveCategories*/) { return -1; }
    int  RenderPresetsPostSave(ITargetedIO* /*root*/, BitArray /*loadCategories*/) { return -1; }
    int  RenderPresetsPreLoad(ITargetedIO* /*root*/, BitArray /*saveCategories*/) { return -1; }
    int  RenderPresetsPostLoad(ITargetedIO* /*root*/, BitArray /*loadCategories*/) { return -1; }
    bool IsStopSupported() const { return false; }
    void StopRendering() { }
    enum class PauseSupport
    {
        None,
        Full,
        Legacy
    };
    Renderer::PauseSupport IsPauseSupported() const { return Renderer::PauseSupport::None; }
    void                   PauseRendering() { }
    void                   ResumeRendering() { }
    enum Requirement
    {
        kRequirement_NoPauseSupport = 0,
        kRequirement_NoVFB = 1,
        kRequirement_DontSaveRenderOutput = 2,
        kRequirement_Wants32bitFPOutput = 3,
        kRequirement_WantsObjectSelection = 4,
        kRequirement_NoGBufferForToneOpPreview = 5,
        kRequirement_SupportsConcurrentRendering = 6
    };
    bool HasRequirement(Renderer::Requirement /*requirement*/) { return false; }
    bool CompatibleWithAnyRenderElement() const { return false; }
    bool CompatibleWithRenderElement(IRenderElement& /*pIRenderElement*/) const { return false; }
    IInteractiveRender* GetIInteractiveRender() { return nullptr; }
    void                GetVendorInformation(MSTR& /*info*/) const { }
    void                GetPlatformInformation(MSTR& /*info*/) const { }
    RefResult           NotifyRefChanged(
                  const Interval& /*changeInt*/,
                  RefTargetHandle /*hTarget*/,
                  PartID& /*partID*/,
                  RefMessage /*message*/,
                  BOOL /*propagate*/)
    {
        return REF_FAIL;
    }
};