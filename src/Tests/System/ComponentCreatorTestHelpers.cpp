//
// Copyright 2026 Autodesk
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

#include "ComponentCreatorTestHelpers.h"

#include "TestHelpers.h" // STAGE_CLASS_ID

#include <MaxUsdObjects/ComponentCreator/Commands/CreateComponentFromNodesCmd.h>
#include <MaxUsdObjects/ComponentCreator/UtilComponentCreator.h>
#include <MaxUsdObjects/MaxUsdUfe/UfeUtils.h>
#include <MaxUsdObjects/Objects/USDStageObject.h>

#include <MaxUsd/Utilities/MathUtils.h>
#include <MaxUsd/Utilities/TranslationUtils.h>

#include <pxr/usd/sdf/layer.h>
#include <pxr/usd/usd/stage.h>
#include <pxr/usd/usdGeom/metrics.h>
#include <pxr/usd/usdGeom/tokens.h>
#include <pxr/usd/usdGeom/xformable.h>

#include <maxscript/maxscript.h>

#include <AdskUsdComponentCreator/ComponentAPI.h>
#include <AdskUsdComponentCreator/Descriptions/ComponentDescription.h>
#include <AdskUsdComponentCreator/Infos/CreatorInfo.h>
#include <AdskUsdComponentCreator/Options/Options.h>
#include <GetCOREInterface.h>
#include <QtCore/QCoreApplication>
#include <QtCore/QEventLoop>
#include <istdplug.h>
#include <mnmesh.h>
#include <polyobj.h>

namespace CCTest {

INode* MakeCubeNode(const wchar_t* name)
{
    auto* polyObj = static_cast<PolyObject*>(
        GetCOREInterface()->CreateInstance(GEOMOBJECT_CLASS_ID, EPOLYOBJ_CLASS_ID));

    MNMesh& cube = polyObj->GetMesh();
    cube.setNumVerts(8);
    cube.setNumFaces(6);
    cube.V(0)->p = Point3(-1.f, -1.f, -1.f);
    cube.V(1)->p = Point3(1.f, -1.f, -1.f);
    cube.V(2)->p = Point3(-1.f, 1.f, -1.f);
    cube.V(3)->p = Point3(1.f, 1.f, -1.f);
    cube.V(4)->p = Point3(-1.f, -1.f, 1.f);
    cube.V(5)->p = Point3(1.f, -1.f, 1.f);
    cube.V(6)->p = Point3(-1.f, 1.f, 1.f);
    cube.V(7)->p = Point3(1.f, 1.f, 1.f);
    const int faces[6][4] = { { 0, 2, 3, 1 }, { 4, 5, 7, 6 }, { 0, 1, 5, 4 },
                              { 1, 3, 7, 5 }, { 3, 2, 6, 7 }, { 2, 0, 4, 6 } };
    for (int f = 0; f < 6; ++f) {
        cube.F(f)->SetDeg(4);
        for (int v = 0; v < 4; ++v) {
            cube.F(f)->vtx[v] = faces[f][v];
        }
    }
    cube.FillInMesh();
    cube.InvalidateGeomCache();

    INode* node = GetCOREInterface()->CreateObjectNode(polyObj);
    node->SetName(const_cast<MCHAR*>(name));
    return node;
}

static void CollectStageObjectNodes(INode* node, std::set<INode*>& out)
{
    if (node == nullptr) {
        return;
    }
    if (Object* obj = node->GetObjectRef()) {
        if (Object* base = obj->FindBaseObject()) {
            if (base->ClassID() == STAGE_CLASS_ID) {
                out.insert(node);
            }
        }
    }
    for (int i = 0; i < node->NumberOfChildren(); ++i) {
        CollectStageObjectNodes(node->GetChildNode(i), out);
    }
}

std::set<INode*> SnapshotStageObjectNodes()
{
    std::set<INode*> nodes;
    CollectStageObjectNodes(GetCOREInterface()->GetRootNode(), nodes);
    return nodes;
}

INode* FindNewStageObjectNode(const std::set<INode*>& before)
{
    for (INode* n : SnapshotStageObjectNodes()) {
        if (before.find(n) == before.end()) {
            return n;
        }
    }
    return nullptr;
}

USDStageObject* StageObjectOf(INode* node)
{
    if (node == nullptr || node->GetObjectRef() == nullptr) {
        return nullptr;
    }
    return static_cast<USDStageObject*>(node->GetObjectRef()->FindBaseObject());
}

USDStageObject* CreateComponentFromCube(const wchar_t* cubeName)
{
    const std::set<INode*> before = SnapshotStageObjectNodes();
    INode*                 cube = MakeCubeNode(cubeName);
    MaxUsd::ComponentCreator::CreateComponentFromNodes(
        { cube }, AdskUsdComponentCreator::Options {});
    return StageObjectOf(FindNewStageObjectNode(before));
}

USDStageObject* CreateComponentFromCubes(const std::vector<const wchar_t*>& cubeNames)
{
    const std::set<INode*> before = SnapshotStageObjectNodes();
    std::vector<INode*>    cubes;
    cubes.reserve(cubeNames.size());
    for (const wchar_t* name : cubeNames) {
        cubes.push_back(MakeCubeNode(name));
    }
    MaxUsd::ComponentCreator::CreateComponentFromNodes(cubes, AdskUsdComponentCreator::Options {});
    return StageObjectOf(FindNewStageObjectNode(before));
}

USDStageObject* CreateMultiVariantsComponentFromCubes(const std::vector<const wchar_t*>& cubeNames)
{
    const std::set<INode*> before = SnapshotStageObjectNodes();
    std::vector<INode*>    cubes;
    cubes.reserve(cubeNames.size());
    for (const wchar_t* name : cubeNames) {
        cubes.push_back(MakeCubeNode(name));
    }
    MaxUsd::ComponentCreator::CreateMultiVariantsComponentFromNodes(
        cubes, AdskUsdComponentCreator::Options {});
    return StageObjectOf(FindNewStageObjectNode(before));
}

USDStageObject* MakeComponentStageObject()
{
    namespace cc = AdskUsdComponentCreator;

    cc::Options opts;
    const auto  tmp = MaxUsd::ComponentCreator::AllocateComponentTempFolder();
    opts.SetComponentFolder(tmp.string());
    opts.SetComponentName("TestComp");
    opts.SetComponentFilename("TestComp");
    opts.SetFileExtension("usda");
    // Create a model=default variant and mark it the default, so the default variant
    // is selected (active) in the composed stage -- required for edits forwarded into
    // the variant layer to remain visible.
    opts.SetComponentVariants({ { "model", "default" } });
    opts.SetIsDefaultVariant(true);

    auto info = cc::ComponentAPI::CreateFromNothing(opts);
    if (!info) {
        return nullptr;
    }
    info->OpenAll();

    const auto infoStage = info->GetStage();
    if (!infoStage) {
        return nullptr;
    }
    const auto rootPath = MaxUsd::UsdStringToMaxString(infoStage->GetRootLayer()->GetRealPath());

    auto* stageObj = static_cast<USDStageObject*>(
        GetCOREInterface()->CreateInstance(GEOMOBJECT_CLASS_ID, STAGE_CLASS_ID));
    GetCOREInterface()->CreateObjectNode(stageObj);
    stageObj->SetRootLayer(rootPath.data(), L"/", /*payloadsLoaded=*/true);
    return stageObj;
}

std::string DumpStage(const pxr::UsdStageRefPtr& stage)
{
    std::string out = "  composed prims:\n";
    for (const auto& p : stage->Traverse()) {
        out += "    " + p.GetPath().GetString() + " (" + p.GetTypeName().GetString() + ")\n";
    }
    std::string session;
    if (stage->GetSessionLayer()) {
        stage->GetSessionLayer()->ExportToString(&session);
    }
    out += "  session layer:\n" + session + "\n";
    std::string root;
    if (stage->GetRootLayer()) {
        stage->GetRootLayer()->ExportToString(&root);
    }
    out += "  root layer:\n" + root + "\n";
    return out;
}

void SetDefaultVariantTarget(const AdskUsdComponentCreator::ComponentDescription::Ptr& desc)
{
    const auto& vs = desc->GetVariantSets().begin()->second;
    const auto  variantDesc = vs->GetVariantDescription(vs->GetDefaultVariant());
    AdskUsdComponentCreator::ComponentAPI::SetVariantTarget(
        desc, AdskUsdComponentCreator::VariantNesting { vs, variantDesc });
}

bool Valid(const pxr::UsdStageRefPtr& stage, const char* path)
{
    return stage->GetPrimAtPath(pxr::SdfPath(path)).IsValid();
}

void MaxUndo() { ExecuteMAXScriptScript(L"max undo", MAXScript::ScriptSource::NonEmbedded); }
void MaxRedo() { ExecuteMAXScriptScript(L"max redo", MAXScript::ScriptSource::NonEmbedded); }

void AnimateNodeRotation(const wchar_t* name)
{
    const std::wstring script = std::wstring(LR"(
        (
            animationRange = interval 0 10
            local n = getNodeByName ")") + name + LR"("
            set animate on
            at time 10 n.rotation = (quat 0.3 0 0 0.954)
            set animate off
        )
        )";
    ExecuteMAXScriptScript(script.c_str(), MAXScript::ScriptSource::NonEmbedded);
}

void MakeSkinnedSetup(INode*& mesh, INode*& boneRoot)
{
    ExecuteMAXScriptScript(
        LR"(
        (
            local m = box name:"CCSkinMesh" width:10 length:10 height:40 heightsegs:8
            local b1 = BoneSys.createBone [0,0,0] [0,0,20] [0,0,1]
            b1.name = "CCSkinBone01"
            local b2 = BoneSys.createBone [0,0,20] [0,0,40] [0,0,1]
            b2.name = "CCSkinBone02"
            b2.parent = b1

            local sk = skin()
            addModifier m sk
            skinOps.addbone sk b1 0 node:m
            skinOps.addbone sk b2 -1 node:m

            animationRange = interval 0 10
            set animate on
            at time 10 b2.rotation = (quat 0.3 0 0 0.954)
            set animate off
        )
        )",
        MAXScript::ScriptSource::NonEmbedded);

    mesh = GetCOREInterface()->GetINodeByName(L"CCSkinMesh");
    boneRoot = GetCOREInterface()->GetINodeByName(L"CCSkinBone01");
}

UsdUfe::UsdSceneItem::Ptr
Item(USDStageObject* comp, const pxr::UsdStageRefPtr& stage, const pxr::SdfPath& path)
{
    return UsdUfe::UsdSceneItem::create(
        MaxUsd::ufe::getUsdPrimUfePath(comp, path), stage->GetPrimAtPath(path));
}

void PumpIdle()
{
    for (int i = 0; i < 5; ++i) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
    }
}

pxr::GfMatrix4d LocalTransform(const pxr::UsdStageRefPtr& stage, const char* primPath)
{
    pxr::GfMatrix4d matrix(1.0);
    if (!stage) {
        return matrix;
    }
    const pxr::UsdPrim prim = stage->GetPrimAtPath(pxr::SdfPath(primPath));
    if (!prim) {
        return matrix;
    }
    bool resetsStack = false;
    pxr::UsdGeomXformable(prim).GetLocalTransformation(
        &matrix, &resetsStack, pxr::UsdTimeCode::Default());
    return matrix;
}

pxr::GfMatrix4d
ExpectedLocalTransform(const pxr::UsdStageRefPtr& stage, const Point3& maxTranslation)
{
    pxr::GfMatrix4d matrix(1.0);
    matrix.SetTranslateOnly(pxr::GfVec3d(maxTranslation.x, maxTranslation.y, maxTranslation.z));
    if (stage && pxr::UsdGeomGetStageUpAxis(stage) == pxr::UsdGeomTokens->y) {
        MaxUsd::MathUtils::ModifyTransformZToYUp(matrix);
    }
    return matrix;
}

} // namespace CCTest
