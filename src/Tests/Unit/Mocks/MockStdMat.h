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

#include <MaxUsd/Utilities/MaxSupportUtils.h>

#include <imtl.h>
#include <map>
#include <stdmat.h>
class MockStdMat : public StdMat
{
protected:
    // This ClassID is the same associated in the BTT Tool
    Class_ID MaterialClassID = Class_ID(DMTL_CLASS_ID, 0x00000000);

public:
    void     SetMaterialID(Class_ID classID) { MaterialClassID = classID; }
    Class_ID ClassID() { return MaterialClassID; }

public:
    /**
     * The following members inherited from the Interface interface are not
     * implemented. Their return values should not be considered, and can cause
     * undefined side-effects.
     */
    void      SetDiffuse(Color /*c*/, TimeValue /*t*/) { }
    void      SetSoften(BOOL /*onoff*/) { }
    void      SetFaceMap(BOOL /*onoff*/) { }
    void      SetTwoSided(BOOL /*onoff*/) { }
    void      SetWire(BOOL /*onoff*/) { }
    void      SetWireUnits(BOOL /*onoff*/) { }
    void      SetFalloffOut(BOOL /*onoff*/) { }
    void      SetTransparencyType(int /*type*/) { }
    void      SetAmbient(Color /*c*/, TimeValue /*t*/) { }
    void      SetSpecular(Color /*c*/, TimeValue /*t*/) { }
    void      SetFilter(Color /*c*/, TimeValue /*t*/) { }
    void      SetShininess(float /*v*/, TimeValue /*t*/) { }
    void      SetShinStr(float /*v*/, TimeValue /*t*/) { }
    void      SetSelfIllum(float /*v*/, TimeValue /*t*/) { }
    void      SetOpacity(float /*v*/, TimeValue /*t*/) { }
    void      SetOpacFalloff(float /*v*/, TimeValue /*t*/) { }
    void      SetWireSize(float /*s*/, TimeValue /*t*/) { }
    void      SetIOR(float /*v*/, TimeValue /*t*/) { }
    void      LockAmbDiffTex(BOOL /*onOff*/) { }
    void      SetSamplingOn(BOOL /*on*/) { }
    void      SetShading(int /*s*/) { }
    BOOL      GetSamplingOn() { return false; }
    int       GetShading() { return 0; }
    void      EnableMap(int /*id*/, BOOL /*onoff*/) { }
    BOOL      MapEnabled(int /*id*/) { return false; }
    void      SetTexmapAmt(int /*id*/, float /*amt*/, TimeValue /*t*/) { }
    float     GetTexmapAmt(int /*id*/, TimeValue /*t*/) { return 0; }
    BOOL      GetSoften() { return false; }
    BOOL      GetFaceMap() { return false; }
    BOOL      GetTwoSided() { return false; }
    BOOL      GetWire() { return false; }
    BOOL      GetWireUnits() { return false; }
    BOOL      GetFalloffOut() { return false; }
    int       GetTransparencyType() { return 0; }
    Color     GetAmbient(TimeValue /*t*/) { return Color(); }
    Color     GetDiffuse(TimeValue /*t*/) { return Color(); }
    Color     GetSpecular(TimeValue /*t*/) { return Color(); }
    Color     GetFilter(TimeValue /*t*/) { return Color(); }
    float     GetShininess(TimeValue /*t*/) { return 0; }
    float     GetShinStr(TimeValue /*t*/) { return 0; }
    float     GetSelfIllum(TimeValue /*t*/) { return 0; }
    float     GetOpacity(TimeValue /*t*/) { return 0; }
    float     GetOpacFalloff(TimeValue /*t*/) { return 0; }
    float     GetWireSize(TimeValue /*t*/) { return 0; }
    float     GetIOR(TimeValue /*t*/) { return 0; }
    BOOL      GetAmbDiffTexLock() { return false; }
    Color     GetAmbient(int /*mtlNum*/ = 0, BOOL /*backFace*/ = FALSE) { return Color(); }
    Color     GetDiffuse(int /*mtlNum*/ = 0, BOOL /*backFace*/ = FALSE) { return Color(); }
    Color     GetSpecular(int /*mtlNum*/ = 0, BOOL /*backFace*/ = FALSE) { return Color(); }
    float     GetShininess(int /*mtlNum*/ = 0, BOOL /*backFace*/ = FALSE) { return 0; }
    float     GetShinStr(int /*mtlNum*/ = 0, BOOL /*backFace*/ = FALSE) { return 0; }
    float     GetXParency(int /*mtlNum*/ = 0, BOOL /*backFace*/ = FALSE) { return 0; }
    void      Shade(ShadeContext& /*sc*/) { }
    void      Update(TimeValue /*t*/, Interval& /*valid*/) { }
    void      Reset() { }
    Interval  Validity(TimeValue /*t*/) { return Interval(); }
    ParamDlg* CreateParamDlg(HWND /*hwMtlEdit*/, IMtlParams* /*imp*/) { return nullptr; }
    RefResult NotifyRefChanged(
        const Interval& /*changeInt*/,
        RefTargetHandle /*hTarget*/,
        PartID& /*partID*/,
        RefMessage /*message*/,
        BOOL /*propagate*/)
    {
        return REF_FAIL;
    }
    BOOL     SupportsShaders() { return TRUE; }
    BOOL     KeyAtTimeByID(ParamID /*id*/, TimeValue /*t*/) { return false; }
    int      GetMapState(int /*indx*/) { return 0; }
    MSTR     GetMapName(int /*indx*/) { return L"a"; }
    void     SyncADTexLock(BOOL /*lockOn*/) { }
    BOOL     SwitchShader(Class_ID /*id*/) { return false; }
    Shader*  GetShader() { return nullptr; }
    BOOL     IsFaceted() { return false; }
    void     SetFaceted(BOOL /*on*/) { }
    long     StdIDToChannel(long /*id*/) { return 0; }
    BOOL     SwitchSampler(Class_ID /*id*/) { return false; }
    Sampler* GetPixelSampler(int /*mtlNum*/, BOOL /*backFace*/) { return nullptr; }
    BOOL     GetSelfIllumColorOn(int /*mtlNum*/ = 0, BOOL /*backFace*/ = FALSE) { return false; }
    Color    GetSelfIllumColor(int /*mtlNum*/, BOOL /*backFace*/) { return Color(); }
    Color    GetSelfIllumColor(TimeValue /*t*/) { return Color(); }
    void     SetSelfIllumColorOn(BOOL /*on*/) { }
    void     SetSelfIllumColor(Color /*c*/, TimeValue /*t*/) { }
    float    GetReflectionDim(float /*diffIllumIntensity*/) { return 1.0f; }
    Color    TranspColor(float /*opac*/, Color /*filt*/, Color /*diff*/) { return Color(); }
    float    GetEffOpacity(ShadeContext& /*sc*/, float /*opac*/) { return 0; }
};

class MockMultiMtl : public MultiMtl
{
public:
    /**
     * The following members inherited from the Interface interface are not
     * implemented. Their return values should not be considered, and can cause
     * undefined side-effects.
     */
    Color    GetAmbient(int /*mtlNum*/ = 0, BOOL /*backFace*/ = FALSE) override { return Color(); }
    Color    GetDiffuse(int /*mtlNum*/ = 0, BOOL /*backFace*/ = FALSE) override { return Color(); }
    Color    GetSpecular(int /*mtlNum*/ = 0, BOOL /*backFace*/ = FALSE) override { return Color(); }
    float    GetShininess(int /*mtlNum*/ = 0, BOOL /*backFace*/ = FALSE) override { return 0; }
    float    GetShinStr(int /*mtlNum*/ = 0, BOOL /*backFace*/ = FALSE) override { return 0; }
    float    GetXParency(int /*mtlNum*/ = 0, BOOL /*backFace*/ = FALSE) override { return 0; }
    void     Shade(ShadeContext& /*sc*/) override { }
    void     Update(TimeValue /*t*/, Interval& /*valid*/) override { }
    void     Reset() override { }
    Interval Validity(TimeValue /*t*/) override { return Interval(); }
    ParamDlg* CreateParamDlg(HWND /*hwMtlEdit*/, IMtlParams* /*imp*/) override { return nullptr; }
    RefResult NotifyRefChanged(
        const Interval& /*changeInt*/,
        RefTargetHandle /*hTarget*/,
        PartID& /*partID*/,
        RefMessage /*message*/,
        BOOL /*propagate*/) override
    {
        return REF_FAIL;
    }
    void SetDiffuse(Color /*c*/, TimeValue /*t*/) override { }
    void SetAmbient(Color /*c*/, TimeValue /*t*/) override { }
    void SetSpecular(Color /*c*/, TimeValue /*t*/) override { }
    void SetShininess(float /*v*/, TimeValue /*t*/) override { }

    void SetNumSubMtls(int /*n*/) override { }

#ifdef IS_MAX2024_OR_GREATER
    void SetSubMtlAndName(int /*mtlid*/, Mtl* /*m*/, const MSTR& /*subMtlName*/) override { }
#else
    void SetSubMtlAndName(int /*mtlid*/, Mtl* /*m*/, MSTR& /*subMtlName*/) override { }
#endif

    void RemoveMtl(int /*mtlid*/) override { }

public:
    /**
     * The following methods are used by the MockMultiMtl in order to
     * control the behavior of the Interface as it is passed to tests.
     */
    Class_ID ClassID() override { return Class_ID(MULTI_CLASS_ID, 0x00000000); }

    Mtl* GetSubMtl(int i) override
    {
        if (subMtls.find(i) != subMtls.end()) {
            return subMtls[i];
        }
        return nullptr;
    }

    int NumSubMtls() override { return int(subMtls.size()); }

    void AddMtl(ReferenceTarget* rt, int mtlid, const MCHAR* pName) override
    {
        subMtls[mtlid] = static_cast<Mtl*>(rt);
        names[mtlid] = pName;
    }

    BOOL IsMultiMtl() override { return TRUE; }

    void GetSubMtlName(int mtlid, MSTR& s) override { s = names[mtlid]; }

private:
    /// List of sub-materials
    std::map<int, Mtl*> subMtls;

    /// List of sub-materials names
    std::map<int, MSTR> names;
};
