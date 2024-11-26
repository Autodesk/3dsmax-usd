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

#include "USDSnaps.h"

#include <osnapapi.h>

Class_ID USDSNAPS_CLASS_ID(0x795d1720, 0x27b303d7);

#define VERTEX_SUB 0
#define EDGE_SUB   1
#define FACE_SUB   2

const MCHAR* USDSnaps::Category() { return GetString(IDS_CATEGORY); }

ClassDesc2* GetUSDSnapsClassDesc()
{
    static USDSnapsClassDesc USDSnapsDesc;
    return &USDSnapsDesc;
}

int USDSnaps::numsubs() { return 3; }

MSTR* USDSnaps::snapname(int index)
{
    static MSTR vertex(GetString(IDS_USDSNAPS_VERTEX));
    static MSTR edge(GetString(IDS_USDSNAPS_EDGE));
    static MSTR face(GetString(IDS_USDSNAPS_FACE));

    switch (index) {
    case VERTEX_SUB: return &vertex;
    case EDGE_SUB: return &edge;
    case FACE_SUB: return &face;
    default: DbgAssert("Unsupported snap index" && 0); return nullptr;
    }
}

MSTR USDSnaps::iconName(int index) const
{
    static MSTR vertexIcon = MSTR(_T("Common/Vertex"));
    static MSTR edgeIcon = MSTR(_T("SnapTools/MaxSDK/Xmesh/EdgeSegment"));
    static MSTR faceIcon = MSTR(_T("Common/Face"));

    switch (index) {
    case VERTEX_SUB: return vertexIcon;
    case EDGE_SUB: return edgeIcon;
    case FACE_SUB: return faceIcon;
    default: DbgAssert("Unsupported snap index" && 0); return {};
    }
}

boolean USDSnaps::ValidInput(SClass_ID, Class_ID cid) { return cid == USDSTAGEOBJECT_CLASS_ID; }

OsnapMarker* USDSnaps::GetMarker(int index)
{
    // These describe the geometry of the markers that appear in the viewport on snap points.

    static IPoint3 verterMarkerPoints[4]
        = { IPoint3(-5, 0, 0), IPoint3(5, 0, 0), IPoint3(0, -5, 0), IPoint3(0, 5, 0) };
    static int vertexMarkerEdgeInfo[4] = { GW_EDGE_VIS, GW_EDGE_SKIP, GW_EDGE_VIS, GW_EDGE_VIS };
    static OsnapMarker vertexMarker { 4, verterMarkerPoints, vertexMarkerEdgeInfo };

    static IPoint3 edgeMarkerPoints[5] = {
        IPoint3(5, 5, 0), IPoint3(-5, 5, 0), IPoint3(-5, -5, 0), IPoint3(5, -5, 0), IPoint3(5, 5, 0)
    };
    static int edgeMarkerEdgeInfo[5]
        = { GW_EDGE_VIS, GW_EDGE_VIS, GW_EDGE_VIS, GW_EDGE_VIS, GW_EDGE_VIS };
    static OsnapMarker edgeMarker { 5, edgeMarkerPoints, edgeMarkerEdgeInfo };

    static IPoint3 faceMarkerPoints[4]
        = { IPoint3(5, -5, 0), IPoint3(0, 5, 0), IPoint3(-5, -5, 0), IPoint3(5, -5, 0) };
    static int faceMarkerEdgeInfo[4] = { GW_EDGE_VIS, GW_EDGE_VIS, GW_EDGE_VIS, GW_EDGE_VIS };
    static OsnapMarker faceMarker { 4, faceMarkerPoints, faceMarkerEdgeInfo };

    switch (index) {
    case VERTEX_SUB: return &vertexMarker;
    case EDGE_SUB: return &edgeMarker;
    case FACE_SUB: return &faceMarker;
    default: DbgAssert("Unsupported snap index" && 0); return {};
    }
}

HBITMAP USDSnaps::getTools()
{
    return nullptr; // unused
}

HBITMAP USDSnaps::getMasks()
{
    return nullptr; // unused
}

WORD USDSnaps::AccelKey(int)
{
    return 0; // unused
}

void USDSnaps::Snap(Object* pobj, IPoint2* point, TimeValue time)
{
    if (pobj->ClassID() != USDSTAGEOBJECT_CLASS_ID) {
        return;
    }

    USDStageObject* stageObject = dynamic_cast<USDStageObject*>(pobj);
    if (!stageObject) {
        return;
    }

    auto objectTM = theman->GetNode()->GetObjectTM(time);
    objectTM.Invert();

    // To prevent "self-snapping", exclude any currently displayed selection from snapping.
    const auto selection = stageObject->GetHydraEngine()->GetRenderDelegate()->GetSelection();
    const auto excludedPaths = selection->GetAllSelectedPrimPaths();

    HitRegion hitRegion;
    MakeHitRegion(hitRegion, POINT_RGN, false, theman->GetSnapStrength() /*epsilon*/, point);

    // Assume we're snapping in the active viewport.
    ViewExp* viewport = &GetCOREInterface()->GetActiveViewExp();

    // Record hits for a sub snap mode.
    auto recordHits
        = [&objectTM, &stageObject, &excludedPaths, &hitRegion, &viewport, time, this](
              int sub, const pxr::TfToken& pickMode, pxr::UsdImagingGLDrawMode drawMode) {
              if (!GetActive(sub)) {
                  return;
              }
              const auto hits = stageObject->PickStage(
                  viewport, theman->GetNode(), hitRegion, drawMode, pickMode, time, excludedPaths);
              for (int i = 0; i < hits.size(); ++i) {
                  auto hitPoint = objectTM.PointTransform(
                      { hits[i].hitPoint.x, hits[i].hitPoint.y, hits[i].hitPoint.z });
                  theman->RecordHit(new OsnapHit(hitPoint, this, sub, nullptr));
              }
          };

    const auto isWireframe = viewport->IsWire();

    // In wireframe we want to hit backfacing points and edges. Render only points/edges and target
    // prims. When not in wireframe, render geometry normally, but with points or edges as pick
    // targets.

    // Snap onto vertices.
    {
        auto target = isWireframe ? pxr::HdxPickTokens->pickPrimsAndInstances
                                  : pxr::HdxPickTokens->pickPoints;
        auto drawMode = isWireframe ? pxr::UsdImagingGLDrawMode::DRAW_POINTS
                                    : pxr::UsdImagingGLDrawMode::DRAW_GEOM_ONLY;
        recordHits(VERTEX_SUB, target, drawMode);
    }

    // Snap onto edges
    {
        const auto target = isWireframe ? pxr::HdxPickTokens->pickPrimsAndInstances
                                        : pxr::HdxPickTokens->pickEdges;
        auto       drawMode = isWireframe ? pxr::UsdImagingGLDrawMode::DRAW_WIREFRAME
                                          : pxr::UsdImagingGLDrawMode::DRAW_GEOM_ONLY;
        recordHits(EDGE_SUB, target, drawMode);
    }

    // Snap onto faces
    {
        // For face, mimic the standard snapping behavior, do not use the snap strength, and allow
        // snapping onto faces even while the viewport is showing wireframe. Need to actually be
        // over geometry to snap onto faces.
        hitRegion.epsilon = 1;
        recordHits(
            FACE_SUB,
            pxr::HdxPickTokens->pickPrimsAndInstances,
            pxr::UsdImagingGLDrawMode::DRAW_GEOM_ONLY);
    }
}