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

#include <control.h>
#include <maxapi.h>
#include <object.h>

/**
 * \brief Mock for 3ds Max's "Interface" interface.
 */
class MockViewExp : public ViewExp
{
protected:
    /**
     * The following properties are instantiated in order to return
     * some data by reference by the ViewExp.
     */

    CtrlHitLog SomeCtrlHitLog;
    HitLog     SomeHitLog;

public:
    /**
     * The following members inherited from the ViewExp interface are not
     * implemented. Their return values should not be considered, and can
     * cause undefined side-effects.
     */

    int      GetViewID() override { return 0; }
    ViewExp* ToPointer() override { return nullptr; }
    bool     IsAlive() override { return false; }
    Point3   GetPointOnCP(const IPoint2& /*ps*/) override { return nullptr; }
    Point3   SnapPoint(
          const IPoint2& /*in*/,
          IPoint2& /*out*/,
          Matrix3* /*plane2d*/ = NULL,
          DWORD /*flags*/ = 0) override
    {
        return nullptr;
    }
    void SnapPreview(
        const IPoint2& /*in*/,
        IPoint2& /*out*/,
        Matrix3* /*plane2d*/ = NULL,
        DWORD /*flags*/ = 0) override
    {
    }
    void GetGridDims(float* /*MinX*/, float* /*MaxX*/, float* /*MinY*/, float* /*MaxY*/) override {
    }
    float SnapLength(float /*in*/) override { return 0.0f; }
    float GetCPDisp(
        const Point3 /*base*/,
        const Point3& /*dir*/,
        const IPoint2& /*sp1*/,
        const IPoint2& /*sp2*/,
        BOOL /*snap*/ = FALSE) override
    {
        return 0.0f;
    }
    GraphicsWindow* getGW() override { return nullptr; }
    int             IsWire() override { return 0; }
    Rect            GetDammageRect() override { return Rect(); }
    Point2          MapViewToScreen(const Point3& /*p*/) override { return nullptr; }
#if MAX_RELEASE >= 25900
    Point3 MapScreenToView(const IPoint2& /*sp*/, float /*depth*/) override { return nullptr; }
#else
    Point3 MapScreenToView(IPoint2& /*sp*/, float /*depth*/) override { return nullptr; }
#endif
    void   MapScreenToWorldRay(float /*sx*/, float /*sy*/, Ray& /*ray*/) override { }
    BOOL   SetAffineTM(const Matrix3& /*m*/) override { return FALSE; }
    void   GetAffineTM(Matrix3& /*tm*/) override { }
    int    GetViewType() override { return 0; }
    BOOL   IsPerspView() override { return FALSE; }
    BOOL   IsCanvasNavigationMode() override { return FALSE; }
    float  GetFOV() override { return 0.0f; }
    float  GetFocalDist() override { return 0.0f; };
    void   SetFocalDist(float /*fd*/) override { }
    float  GetFPS() override { return 0.0f; }
    float  GetScreenScaleFactor(const Point3 /*worldPoint*/) override { return 0.0f; }
    float  GetVPWorldWidth(const Point3 /*wPoint*/) override { return 0.0f; }
    Point3 MapCPToWorld(const Point3 /*cpPoint*/) override { return nullptr; }
    float  NonScalingObjectSize() override { return 0.0f; }
    void   GetConstructionTM(Matrix3& /*tm*/) override { }
    void   SetGridSize(float /*size*/) override { }
    float  GetGridSize() override { return 0.0f; }
    BOOL   IsGridVisible() override { return FALSE; }
    void   SetGridVisibility(BOOL /*bVisible*/) override { }
    int    GetGridType() override { return 0; }
    INode* GetViewCamera() override { return nullptr; }
    void   SetViewCamera(INode* /*camNode*/) override { }
    void   SetViewUser(BOOL /*persp*/) override { }
    INode* GetViewSpot() override { return nullptr; }
    void   SetViewSpot(INode* /*spotNode*/) override { }
    void   ClearHitList() override { }
    INode* GetClosestHit() override { return nullptr; }
    INode* GetHit(int /*i*/) override { return nullptr; }
    int    HitCount() override { return 0; }
    void   LogHit(
          INode* /*nr*/,
          ModContext* /*mc*/,
          DWORD /*dist*/,
          ulong /*info*/,
          HitData* /*hitdata*/ = NULL) override
    {
    }
    HitLog& GetSubObjHitList() override { return SomeHitLog; }
    void    ClearSubObjHitList() override { }
    int     NumSubObjHits() override { return 0; }
    void CtrlLogHit(INode* /*nr*/, DWORD /*dist*/, ulong /*info*/, DWORD /*infoExtra*/) override { }
    CtrlHitLog& GetCtrlHitList() override { return SomeCtrlHitLog; }
    void        ClearCtrlHitList() override { }
    BOOL        setBkgImageDsp(BOOL /*onOff*/) override { return FALSE; }
    int         getBkgImageDsp(void) override { return 0; }
    void        setSFDisplay(int /*onOff*/) override { }
    int         getSFDisplay(void) override { return 0; }
    HWND        GetHWnd() override { return nullptr; }
    BOOL        IsActive() override { return FALSE; }
    BOOL        IsEnabled() override { return FALSE; }
    void        SetSolidBackgroundColorMode(bool /*bSolidColor*/) override { }
    bool        IsSolidBackgroundColorMode() override { return false; }
    void        ResetBackgroundColorMode() override { }
    void
    TrackImplicitGrid(IPoint2 /*m*/, Matrix3* /*mat*/ = NULL, ULONG /*hitTestFlags*/ = 0) override
    {
    }
    void CommitImplicitGrid(IPoint2 /*m*/, int /*mouseflags*/, Matrix3* /*mat*/ = NULL) override { }
    void ReleaseImplicitGrid() override { }
    void InvalidateRect(const Rect& /*rect*/) override { }
};
