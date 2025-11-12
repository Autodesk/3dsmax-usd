--
-- Copyright 2025 Autodesk
--
-- Licensed under the Apache License, Version 2.0 (the "License");
-- you may not use this file except in compliance with the License.
-- You may obtain a copy of the License at
--
--     http://www.apache.org/licenses/LICENSE-2.0
--
-- Unless required by applicable law or agreed to in writing, software
-- distributed under the License is distributed on an "AS IS" BASIS,
-- WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
-- See the License for the specific language governing permissions and
-- limitations under the License.
--
FileIn "helpers/usd_test_utils.mxs"

-- Make sure the USD plugin is properly loaded.
setupUsdPlugins()

-- Some useful python modules for USD validation.
pyUsd = python.import("pxr.Usd")
pyUsdGeom = python.import("pxr.UsdGeom")
pyUsdUtils = python.import("pxr.UsdUtils")

struct export_to_stage
(
    private
    emptyLayer,
    initialUnits,
    initialScale,
    initialFrameRate,
    
    public
    function setup =
    (
        quickReset()
        local testDataDir = getFilenamePath (getThisScriptFilename())
        emptyLayer = (testDataDir + @"data\empty.usda")

        initialUnits = units.SystemType
        initialScale = units.SystemScale
        units.SystemType = #inches
        units.SystemScale = 1
            
        initialFrameRate = frameRate
        frameRate = 24 -- usd default.
    ),

    function test_export_selection = (
        boxA = box()
        boxA.name = "boxA"
        boxB = box()
        boxB.name = "boxB"
        boxC = box()
        boxC.name = "boxC"
        
        boxX = box()
        boxX.name = "boxX"
        boxX.parent = boxA
        
        stageObject1 = USDStageObject()
        stageObject1.SetRootLayer emptyLayer
    
        local stageCache = pyUsdUtils.StageCache.Get()
        local stage1 = stageCache.Find(pyUsd.StageCache.Id.FromLongInt(stageObject1.cacheId))
        stage1.Reload()
        
        select #(boxA)
        local success = UsdExporter.ExportToStage stageObject1 contentSource:#selected
        assert_true success
        assert_true ((stage1.GetPrimAtPath("/root/boxA")).IsValid())
        assert_false ((stage1.GetPrimAtPath("/root/boxA/boxX")).IsValid())
        assert_false ((stage1.GetPrimAtPath("/root/boxB")).IsValid())
        assert_false ((stage1.GetPrimAtPath("/root/boxC")).IsValid())
        stage1.Reload()
        
        select #(boxA, boxB, boxC)
        UsdExporter.ExportToStage stageObject1 contentSource:#selected
        assert_true ((stage1.GetPrimAtPath("/root/boxA")).IsValid())
        assert_false ((stage1.GetPrimAtPath("/root/boxA/boxX")).IsValid())
        assert_true ((stage1.GetPrimAtPath("/root/boxB")).IsValid())
        assert_true ((stage1.GetPrimAtPath("/root/boxC")).IsValid())
        stage1.Reload()
            
        select #(boxA, boxX)
        UsdExporter.ExportToStage stageObject1 contentSource:#selected
        assert_true ((stage1.GetPrimAtPath("/root/boxA")).IsValid())
        assert_true ((stage1.GetPrimAtPath("/root/boxA/boxX")).IsValid())
        assert_false ((stage1.GetPrimAtPath("/root/boxB")).IsValid())
        assert_false ((stage1.GetPrimAtPath("/root/boxC")).IsValid())
        stage1.Reload()
            
        select #(boxX, boxB)
        UsdExporter.ExportToStage stageObject1 contentSource:#selected
        assert_true ((stage1.GetPrimAtPath("/root/boxB")).IsValid())
        assert_true ((stage1.GetPrimAtPath("/root/boxX")).IsValid())
        assert_false ((stage1.GetPrimAtPath("/root/boxA")).IsValid())
        assert_false ((stage1.GetPrimAtPath("/root/boxC")).IsValid())
        stage1.Reload()
    ),
    
    function test_export_node_list = (
        boxA = box()
        boxA.name = "boxA"
        boxB = box()
        boxB.name = "boxB"
        boxC = box()
        boxC.name = "boxC"
        
        boxX = box()
        boxX.name = "boxX"
        boxX.parent = boxA
        
        stageObject1 = USDStageObject()
        stageObject1.SetRootLayer emptyLayer
    
        local stageCache = pyUsdUtils.StageCache.Get()
        local stage1 = stageCache.Find(pyUsd.StageCache.Id.FromLongInt(stageObject1.cacheId))
        stage1.Reload()
        
        UsdExporter.ExportToStage stageObject1 contentSource:#nodeList nodeList:#(boxA)
        assert_true ((stage1.GetPrimAtPath("/root/boxA")).IsValid())
        assert_false ((stage1.GetPrimAtPath("/root/boxA/boxX")).IsValid())
        assert_false ((stage1.GetPrimAtPath("/root/boxB")).IsValid())
        assert_false ((stage1.GetPrimAtPath("/root/boxC")).IsValid())
        stage1.Reload()
        
        UsdExporter.ExportToStage stageObject1 contentSource:#nodeList nodeList:#(boxA, boxB, boxC)
        assert_true ((stage1.GetPrimAtPath("/root/boxA")).IsValid())
        assert_false ((stage1.GetPrimAtPath("/root/boxA/boxX")).IsValid())
        assert_true ((stage1.GetPrimAtPath("/root/boxB")).IsValid())
        assert_true ((stage1.GetPrimAtPath("/root/boxC")).IsValid())
        stage1.Reload()
        
        UsdExporter.ExportToStage stageObject1 contentSource:#nodeList nodeList:#(boxA, boxX)
        assert_true ((stage1.GetPrimAtPath("/root/boxA")).IsValid())
        assert_true ((stage1.GetPrimAtPath("/root/boxA/boxX")).IsValid())
        assert_false ((stage1.GetPrimAtPath("/root/boxB")).IsValid())
        assert_false ((stage1.GetPrimAtPath("/root/boxC")).IsValid())
        stage1.Reload()
        
        UsdExporter.ExportToStage stageObject1 contentSource:#nodeList nodeList:#(boxX, boxB)
        assert_true ((stage1.GetPrimAtPath("/root/boxB")).IsValid())
        assert_true ((stage1.GetPrimAtPath("/root/boxX")).IsValid())
        assert_false ((stage1.GetPrimAtPath("/root/boxA")).IsValid())
        assert_false ((stage1.GetPrimAtPath("/root/boxC")).IsValid())
        stage1.Reload()
    ),
    
    function test_export_all = (
        boxA = box()
        boxA.name = "boxA"
        boxB = box()
        boxB.name = "boxB"
        boxC = box()
        boxC.name = "boxC"
        
        boxX = box()
        boxX.name = "boxX"
        boxX.parent = boxA
        
        stageObject1 = USDStageObject name:"ignore"
        stageObject1.SetRootLayer emptyLayer
    
        local stageCache = pyUsdUtils.StageCache.Get()
        local stage1 = stageCache.Find(pyUsd.StageCache.Id.FromLongInt(stageObject1.cacheId))
        stage1.Reload()
        
        UsdExporter.ExportToStage stageObject1
            
        assert_true ((stage1.GetPrimAtPath("/root/boxA")).IsValid())
        assert_true ((stage1.GetPrimAtPath("/root/boxA/boxX")).IsValid())
        assert_true ((stage1.GetPrimAtPath("/root/boxB")).IsValid())
        assert_true ((stage1.GetPrimAtPath("/root/boxC")).IsValid())
        
        -- Stage object should not export itself to its own stage...
        assert_false ((stage1.GetPrimAtPath("/root/ignore")).IsValid())
        stage1.Reload()
    ),
    
    function test_inherit_stage_transform = (
        boxA = box()
        boxA.name = "boxA"
        boxA.position = [10,5,0]
        
        boxB = box()
        boxB.name = "boxB"
        boxB.parent = boxA
        boxB.position = [10,10,0]
        
        boxC = box()
        boxC.name = "boxC"
        boxC.position = [9,9,9]
        
        stageObject1 = USDStageObject()
        stageObject1.position = [0,0,15]
        stageObject1.SetRootLayer emptyLayer
        
        opts = USDExporter.CreateOptions()
        opts.RootPrimPath = "/"
        
        local stageCache = pyUsdUtils.StageCache.Get()
        local stage1 = stageCache.Find(pyUsd.StageCache.Id.FromLongInt(stageObject1.cacheId))
        stage1.Reload()
        
        -- Default should inherit the stage object transform.
        UsdExporter.ExportToStage stageObject1 exportOptions:opts contentSource:#nodeList nodeList:#(boxA, boxB, boxC)
        boxAPrim = (stage1.GetPrimAtPath("/boxA"))
        assert_true ((boxAPrim).IsValid())
        boxAXform = pyUsdGeom.Xformable(boxAPrim)
        assert_true (compareUsdAndMaxTransforms (boxAXform.GetLocalTransformation()) (boxA.transform * inverse stageObject1.transform))
        boxBPrim = (stage1.GetPrimAtPath("/boxA/boxB"))
        assert_true ((boxBPrim).IsValid())
        boxBXform = pyUsdGeom.Xformable(boxBPrim)
        assert_true (compareUsdAndMaxTransforms (boxBXform.GetLocalTransformation()) ((boxB.transform * inverse boxB.parent.transform)))
        boxCPrim = (stage1.GetPrimAtPath("/boxC"))
        assert_true ((boxCPrim).IsValid())
        boxCXform = pyUsdGeom.Xformable(boxCPrim)
        assert_true (compareUsdAndMaxTransforms (boxCXform.GetLocalTransformation()) (boxC.transform * inverse stageObject1.transform))
        stage1.Reload()
            
        -- Inherit OFF
        UsdExporter.ExportToStage stageObject1 exportOptions:opts contentSource:#nodeList nodeList:#(boxA, boxB, boxC) inheritStageObjectTransform:false
        boxAPrim = (stage1.GetPrimAtPath("/boxA"))
        assert_true ((boxAPrim).IsValid())
        boxAXform = pyUsdGeom.Xformable(boxAPrim)
        assert_true (compareUsdAndMaxTransforms (boxAXform.GetLocalTransformation()) boxA.transform)
        boxBPrim = (stage1.GetPrimAtPath("/boxA/boxB"))
        assert_true ((boxBPrim).IsValid())
        boxBXform = pyUsdGeom.Xformable(boxBPrim)
        assert_true (compareUsdAndMaxTransforms (boxBXform.GetLocalTransformation()) ((boxB.transform * inverse boxB.parent.transform)))
        boxCPrim = (stage1.GetPrimAtPath("/boxC"))
        assert_true ((boxCPrim).IsValid())
        boxCXform = pyUsdGeom.Xformable(boxCPrim)
        assert_true (compareUsdAndMaxTransforms (boxCXform.GetLocalTransformation()) (boxC.transform))
        stage1.Reload()
        
        -- Inherit ON
        UsdExporter.ExportToStage stageObject1 exportOptions:opts contentSource:#nodeList nodeList:#(boxA, boxB, boxC)
        boxAPrim = (stage1.GetPrimAtPath("/boxA"))
        assert_true ((boxAPrim).IsValid())
        boxAXform = pyUsdGeom.Xformable(boxAPrim)
        assert_true (compareUsdAndMaxTransforms (boxAXform.GetLocalTransformation()) (boxA.transform * inverse stageObject1.transform))
        boxBPrim = (stage1.GetPrimAtPath("/boxA/boxB"))
        assert_true ((boxBPrim).IsValid())
        boxBXform = pyUsdGeom.Xformable(boxBPrim)
        assert_true (compareUsdAndMaxTransforms (boxBXform.GetLocalTransformation()) ((boxB.transform * inverse boxB.parent.transform)))
        boxCPrim = (stage1.GetPrimAtPath("/boxC"))
        assert_true ((boxCPrim).IsValid())
        boxCXform = pyUsdGeom.Xformable(boxCPrim)
        assert_true (compareUsdAndMaxTransforms (boxCXform.GetLocalTransformation()) (boxC.transform * inverse stageObject1.transform))
        stage1.Reload()
    ),
    
    function test_export_under_transformed_prim = (
        -- 2 object, not parented in 3ds max.
        boxA = box()
        boxA.name = "boxA"
        boxA.position = [10,5,0]
        
        boxB = box()
        boxB.name = "boxB"        
        boxB.position = [10,10,0]
        
        
        stageObject1 = USDStageObject()
        stageObject1.position = [0,0,15]
        stageObject1.SetRootLayer emptyLayer
        
        opts = USDExporter.CreateOptions()
        opts.RootPrimPath = "/"
        
        local stageCache = pyUsdUtils.StageCache.Get()
        local stage1 = stageCache.Find(pyUsd.StageCache.Id.FromLongInt(stageObject1.cacheId))
        stage1.Reload()
        
        UsdExporter.ExportToStage stageObject1 exportOptions:opts contentSource:#nodeList nodeList:#(boxA)
        -- Parent boxB under boxA on the USD side.
        opts.RootPrimPath = "/boxA"
        UsdExporter.ExportToStage stageObject1 exportOptions:opts contentSource:#nodeList nodeList:#(boxB)
            
        boxAPrim = (stage1.GetPrimAtPath("/boxA"))
        assert_true ((boxAPrim).IsValid())
        boxAXform = pyUsdGeom.Xformable(boxAPrim)
        assert_true (compareUsdAndMaxTransforms (boxAXform.GetLocalTransformation()) (boxA.transform * inverse stageObject1.transform))
        boxBPrim = (stage1.GetPrimAtPath("/boxA/boxB"))
        assert_true ((boxBPrim).IsValid())
        boxBXform = pyUsdGeom.Xformable(boxBPrim)
        translate5 = matrix3 [1,0,0] [0,1,0] [0,0,1] [0,5,0]
        assert_true (compareUsdAndMaxTransforms (boxBXform.GetLocalTransformation()) (translate5))           
    ),    
    
    function test_allow_overwrite = (
        boxA = box()
        boxA.name = "boxA"
        boxA1 = box()
        boxA1.name = "boxA1"        
        boxB = box()
        boxB.name = "boxB"
        boxB.parent = boxA
        boxC = box()
        boxC.name = "boxC"

        stageObject1 = USDStageObject()
        stageObject1.SetRootLayer emptyLayer
        
        opts = USDExporter.CreateOptions()
        opts.RootPrimPath = "/"
        
        local stageCache = pyUsdUtils.StageCache.Get()
        local stage1 = stageCache.Find(pyUsd.StageCache.Id.FromLongInt(stageObject1.cacheId))
        stage1.Reload()
        
        -- Two successive exports of the same thing, by default should be handling name conflicts.
        UsdExporter.ExportToStage stageObject1 exportOptions:opts
        boxAPrim = (stage1.GetPrimAtPath("/boxA"))
        boxA1Prim = (stage1.GetPrimAtPath("/boxA1"))
        boxBPrim = (stage1.GetPrimAtPath("/boxA/boxB"))
        boxCPrim = (stage1.GetPrimAtPath("/boxC"))
        assert_true ((boxAPrim).IsValid())
        assert_true ((boxA1Prim).IsValid())
        assert_true ((boxBPrim).IsValid())
        assert_true ((boxCPrim).IsValid())
        UsdExporter.ExportToStage stageObject1 exportOptions:opts
        boxAPrim = (stage1.GetPrimAtPath("/boxA"))
        boxA1Prim = (stage1.GetPrimAtPath("/boxA1"))
        boxA2Prim = (stage1.GetPrimAtPath("/boxA2"))
        boxA3Prim = (stage1.GetPrimAtPath("/boxA3"))
        boxBPrim = (stage1.GetPrimAtPath("/boxA/boxB"))
        boxB2Prim = (stage1.GetPrimAtPath("/boxA2/boxB"))
        boxCPrim = (stage1.GetPrimAtPath("/boxC"))
        boxC1Prim = (stage1.GetPrimAtPath("/boxC1"))
        assert_true ((boxAPrim).IsValid())
        assert_true ((boxA1Prim).IsValid())
        assert_true ((boxA2Prim).IsValid())
        assert_true ((boxA3Prim).IsValid())
        assert_true ((boxBPrim).IsValid())
        assert_true ((boxB2Prim).IsValid())
        assert_true ((boxCPrim).IsValid())
        assert_true ((boxC1Prim).IsValid())
        stage1.Reload()
        
        -- Now allowing ovewrites on the second export...
        UsdExporter.ExportToStage stageObject1 exportOptions:opts
        boxAPrim = (stage1.GetPrimAtPath("/boxA"))
        boxA1Prim = (stage1.GetPrimAtPath("/boxA1"))
        boxBPrim = (stage1.GetPrimAtPath("/boxA/boxB"))
        boxCPrim = (stage1.GetPrimAtPath("/boxC"))
        assert_true ((boxAPrim).IsValid())
        assert_true ((boxA1Prim).IsValid())
        assert_true ((boxBPrim).IsValid())
        assert_true ((boxCPrim).IsValid())
        UsdExporter.ExportToStage stageObject1 exportOptions:opts allowOverwrite:true
        boxAPrim = (stage1.GetPrimAtPath("/boxA"))
        boxA1Prim = (stage1.GetPrimAtPath("/boxA1"))
        boxA2Prim = (stage1.GetPrimAtPath("/boxA2"))
        boxA3Prim = (stage1.GetPrimAtPath("/boxA3"))
        boxBPrim = (stage1.GetPrimAtPath("/boxA/boxB"))
        boxB2Prim = (stage1.GetPrimAtPath("/boxA2/boxB"))
        boxCPrim = (stage1.GetPrimAtPath("/boxC"))
        boxC1Prim = (stage1.GetPrimAtPath("/boxC1"))
        assert_true ((boxAPrim).IsValid())
        assert_true ((boxA1Prim).IsValid())
        assert_false ((boxA2Prim).IsValid())
        assert_false ((boxA3Prim).IsValid())
        assert_true ((boxBPrim).IsValid())
        assert_false ((boxB2Prim).IsValid())
        assert_true ((boxCPrim).IsValid())
        assert_false ((boxC1Prim).IsValid())
        stage1.Reload()
                    
        -- Explicit disallow overwrites.
        UsdExporter.ExportToStage stageObject1 exportOptions:opts
        boxAPrim = (stage1.GetPrimAtPath("/boxA"))
        boxA1Prim = (stage1.GetPrimAtPath("/boxA1"))
        boxBPrim = (stage1.GetPrimAtPath("/boxA/boxB"))
        boxCPrim = (stage1.GetPrimAtPath("/boxC"))
        assert_true ((boxAPrim).IsValid())
        assert_true ((boxA1Prim).IsValid())
        assert_true ((boxBPrim).IsValid())
        assert_true ((boxCPrim).IsValid())
        UsdExporter.ExportToStage stageObject1 exportOptions:opts allowOverwrite:false
        boxAPrim = (stage1.GetPrimAtPath("/boxA"))
        boxA1Prim = (stage1.GetPrimAtPath("/boxA1"))
        boxA2Prim = (stage1.GetPrimAtPath("/boxA2"))
        boxA3Prim = (stage1.GetPrimAtPath("/boxA3"))
        boxBPrim = (stage1.GetPrimAtPath("/boxA/boxB"))
        boxB2Prim = (stage1.GetPrimAtPath("/boxA2/boxB"))
        boxCPrim = (stage1.GetPrimAtPath("/boxC"))
        boxC1Prim = (stage1.GetPrimAtPath("/boxC1"))
        assert_true ((boxAPrim).IsValid())
        assert_true ((boxA1Prim).IsValid())
        assert_true ((boxA2Prim).IsValid())
        assert_true ((boxA3Prim).IsValid())
        assert_true ((boxBPrim).IsValid())
        assert_true ((boxB2Prim).IsValid())
        assert_true ((boxCPrim).IsValid())
        assert_true ((boxC1Prim).IsValid())
        stage1.Reload()
    ),
    
    function test_respect_edit_target = (
        boxA = box()
        boxA.name = "boxA"
        
        boxB = box()
        boxB.name = "boxB"
        
        stageObject1 = USDStageObject()
        stageObject1.position = [0,0,15]
        stageObject1.SetRootLayer emptyLayer
        
        opts = USDExporter.CreateOptions()
        opts.RootPrimPath = "/"
        
        local stageCache = pyUsdUtils.StageCache.Get()
        local stage1 = stageCache.Find(pyUsd.StageCache.Id.FromLongInt(stageObject1.cacheId))
        stage1.Reload()
        
        -- Default should inherit the stage object transform.
        UsdExporter.ExportToStage stageObject1 exportOptions:opts contentSource:#nodeList nodeList:#(boxA)
        
        -- Check that we authored on the default edit target of USDStageObject, i.e. the root layer..
        boxAPrim = (stage1.GetRootLayer()).GetPrimAtPath("/boxA")
        assert_defined boxAPrim
        
        -- Target the session layer...
        stage1.SetEditTarget(stage1.GetSessionLayer())        
        UsdExporter.ExportToStage stageObject1 exportOptions:opts contentSource:#nodeList nodeList:#(boxB)
        boxBOnRoot = (stage1.GetRootLayer()).GetPrimAtPath("/boxB")
        assert_undefined boxBOnRoot
        boxBOnSession = (stage1.GetSessionLayer()).GetPrimAtPath("/boxB")
        assert_defined boxBOnSession
    ),
    
    function test_export_to_variant = (
        boxA = box()
        boxA.name = "boxA"
        boxB = box()
        boxB.name = "boxB"
        
        stageObject1 = USDStageObject()
        stageObject1.position = [0,0,15]
        stageObject1.SetRootLayer emptyLayer
        
        opts = USDExporter.CreateOptions()    
        
        local stageCache = pyUsdUtils.StageCache.Get()
        local stage1 = stageCache.Find(pyUsd.StageCache.Id.FromLongInt(stageObject1.cacheId))
        stage1.Reload()
            
        editTargetBefore = stage1.GetEditTarget()
        
        -- Export 2 variants
        opts.RootPrimPath = "/root/variant{test=A}"
        UsdExporter.ExportToStage stageObject1 exportOptions:opts contentSource:#nodeList nodeList:#(boxA)
        opts.RootPrimPath = "/root/variant{test=B}"
        UsdExporter.ExportToStage stageObject1 exportOptions:opts contentSource:#nodeList nodeList:#(boxB)    
        
        -- Check that we authored on the default edit target of USDStageObject, i.e. the root layer..
        variantPrim = (stage1).GetPrimAtPath("/root/variant")
        varSet = (variantPrim.GetVariantSets()).GetVariantSet("test")
        assert_equal "['A', 'B']" (varSet.GetVariantNames() as string)
        -- Variant selection set to the last written.
        assert_equal "B" (varSet.GetVariantSelection())
        
        varSet.SetVariantSelection("A")
        assert_true ((stage1.GetPrimAtPath("/root/variant/boxA")).IsValid())
        assert_false ((stage1.GetPrimAtPath("/root/variant/boxB")).IsValid())
        
        varSet.SetVariantSelection("B")
        assert_false ((stage1.GetPrimAtPath("/root/variant/boxA")).IsValid())
        assert_true ((stage1.GetPrimAtPath("/root/variant/boxB")).IsValid())
        stage1.Reload()
        
        -- Make sure the edit target was reset to what is was before, it is swtiched
        -- to the variant target internally as we export variants.
        assert_equal editTargetBefore (stage1.GetEditTarget())
    ),

    function test_preserve_existing_root_type = (
        -- This tests that if the root prim for the export already exists, we do not override its type.
        
        boxA = box()
        boxA.name = "boxA"
        
        stageObject1 = USDStageObject()
        stageObject1.position = [0,0,15]
        stageObject1.SetRootLayer emptyLayer
        
        opts = USDExporter.CreateOptions()    
        
        local stageCache = pyUsdUtils.StageCache.Get()
        local stage1 = stageCache.Find(pyUsd.StageCache.Id.FromLongInt(stageObject1.cacheId))
        stage1.Reload()
        
        -- If root prim doesnt exist, created as Xform
        opts.RootPrimPath = "/test"
        UsdExporter.ExportToStage stageObject1 exportOptions:opts contentSource:#nodeList nodeList:#(boxA)
        assert_true ((stage1.GetPrimAtPath("/test")).IsA(pyUsdGeom.Xform))
        stage1.Reload()
            
        pyUsdGeom.Scope.Define stage1 "/test"
        -- If it does exist, keep its type.
        UsdExporter.ExportToStage stageObject1 exportOptions:opts contentSource:#nodeList nodeList:#(boxA)
        assert_true ((stage1.GetPrimAtPath("/test")).IsA(pyUsdGeom.Scope))
        stage1.Reload()
    ),
    
    function test_preserve_default_prim = (
        -- This tests that if there is already a default prim setup, we do not overwrite it..
        boxA = box()
        boxA.name = "boxA"
        
        stageObject1 = USDStageObject()
        stageObject1.position = [0,0,15]
        stageObject1.SetRootLayer emptyLayer
        
        opts = USDExporter.CreateOptions()    
        
        local stageCache = pyUsdUtils.StageCache.Get()
        local stage1 = stageCache.Find(pyUsd.StageCache.Id.FromLongInt(stageObject1.cacheId))
        stage1.Reload()
            
        testPrimScope = pyUsdGeom.Scope.Define stage1 "/test"
        testPrim = testPrimScope.GetPrim()

        stage1.SetDefaultPrim(testPrim)
                
        opts.RootPrimPath = "/foo"
        UsdExporter.ExportToStage stageObject1 exportOptions:opts contentSource:#nodeList nodeList:#(boxA)
        
        -- Default prim has not changed...
        assert_equal testPrim (stage1.GetDefaultPrim())
        
        -- Start over, this time, just export.     
        stage1.Reload()
        opts.RootPrimPath = "/foo/bar/baz"
        UsdExporter.ExportToStage stageObject1 exportOptions:opts contentSource:#nodeList nodeList:#(boxA)
        -- Standard exporter behavior, use the root most prim created as default.
        assert_equal (stage1.GetPrimAtPath("/foo")) (stage1.GetDefaultPrim())
    ),    
    
    function test_seperate_material_layer_option_ignored = (
        -- This tests that if there is already a default prim setup, we do not overwrite it..
        boxA = box()
        boxA.name = "boxA"
        boxA.material = MaxUsdPreviewSurface()
        
        stageObject1 = USDStageObject()
        stageObject1.SetRootLayer emptyLayer
        
        opts = USDExporter.CreateOptions()    
        opts.UseSeparateMaterialLayer = true
        
        local stageCache = pyUsdUtils.StageCache.Get()
        local stage1 = stageCache.Find(pyUsd.StageCache.Id.FromLongInt(stageObject1.cacheId))
        stage1.Reload()
        
        UsdExporter.ExportToStage stageObject1 exportOptions:opts contentSource:#nodeList nodeList:#(boxA)
        
        sublayers = (stage1.GetRootLayer()).subLayerPaths
        assert_equal "[]" (sublayers as string)
    ),
    
    function test_export_to_cached_stage = (
        boxA = box()
        boxA.name = "boxA"
        
        local stage = pyUsd.Stage.CreateInMemory()
        -- Not interested in testing axis/unit conversion here.
        pyUsdGeom.SetStageUpAxis stage pyUsdGeom.Tokens.z
        pyUsdGeom.SetStageMetersPerUnit stage 0.0254
        
        local stageCache = pyUsdUtils.StageCache.Get()
        stageCache.Insert(stage)
        local stageId = (stageCache.GetId(stage)).ToLongInt()
        
        opts = USDExporter.CreateOptions()    
        
        translate10 = matrix3 [1,0,0] [0,1,0] [0,0,1] [0,0,10]
            
        local success = UsdExporter.ExportToCachedStage stageId exportOptions:opts contentSource:#nodeList nodeList:#(boxA) rootTransform:translate10
        assert_true success
            
        boxAPrim = stage.GetPrimAtPath("/root/boxA")
        assert_true (boxAPrim.IsValid())
        xformableA = pyUsdGeom.Xformable(boxAPrim)
        -- Test the root transform was applied.
        assert_true (compareUsdAndMaxTransforms (xformableA.GetLocalTransformation()) (inverse translate10))
        
        (stage.GetRootLayer()).Clear()
        -- Not interested in testing axis/unit conversion in this test.
        pyUsdGeom.SetStageUpAxis stage pyUsdGeom.Tokens.z
        pyUsdGeom.SetStageMetersPerUnit stage 0.0254
            
        UsdExporter.ExportToCachedStage stageId exportOptions:opts contentSource:#nodeList nodeList:#(boxA)
                    
        boxAPrim = stage.GetPrimAtPath("/root/boxA")
        assert_true (boxAPrim.IsValid())
        xformableA = pyUsdGeom.Xformable(boxAPrim)
            
        identMatrix = matrix3 [1,0,0] [0,1,0] [0,0,1] [0,0,0]
        assert_true (compareUsdAndMaxTransforms (xformableA.GetLocalTransformation()) identMatrix)
        
        -- Bad cache id, fails export
        assert_false (UsdExporter.ExportToCachedStage 0)
    ),
        
    function test_respect_stage_up_axis_and_units = (
        -- This tests that if there is already a default prim setup, we do not overwrite it..
        coneA = Cone()
        coneA.name = "coneA"
        
        -- Units in the output usd layer are in centimeters.
        -- Intentionnally setup a mismatching unit in the scene.
        units.SystemType = #inches
        units.SystemScale = 1
        
        stageObject1 = USDStageObject()
        stageObject1.SetRootLayer emptyLayer
        -- Give the stage a transform, to make sure the scaling applies to its offset as well.
        stageObject1.position = [0,10,0]
        
        opts = USDExporter.CreateOptions()    
        
        local stageCache = pyUsdUtils.StageCache.Get()
        local stage1 = stageCache.Find(pyUsd.StageCache.Id.FromLongInt(stageObject1.cacheId))
        stage1.Reload()
            
        -- Configure the export options with a mismatched up axis.
        pyUsdGeom.SetStageUpAxis stage1 pyUsdGeom.Tokens.y
        opts.UpAxis = #z    
        -- Similarly, setup different target units.
        pyUsdGeom.SetStageMetersPerUnit stage1 0.01
        
        -- Make sure the option was ignored, stage should still be Y up.
        UsdExporter.ExportToStage stageObject1 exportOptions:opts contentSource:#nodeList nodeList:#(coneA)
        assert_equal pyUsdGeom.Tokens.y (pyUsdGeom.GetStageUpAxis(stage1))
        -- Stage should also still be in the same units.
        assert_equal 0.01 (pyUsdGeom.GetStageMetersPerUnit(stage1))
            
        
        conePrim = stage1.GetPrimAtPath("/root/coneA")
        assert_true (conePrim.IsValid())
        xformableCone = pyUsdGeom.Xformable(conePrim)
            
        -- Make sure the transform to Y-up was applied, and scaling for units (inches->cm)
        axisRotMatrix = matrix3 [2.54,0,0] [0,0,-2.54] [0,2.54,0] [0,0,25.4]
        assert_true (compareUsdAndMaxTransforms (xformableCone.GetLocalTransformation()) axisRotMatrix)
    ),
    
    function test_update_time_metadata = (
        
        local opts = USDExporter.CreateOptions()        
        
        -- Animate a cube, moving the top face [0-10]
        local cube = Box name:"cube"
        convertToPoly cube
        polyop.setFaceSelection cube  #{2}
        with animate on 
        (
            at time 10 move cube.selectedFaces [0,0,10]
        )
        
            
        stageObject1 = USDStageObject()
        stageObject1.SetRootLayer emptyLayer
        
        opts = USDExporter.CreateOptions()
        opts.TimeMode = #frameRange
        opts.StartFrame = 0
        opts.EndFrame = 10
        
        local stageCache = pyUsdUtils.StageCache.Get()
        local stage = stageCache.Find(pyUsd.StageCache.Id.FromLongInt(stageObject1.cacheId))
        stage.Reload()
            
        UsdExporter.ExportToStage stageObject1 exportOptions:opts contentSource:#nodeList nodeList:#(cube)
        -- Nothing was previously authored.
        assert_equal 0 (stage.GetStartTimeCode())
        assert_equal 10 (stage.GetEndTimeCode())
                            
        -- Now test that that the authored usd time code range is expanded if necessary.
        
        stage.Reload()
        stage.SetStartTimeCode(-5)
        stage.SetEndTimeCode(5)
        UsdExporter.ExportToStage stageObject1 exportOptions:opts contentSource:#nodeList nodeList:#(cube)
        assert_equal -5 (stage.GetStartTimeCode())
        assert_equal 10 (stage.GetEndTimeCode())
            
        stage.Reload()
        stage.SetStartTimeCode(5)
        stage.SetEndTimeCode(15)
        UsdExporter.ExportToStage stageObject1 exportOptions:opts contentSource:#nodeList nodeList:#(cube)
        assert_equal 0 (stage.GetStartTimeCode())
        assert_equal 15 (stage.GetEndTimeCode())
            
        stage.Reload()
        stage.SetStartTimeCode(2)
        stage.SetEndTimeCode(8)
        UsdExporter.ExportToStage stageObject1 exportOptions:opts contentSource:#nodeList nodeList:#(cube)
        assert_equal 0 (stage.GetStartTimeCode())
        assert_equal 10 (stage.GetEndTimeCode())
            
        stage.Reload()
        stage.SetStartTimeCode(-5)
        stage.SetEndTimeCode(15)
        UsdExporter.ExportToStage stageObject1 exportOptions:opts contentSource:#nodeList nodeList:#(cube)
        assert_equal -5 (stage.GetStartTimeCode())
        assert_equal 15 (stage.GetEndTimeCode())
        
        -- Force a mismatch in frames per second configs.
        -- For now we do not support this, only warn about it. The stage's metadata should not 
        -- change.
        stage.Reload()
        stage.SetTimeCodesPerSecond(99)
        UsdExporter.ExportToStage stageObject1 exportOptions:opts contentSource:#nodeList nodeList:#(cube)
        assert_equal 99 (stage.GetTimeCodesPerSecond())
    ),

    function test_worldspace_roots = (
        
        boxA = box()
        boxA.name = "boxA"
        boxA.position = [10,5,0]
        
        boxB = box()
        boxB.name = "boxB"
        boxB.parent = boxA
        boxB.position = [10,10,0]
        
        stageObject1 = USDStageObject()
        stageObject1.position = [0,0,9]
        stageObject1.SetRootLayer emptyLayer
        
        opts = USDExporter.CreateOptions()
        opts.RootPrimPath = "/"
        
        local stageCache = pyUsdUtils.StageCache.Get()
        local stage1 = stageCache.Find(pyUsd.StageCache.Id.FromLongInt(stageObject1.cacheId))
        stage1.Reload()
        
        -- Export only the parented box. The "use worldspace roots" option only applies to nodes exported without their parents.
        translate5 = matrix3 [1,0,0] [0,1,0] [0,0,1] [0,5,0]
                
        -- Enable world space roots.
        stage1.Reload()
        opts.UseWorldspaceRoot  = true
        UsdExporter.ExportToStage stageObject1 exportOptions:opts contentSource:#nodeList nodeList:#(boxB)inheritStageObjectTransform:true
        boxBPrim = (stage1.GetPrimAtPath("/boxB"))
        xformableB = pyUsdGeom.Xformable(boxBPrim)
        assert_true (compareUsdAndMaxTransforms (xformableB.GetLocalTransformation()) (boxB.transform * inverse stageObject1.transform))
        
        -- Disable again
        stage1.Reload()
        opts.UseWorldspaceRoot = false
        UsdExporter.ExportToStage stageObject1 exportOptions:opts contentSource:#nodeList nodeList:#(boxB) inheritStageObjectTransform:true
        boxBPrim = (stage1.GetPrimAtPath("/boxB"))
        xformableB = pyUsdGeom.Xformable(boxBPrim)
        assert_true (compareUsdAndMaxTransforms (xformableB.GetLocalTransformation()) (translate5))
            
        -- Enable world space roots when NOT inheriting the stage transform...
        stage1.Reload()
        opts.UseWorldspaceRoot  = true
        UsdExporter.ExportToStage stageObject1 exportOptions:opts contentSource:#nodeList nodeList:#(boxB) inheritStageObjectTransform:false
        boxBPrim = (stage1.GetPrimAtPath("/boxB"))
        xformableB = pyUsdGeom.Xformable(boxBPrim)
        assert_true (compareUsdAndMaxTransforms (xformableB.GetLocalTransformation()) (boxB.transform))
        
        -- Disable again
        stage1.Reload()
        opts.UseWorldspaceRoot = false
        UsdExporter.ExportToStage stageObject1 exportOptions:opts contentSource:#nodeList nodeList:#(boxB) inheritStageObjectTransform:false
        boxBPrim = (stage1.GetPrimAtPath("/boxB"))
        xformableB = pyUsdGeom.Xformable(boxBPrim)
        assert_true (compareUsdAndMaxTransforms (xformableB.GetLocalTransformation()) (translate5))
    ),
    
    function test_logging = (
        boxA = box()
        boxA.name = "boxA"
                
        stageObject1 = USDStageObject()

        stageObject1.SetRootLayer emptyLayer
        local stageCache = pyUsdUtils.StageCache.Get()
        local stage1 = stageCache.Find(pyUsd.StageCache.Id.FromLongInt(stageObject1.cacheId))
        stage1.Reload()
        
        opts = USDExporter.CreateOptions()
        opts.LogLevel = #warn
        
        -- Force a mismatch in frames per second configs. This will generate a warning in the log.
        stage1.SetTimeCodesPerSecond(99)
        UsdExporter.ExportToStage stageObject1 exportOptions:opts contentSource:#nodeList nodeList:#(cube)
        assert_equal 99 (stage1.GetTimeCodesPerSecond())
        
        local fs = openFile opts.LogPath
        assert_defined (skipToString fs "Inconsistent frames per second")
        deleteFile opts.LogPath
    ),
    
    function test_undo_redo = (
        boxA = box()
        boxA.name = "boxA"                
        stageObject1 = USDStageObject()

        stageObject1.SetRootLayer emptyLayer
        local stageCache = pyUsdUtils.StageCache.Get()
        local stage1 = stageCache.Find(pyUsd.StageCache.Id.FromLongInt(stageObject1.cacheId))
        stage1.Reload()
        
        opts = USDExporter.CreateOptions()
        UsdExporter.ExportToStage stageObject1 exportOptions:opts contentSource:#nodeList nodeList:#(boxA)
        assert_true ((stage1.GetPrimAtPath("/root/boxA")).IsValid())
        
        max undo
        assert_false ((stage1.GetPrimAtPath("/root/boxA")).IsValid())
        max redo
        assert_true ((stage1.GetPrimAtPath("/root/boxA")).IsValid())
    ),
    
    function test_default_prim_token = (
        box1 = box()
        box1.name = "box1"                
        stageObject = USDStageObject()
        
        local stageCache = pyUsdUtils.StageCache.Get()
        local stage = stageCache.Find(pyUsd.StageCache.Id.FromLongInt(stageObject.cacheId))
        
        pyUsdGeom.SetStageUpAxis stage pyUsdGeom.Tokens.z
        fooPrim = stage.DefinePrim "/foo" "Xform"
        
        opts = USDExporter.CreateOptions()

        -- Use the token, but there is no current default prim.
        -- Should fallback to option defaults (/root)
        opts.RootPrimPath = "DEFAULT_PRIM"
            
        UsdExporter.ExportToStage stageObject exportOptions:opts contentSource:#nodeList nodeList:#(box1)
        assert_true ((stage.GetPrimAtPath("/root/box1")).IsValid())

        stage.SetDefaultPrim fooPrim
            
        box2 = box()
        box2.name = "box2"
            
        UsdExporter.ExportToStage stageObject exportOptions:opts contentSource:#nodeList nodeList:#(box2)
        assert_true ((stage.GetPrimAtPath("/foo/box2")).IsValid())

        barPrim = stage.DefinePrim "/bar" "Xform"    
        
        box3 = box()
        box3.name = "box3"
        
        stage.SetDefaultPrim barPrim
        
        UsdExporter.ExportToStage stageObject exportOptions:opts contentSource:#nodeList nodeList:#(box3)
        assert_true ((stage.GetPrimAtPath("/bar/box3")).IsValid())
                
        box4 = box()
        box4.name = "box4"
        opts.RootPrimPath = "DEFAULT_PRIM/some/child"
            
        UsdExporter.ExportToStage stageObject exportOptions:opts contentSource:#nodeList nodeList:#(box4)
        assert_true ((stage.GetPrimAtPath("/bar/some/child/box4")).IsValid())
    ),
    
    function test_export_only_materials_to_stage = (
        local box001 = Box pos:[0,0,0] name:"box001"
        local physicalMtl = Physical_Material name:"test_only_mtl"
        local nonExportedMtl = Physical_Material name:"nonExportedMtl"
        
        box001.material = physicalMtl
        
        local stageObject1 = USDStageObject()
        stageObject1.SetRootLayer emptyLayer
    
        local stageCache = pyUsdUtils.StageCache.Get()
        local stage1 = stageCache.Find(pyUsd.StageCache.Id.FromLongInt(stageObject1.cacheId))
        stage1.Reload()
        
        local opts = USDExporter.CreateOptions()
        opts.Materials = true
        local mList = #(physicalMtl)
        
        UsdExporter.ExportToStage stageObject1 exportOptions:opts contentSource:#materialList materialList:mList
        
        -- Validate that material exists
        local mtlPrim = stage1.GetPrimAtPath("/root/mtl/test_only_mtl")
        assert_true (mtlPrim.IsValid())
        
        -- Validate that geometry does not exist  
        local boxPrim = stage1.GetPrimAtPath("/root/box001")
        assert_false (boxPrim.IsValid())

        -- Validate that the material that was not part of the list was not exported
        local invalidMtl = stage1.GetPrimAtPath("/root/mtl/nonExportedMtl")
        assert_false (invalidMtl.IsValid())
    ),
    
    function test_export_node_and_materials_to_stage = (
        local box001 = Box pos:[0,0,0] name:"box001"
        local physicalMtl = Physical_Material name:"assigned_material"
        local nonAssignedMat = Physical_Material name:"nonAssigned_material"
        box001.material = physicalMtl
        local nonExportedBox = Box pos:[10,10,10] name:"nonExportedBox"
        local nonExportedMtl = Physical_Material name:"nonExportedMtl"
        local mtlList = #(nonAssignedMat)
        local nList = #(box001)
        
        local stageObject1 = USDStageObject()
        stageObject1.SetRootLayer emptyLayer
    
        local stageCache = pyUsdUtils.StageCache.Get()
        local stage1 = stageCache.Find(pyUsd.StageCache.Id.FromLongInt(stageObject1.cacheId))
        stage1.Reload()
        
        local opts = USDExporter.CreateOptions()
        opts.Materials = true
        
        UsdExporter.ExportToStage stageObject1 exportOptions:opts contentSource:#nodeAndMaterialList nodeList:nList materialList:mtlList
        
        -- Validate that mesh geometry exists
        local boxPrim = stage1.GetPrimAtPath("/root/box001")
        assert_true (boxPrim.IsValid())
        
        -- Validate that material bound to the box is exported
        local assignedMtlPrim = stage1.GetPrimAtPath("/root/mtl/assigned_material")
        assert_true (assignedMtlPrim.IsValid())
        
        -- Validate that the non assigned material is exported
        local mtlPrim = stage1.GetPrimAtPath("/root/mtl/nonAssigned_material")
        assert_true (mtlPrim.IsValid())

        -- Validate that non-list members are not exported
        local invalidPrim = stage1.GetPrimAtPath("/root/nonExportedBox")
        assert_false (invalidPrim.IsValid())
        local invalidMtl = stage1.GetPrimAtPath("/root/mtl/nonExportedMtl")
        assert_false (invalidMtl.IsValid())
    ),
        
    function teardown = (
        units.SystemType = initialUnits
        units.SystemScale = initialScale
        frameRate = initialFrameRate
    ),
        
    Tests = #(test_export_selection, 
        test_export_node_list,
        test_export_all,
        test_inherit_stage_transform,
        test_export_under_transformed_prim,
        test_allow_overwrite,
        test_respect_edit_target,
        test_export_to_variant,
        test_preserve_existing_root_type,
        test_preserve_default_prim,
        test_seperate_material_layer_option_ignored,
        test_export_to_cached_stage,
        test_respect_stage_up_axis_and_units,
        test_update_time_metadata,
        test_worldspace_roots,
        test_logging,
        test_undo_redo,
        test_default_prim_token,
        test_export_only_materials_to_stage,
        test_export_node_and_materials_to_stage)
)

runUsdTestFixture export_to_stage (getThisScriptFilename())