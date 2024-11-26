--
-- Copyright 2023 Autodesk
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
pyUsdShade = python.import("pxr.UsdShade")
pySdf = python.import("pxr.Sdf")
pyGf = python.import("pxr.Gf")
pyUsdSkel = python.import("pxr.UsdSkel")

filename_prefix = "EXPORT_OPTIONS_TEST_"
output_prefix = pathConfig.appendPath (getDir #temp) ("/" + filename_prefix)
deleteTestFiles(output_prefix + "*.*")

struct export_options_test
(  
    public 
    function setup =
    (
        resetmaxfile #noprompt
    ),

    function test_ui_options_accessors = (
        -- Make sure UI options are properly set by default
        local uiExportOptions = USDExporter.UIOptions
        uiExportOptions.Reset()

        assert_true uiExportOptions.Meshes message:"Meshes property was not properly initialized to defaults (true)."
        assert_true uiExportOptions.Shapes message:"Shapes property was not properly initialized to defaults (true)."
        assert_true uiExportOptions.Lights message:"Lights property was not properly initialized to defaults (true)."
        assert_true uiExportOptions.Cameras message:"Cameras property was not properly initialized to defaults (true)."
        assert_true uiExportOptions.Materials message:"Materials property was not properly initialized to defaults (true)."
        assert_false uiExportOptions.Skin message:"Skin property was not properly initialized to defaults (false)."
        assert_false uiExportOptions.Morpher message:"Morpher property was not properly initialized to defaults (false)."
        assert_equal "Bones" uiExportOptions.BonesPrimName message:"BonesPrimName property was not properly initialized to defaults (Bones)."
        assert_equal "Animations" uiExportOptions.AnimationsPrimName message:"AnimationsPrimName property was not properly initialized to defaults (Animations)."
        assert_equal "useRegistry" uiExportOptions.ShadingMode message:"ShadingMode was now properly initialized to defaults (\"useRegistry\")"
        -- FIXME : Account for the maxToA export plugin maybe being installed. We do not have a way know for sure if a version
        -- of maxToA with USD support will be installed in the 3dsMax build running in the pipeline.
        local matTargets = (uiExportOptions.AvailableMaterialTargets() as string)
        assert_true ((#("MaterialX", "UsdPreviewSurface") as string) == matTargets or (#("Arnold", "MaterialX", "UsdPreviewSurface") as string) == matTargets) message:"Current list of available material targets is invalid)."
        assert_equal (#("UsdPreviewSurface") as string) (uiExportOptions.AllMaterialTargets as string) message:"Selected material target was not properly initialized to default (UsdPreviewSurface)."
        assert_true uiExportOptions.HiddenObjects message:"HiddenObjects property was not properly initialized to defaults (true)."
        assert_false uiExportOptions.UseUSDVisibility message:"UseUSDVisibility property was not properly initialized to defaults (false)."
        assert_equal #binary uiExportOptions.FileFormat message:"File format property was not properly initialized to defaults (#binary). "
        assert_equal #z uiExportOptions.UpAxis message:"UpAxis property was not properly initialized to defaults (#z)."
        assert_equal ((GetDir #temp) + "\\MaxUsdExport.log") uiExportOptions.LogPath message:"The LogPath property was not properly initialized."
        assert_equal #off uiExportOptions.LogLevel message:"The LogLevel property was not properly initialized."
        assert_false uiExportOptions.PreserveEdgeOrientation message:"The PreserveEdgeOrientation property was not properly initialized to default (false)."
        assert_equal #asPrimvar uiExportOptions.Normals message:"Normals property was not properly initialized to defaults (#asPrimvar)."
        assert_equal #fromScene uiExportOptions.MeshFormat message:"MeshFormat property was not properly initialized to defaults (#fromScene)."
        assert_equal #current uiExportOptions.TimeMode message:"The TimeMode property was not properly initialized to defaults (#current)."
        assert_equal 0.0 uiExportOptions.StartFrame message:"The StartFrame property was not properly initialized to defaults (0.0)."
        assert_equal 0.0 uiExportOptions.EndFrame message:"The EndFrame property was not properly initialized to defaults (0.0)."
        assert_equal 1.0 uiExportOptions.SamplesPerFrame message:"The SamplesPerFrame property was not properly initialized to defaults (1.0)."
        assert_equal "/root" uiExportOptions.RootPrimPath message:"The RootPrimPath property was not properly initialized to defaults (\"/root\")."
        assert_false uiExportOptions.OpenInUsdview message:"The OpenInUsdview property was not properly initialized to default (false)."
        assert_true uiExportOptions.UsdStagesAsReferences message:"The UsdStagesAsReferences property was not properly initialized to default (true)."
        assert_equal false uiExportOptions.AllowNestedGprims message:"The AllowNestedGprims property was not properly initialized to defaults (false)."
        assert_equal (#() as string) (uiExportOptions.ContextNames as string) message:"The ContextNames property was not properly initialized."
        assert_equal (#() as string) (uiExportOptions.ChaserNames as string) message:"The ChaserNames property was not properly initialized."
        maxver = maxversion()
        if maxver[1] < 27000 or (maxver[1] == 27000 and maxver[5] == 0) then (
            assert_equal ("Dictionary #string " as string) (uiExportOptions.AllChaserArgs as string) message:"The AllChaserArgs property was not properly initialized."
        )
        else (
            assert_equal ("(Dictionary #string )" as string) (uiExportOptions.AllChaserArgs as string) message:"The AllChaserArgs property was not properly initialized."
        )
        assert_equal "<filename>_mtl.usda" uiExportOptions.MaterialLayerPath message:"The MaterialLayerPath property was not properly initialized."
        if maxver[1] >= 26000 then (  -- 3ds Max 2024 and up
            assert_equal #asVariantSets uiExportOptions.MtlSwitcherExportStyle message:"Material Switcher export style property was not properly initialized to defaults (#asVariantSets). "
        )
        assert_true uiExportOptions.UseProgressBar message:"UseProgressBar property was not properly initialized to defaults (true)."
        assert_true uiExportOptions.UseLastResortUSDPreviewSurfaceWriter message:"UseLastResortUSDPreviewSurfaceWriter property was not properly initialized to defaults (true)."

        -- Create new options and set them all to non-default values
        local exportOptions = USDExporter.CreateOptions()
        local mtlPath = (output_prefix + "mtl_path.usd")

        exportOptions.Meshes = false
        exportOptions.Shapes = false
        exportOptions.Lights = false
        exportOptions.Cameras = false
        exportOptions.Materials = false
        exportOptions.Skin = true
        exportOptions.Morpher = true
        exportOptions.BonesPrimName = "TestBones"
        exportOptions.AnimationsPrimName = "TestAnimations"
        exportOptions.AllMaterialTargets = #()
        exportOptions.HiddenObjects = false
        exportOptions.UseUSDVisibility = true
        exportOptions.FileFormat = #ascii
        exportOptions.UpAxis = #y
        exportOptions.LogLevel = #info
        exportOptions.LogPath = (output_prefix + "log_path.txt")
        exportOptions.PreserveEdgeOrientation = true
        exportOptions.Normals = #none
        exportOptions.MeshFormat = #triMesh
        exportOptions.TimeMode = #explicit
        exportOptions.StartFrame = 10.5
        exportOptions.EndFrame = 20.5
        exportOptions.SamplesPerFrame = 3.5
        exportOptions.RootPrimPath = "/foo/bar"
        exportOptions.OpenInUsdview = true
        exportOptions.UsdStagesAsReferences = false
        exportOptions.AllowNestedGprims = true
        exportOptions.ContextNames = #("foo", "bar")
        exportOptions.ChaserNames = #("baz", "qux")
        exportOptions.AllChaserArgs = #("chaser1", "param", "val")
        exportOptions.MaterialLayerPath = mtlPath
        if maxver[1] >= 26000 then (  -- 3ds Max 2024 and up
            exportOptions.MtlSwitcherExportStyle = #activeMaterial
        )
        exportOptions.UseProgressBar = false
        exportOptions.UseLastResortUSDPreviewSurfaceWriter = false
        
        -- Test the UI options setter
        USDExporter.UIOptions = exportOptions

        -- The new global options should have been updated
        uiExportOptions = USDExporter.UIOptions

        assert_false uiExportOptions.Meshes message:"Meshes property was not properly set to false."
        assert_false uiExportOptions.Shapes message:"Shapes property was not properly set to false."
        assert_false uiExportOptions.Lights message:"Lights property was not properly set to false."
        assert_false uiExportOptions.Cameras message:"Cameras property was not properly set to false."
        assert_false uiExportOptions.Materials message:"Materials property was not properly set to false."
        assert_true uiExportOptions.Skin message:"Skin property was not properly set to true."
        assert_true uiExportOptions.Morpher message:"Morpher property was not properly set to true."
        assert_equal "TestBones" uiExportOptions.BonesPrimName message:"BonesPrimName was not properly set to 'TestBones'."
        assert_equal "TestAnimations" uiExportOptions.AnimationsPrimName message:"AnimationsPrimName was not properly set to 'TestAnimations'."
        assert_equal "none" uiExportOptions.ShadingMode message:"ShadingMode was now properly set to 'none' when no material target is selected."
        assert_true ((#("MaterialX", "UsdPreviewSurface") as string) == matTargets or (#("Arnold", "MaterialX", "UsdPreviewSurface") as string) == matTargets)
        assert_equal (#() as string) (uiExportOptions.AllMaterialTargets as string) message:"Selected material target should be set to 'none'."
        assert_false uiExportOptions.HiddenObjects message:"HiddenObjects property was not properly set to false."
        assert_true uiExportOptions.UseUSDVisibility message:"UseUSDVisibility property was not properly set to true."
        assert_equal #ascii uiExportOptions.FileFormat message:"File format property was not properly set to #ascii. "
        assert_equal #y uiExportOptions.UpAxis message:"UpAxis property was not properly set to #y)."
        assert_equal (output_prefix + "log_path.txt") uiExportOptions.LogPath message:"The LogPath property was not properly set."
        assert_equal #info uiExportOptions.LogLevel message:"The LogLevel property was not properly set."
        assert_true uiExportOptions.PreserveEdgeOrientation message:"The PreserveEdgeOrientation property was not properly set to true."
        assert_equal #none uiExportOptions.Normals message:"The Normals property was not properly set to #none."
        assert_equal #triMesh uiExportOptions.MeshFormat message:"The MeshFormat property was not properly set to #triMesh."
        assert_equal #explicit uiExportOptions.TimeMode message:"The TimeMode property was not properly set to #explicit."
        assert_equal 10.5 uiExportOptions.StartFrame message:"The StartFrame property was not properly set to 10.5."
        assert_equal 20.5 uiExportOptions.EndFrame message:"The EndFrame property was not properly set to 20.5."
        assert_equal 3.5 uiExportOptions.SamplesPerFrame message:"The SamplesPerFrame property was not properly set to 3.5."
        assert_equal "/foo/bar" uiExportOptions.RootPrimPath message:"The RootPrimPath property was not properly set to /foo/bar."
        assert_true uiExportOptions.OpenInUsdview message:"The OpenInUsdview property was not properly set to true."
        assert_false uiExportOptions.UsdStagesAsReferences message:"The UsdStagesAsReferences property was not properly set to false."
        assert_true uiExportOptions.AllowNestedGprims message:"The AllowNestedGprims property was not properly set to true."
        assert_equal (#("bar", "foo") as string) (uiExportOptions.ContextNames as string) message:"The ContextNames property was not properly set to foo,bar."
        assert_equal (#("baz", "qux") as string) (uiExportOptions.ChaserNames as string) message:"The ChaserNames property was not properly baz,qux."
        if maxver[1] < 27000 or (maxver[1] == 27000 and maxver[5] == 0) then (
            assert_equal ("Dictionary #string (DataPair \"chaser1\" Dictionary #string (DataPair \"param\" \"val\") ) " as string) (uiExportOptions.AllChaserArgs as string) message:"The AllChaserArgs property was not properly set to {chaser1:{param = val}."
        )
        else (
            assert_equal ("(Dictionary #string (DataPair \"chaser1\" (Dictionary #string (DataPair \"param\" \"val\") )) )" as string) (uiExportOptions.AllChaserArgs as string) message:"The AllChaserArgs property was not properly set to {chaser1:{param = val}."
        )
        assert_equal mtlPath uiExportOptions.MaterialLayerPath message:"The MaterialLayerPath property was not properly set."
        
        if maxver[1] >= 26000 then (  -- 3ds Max 2024 and up
            assert_equal #activeMaterial uiExportOptions.MtlSwitcherExportStyle message:"Material Switcher export style property was not properly to #activeMaterial."
        )
        assert_false uiExportOptions.UseProgressBar message:"UseProgressBar property was not properly set to false."
        assert_false uiExportOptions.UseLastResortUSDPreviewSurfaceWriter message:"UseLastResortUSDPreviewSurfaceWriter property was not properly set to false."

        -- Reset & test

        uiExportOptions.Reset()

        assert_true uiExportOptions.Meshes message:"Meshes property was not properly reset to defaults (true)."
        assert_true uiExportOptions.Shapes message:"Shapes property was not properly reset to defaults (true)."
        assert_true uiExportOptions.Lights message:"Lights property was not properly reset to defaults (true)."
        assert_true uiExportOptions.Cameras message:"Cameras property was not properly reset to defaults (true)."
        assert_true uiExportOptions.Materials message:"Materials property was not properly reset to defaults (true)."
        assert_false uiExportOptions.Skin message:"Skin property was not properly reset to defaults (false)."
        assert_false uiExportOptions.Morpher message:"Morpher property was not properly reset to defaults (false)."
        assert_equal "Bones" uiExportOptions.BonesPrimName message:"BonesPrimName property was not properly reset to defaults (Bones)."
        assert_equal "Animations" uiExportOptions.AnimationsPrimName message:"AnimationsPrimName property was not properly reset to defaults (Animations)."
        assert_equal "useRegistry" uiExportOptions.ShadingMode message:"ShadingMode was now properly initialized to defaults (\"useRegistry\")"
        assert_true ((#("MaterialX", "UsdPreviewSurface") as string) == matTargets or (#("Arnold", "MaterialX", "UsdPreviewSurface") as string) == matTargets) message:"Unexpected list of available material targets."
        assert_equal (#("UsdPreviewSurface") as string) (uiExportOptions.AllMaterialTargets as string) message:"Selected material target was not properly initialized to default (UsdPreviewSurface)."
        assert_true uiExportOptions.HiddenObjects message:"HiddenObjects property was not properly reset to defaults (true)."
        assert_false uiExportOptions.UseUSDVisibility message:"UseUSDVisibility property was not properly reset to defaults (false)."
        assert_equal #binary uiExportOptions.FileFormat message:"File format property was not properly reset to defaults (#binary). "
        assert_equal #z uiExportOptions.UpAxis message:"UpAxis property was not properly reset to defaults (#z)."
        assert_equal ((GetDir #temp) + "\\MaxUsdExport.log") uiExportOptions.LogPath message:"The LogPath property was not properly reset."
        assert_equal #off uiExportOptions.LogLevel message:"The LogLevel property was not properly reset."
        assert_false uiExportOptions.PreserveEdgeOrientation message:"The PreserveEdgeOrientation property was not properly reset to default (false)."
        assert_equal #asPrimvar uiExportOptions.Normals message:"Normals property was not properly reset to defaults (#asPrimvar)."
        assert_equal #fromScene uiExportOptions.MeshFormat message:"MeshFormat property was not properly reset to defaults (#fromScene)."
        assert_equal #current uiExportOptions.TimeMode message:"The TimeMode property was not properly reset to defaults (#current)."
        assert_equal 0.0 uiExportOptions.StartFrame message:"The StartFrame property was not properly reset to defaults (0.0)."
        assert_equal 0.0 uiExportOptions.EndFrame message:"The EndFrame property was not properly reset to defaults (0.0)."
        assert_equal 1.0 uiExportOptions.SamplesPerFrame message:"The SamplesPerFrame property was not properly reset to defaults (1.0)."
        assert_equal "/root" uiExportOptions.RootPrimPath message:"The RootPrimPath property was not properly reset to defaults (\"/root\")."
        assert_false uiExportOptions.OpenInUsdview message:"The OpenInUsdview property was not properly reset to default (false)."
        assert_true uiExportOptions.UsdStagesAsReferences message:"The UsdStagesAsReferences property was not properly reset to true."
        assert_false uiExportOptions.AllowNestedGprims message:"The AllowNestedGprims property was not properly reset to false."
        assert_equal (#() as string) (uiExportOptions.ContextNames as string) message:"The ContextNames property was not properly reset."
        assert_equal (#() as string) (uiExportOptions.ChaserNames as string) message:"The ChaserNames property was not properly reset."
        if maxver[1] < 27000 or (maxver[1] == 27000 and maxver[5] == 0) then (
            assert_equal ("Dictionary #string " as string) (uiExportOptions.AllChaserArgs as string) message:"The AllChaserArgs property was not properly reset."
        )
        else (
            assert_equal ("(Dictionary #string )" as string) (uiExportOptions.AllChaserArgs as string) message:"The AllChaserArgs property was not properly reset."
        )
        assert_equal "<filename>_mtl.usda" uiExportOptions.MaterialLayerPath message:"The MaterialLayerPath property was not properly reset."
        if maxver[1] >= 26000 then (  -- 3ds Max 2024 and up
            assert_equal #asVariantSets uiExportOptions.MtlSwitcherExportStyle message:"Material Switcher export style property was not properly reset to default (#asVariantSets)."
        )
        assert_true uiExportOptions.UseProgressBar message:"UseProgressBar property was not properly reset to defaults (true)."
        assert_true uiExportOptions.UseLastResortUSDPreviewSurfaceWriter message:"UseLastResortUSDPreviewSurfaceWriter property was not properly reset to default (true)."
    ),
    
    -- on top of the classic material option settings
    -- verify the consequences of settings some options on other options
    function test_material_options_ripple = (
        local uiExportOptions = USDExporter.UIOptions
        uiExportOptions.Reset()

        assert_true uiExportOptions.Materials message:"Materials property was not properly reset to defaults (true)."
        assert_equal "useRegistry" uiExportOptions.ShadingMode message:"ShadingMode was now properly initialized to defaults (\"useRegistry\")"

        uiExportOptions.ShadingMode = "none"
        assert_false uiExportOptions.Materials message:"Materials property was not properly set to false when ShadingMode is set to 'none'."
        assert_equal "none" uiExportOptions.ShadingMode message:"ShadingMode was now properly set to 'none' when specified."

        uiExportOptions.Reset()
        assert_true uiExportOptions.Materials message:"Materials property was not properly reset to defaults (true)."
        assert_equal "useRegistry" uiExportOptions.ShadingMode message:"ShadingMode was now properly initialized to defaults (\"useRegistry\")"

        uiExportOptions.ShadingMode = "none"
        uiExportOptions.Materials = true
        assert_true uiExportOptions.Materials message:"Materials property was not properly set to true."
        assert_equal "useRegistry" uiExportOptions.ShadingMode message:"ShadingMode was now properly set back 'useRegistry' when materials need to be exported but no ShadingMode was set."

    ),
    
    function test_export_meshes_option = (
        exportPath = output_prefix + "export_meshes.usd"
        
        -- Create a teapot and export it.
        local exportOptions = USDExporter.CreateOptions()
        exportOptions.FileFormat = #ascii
        exportOptions.Meshes = true
        exportOptions.RootPrimPath = "/"
        assert_true exportOptions.Meshes message:"Meshes property could not be set to true."
        teapot()    
        USDExporter.ExportFile exportPath exportOptions:exportOptions
        resetmaxfile #noprompt

        -- Reimport to make sure we get a teapot.
        USDImporter.ImportFile exportPath
        assert_equal 1 $objects.count message:"Unexpected object count with Meshes = true"
        
        -- Now with Meshes off...
        resetmaxfile #noprompt
        exportOptions.Meshes = false
        assert_false exportOptions.Meshes message:"Meshes property could not be set to false."
        teapot()
        USDExporter.ExportFile exportPath exportOptions:exportOptions
        resetmaxfile #noprompt
        
        USDImporter.ImportFile exportPath
        assert_equal 0 $objects.count message:"Unexpected object count with Meshes = false"
    ),
    
    function test_export_shapes_option = (
        -- Create a rectangle shape and export it...
        local rectNode = rectangle name:"rectangle"
        
        local exportOptions = USDExporter.CreateOptions()
        exportOptions.FileFormat = #ascii
        
        -- Test with Shapes ON, and disabled in VP -> should export as a UsdGeomCurve.
        exportPath = output_prefix + "export_shapes_on.usd"
        rectNode.render_displayRenderMesh = false
        exportOptions.Shapes = true
        exportOptions.RootPrimPath = "/"
        
        USDExporter.ExportFile exportPath exportOptions:exportOptions
        local stage = pyUsd.Stage.Open(exportPath)
        local rectanglePrim = stage.GetPrimAtPath("/rectangle")
        assert_true (pyUsd.Prim.IsValid(rectanglePrim)) message:"The rectangle was not exported to a USD prim as expected."
        assert_true (rectanglePrim.IsA(pyUsdGeom.BasisCurves)) message:"The rectangle prim is not a curve."
        
        -- Test with Shapes ON, and enabled in VP -> should export as a UsdGeomMesh.
        exportPath = output_prefix + "export_shapes_on_vp_mesh.usd"
        rectNode.render_displayRenderMesh = true
        USDExporter.ExportFile exportPath exportOptions:exportOptions
        local stage = pyUsd.Stage.Open(exportPath)
        local rectanglePrim = stage.GetPrimAtPath("/rectangle")
        assert_true (pyUsd.Prim.IsValid(rectanglePrim)) message:"The rectangle was not exported to a USD prim as expected."
        assert_true (rectanglePrim.IsA(pyUsdGeom.Mesh)) message:"The rectangle prim should be a Mesh, as the shape is enabled in the viewport."
        
        -- Test with Shapes OFF and disabled in VP -> should not export.
        exportPath = output_prefix + "export_shapes_off.usd"
        rectNode.render_displayRenderMesh = false
        exportOptions.Shapes = false
        USDExporter.ExportFile exportPath exportOptions:exportOptions
        local stage = pyUsd.Stage.Open(exportPath)
        local rectanglePrim = stage.GetPrimAtPath("/rectangle")
        assert_false (pyUsd.Prim.IsValid(rectanglePrim)) message:"The rectangle was not exported to a USD prim as expected."
            
        -- Test with Shapes OFF, and enabled in VP -> should not export.
        exportPath = output_prefix + "export_shapes_off_vp_mesh.usd"
        rectNode.render_displayRenderMesh = true
        exportOptions.Shapes = false
        USDExporter.ExportFile exportPath exportOptions:exportOptions
        local stage = pyUsd.Stage.Open(exportPath)
        local rectanglePrim = stage.GetPrimAtPath("/rectangle")
        assert_false (pyUsd.Prim.IsValid(rectanglePrim)) message:"The rectangle was not exported to a USD prim as expected."
    ),
    
    function test_export_lights_option = (
        exportPath = output_prefix + "export_lights.usd"
        
        -- Create a light and export it.
        local exportOptions = USDExporter.CreateOptions()
        exportOptions.FileFormat = #ascii
        exportOptions.Lights = true
        assert_true exportOptions.Lights message:"Lights property could not be set to true."
        free_light()
        USDExporter.ExportFile exportPath exportOptions:exportOptions
        resetmaxfile #noprompt

        -- Reimport to make sure we get a teapot.
        USDImporter.ImportFile exportPath
        assert_equal 1 $lights.count message:"Unexpected lights count with Lights = true"
        
        -- Now with Lights off...
        resetmaxfile #noprompt
        exportOptions.Lights = false
        assert_false exportOptions.Lights message:"Lights property could not be set to false."
        free_light()
        USDExporter.ExportFile exportPath exportOptions:exportOptions
        resetmaxfile #noprompt
        
        USDImporter.ImportFile exportPath
        assert_equal 0 $lights.count message:"Unexpected object count with Lights = false"
    ),
    
    function test_export_cameras_option = (
        exportPath = output_prefix + "export_cameras.usd"
        
        -- Create a cameras and export it.
        local exportOptions = USDExporter.CreateOptions()
        exportOptions.FileFormat = #ascii
        exportOptions.Cameras = true
        assert_true exportOptions.Cameras message:"Cameras property could not be set to true."
        freecamera()
        USDExporter.ExportFile exportPath exportOptions:exportOptions
        resetmaxfile #noprompt
        
        -- Reimport to make sure we get a camera.
        USDImporter.ImportFile exportPath
        assert_equal 1 $cameras.count message:"Unexpected camera count with Cameras = true"
        
        -- Now with Cameras off...
        resetmaxfile #noprompt
        exportOptions.Cameras = false
        assert_false exportOptions.Cameras message:"Cameras property could not be set to false."
        freecamera()
        USDExporter.ExportFile exportPath exportOptions:exportOptions
        resetmaxfile #noprompt
        
        USDImporter.ImportFile exportPath
        assert_equal 0 $cameras.count message:"Unexpected camera count with Cameras = false"
    ),

    function test_export_skin_option = (
        exportName = "export_skin"
        exportPath = output_prefix + exportName + ".usda"
        noExportPath = output_prefix + "export_no_skin.usda"

        local exportOptions = USDExporter.CreateOptions()
        exportOptions.FileFormat = #ascii
        exportOptions.Skin = true
        exportOptions.RootPrimPath = "/root"
        assert_true exportOptions.Skin message:"Skin property could not be set to true."
        
        exportOptions.BonesPrimName = "TestSkel"
        exportOptions.AnimationsPrimName = "TestAnim"
        assert_equal "TestSkel" exportOptions.BonesPrimName message:"The BonesPrimName property could not be set to correct value (TestSkel)."
        assert_equal "TestAnim" exportOptions.AnimationsPrimName message:"The AnimationsPrimName property could not be set to correct value (TestAnim)."

        -- bones won't be exported as UsdSkel if they aren't being used by a skin modifier
        local b = bonesys.createbone [0,0,0] [0,1,0] [0,0,1]
        local char = Cylinder radius:6 height:36 heightsegs:30 pos:[0,0,0] isSelected:on
        local sk = skin()
        addModifier char sk
        skinOps.addbone sk b 0 node:char

        USDExporter.ExportFile exportPath exportOptions:exportOptions
        local stage = pyUsd.Stage.Open(exportPath)
        local skelRootPrim = stage.GetPrimAtPath("/root")
        local skelPrim = stage.GetPrimAtPath("/root/" + exportOptions.BonesPrimName)
        local animPrim = stage.GetPrimAtPath("/root/" + exportOptions.BonesPrimName + "/" + exportOptions.AnimationsPrimName)

        assert_true (skelRootPrim.IsA(pyUsdSkel.Root)) message:"The root prim was not exported to a USD SkelRoot prim as expected."
        assert_true (skelPrim.IsA(pyUsdSkel.Skeleton)) message:"The bones were not exported to a skeleton prim as expected."
        assert_true (animPrim.IsA(pyUsdSkel.Animation)) message:"The animations were not exported to a SkelAnimtion prim as expected."
        
        resetmaxfile #noprompt
        exportOptions.Skin = false
        bonesys.createbone [0,0,0] [0,1,0] [0,0,1]
        USDExporter.ExportFile noExportPath exportOptions:exportOptions

        local stage = pyUsd.Stage.Open(noExportPath)
        local skelRootPrim = stage.GetPrimAtPath("/root")
        local skelPrim = stage.GetPrimAtPath("/root/Bones")
        local animPrim = stage.GetPrimAtPath("/root/Bones/Anim")

        assert_false (skelRootPrim.IsA(pyUsdSkel.Root)) message:"The root prim was exported as SkelRoot when it shouldn't have."
        assert_false (pyUsd.Prim.IsValid(skelPrim)) message:"The Bones prim was created when it shouldn't have"
        assert_false (pyUsd.Prim.IsValid(animPrim)) message:"The SkelAnimation prim was created when it shouldn't have"

        -- Skins shouldn't be exported if the export meshes option is disabled
        resetmaxfile #noprompt
        exportOptions.Skin = true
        exportOptions.Meshes = false
        bonesys.createbone [0,0,0] [0,1,0] [0,0,1]
        USDExporter.ExportFile noExportPath exportOptions:exportOptions

        local stage = pyUsd.Stage.Open(noExportPath)
        local skelRootPrim = stage.GetPrimAtPath("/root")
        local skelPrim = stage.GetPrimAtPath("/root/Bones")
        local animPrim = stage.GetPrimAtPath("/root/Bones/Anim")

        assert_false (skelRootPrim.IsA(pyUsdSkel.Root)) message:"The root prim was exported as SkelRoot even though export Meshes disabled."
        assert_false (pyUsd.Prim.IsValid(skelPrim)) message:"The Bones prim was created even though export Meshes is disabled."
        assert_false (pyUsd.Prim.IsValid(animPrim)) message:"The SkelAnimation prim was created even though export Meshes disabled."
    ),
    
    function test_bake_object_offset_option = (
        
        -- Create a teapot with no object-offset transform, only a node transform.
        local teapotWithOffset = teapot()
        teapotWithOffset.name = "teapotWithOffset"
        teapotWithOffset.pos = [0,0,10]
        
        -- Path the teapot's node will be exported to.
        local teapotPath = "/" + teapotWithOffset.name
        -- Path the mesh object will be exported to - this will only be used if the 
        -- object-offet transform is not the identity transform and BakeObjectOffsetTransform
        -- is turned off.
        local teapotShapePath = "/" + teapotWithOffset.name + "/" + teapotWithOffset.name + "_Shape"
        
        -- Test BakeObjectOffsetTransform = false, object-offet = identity.
        exportPath = output_prefix + "bake_offset_false_with_identity_offset.usd"
        
        local exportOptions = USDExporter.CreateOptions()
        exportOptions.FileFormat = #ascii
        exportOptions.BakeObjectOffsetTransform = false
        exportOptions.RootPrimPath = "/"
        
        USDExporter.ExportFile exportPath exportOptions:exportOptions
        local stage = pyUsd.Stage.Open(exportPath)
        -- BakeObjectOffsetTransform is set to false, but the offset is the identity transform
        -- so there is no need to use an Xform to represent the offset.
        usdTeaPot = stage.GetPrimAtPath(teapotPath)
        usdTeaPotShape = stage.GetPrimAtPath(teapotShapePath)
        assert_true (pyUsd.Prim.IsValid(usdTeaPot)) message:"The teapot was not exported to a USD prim as expected."
        assert_true (usdTeaPot.IsA(pyUsdGeom.Mesh)) message:"The teapot prim is not a mesh."
        assert_false (pyUsd.Prim.IsValid(usdTeaPotShape)) message:"The USD teapot should not have a Shape sub-object. The offset was the identity transform."
        local xformable = pyUsdGeom.Xformable(usdTeaPot)
        assert_true (compareUsdAndMaxTransforms (xformable.GetLocalTransformation()) teapotWithOffset.transform) message:"The teapot prim's transform should equal the max node's transform"
                    
        -- Test BakeObjectOffsetTransform = false, object-offet != identity. 
        exportPath = output_prefix + "bake_object_offset_false_with_offset.usd"
        local zPosOffset = 10
        teapotWithOffset.objectOffsetPos = [0,0,zPosOffset]
        USDExporter.ExportFile exportPath exportOptions:exportOptions
        stage = pyUsd.Stage.Open(exportPath)
        usdTeaPot = stage.GetPrimAtPath(teapotPath)
        usdTeaPotShape = stage.GetPrimAtPath(teapotShapePath)
        assert_true (pyUsd.Prim.IsValid(usdTeaPotShape)) message:"The teapot prim should have a sub-object shape."
        assert_true (usdTeaPotShape.IsA(pyUsdGeom.Mesh)) message:"The teapot sub-object should be a mesh prim."
        assert_true (usdTeaPot.IsA(pyUsdGeom.Xform)) message:"The teapot geometry should be wrapped in an Xform representing the object-offset transform."
        xformable = pyUsdGeom.Xformable(usdTeaPot)
        assert_true (compareUsdAndMaxTransforms (xformable.GetLocalTransformation()) teapotWithOffset.transform)  message:"The teapot prim's transform should equal the max node's transform"
        xformable = pyUsdGeom.Xformable(usdTeaPotShape)
        objectOffset = teapotWithOffset.objecttransform * inverse teapotWithOffset.transform
        assert_true (compareUsdAndMaxTransforms (xformable.GetLocalTransformation()) objectOffset)  message:"The teapot sub-object shape's transform should equal the max object's offset transform."
        
        -- Keep track of the extent of the teapot. We will need it later to validate the baked offset-transform.
        local teapotExtent = (usdTeaPotShape.GetAttribute("extent")).Get()
        local teapotMinZ = teapotExtent[1][3]
        local teapotMaxZ = teapotExtent[2][3]
        
        -- Test BakeObjectOffsetTransform = true, object-offet != identity.
        exportOptions.BakeObjectOffsetTransform = true
        exportPath = output_prefix + "bake_object_offset_true.usd"
        USDExporter.ExportFile exportPath exportOptions:exportOptions
        stage = pyUsd.Stage.Open(exportPath)
        usdTeaPot = stage.GetPrimAtPath(teapotPath)
        usdTeaPotShape = stage.GetPrimAtPath(teapotShapePath)
        assert_true (pyUsd.Prim.IsValid(usdTeaPot)) message:"The teapot was not exported to a USD prim as expected."
        assert_true (usdTeaPot.IsA(pyUsdGeom.Mesh)) message:"The teapot prim be a mesh."
        assert_false (pyUsd.Prim.IsValid(usdTeaPotShape)) message:"The teapot prim should not have a sub-object shape."
        xformable = pyUsdGeom.Xformable(usdTeaPot)
        assert_true (compareUsdAndMaxTransforms (xformable.GetLocalTransformation()) teapotWithOffset.transform) message:"The teapot prim's transform should equal the max node's transform"
        local bakedTeapotExtent = (usdTeaPot.GetAttribute("extent")).Get()
        local bakedTeapotMinZ = bakedTeapotExtent[1][3]
        local bakedTeapotMaxZ = bakedTeapotExtent[2][3]
        -- Validate that the offset was baked in the geometry correctly: 
        assert_equal (teapotMinZ + zPosOffset) bakedTeapotMinZ message:"Object-offset transform was not correctly baked in the geometry."
        assert_equal (teapotMaxZ + zPosOffset) bakedTeapotMaxZ message:"Object-offset transform was not correctly baked in the geometry."
        
        -- The following tests are for the interaction between AllowNestedGprims and the BakeObjectOffset options. We test all 4 combinations.
        -- I.e. Test that the offset baking option is respected whether or not nested prims are allowed.
            
        resetMaxFile #noPrompt    
            
        local teapot1 = teapot name:"teapot1"
        teapot1.pos = [0,0,10] 
        teapot1.objectOffsetPos = [0,0,5]
        local teapot2 = teapot name:"teapot2"
        teapot2.pos = [10,0,0] 
        teapot2.objectOffsetPos = [0,0,15]
        teapot2.parent = teapot1
        local exportOptions = USDExporter.CreateOptions()
        exportOptions.FileFormat = #ascii
        
        exportOptions.RootPrimPath = "/"

        -- 1) Baking offsets + no nesting

        exportOptions.AllowNestedGprims = false
        exportOptions.BakeObjectOffsetTransform = true

        exportPath = output_prefix + "bake_offset_no_gprim_nesting.usd"
        
        USDExporter.ExportFile exportPath exportOptions:exportOptions
        stage = pyUsd.Stage.Open(exportPath)
        usdTeaPot = stage.GetPrimAtPath("/teapot1")
        usdTeaPotShape = stage.GetPrimAtPath("/teapot1/teapot1_Shape")
        assert_true (pyUsd.Prim.IsValid(usdTeaPot)) message:"The teapot1 was not exported to a USD prim as expected."
        assert_true (usdTeaPot.IsA(pyUsdGeom.Xform)) message:"The teapot1 prim should be a mesh."
        assert_true (pyUsd.Prim.IsValid(usdTeaPotShape)) message:"The teapot prim should have a sub-object shape."
        assert_true (usdTeaPotShape.IsA(pyUsdGeom.Mesh)) message:"The teapot shape prim should be a mesh."
        xformable = pyUsdGeom.Xformable(usdTeaPot)
        assert_true (compareUsdAndMaxTransforms (xformable.GetLocalTransformation()) teapot1.transform) message:"The teapot prim's transform should equal the max node's transform"
        local bakedTeapotExtent = (usdTeaPotShape.GetAttribute("extent")).Get()
        local bakedTeapotMinZ = bakedTeapotExtent[1][3]
        local bakedTeapotMaxZ = bakedTeapotExtent[2][3]
        -- Validate that the offset was baked in the geometry correctly: 
        assert_equal (teapotMinZ + teapot1.objectOffsetPos[3]) bakedTeapotMinZ message:"Object-offset transform was not correctly baked in the geometry."
        assert_equal (teapotMaxZ + teapot1.objectOffsetPos[3]) bakedTeapotMaxZ message:"Object-offset transform was not correctly baked in the geometry."

        usdTeaPot2 = stage.GetPrimAtPath("/teapot1/teapot2")
        assert_true (pyUsd.Prim.IsValid(usdTeaPot2)) message:"The teapot2 was not exported to a USD prim as expected."
        assert_true (usdTeaPot2.IsA(pyUsdGeom.Mesh)) message:"The teapot2 shape prim should be a mesh."
        xformable = pyUsdGeom.Xformable(usdTeaPot2)
        assert_true (compareUsdAndMaxTransforms (xformable.GetLocalTransformation()) (teapot2.transform * inverse teapot1.transform)) message:"The teapot2 prim's transform should equal the max node's transform"
        local bakedTeapotExtent = (usdTeaPot2.GetAttribute("extent")).Get()
        local bakedTeapotMinZ = bakedTeapotExtent[1][3]
        local bakedTeapotMaxZ = bakedTeapotExtent[2][3]
        -- Validate that the offset was baked in the geometry correctly: 
        assert_equal (teapotMinZ + teapot2.objectOffsetPos[3]) bakedTeapotMinZ message:"Object-offset transform was not correctly baked in the geometry."
        assert_equal (teapotMaxZ + teapot2.objectOffsetPos[3]) bakedTeapotMaxZ message:"Object-offset transform was not correctly baked in the geometry."
            
        -- 2) Baking offsets + nesting
            
        exportPath = output_prefix + "bake_offset_gprim_nesting.usd"
        exportOptions.AllowNestedGprims = true   
        exportOptions.BakeObjectOffsetTransform = true
            
        USDExporter.ExportFile exportPath exportOptions:exportOptions
        stage = pyUsd.Stage.Open(exportPath)
        usdTeaPot1 = stage.GetPrimAtPath("/teapot1")
        assert_true (pyUsd.Prim.IsValid(usdTeaPot1)) message:"The teapot1 was not exported to a USD prim as expected."
        assert_true (usdTeaPot1.IsA(pyUsdGeom.Mesh)) message:"teapot1 be a mesh prim."
        xformable = pyUsdGeom.Xformable(usdTeaPot1)
        assert_true (compareUsdAndMaxTransforms (xformable.GetLocalTransformation()) teapot1.transform)  message:"The teapot1 prim's transform should equal the max node's transform"
        local bakedTeapotExtent = (usdTeaPot1.GetAttribute("extent")).Get()
        local bakedTeapotMinZ = bakedTeapotExtent[1][3]
        local bakedTeapotMaxZ = bakedTeapotExtent[2][3]
        -- Validate that the offset was baked in the geometry correctly: 
        assert_equal (teapotMinZ + teapot1.objectOffsetPos[3]) bakedTeapotMinZ message:"Object-offset transform was not correctly baked in the geometry."
        assert_equal (teapotMaxZ + teapot1.objectOffsetPos[3]) bakedTeapotMaxZ message:"Object-offset transform was not correctly baked in the geometry."
            
        usdTeaPot2 = stage.GetPrimAtPath("/teapot1/teapot2")
        assert_true (pyUsd.Prim.IsValid(usdTeaPot2)) message:"The teapot2 was not exported to a USD prim as expected."
        assert_true (usdTeaPot2.IsA(pyUsdGeom.Mesh)) message:"teapot2 be a mesh prim."
        xformable = pyUsdGeom.Xformable(usdTeaPot2)
        assert_true (compareUsdAndMaxTransforms (xformable.GetLocalTransformation()) (teapot2.transform * inverse teapot1.transform))  message:"The teapot2 prim's transform should equal the max node's transform"
        local bakedTeapotExtent = (usdTeaPot2.GetAttribute("extent")).Get()
        local bakedTeapotMinZ = bakedTeapotExtent[1][3]
        local bakedTeapotMaxZ = bakedTeapotExtent[2][3]
        -- Validate that the offset was baked in the geometry correctly: 
        assert_equal (teapotMinZ + teapot2.objectOffsetPos[3]) bakedTeapotMinZ message:"Object-offset transform was not correctly baked in the geometry."
        assert_equal (teapotMaxZ + teapot2.objectOffsetPos[3]) bakedTeapotMaxZ message:"Object-offset transform was not correctly baked in the geometry."            
        
        -- 3) no baking + no nesting
        exportPath = output_prefix + "no_bake_offset_no_gprim_nesting.usd"
        exportOptions.AllowNestedGprims = false   
        exportOptions.BakeObjectOffsetTransform = false
        USDExporter.ExportFile exportPath exportOptions:exportOptions
        stage = pyUsd.Stage.Open(exportPath)

        usdTeaPotShape1 = stage.GetPrimAtPath("/teapot1/teapot1_Shape")
        assert_true (pyUsd.Prim.IsValid(usdTeaPotShape1)) message:"The teapot1 was not exported to a USD shape prim as expected."
        assert_true (usdTeaPotShape1.IsA(pyUsdGeom.Mesh)) message:"The teapot1 should have a shape Mesh prim, is is offset."
        xformable = pyUsdGeom.Xformable(usdTeaPotShape1)        
        assert_true (compareUsdAndMaxTransforms (xformable.GetLocalTransformation()) (teapot1.objecttransform * inverse teapot1.transform))  message:"The teapot2 prim's transform should equal the max node's transform"
        usdTeaPotShape2 = stage.GetPrimAtPath("/teapot1/teapot2/teapot2_Shape")
        assert_true (pyUsd.Prim.IsValid(usdTeaPotShape2)) message:"The teapot2 was not exported to a USD shape prim as expected."
        assert_true (usdTeaPotShape2.IsA(pyUsdGeom.Mesh)) message:"The teapot2 should have a shape Mesh prim, is is offset."
        xformable = pyUsdGeom.Xformable(usdTeaPotShape2)
        assert_true (compareUsdAndMaxTransforms (xformable.GetLocalTransformation()) (teapot2.objecttransform * inverse teapot2.transform))  message:"The teapot2 prim's transform should equal the max node's transform"

        -- 4) no baking + nesting -> this should produce the exact same result as 3), indeed in this scenario, nodes are split into xform + mesh 
        -- for the need of object offsets.
        exportPath = output_prefix + "no_bake_offset_no_gprim_nesting.usd"
        exportOptions.AllowNestedGprims = false  
        exportOptions.BakeObjectOffsetTransform = false
        USDExporter.ExportFile exportPath exportOptions:exportOptions
        stage = pyUsd.Stage.Open(exportPath)

        usdTeaPotShape1 = stage.GetPrimAtPath("/teapot1/teapot1_Shape")
        assert_true (pyUsd.Prim.IsValid(usdTeaPotShape1)) message:"The teapot1 was not exported to a USD shape prim as expected."
        assert_true (usdTeaPotShape1.IsA(pyUsdGeom.Mesh)) message:"The teapot1 should have a shape Mesh prim, is is offset."
        xformable = pyUsdGeom.Xformable(usdTeaPotShape1)        
        assert_true (compareUsdAndMaxTransforms (xformable.GetLocalTransformation()) (teapot1.objecttransform * inverse teapot1.transform))  message:"The teapot2 prim's transform should equal the max node's transform"
        usdTeaPotShape2 = stage.GetPrimAtPath("/teapot1/teapot2/teapot2_Shape")
        assert_true (pyUsd.Prim.IsValid(usdTeaPotShape2)) message:"The teapot2 was not exported to a USD shape prim as expected."
        assert_true (usdTeaPotShape2.IsA(pyUsdGeom.Mesh)) message:"The teapot2 should have a shape Mesh prim, is is offset."
        xformable = pyUsdGeom.Xformable(usdTeaPotShape2)
        assert_true (compareUsdAndMaxTransforms (xformable.GetLocalTransformation()) (teapot2.objecttransform * inverse teapot2.transform))  message:"The teapot2 prim's transform should equal the max node's transform"    
    ),

    function test_preserve_edge_orientation_option = (
        -- Create am Editable Poly plane and move a couple of vertices on the z axis
        local polyPlane = convertToPoly(Plane length:49.0 width:49.0 pos:[0,0,0] lengthsegs:7 widthsegs:7)
        local numFaces = polyPlane.numfaces
        polyop.moveVert polyPlane 31 [0, 0, 2.52511]
        polyop.moveVert polyPlane 32 [0, 0, 2.52511]
        polyop.moveVert polyPlane 41 [0, 0, 2.52511]
        polyop.moveVert polyPlane 42 [0, 0, -2.52511]
        polyop.moveVert polyPlane 43 [0, 0, -4.52511]
        polyop.moveVert polyPlane 44 [0, 0, 2.52511]

        -- Setup preserve edge orientation to false and export.
        local exportOptions = USDExporter.CreateOptions()
        exportOptions.FileFormat = #ascii
        exportOptions.RootPrimPath = "/"
        
        exportPath = output_prefix + "preserve_edge_orientation.usd"
        exportOptions.PreserveEdgeOrientation = false
        assert_false exportOptions.PreserveEdgeOrientation message:"PreserveEdgeOrientation property could not be set to false."
        USDExporter.ExportFile exportPath exportOptions:exportOptions

        resetmaxfile #noprompt

        -- Reimport to make sure we get a plane with the same number of faces
        USDImporter.ImportFile exportPath
        assert_equal numFaces $objects[1].numfaces message:"Unexpected numfaces count with PreserveEdgeOrientation = false"

        -- Now, setup preserve edge orientation to true and export.
        exportPath = output_prefix + "do_not_preserve_edge_orientation.usd"
        exportOptions.PreserveEdgeOrientation = true
        assert_true exportOptions.PreserveEdgeOrientation message:"PreserveEdgeOrientation property could not be set to true."
        USDExporter.ExportFile exportPath exportOptions:exportOptions

        resetmaxfile #noprompt

        -- Reimport to make sure we get a plane with more faces (some triangulation occurred)
        USDImporter.ImportFile exportPath
        assert_equal 59 $objects[1].numfaces message:"Unexpected numfaces count with PreserveEdgeOrientation = true"
    ),
    
    function test_up_axis_option = (
        -- Setup up axis to Y and export.
        local exportOptions = USDExporter.CreateOptions()
        exportOptions.FileFormat = #ascii
        exportPath = output_prefix + "up_axis_Y.usd"
        exportOptions.UpAxis = #y
        assert_equal #y exportOptions.UpAxis message:"UpAxis property could not be set to #y."
        USDExporter.ExportFile exportPath exportOptions:exportOptions
        
        -- Make sure the stage has the correct up-axis.
        local stage = pyUsd.Stage.Open(exportPath)
        local upAxis = pyUsdGeom.GetStageUpAxis(stage)
        assert_equal "Y" upAxis
        
        -- Now test with Z
        exportPath = output_prefix + "up_axis_Z.usd"
        exportOptions.UpAxis = #z
        assert_equal #z exportOptions.UpAxis message:"UpAxis property could not be set to #z."
        USDExporter.ExportFile exportPath exportOptions:exportOptions
        
        stage = pyUsd.Stage.Open(exportPath)
        upAxis = pyUsdGeom.GetStageUpAxis(stage)
        assert_equal "Z" upAxis
        
        -- Test that an invalid up axis is rejected.
        try (
            local exportOptions = USDExporter.CreateOptions()
            exportOptions.UpAxis = 10
        )
        catch 
        (
            exceptionThrown = true
        )
        assert_true exceptionThrown message:"Attempting to set an invalid UpAxis should have thrown an exception."
    ),
    
    function test_file_format_option = (
       -- Set file format to ascii and export.        
        local exportOptions = USDExporter.CreateOptions()
        exportPath = output_prefix + "file_format_ascii.usd"
        exportOptions.FileFormat = #ascii
        assert_equal #ascii exportOptions.FileFormat message:"FileFormat property could not be set to #ascii."
        USDExporter.ExportFile exportPath exportOptions:exportOptions
        
        -- Make sure the file format was correctly set for the root layer.
        local stage = pyUsd.Stage.Open(exportPath)
        local rootLayer = stage.GetRootLayer()
        local usdAsciiFormat = pySdf.FileFormat.FindById("usda")
        local isAscii = usdAsciiFormat.CanRead(rootLayer.identifier)
        assert_equal true isAscii
        -- test the binary file format.
        exportPath = output_prefix + "file_format_binary.usd"
        exportOptions.FileFormat = #binary
        assert_equal #binary exportOptions.FileFormat message:"FileFormat property could not be set to #binary."
        USDExporter.ExportFile exportPath exportOptions:exportOptions
        
        stage = pyUsd.Stage.Open(exportPath)
        rootLayer = stage.GetRootLayer()
        local usdBinaryFormat = pySdf.FileFormat.FindById("usdc")
        local isBinary = usdBinaryFormat.CanRead(rootLayer.identifier)
        assert_equal true isBinary

        -- test usdz file format.
        stageFilename = "file_format_binary"
        exportPath = output_prefix + (stageFilename + ".usdz")
        
        status = USDExporter.ExportFile exportPath exportOptions:exportOptions
        assert_true status message:"Failed to export to usdz"
        assert_true (doesFileExist exportPath) message:"cannot find exported usdz file"

        try
        (
            pyZip = python.import("zipfile")
            zipfile = pyZip.ZipFile(exportPath)
            zipStatus = zipfile.testzip()
            assert_undefined zipStatus message:("Exported usdz is a corrupted zip file")
            
            -- verify the presence of the exported stage USD file named the same as the USDZ file
            stageFilePresent = false
            local expectedFilePath
            maxver = maxVersion()

            if maxver[1] <= 26000 then ( 
                -- 2024 RC and before
                expectedFilePath = pathConfig.removePathTopParent (output_prefix + (stageFilename + ".usd")) -- remove 'c:/'
            )
            else (
                -- we introduce 23.11 in 2025
                -- due to changes in pixar USD 23.11 in the following commit:
                -- https://github.com/PixarAnimationStudios/OpenUSD/commit/7110d3f6e5db69bea384d8874665666aa7dce755
                -- the way the zip is created will no longer hold the directory structure to the location of the .usdz
                -- within the compressed contents of the .usdz itself
                expectedFilePath = filename_prefix + stageFilename + ".usd"
            )
            for item in (zipfile.namelist() as Array) do
            (
                if pathConfig.pathsResolveEquivalent expectedFilePath item do
                (
                    stageFilePresent = true
                )
            )
            assert_true stageFilePresent message:("Exported usdz shall contain the export stage (.usd) with the same filename as the usdz file")
        )
        catch
        (
            assert_true false message:("Exported usdz is not a valid zip file")
        )
        
        -- Test that invalid file formats are rejected.
        try (
            local exportOptions = USDExporter.CreateOptions()
            exportOptions.FileFormat = 10
        )
        catch 
        (
            exceptionThrown = true
        )
        assert_true exceptionThrown message:"Attempting to set an invalid FileFormat should have thrown an exception."
    ),

    function test_file_format_versus_extension = (
        local exportOptions = USDExporter.CreateOptions()
        
        -- Test that a mismatch between the extension and the FileFormat option fails the export.
        exportPath = output_prefix + "mismatch.usda"
        exportOptions.FileFormat = #binary
        local result = USDExporter.ExportFile exportPath exportOptions:exportOptions
        assert_false result message:"The export should have failed on FileFormat / extension mismatch (#binary with .usda)"
        
        exportOptions.FileFormat = #ascii
        exportPath = output_prefix + "mismatch.usdc"
        result = USDExporter.ExportFile exportPath exportOptions:exportOptions
        assert_false result message:"The export should have failed on FileFormat / extension mismatch (#ascii with .usdc)"
        
        -- Test good cases, #binary/usdc and #ascii/usda
        exportPath = output_prefix + "match.usda"
        exportOptions.FileFormat = #ascii
        result = USDExporter.ExportFile exportPath exportOptions:exportOptions
        assert_true result message:"The export should have succeeded on matching extension / FileFormat (#ascii with .usda)"
        
        exportPath = output_prefix + "match.usdc"
        exportOptions.FileFormat = #binary
        result = USDExporter.ExportFile exportPath exportOptions:exportOptions
        assert_true result message:"The export should have succeeded on matching extension / FileFormat (#binary with .usdc)"
    ),

    function test_mtl_layer_path = (
        local exportOptions = USDExporter.CreateOptions()
        local exceptionThrown = false

        -- Good case, no unicode chars in the file path
        try(
            local pathName = output_prefix + "mtl_<path>.usda"
            exportOptions.MaterialLayerPath = pathName
        )
        catch
        (
            exceptionThrown = true
        )
        assert_equal exportOptions.MaterialLayerPath pathName
        assert_false exceptionThrown message:"Attempting to create a mtl layer path with no unicode characters."

        -- Test exception from using unicode chars
        try
        (
            exportOptions.MaterialLayerPath = "mtl©path.usda"
        )
        catch
        (
            exceptionThrown = true
        )
        assert_true exceptionThrown message:"Attempting to create a mtl layer path with a unicode character."
    ),
    
    function test_log_options = (
        local exportOptions = USDExporter.CreateOptions()
        exportOptions.FileFormat = #ascii
        local logPath = (output_prefix + "log_path_test.log")
        exportOptions.LogPath = logPath;
        assert_equal logPath exportOptions.LogPath
        
        -- Log off..
        exportOptions.LogLevel = #off
        assert_equal #off exportOptions.LogLevel
        
        -- Make sure the log doesnt currently exist.
        deleteFile logPath
        
        -- No log should be created on export.
        exportOptions.FileFormat = #ascii
        local exportPath = (output_prefix + "teapot.usda")
        teapot()
        USDExporter.ExportFile exportPath exportOptions:exportOptions
        assert_equal 0 (getfiles logPath).count

        local levels = #(#info, #warn, #error)
        -- For other levels, the log should be created.
        for level in levels do (
            -- Log should now be created.
            exportOptions.LogLevel = level
            assert_equal level exportOptions.LogLevel
            USDExporter.ExportFile exportPath exportOptions:exportOptions
            assert_not_equal 0 (getfiles logPath).count
            local logSize = getfileSize logPath
            if level == #info then (
                assert_not_equal 0 logSize message:"#info log should not be empty."
            )
            else (
                assert_equal 0 logSize message:((level as string) + " log should be empty.")
            )
            deleteFile logPath
        )
        
        try (
            -- Test that a bad path throws an error.
            local exportOptions = USDExporter.CreateOptions()
            exportOptions.LogPath = "<not valid!>"
        )
        catch 
        (
            exceptionThrown = true
        )
        assert_true exceptionThrown
        
        -- Test that invalid log levels are rejected.
        try (
            local exportOptions = USDExporter.CreateOptions()
            exportOptions.LogLevel = 10
        )
        catch 
        (
            exceptionThrown = true
        )
        assert_true exceptionThrown message:"Attempting to set an invalid LogLevel should have thrown an exception."
    ),

    function test_no_file_extension = (
        -- given
        local exportOptions = USDExporter.CreateOptions()
        exportPath = output_prefix + "no_file_extension"

        -- when
        exportOptions.FileFormat = #ascii
        -- then
        status = USDExporter.ExportFile exportPath exportOptions:exportOptions
        assert_false (doesFileExist exportPath)
        resultingExportPath = exportPath + ".usd"
        assert_equal 1 status message:("export failed")
        assert_true (doesFileExist resultingExportPath)

        -- when
        exportOptions.FileFormat = #binary
        -- then
        USDExporter.ExportFile exportPath exportOptions:exportOptions
        assert_false (doesFileExist exportPath)
        resultingExportPath = exportPath + ".usd"
        assert_true (doesFileExist resultingExportPath)
    ),
    
    function test_file_extension_uppercase = (
        -- given
        local exportOptions = USDExporter.CreateOptions()
        -- when
        -- simulate scenario in old versions of max when in ui user does not specify file extension
        exportPath = output_prefix + "file_extension.USD"
        exportOptions.FileFormat = #binary
        -- then
        status = USDExporter.ExportFile exportPath exportOptions:exportOptions
        assert_equal 1 status message:("export failed")
        assert_true (doesFileExist exportPath)

        -- when
        exportPath = output_prefix + "file_extension.usDA"
        exportOptions.FileFormat = #ascii
        -- then
        status = USDExporter.ExportFile exportPath exportOptions:exportOptions
        assert_equal 1 status message:("Export failed for " + exportPath)
        resultingExportPath = output_prefix + "file_extension.usda"
        assert_true (doesFileExist resultingExportPath)
    ),
    
    function test_normals_option = (
        local nodeName = "cube"
        local cube = Box name:"cube"
        local exportOptions = USDExporter.CreateOptions()
        
        exportOptions.FileFormat = #ascii
        local exportPath = output_prefix + "no_normals.usda"

        -- Test exporting without normals.
        
        exportOptions.Normals = #none
        exportOptions.RootPrimPath = "/" 
        
        UsdExporter.ExportFile exportPath exportOptions:exportOptions

        local stage = pyUsd.Stage.Open(exportPath)
        local cubePrim = stage.GetPrimAtPath("/" + nodeName)
        assert_true (pyUsd.Prim.IsValid(cubePrim)) message:"Box should have exported to a valid prim."
        assert_true (cubePrim.IsA(pyUsdGeom.Mesh)) message:"Box should have exported to a valid UsdMesh"
        -- Should not have primvar normals
        local primvarApi = pyUsdGeom.PrimvarsAPI(cubePrim)
        assert_false (primvarApi.HasPrimvar("normals")) message:"Should not have a normals primvar when using #none."
        -- Should not have a normals attribute either.
        local cubeGeom = pyUsdGeom.Mesh(cubePrim)
        assert_undefined ((cubeGeom.GetNormalsAttr()).Get()) message:"Should not have a normals attribute when using #none."
        -- Subdiv scheme should be default.
        assert_equal "catmullClark" ((cubeGeom.GetSubdivisionSchemeAttr()).Get()) message:"When not exporting normals, should be using the default subdiv scheme."
            
        -- Test exporting normals as primvar.
            
        exportPath = output_prefix + "primvar_normals.usda"
        exportOptions.Normals = #asPrimvar
        UsdExporter.ExportFile exportPath exportOptions:exportOptions
        local stage = pyUsd.Stage.Open(exportPath)
        local cubePrim = stage.GetPrimAtPath("/" + nodeName)
        assert_true (pyUsd.Prim.IsValid(cubePrim)) message:"Box should have exported to a valid prim."
        assert_true (cubePrim.IsA(pyUsdGeom.Mesh)) message:"Box should have exported to a valid UsdMesh"
        -- Should have a normals primvar
        local primvarApi = pyUsdGeom.PrimvarsAPI(cubePrim)
        assert_true (primvarApi.HasPrimvar("normals")) message:"Should have exported normals as primvars."
        -- Not have a normals attribute either.
        local cubeGeom = pyUsdGeom.Mesh(cubePrim)
        assert_undefined ((cubeGeom.GetNormalsAttr()).Get()) message:"Should not have a normals attribute when using #asPrimvar."
        -- Subdiv scheme should be None. As normals are explicit.
        assert_equal "none" ((cubeGeom.GetSubdivisionSchemeAttr()).Get()) message:"Should be using none subdiv scheme when exporting normals."
            
        -- Test exporting normals as attribute
            
        exportPath = output_prefix + "attribute_normals.usda"
        exportOptions.Normals = #asAttribute
        UsdExporter.ExportFile exportPath exportOptions:exportOptions
        local stage = pyUsd.Stage.Open(exportPath)
        local cubePrim = stage.GetPrimAtPath("/" + nodeName)
        assert_true (pyUsd.Prim.IsValid(cubePrim)) message:"Box should have exported to a valid prim."
        assert_true (cubePrim.IsA(pyUsdGeom.Mesh)) message:"Box should have exported to a valid UsdMesh"
        -- Should not have a normals primvar
        local primvarApi = pyUsdGeom.PrimvarsAPI(cubePrim)
        assert_false (primvarApi.HasPrimvar("normals")) message:"Should not have primvar normals when using #asAttribute"
        -- Should have a normals attribute.
        local cubeGeom = pyUsdGeom.Mesh(cubePrim)
        assert_defined ((cubeGeom.GetNormalsAttr()).Get()) message:"Should have exported normals to an attribute."
        -- Subdiv scheme should be None. As normals are explicit.
        assert_equal "none" ((cubeGeom.GetSubdivisionSchemeAttr()).Get())  message:"Should be using none subdiv scheme when exporting normals."
            
        -- Test that invalid normal modes are rejected.
        try (
            local exportOptions = USDExporter.CreateOptions()
            exportOptions.Normals = 10
        )
        catch 
        (
            exceptionThrown = true
        )
        assert_true exceptionThrown message:"Attempting to set an invalid NormalMode should have thrown an exception."
    ),
    
    function test_mesh_format_option = (
        
        -- Local utility functions.
        function validate_usd_cube_using_triangles nodeName prim = (
            assert_true (pyUsd.Prim.IsValid(prim)) message:(nodeName + " should have exported to a valid prim.")
            assert_true (prim.IsA(pyUsdGeom.Mesh)) message:(nodeName + " should have exported to a valid UsdMesh")
            local cubeTriUsdMesh = pyUsdGeom.Mesh(prim)
            local triFaceCount = cubeTriUsdMesh.GetFaceCount()            
            assert_equal 12 triFaceCount message:(nodeName + " should have exporting to a cube with 12 faces. 2 triangles per face.")
                
            local triFaceVertexCounts = (cubeTriUsdMesh.GetFaceVertexCountsAttr()).Get()
            for face = 1 to triFaceCount do ( 
                assert_equal 3 triFaceVertexCounts[face] message:(nodeName + " should have exported to a cube mesh with all triangular faces.")
            )
        )
        
        function validate_usd_cube_using_polys nodeName prim = (
            assert_true (pyUsd.Prim.IsValid(prim)) message:(nodeName + " should have exported to a valid prim.")
            assert_true (prim.IsA(pyUsdGeom.Mesh)) message:(nodeName + " should have exported to a valid UsdMesh")
            local cubePolyUsdMesh = pyUsdGeom.Mesh(prim)
            local polyFaceCount = cubePolyUsdMesh.GetFaceCount()        
                
            assert_equal 6 polyFaceCount message:(nodeName + " should have exporting to a cube with 6 faces. 1 quad per face.")
            
            local polyFaceVertexCounts = (cubePolyUsdMesh.GetFaceVertexCountsAttr()).Get()
            for face = 1 to polyFaceCount do ( 
                assert_equal 4 polyFaceVertexCounts[face] message:(nodeName + " should have exported to a cube mesh with all polygonal faces.")
            )
        )
        
        local cube = Box name:"cube"
        
        local cubeTri = Box name:"cubeTri"
        convertToMesh cubeTri
        
        local cubePoly = Box name:"cubePoly"
        convertToPoly cubePoly
                
        local exportOptions = USDExporter.CreateOptions()
        exportOptions.FileFormat = #ascii
        
        -- Test exporting with MeshFormat = #fromScene (default)
        local exportPath = output_prefix + "meshformat_from_scene.usda"
        exportOptions.MeshFormat = #fromScene
        exportOptions.RootPrimPath = "/"
        
        UsdExporter.ExportFile exportPath exportOptions:exportOptions
        local stage = pyUsd.Stage.Open(exportPath)
        -- The "Box" cube will be converted to poly as fallback, it is neither a triMesh nor a polyMesh.
        local cubePrim = stage.GetPrimAtPath("/" + cube.Name)
        validate_usd_cube_using_polys cube.Name cubePrim
        -- The editable mesh cube should be all triangles on the USD side.
        local cubeTriPrim = stage.GetPrimAtPath("/" + cubeTri.Name)
        validate_usd_cube_using_triangles cubeTri.Name cubeTriPrim
        -- The editable poly cube should be all quads on the USD side.
        local cubePolyPrim = stage.GetPrimAtPath("/" + cubePoly.Name)
        validate_usd_cube_using_polys cubePoly.Name cubePolyPrim
        
        -- Test exporting with MeshFormat = #polyMesh 
        local exportPath = output_prefix + "meshformat_polyMesh.usda"
        exportOptions.MeshFormat = #polyMesh
        UsdExporter.ExportFile exportPath exportOptions:exportOptions
        local stage = pyUsd.Stage.Open(exportPath)
        -- The "Box" cube will be converted to polygonal mesh.
        local cubePrim = stage.GetPrimAtPath("/" + cube.Name)
        validate_usd_cube_using_polys cube.Name cubePrim
        -- The editable mesh cube should be all quads on the USD side. Hidden edges were removed to build polys.
        local cubeTriPrim = stage.GetPrimAtPath("/" + cubeTri.Name)
        validate_usd_cube_using_polys cubeTri.Name cubeTriPrim
        -- The editable poly cube should be all quads on the USD side.
        local cubePolyPrim = stage.GetPrimAtPath("/" + cubePoly.Name)
        validate_usd_cube_using_polys cubePoly.Name cubePolyPrim
        
        -- Test exporting with MeshFormat = #triMesh
        local exportPath = output_prefix + "meshformat_triMesh.usda"
        exportOptions.MeshFormat = #triMesh
        UsdExporter.ExportFile exportPath exportOptions:exportOptions
        local stage = pyUsd.Stage.Open(exportPath)
        -- The "Box" cube will be converted to a triangular mesh.
        local cubePrim = stage.GetPrimAtPath("/" + cube.Name)
        validate_usd_cube_using_triangles cube.Name cubePrim
        -- The editable mesh cube should be all triangles on the USD side.        
        local cubeTriPrim = stage.GetPrimAtPath("/" + cubeTri.Name)
        validate_usd_cube_using_triangles cubeTri.Name cubeTriPrim
        -- The editable poly cube should be all triangles on the USD side.
        local cubePolyPrim = stage.GetPrimAtPath("/" + cubePoly.Name)
        validate_usd_cube_using_triangles cubePoly.Name cubePolyPrim
        
        -- Test that invalid MeshFormat modes are rejected.
        try (
            local exportOptions = USDExporter.CreateOptions()
            exportOptions.MeshFormat = 10
        )
        catch 
        (
            exceptionThrown = true
        )
        assert_true exceptionThrown message:"Attempting to set an invalid MeshFormat should have thrown an exception."        
        
    ),    
    
    function test_export_at_time = (
        
        local exportOptions = USDExporter.CreateOptions()
        exportOptions.FileFormat = #ascii
        exportOptions.RootPrimPath = "/"
        
        local nodeName = "cube"
        local cube = Box name:"cube"
        
        convertToPoly cube
        polyop.setFaceSelection cube  #{2}
        
        with animate on 
        (
            -- Animate both the position and the geometry.
            at time 10 move cube [0,0,10]
            at time 10 move cube.selectedFaces [0,0,10]
        )
        
        -- Time values. The animation runs from 0 to 10.
        local testValues = #(-1,0,5,10,11)
        
        -- Testing #current as TimeMode
        exportOptions.TimeMode = #current
        for val in testValues do (
            local exportPath = output_prefix + "export_time_current_" + val as string + ".usda"
            -- Change the currentTime.
            sliderTime = val
            USDExporter.ExportFile exportPath exportOptions:exportOptions
            local stage = pyUsd.Stage.Open(exportPath)
            usdCube = stage.GetPrimAtPath("/cube")
            
            -- Test transform.
            local xformable = pyUsdGeom.Xformable(usdCube)
            local t = at time val cube.transform
            assert_true (compareUsdAndMaxTransforms (xformable.GetLocalTransformation()) t) message:("The cube's transform is wrong when exporting with #current at t = " + val as string)
        
            -- Test geometry.
            local mesh = pyUsdGeom.Mesh(usdCube)
            local points = usdCube.GetAttribute("points")
            local vertices = (points).Get()
            
            for i = 5 to 8 do (
                local pos = at time val in coordsys local cube.max[3]
                assert_equal pos vertices[i][3] message:("The cube's top face position is wrong when exporting with #current at t = " + val as string)
            )
            assert_equal 0.0 (stage.GetStartTimeCode()) message:"Unexpected start timeCode set in the USD stage."
            assert_equal 0.0 (stage.GetEndTimeCode()) message:"Unexpected end timeCode set in the USD stage."
        )

        -- Testing #explicit as TimeMode
        exportOptions.TimeMode = #explicit
        for val in testValues do (
            local exportPath = output_prefix + "export_time_explicit" + val as string + ".usda"
            
            -- Specify the time at which to export explicitly.
            exportOptions.StartFrame = val
            USDExporter.ExportFile exportPath exportOptions:exportOptions
            local stage = pyUsd.Stage.Open(exportPath)
            usdCube = stage.GetPrimAtPath("/cube")
            
            -- Test transform.
            local xformable = pyUsdGeom.Xformable(usdCube)
            local t = at time val cube.transform
            assert_true (compareUsdAndMaxTransforms (xformable.GetLocalTransformation()) t) message:("The cube's transform is wrong when exporting with #explicit at t = " + val as string)
                
            -- Test geometry.
            local mesh = pyUsdGeom.Mesh(usdCube)
            local points = usdCube.GetAttribute("points")
            local vertices = (points).Get()
            for i = 5 to 8 do (
                local pos = at time val in coordsys local cube.max[3]
                assert_equal pos vertices[i][3] message:("The cube's top face position is wrong when exporting with #explicit at t = " + val as string)
            )
            assert_equal 0.0 (stage.GetStartTimeCode()) message:"Unexpected start timeCode set in the USD stage."
            assert_equal 0.0 (stage.GetEndTimeCode()) message:"Unexpected end timeCode set in the USD stage."            
        )
    
        -- Testing #animationRange as TimeMode
        -- Configure the time slider to go from 2 to 8.        
        local rangeStart = 2
        local rangeEnd = 8
        
        animationRange = interval rangeStart rangeEnd
        local exportPath1 = output_prefix + "export_time_animationrange.usda"
        exportOptions.TimeMode = #animationRange
        
        USDExporter.ExportFile exportPath1 exportOptions:exportOptions
        local stage = pyUsd.Stage.Open(exportPath1)
        usdCube = stage.GetPrimAtPath("/cube")
        
        -- Test transform.
        local xformable = pyUsdGeom.Xformable(usdCube)
        local mesh = pyUsdGeom.Mesh(usdCube)
        local points = usdCube.GetAttribute("points")    
        
        for timeCode in rangeStart to rangeEnd do (
            local t = at time timeCode cube.transform
            assert_true (compareUsdAndMaxTransforms (xformable.GetLocalTransformation(timeCode)) t) message:("The cube's transform is wrong when exporting with #animationRange at t = " + timeCode as string)
            -- Test geometry.
            local vertices = (points).Get(timeCode)
            for i = 5 to 8 do (
                local pos = at time timeCode in coordsys local cube.max[3]
                assert_equal pos vertices[i][3] message:("The cube's top face position is wrong when exporting with #animationRange at t = " + timeCode as string)
            )
        )
        assert_equal 2.0 (stage.GetStartTimeCode()) message:"Unexpected start timeCode set in the USD stage."
        assert_equal 8.0 (stage.GetEndTimeCode()) message:"Unexpected end timeCode set in the USD stage."
        
        local exportPath = output_prefix + "export_time_framerange.usda"
        exportOptions.TimeMode = #frameRange
        exportOptions.StartFrame = -5
        exportOptions.EndFrame = 5
        
        USDExporter.ExportFile exportPath exportOptions:exportOptions
        local stage = pyUsd.Stage.Open(exportPath)
        usdCube = stage.GetPrimAtPath("/cube")
        
        -- Test transform.
        local xformable = pyUsdGeom.Xformable(usdCube)
        local mesh = pyUsdGeom.Mesh(usdCube)
        local points = usdCube.GetAttribute("points")    
        
        for timeCode = exportOptions.StartFrame to exportOptions.EndFrame do (
            local t = at time timeCode cube.transform
            assert_true (compareUsdAndMaxTransforms (xformable.GetLocalTransformation(timeCode)) t) message:("The cube's transform is wrong when exporting with #animationRange at t = " + val as string)
            -- Test geometry.
            local vertices = (points).Get(timeCode)
            for i = 5 to 8 do (
                local pos = at time timeCode in coordsys local cube.max[3]
                assert_equal pos vertices[i][3] message:("The cube's top face position is wrong when exporting with #frameRange at t = " + timeCode as string)
            )
        )
        assert_equal exportOptions.StartFrame (stage.GetStartTimeCode()) message:"Unexpected start timeCode set in the USD stage."
        assert_equal exportOptions.EndFrame (stage.GetEndTimeCode()) message:"Unexpected end timeCode set in the USD stage."
        
        -- Test samples per frame.
        samplePerFrameToTest = #(0.5, 0.75, 1.0, 1.5, 3.0)
        expectedSampleCount = #(2, 3, 3, 5, 8)
        
        for i in 1 to samplePerFrameToTest.Count do (
            
            local exportPath = output_prefix + "export_samples_per_frame_" + samplePerFrameToTest[i] as string + ".usda"            
            exportOptions.TimeMode = #frameRange
            exportOptions.StartFrame = 0
            exportOptions.EndFrame = 2
            exportOptions.SamplesPerFrame = samplePerFrameToTest[i]
            
            USDExporter.ExportFile exportPath exportOptions:exportOptions
            local stage = pyUsd.Stage.Open(exportPath)
            usdCube = stage.GetPrimAtPath("/cube")
            
            -- Test transform.
            local xformable = pyUsdGeom.Xformable(usdCube)
            local mesh = pyUsdGeom.Mesh(usdCube)
            local points = usdCube.GetAttribute("points")    
            
            local timeSamples = points.GetTimeSamples()
            
            assert_equal expectedSampleCount[i] timeSamples.Count message:("Unexpected exported sample counts at " + samplePerFrameToTest[i] as string)    
        )
        
        -- Test that a warning is raised for EndTime < StartTime, and only the StartTime is exported.
        exportOptions.StartFrame = 0
        exportOptions.EndFrame = -10
        exportOptions.TimeMode = #frameRange
        exportOptions.LogLevel = #warn 
        exportOptions.LogPath = output_prefix + "end_time_before_start_time.log"
        deleteFile exportOptions.LogPath
        local exportPath = output_prefix + "end_time_before_start_time.usda"
        USDExporter.ExportFile exportPath exportOptions:exportOptions
        assert_not_equal 0 (getfiles exportOptions.LogPath).count
        local logSize = getfileSize exportOptions.LogPath
        assert_not_equal 0 logSize message:"A warning should have been raised for EndTime < StartTime."
        
        -- Test validation of the TimeMode.
        exportOptions.Reset()
        try (
            exportOptions.TimeMode = -1
        )
        catch 
        (
            exceptionThrown = true
        )
        assert_true exceptionThrown message:"No exception was thrown when setting an invalid TimeMode."
        
        -- Test validation of the SamplesPerFrame.
        exportOptions.Reset()
        try (
            exportOptions.SamplesPerFrame = -1
        )
        catch 
        (
            exceptionThrown = true
        )
        assert_true exceptionThrown message:"No exception was thrown when setting an invalid SamplesPerFrame value."
    ), 
    
    function test_root_prim_path_option = (        
        local box1 = Box name:"Box1"
        local exportPath = (output_prefix + "empty_prim_path_test.usda")
        local exportOptions = USDExporter.CreateOptions()
        exportOptions.FileFormat = #ascii
        
        -- Test default behavior, when the RootPrimPath is empty, use the target file name as root prim.
        exportOptions.RootPrimPath = ""
        USDExporter.ExportFile exportPath exportOptions:exportOptions
        local stage = pyUsd.Stage.Open(exportPath)
        boxPrim = stage.GetPrimAtPath("/" + (getFilenameFile exportPath) + "/" + box1.Name)
        assert_true (pyUsd.Prim.IsValid(boxPrim)) message:"The box was not exported to the expected path."
                    
        local exportPath = (output_prefix + "absolute_root_prim_path_test.usda")
        -- Test "/" as the root. 
        exportOptions.RootPrimPath = "/"
        
        USDExporter.ExportFile exportPath exportOptions:exportOptions
        local stage = pyUsd.Stage.Open(exportPath)
        boxPrim = stage.GetPrimAtPath("/" + box1.Name)
        assert_true (pyUsd.Prim.IsValid(boxPrim)) message:"The box was not exported to the expected path."
            
        -- Now specify the target root prim explicitely.            
        local exportPath = (output_prefix + "specified_root_prim_path_test.usda")
        
        exportOptions.RootPrimpath = "/foo/bar/baz"
        USDExporter.ExportFile exportPath exportOptions:exportOptions
        
        local stage = pyUsd.Stage.Open(exportPath)
        boxPrim = stage.GetPrimAtPath(exportOptions.RootPrimPath + "/" + box1.Name)
        assert_true (pyUsd.Prim.IsValid(boxPrim)) message:"The box was not exported to the expected path."
        
        -- Make sure that prototype prims and the materials scope are also created under the specified root.
        local exportPath = (output_prefix + "specified_root_prim_path_with_instances_and_mat_test.usda")
        box2 = create_clone box1 #instance "Box2"
        box1.Material = PhysicalMaterial()
        USDExporter.ExportFile exportPath exportOptions:exportOptions
        local stage = pyUsd.Stage.Open(exportPath)
        box1Prim = stage.GetPrimAtPath(exportOptions.RootPrimPath + "/" + box1.Name)
        assert_true (pyUsd.Prim.IsValid(box1Prim)) message:"The box was not exported to the expected path."
        box2Prim = stage.GetPrimAtPath(exportOptions.RootPrimPath + "/" + box2.Name)
        assert_true (pyUsd.Prim.IsValid(box2Prim)) message:"The instanced box was not exported to the expected path."
        prototypePrim = stage.GetPrimAtPath(exportOptions.RootPrimPath + "/_class_Box1")
        assert_true (pyUsd.Prim.IsValid(prototypePrim)) message:"The prototype prim for the boxes was not exported to the expected path."
        materialScope = stage.GetPrimAtPath(exportOptions.RootPrimPath + "/mtl")
        assert_true (pyUsd.Prim.IsValid(materialScope)) message:"The material scope prim was not exported to the expected path."
            
        -- Make sure only valid prim paths are accepted.
        -- Test attempting to set an invalid path throws.
        local exportOptions = USDExporter.CreateOptions()
        local errorMessage = "No exception was thrown when setting invalid USD path as root prim."
        
        -- Completely wrong path.
        local exceptionThrown = false
        try (
            exportOptions.RootPrimPath = "#&$"
        )
        catch 
        (
            exceptionThrown = true
        )
        assert_true exceptionThrown message:errorMessage

        -- Valid prim path, but not absolute.
        local exceptionThrown = false
        try (
            exportOptions.RootPrimPath = "Foo/bar"
        )
        catch 
        (
            exceptionThrown = true
        )
        assert_true exceptionThrown message:errorMessage
        
        -- Absolute path, but to a property, not a prim.
        local exceptionThrown = false
        try (
            exportOptions.RootPrimPath = "/Foo.prop"
        )
        catch 
        (
            exceptionThrown = true
        )
        assert_true exceptionThrown message:errorMessage
    ),
    
    function test_export_hidden_objects_option = (
        /*
        Create a simple scene with this hierarchy.
          - Box1
            - Box2  
              - Box3
              - Box4
        */
        box1 = box name:"Box1"
        box2 = box name:"Box2"
        box2.parent = Box1
        box3 = box name:"Box3"
        box3.parent = Box2
        box4 = box name:"Box4"
        box4.parent = Box2
        
        local exportOptions = USDExporter.CreateOptions()
        exportOptions.FileFormat = #ascii
        exportOptions.RootPrimPath = "/"
        
        -- Test HiddenObjects = true : 
        local exportPath = (output_prefix + "export_hidden_true.usda")
        -- Hide all objects
        box1.isHidden = true
        box2.isHidden = true
        box3.isHidden = true
        box4.isHidden = true
        exportOptions.HiddenObjects = true
        
        USDExporter.ExportFile exportPath exportOptions:exportOptions
        local stage = pyUsd.Stage.Open(exportPath)
        
        box1Prim = stage.GetPrimAtPath("/Box1")
        assert_true (pyUsd.Prim.IsValid(box1Prim)) message:"Box1 was not exported to the expected path."
        box2Prim = stage.GetPrimAtPath("/Box1/Box2")
        assert_true (pyUsd.Prim.IsValid(box2Prim)) message:"Box2 was not exported to the expected path."
        box3Prim = stage.GetPrimAtPath("/Box1/Box2/Box3")
        assert_true (pyUsd.Prim.IsValid(box3Prim)) message:"Box3 was not exported to the expected path."
        box4Prim = stage.GetPrimAtPath("/Box1/Box2/Box4")
        assert_true (pyUsd.Prim.IsValid(box4Prim)) message:"Box4 was not exported to the expected path."
        
        -- Test HiddenObjects = false : 
        local exportPath = (output_prefix + "export_hidden_false.usda")
        exportOptions.HiddenObjects = false
        
        USDExporter.ExportFile exportPath exportOptions:exportOptions
        local stage = pyUsd.Stage.Open(exportPath)
        
        root = stage.GetPseudoRoot()
        assert_equal 0 (root.GetChildren()).Count message:"No prims should have been exported, all nodes were hidden."
        
        -- Test HiddenObjects = false with hidden parents and visible children.
        box1.isHidden = true
        box2.isHidden = true
        box3.isHidden = false
        box4.isHidden = false
        
        local exportPath = (output_prefix + "export_hidden_parent_visible_children.usda")
        USDExporter.ExportFile exportPath exportOptions:exportOptions
        local stage = pyUsd.Stage.Open(exportPath)
        
        -- Box1 and Box2 should still have been exported, as they have visible descendants, and we need their transforms,
        -- however, they should only be exported as Xforms.
        box1Prim = stage.GetPrimAtPath("/Box1")
        assert_true (pyUsd.Prim.IsValid(box1Prim)) message:"Box1 was not exported to the expected path."
        assert_true (box1Prim.IsA(pyUsdGeom.Xform)) message:"Box1 should have been exported as an Xform."
        box2Prim = stage.GetPrimAtPath("/Box1/Box2")
        assert_true (pyUsd.Prim.IsValid(box2Prim)) message:"Box2 was not exported to the expected path."
        assert_true (box2Prim.IsA(pyUsdGeom.Xform)) message:"Box2 should have been exported as an Xform."
        box3Prim = stage.GetPrimAtPath("/Box1/Box2/Box3")
        assert_true (pyUsd.Prim.IsValid(box3Prim)) message:"Box3 was not exported to the expected path."
        assert_true (box3prim.IsA(pyUsdGeom.Mesh)) message:"Box3 should have been exported as a Mesh."
        box4Prim = stage.GetPrimAtPath("/Box1/Box2/Box4")
        assert_true (pyUsd.Prim.IsValid(box4Prim)) message:"Box4 was not exported to the expected path."
        assert_true (box4prim.IsA(pyUsdGeom.Mesh)) message:"Box4 should have been exported as a Mesh."
            
        -- Test behavior with a node list...
        -- HiddenObjects = false 
        -- Hidden parents and visible children which are not part of the node list.
        box1.isHidden = true
        box2.isHidden = true
        box3.isHidden = false
        box4.isHidden = false
            
        local nodesToExport = #(box1, box2)
        exportPath = (output_prefix + "export_hidden_parent_visible_children_nodelist.usda")
        USDExporter.ExportFile exportPath exportOptions:exportOptions contentSource:#nodeList nodeList:nodesToExport
            
        -- Should get nothing on export. 
        local stage = pyUsd.Stage.Open(exportPath)
        root = stage.GetPseudoRoot()
        assert_equal 0 (root.GetChildren()).Count message:"No prims should have been exported, all nodes were hidden or excluded from the node list."
            
        -- Test behavior with a node list...
        -- HiddenObjects = false 
        -- Hidden parent and visible non immediate descendant which is part of the node list.
        box1.isHidden = true
        box2.isHidden = true
        box3.isHidden = false
        box4.isHidden = true
        
        local nodesToExport = #(box1, box3)
        exportPath = (output_prefix + "export_hidden_parent_visible_descendant_nodelist.usda")
        USDExporter.ExportFile exportPath exportOptions:exportOptions contentSource:#nodeList nodeList:nodesToExport
            
        -- Should only get box3 exported at the root. box1 is part of the list and has a visible descendant but box2 is excluded, 
        -- so box3 will be exported at the root, and therefor box1 does not need to be exported as an Xform.
        local stage = pyUsd.Stage.Open(exportPath)
        root = stage.GetPseudoRoot()
        assert_equal 1 (root.GetChildren()).Count message:"A single prim should have been exported, none found."
        box3Prim = stage.GetPrimAtPath("/Box3")
        assert_true (pyUsd.Prim.IsValid(box3Prim)) message:"Box3 was not exported to the expected path."
        assert_true (box3prim.IsA(pyUsdGeom.Mesh)) message:"Box3 should have been exported as a Mesh."
            
        -- Test behavior with a node list...
        -- HiddenObjects = false 
        -- All nodes part of the list are hidden, other nodes are visible.
        box1.isHidden = true
        box2.isHidden = true
        box3.isHidden = true
        box4.isHidden = false
        
        local nodesToExport = #(box1, box2, box3)
        exportPath = (output_prefix + "all_nodes_in_list_hidden.usda")
        USDExporter.ExportFile exportPath exportOptions:exportOptions contentSource:#nodeList nodeList:nodesToExport
            
        -- Should get nothing on export. 
        local stage = pyUsd.Stage.Open(exportPath)
        root = stage.GetPseudoRoot()
        assert_equal 0 (root.GetChildren()).Count message:"No prims should have been exported, all nodes in the node list were hidden."
            
        -- Test behavior with a node list...
        -- HiddenObjects = false 
        -- Single node in the node list, all parents invisible.
            
        box1.isHidden = true
        box2.isHidden = true
        box3.isHidden = false
        box4.isHidden = true
            
        local nodesToExport = #(box3)
        exportPath = (output_prefix + "single_export_node_visible_parents_hidden.usda")
        USDExporter.ExportFile exportPath exportOptions:exportOptions contentSource:#nodeList nodeList:nodesToExport
            
        local stage = pyUsd.Stage.Open(exportPath)
        root = stage.GetPseudoRoot()
        assert_equal 1 (root.GetChildren()).Count message:"Only Box3 should have been exported."
        box3Prim = stage.GetPrimAtPath("/Box3")
        assert_true (pyUsd.Prim.IsValid(box3Prim)) message:"Box3 was not exported to the expected path."
        assert_true (box3prim.IsA(pyUsdGeom.Mesh)) message:"Box3 should have been exported as a Mesh."
    ),
    
    function test_use_usd_visibility_option = (
        -- Create a single, hidden box.
        box1 = box name:"Box1"
        box1.isHidden = true
        
        local exportOptions = USDExporter.CreateOptions()
        exportOptions.FileFormat = #ascii
        exportOptions.RootPrimPath = "/"
        -- HiddenObjects must be exported so we can test UseUSDVisibility.
        exportOptions.HiddenObjects = true
        
        -- Test UseUSDVisibility = true
        exportOptions.UseUSDVisibility = true
        local exportPath = (output_prefix + "use_usd_visibility_on.usda")
        USDExporter.ExportFile exportPath exportOptions:exportOptions
        
        local stage = pyUsd.Stage.Open(exportPath)
        box1Prim = stage.GetPrimAtPath("/Box1")
        local xformable = pyUsdGeom.Xformable(box1Prim)
        local visibilityAttr = xformable.GetVisibilityAttr()
        local visibility = (visibilityAttr.Get())
        assert_equal "invisible" visibility message:"The exported box should have been flagged as invisible."
        
        -- Test UseUSDVisibility = false
        exportOptions.UseUSDVisibility = false
        local exportPath = (output_prefix + "use_usd_visibility_off.usda")
        USDExporter.ExportFile exportPath exportOptions:exportOptions
        
        local stage = pyUsd.Stage.Open(exportPath)
        box1Prim = stage.GetPrimAtPath("/Box1")
        local xformable = pyUsdGeom.Xformable(box1Prim)
        local visibilityAttr = xformable.GetVisibilityAttr()
        local visibility = (visibilityAttr.Get())
        assert_equal "inherited" visibility message:"The exported box should should not have been flagged invisible, UseUSDVisibility is false."
    ),
    
    function check_usd_view_opened exportPath = (
        -- Usdview is started via python, we should find a python process with the exported USD file
        -- somewhere in its arguments. We output the WMIC result to a file to read it back bellow.
        local wmicOutputFile = (exportPath + ".wmic.txt")
        DOSCommand ("WMIC path win32_process where Caption=\"python.exe\" get Commandline,processid | find \"" + exportPath + "\" > \"" + wmicOutputFile + "\"")
        
        local usdViewOpened = false
        local fileHandle = openFile wmicOutputFile
        if not eof fileHandle do 
        (
            -- Makes sure that the file was actually found.
            local firstLine =  readLine fileHandle
            usdViewOpened = (findString firstLine exportPath) != undefined
            
            -- We can now close usdview...
            -- We requested the PID from WMIC, so it will be found at the end of the space seperated string.
            local tokens = filterString firstLine " "
            local PID = tokens[tokens.Count]
            DOSCommand ("taskkill /F /PID " + PID)
        )
        return usdViewOpened
    ),
    
    function test_open_in_usdview = (
        -- WARNING : automation machines need to have formal 3ds Max versions (for each target version)
        --           in order for the registry keys to be present
        Teapot()
        
        local exportOptions = USDExporter.CreateOptions()
        exportOptions.FileFormat = #ascii
        
        -- Test open in Usdview off.
        exportOptions.OpenInUsdview = false
        local exportPath = (output_prefix + "test_do_not_open_in_usdview.usda")
        USDExporter.ExportFile exportPath exportOptions:exportOptions
        forceCompleteRedraw()  -- force a running loop in 3ds Max
        sleep 5 -- Give it some time to open...
        assert_false (check_usd_view_opened exportPath) message:"Usdview was unexpectedly opened."

        -- Test open in Usdview on.
        exportOptions.OpenInUsdview = true
        local exportPath = (output_prefix + "test_open_in_usdview.usda")
        USDExporter.ExportFile exportPath exportOptions:exportOptions
        forceCompleteRedraw()  -- force a running loop in 3ds Max
        sleep 5 -- Give it some time to open...
        assert_true (check_usd_view_opened exportPath) message:"Usdview was not opened as expected."
    ),
    
    function test_usd_stages_as_references_option = (
        -- Create a simple layer to bring in as a USD stage.
        local stage = pyUsd.Stage.CreateInMemory()
        local sphere = pyUsdGeom.Sphere.Define stage "/sphere"
        stage.SetDefaultPrim (sphere.GetPrim())
            
        local layerName = "simple_layer_sphere"
        local layerPath = output_prefix + layerName + ".usda"
        stage.Export layerPath
        
        -- Create a USD Stage Object from the layer.
        stage = UsdStageObject name:"UsdStageObject"
        stage.SetRootLayer layerPath
            
        -- Assign a material to the stage, this material should be ignored.        
        local dummyMaterial = MaxUsdPreviewSurface()
        stage.material = dummyMaterial
            
        -- Export the scene to USD.
        local exportOptions = USDExporter.CreateOptions()
        exportOptions.FileFormat = #ascii
        exportOptions.RootPrimPath = "/"
        exportOptions.UsdStagesAsReferences = false
            
        local exportPath = output_prefix + "export_stage_as_reference_off.usd"
        USDExporter.ExportFile exportPath exportOptions:exportOptions
            
        -- Validate that USD Stage was not exported.
        local exporterLayerStage = pyUsd.Stage.Open exportPath
        
        local usdStagePrim = exporterLayerStage.GetPrimAtPath ("/UsdStageObject")
        assert_false (usdStagePrim.IsValid())
            
        -- Now configure USD Stages to export as reference..
        exportOptions.UsdStagesAsReferences = true
        exportPath = output_prefix + "export_stage_as_reference_on.usd"
        USDExporter.ExportFile exportPath exportOptions:exportOptions
            
        -- Validate that the reference was made.
        local exporterLayerStage = pyUsd.Stage.Open exportPath
        sphereRef = exporterLayerStage.GetPrimAtPath ("/UsdStageObject/UsdStageObject_Layer/sphere")
        assert_true (sphereRef.IsValid())
        assert_true (sphereRef.IsA(pyUsdGeom.Sphere)) message:"sphereRef should have been exported as a Sphere."
        
        -- Validate that the the material from the 3dsMax scene was not applied.
        binding = pyUsdShade.MaterialBindingAPI(sphereRef)
        local matAndRel = binding.ComputeBoundMaterial()
        assert_false (((matAndRel[1]).GetPrim()).IsValid())
    ),
    
    function test_all_material_conversions_option = (
        -- Test that an invalid material conversion type.
        try (
            local exportOptions = USDExporter.CreateOptions()
            exportOptions.AllMaterialTargets = #("UsdPreviewSurface", "bingo")
        )
        catch 
        (
            exceptionThrown = true
        )
        assert_true exceptionThrown message:"Attempting to set an invalid material target type should have thrown an exception."

    ),
        
    function test_allow_nested_gprims = (
        -- Base export options for all the tests in this function.
        local exportOptions = USDExporter.CreateOptions()
        exportOptions.RootPrimPath = "/"
        exportOptions.FileFormat = #ascii
        
        -- UsdGeomMesh
        (
            -- Start with allowing nesting.            
            exportOptions.AllowNestedGprims = true
            
            b1 = Box()
            b2 = Box()
            b3 = Box()
            b2.parent = b1
            b3.parent = b2
            stageFilename = "mesh_nested_gprims_allowed"
            local exportPath = output_prefix + (stageFilename + ".usda")
            
            status = USDExporter.ExportFile exportPath exportOptions:exportOptions
                    
            stage = pyUsd.Stage.Open(exportPath)
            local box1 = stage.GetPrimAtPath ("/Box001")
            assert_true (pyUsd.Prim.IsValid(box1))
            assert_true (box1.IsA(pyUsdGeom.Mesh)) message:"Box3 should have been exported as a Mesh."
            local box2 = stage.GetPrimAtPath("/Box001/Box002")
            assert_true (pyUsd.Prim.IsValid(box2))
            assert_true (box2.IsA(pyUsdGeom.Mesh)) message:"Box3 should have been exported as a Mesh."
            local box3 = stage.GetPrimAtPath("/Box001/Box002/Box003")
            assert_true (pyUsd.Prim.IsValid(box3))
            assert_true (box3.IsA(pyUsdGeom.Mesh)) message:"Box3 should have been exported as a Mesh."       
                        
            -- usdchecker should flag this file invalid, as it has nested gprims.
            assert_false (check_usd_file_valid exportPath)
            
            -- re-export, with disallowed nesting
            exportOptions.AllowNestedGprims = false
            stageFilename = "mesh_nested_gprims_disallowed"
            local exportPath = output_prefix + (stageFilename + ".usda")
            status = USDExporter.ExportFile exportPath exportOptions:exportOptions
            
            stage = pyUsd.Stage.Open(exportPath)
            local box1 = stage.GetPrimAtPath ("/Box001")
            assert_true (pyUsd.Prim.IsValid(box1))
            assert_true (box1.IsA(pyUsdGeom.Xform)) message:"box1 should have been exported as a Xform."
            local box1shape = stage.GetPrimAtPath ("/Box001/Box001_Shape")
            assert_true (pyUsd.Prim.IsValid(box1shape))
            assert_true (box1shape.IsA(pyUsdGeom.Mesh)) message:"box1shape should have been exported as a Mesh."       
            local box2 = stage.GetPrimAtPath ("/Box001/Box002")
            assert_true (pyUsd.Prim.IsValid(box2))
            assert_true (box2.IsA(pyUsdGeom.Xform)) message:"box2 should have been exported as a XForm."
            local box2shape = stage.GetPrimAtPath ("/Box001/Box002/Box002_Shape")
            assert_true (pyUsd.Prim.IsValid(box2shape))
            assert_true (box2shape.IsA(pyUsdGeom.Mesh)) message:"box2shape should have been exported as a Mesh."
            -- Box003 has no children, so no need for a shape prim.
            local box3 = stage.GetPrimAtPath ("/Box001/Box002/Box003")
            assert_true (pyUsd.Prim.IsValid(box3))
            assert_true (box3.IsA(pyUsdGeom.Mesh)) message:"box3 should have been exported as a Mesh."
            
            -- usdchecker should flag this file valid
            assert_true (check_usd_file_valid exportPath)
            
            resetMaxFile #noprompt
        )
        
        -- UsdGeomCurves
        (
            -- Start with allowing nesting.            
            exportOptions.AllowNestedGprims = true
            
            c1 = Circle()
            c2 = Circle()
            c3 = Circle()
            c2.parent = c1
            c3.parent = c2
                        
            stageFilename = "shape_nested_gprims_allowed"
            local exportPath = output_prefix + (stageFilename + ".usda")
            
            status = USDExporter.ExportFile exportPath exportOptions:exportOptions
                    
            stage = pyUsd.Stage.Open(exportPath)
            local circle1 = stage.GetPrimAtPath ("/Circle001")
            assert_true (pyUsd.Prim.IsValid(circle1))
            assert_true (circle1.IsA(pyUsdGeom.Curves)) message:"circle1 should have been exported as a Curves."
            local circle2 = stage.GetPrimAtPath("/Circle001/Circle002")
            assert_true (pyUsd.Prim.IsValid(circle2))
            assert_true (circle2.IsA(pyUsdGeom.Curves)) message:"circle2 should have been exported as a Curves."
            
            -- usdchecker should flag this file invalid, as it has nested gprims.
            assert_false (check_usd_file_valid exportPath)
            
            -- re-export, with disallowed nesting
            exportOptions.AllowNestedGprims = false
            stageFilename = "shape_nested_gprims_disallowed"
            local exportPath = output_prefix + (stageFilename + ".usda")
            status = USDExporter.ExportFile exportPath exportOptions:exportOptions
            
            stage = pyUsd.Stage.Open(exportPath)
            local circle1 = stage.GetPrimAtPath ("/Circle001")
            assert_true (pyUsd.Prim.IsValid(circle1))
            assert_true (circle1.IsA(pyUsdGeom.Xform)) message:"Circle001 should have been exported as a Xform."
            local circle1shape = stage.GetPrimAtPath ("/Circle001/Circle001_Shape")
            assert_true (pyUsd.Prim.IsValid(circle1shape))
            assert_true (circle1shape.IsA(pyUsdGeom.Curves)) message:"circle1shape should have been exported as a Curves."
            local circle2 = stage.GetPrimAtPath ("/Circle001/Circle002")
            assert_true (pyUsd.Prim.IsValid(circle2))
            assert_true (circle2.IsA(pyUsdGeom.Xform)) message:"circle2 should have been exported as a Xform.."
            local circle2shape = stage.GetPrimAtPath ("/Circle001/Circle002/Circle002_Shape")
            assert_true (pyUsd.Prim.IsValid(circle2shape))
            assert_true (circle2shape.IsA(pyUsdGeom.Curves)) message:"circle2shape should have been exported as a Curves."
            -- Circle003 has no children, so no need for a shape prim.
            local circle3shape = stage.GetPrimAtPath ("/Circle001/Circle002/Circle003")
            assert_true (pyUsd.Prim.IsValid(circle3shape))
            assert_true (circle3shape.IsA(pyUsdGeom.Curves)) message:"circle3shape should have been exported as a Curves."
            
            -- usdchecker should flag this file valid
            assert_true (check_usd_file_valid exportPath)
            
            resetMaxFile #noprompt
        )
    ),

    function test_use_last_resort_USDPreviewSurface_Writer = (
        local exportOptions = USDExporter.CreateOptions()
        exportOptions.RootPrimPath = "/"
        exportOptions.FileFormat = #ascii
        exportOptions.UseLastResortUSDPreviewSurfaceWriter = false

        local boxWithMtlX = Box name:"mtlx"
        boxWithMtlX.pos = [0,0,50] -- just to see it!
        local mtlx = MaterialXMaterial name:"mtlx1"
        local scriptDir = getFilenamePath (getThisScriptFilename())
        local mtlxDoc = (scriptDir + @"data\Iberian_Blue_Ceramic_Tiles_1k_8b\Iberian_Blue_Ceramic_Tiles.mtlx")
        mtlx.importMaterial mtlxDoc
        boxWithMtlX.material = mtlx

        exportOptions.AllMaterialTargets = #("UsdPreviewSurface")
        local exportPath = output_prefix + "UseLastResortUSDPreviewSurfaceWriter.usda"
        status = USDExporter.ExportFile exportPath exportOptions:exportOptions
        
        stage = pyUsd.Stage.Open(exportPath)
        local mtlScope = stage.GetPrimAtPath ("/mtl")
        assert_true (pyUsd.Prim.IsValid(mtlScope)) message:"/mtl should exist and should be valid"
        assert_equal 0 ((pyUsd.Prim.GetAllChildren(mtlScope)).count) message:"Didn't expect any children under the mtl prim"
    ),

    -- Small test to validate the maxscript interface, more testing done in python.
    -- See options_serialization_test.py
    function test_serialize = (
        local exportOptions = USDExporter.CreateOptions()
        exportOptions.Meshes = false
        jsonString = exportOptions.Serialize()
        
        local readOptions = USDExporter.CreateOptionsFromJson(jsonString)
        assert_false readOptions.Meshes message:"Expected Meshes to be disabled after loading serialized options"		
    ),

    function teardown = (
    ),

    Tests = #(
        test_export_meshes_option,
        test_export_shapes_option,
        test_export_lights_option,
        test_export_cameras_option,
        test_export_skin_option,
        test_bake_object_offset_option,
        test_up_axis_option,
        test_ui_options_accessors,
        test_material_options_ripple,
        test_file_format_option,
        test_file_format_versus_extension,
        test_mtl_layer_path,
        test_log_options,
        test_preserve_edge_orientation_option,
        test_no_file_extension,
        test_file_extension_uppercase,
        test_normals_option,
        test_export_at_time,
        test_root_prim_path_option,
        test_export_hidden_objects_option,
        test_use_usd_visibility_option,
        test_open_in_usdview,
        test_mesh_format_option,
        test_usd_stages_as_references_option,
        test_all_material_conversions_option,
        test_allow_nested_gprims,
        test_use_last_resort_USDPreviewSurface_Writer,
        test_serialize
    )
)

runUsdTestFixture export_options_test (getThisScriptFilename())