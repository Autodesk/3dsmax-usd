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
pyUsdShade = python.import("pxr.UsdShade")
pySdf = python.import("pxr.Sdf")
pyGf = python.import("pxr.Gf")

filename_prefix = "SHELL_MTL_SHADER_WRITER_TEST_"
output_prefix = pathConfig.appendPath (getDir #temp) ("/" + filename_prefix)
deleteTestFiles(output_prefix + "*.*")

struct shell_mtl_shader_writer_test
(  
    public 
    function setup =
    (
        quickReset()
    ),

    -- Helper function to create a shell material with original and baked materials
    function create_shell_material shellName originalMatName bakedMatName =
    (
        local shellMtl = Shell_Material name:shellName
        
        -- Create original material (typically for viewport)
        local originalMtl = PhysicalMaterial name:originalMatName
        originalMtl.base_color = color 255 0 0  -- Red for original
        
        -- Create baked material (typically for render)
        local bakedMtl = PhysicalMaterial name:bakedMatName
        bakedMtl.base_color = color 0 255 0  -- Green for baked
        
        -- Assign materials to shell
        shellMtl.originalMaterial = originalMtl
        shellMtl.bakedMaterial = bakedMtl
        
        return shellMtl
    ),

    -- Helper function to validate material binding with purpose
    function validate_material_binding geomPrim expectedMatPath expectedPurpose =
    (
        local bindingAPI = pyUsdShade.MaterialBindingAPI(geomPrim)
        local boundMaterial = undefined
        
        if expectedPurpose != undefined then (
            -- Check purpose-specific binding
            local result = bindingAPI.GetDirectBinding(expectedPurpose)
            boundMaterial = result.GetMaterial()
        ) else (
            -- Check general binding
            local result = bindingAPI.GetDirectBinding()
            boundMaterial = result.GetMaterial()
        )
        if boundMaterial != undefined then (
            local matPrim = boundMaterial.GetPrim()
            local matPath = matPrim.GetPath()
            local expectedSdfpath = pySdf.Path(expectedMatPath)
            assert_equal expectedSdfpath matPath message:("Expected material path: " + expectedMatPath + ", got: " + matPath)
            return true
        ) else (
            assert_true false message:("No material binding found for purpose: " + (expectedPurpose as string))
            return false
        )
    ),

    function test_shell_material_export_baked_only = (
        -- Test exporting Shell Material with Baked export style
        local b = Box name:"TestBox"
        local shellMtl = create_shell_material "TestShell" "OriginalMat" "BakedMat"
        b.material = shellMtl
        
        local exportOptions = USDExporter.CreateOptions()
        exportOptions.FileFormat = #ascii
        exportOptions.RootPrimPath = "/"
        exportOptions.ShellMtlExportStyle = #baked
        
        local exportPath = output_prefix + "shell_baked_only.usda"
        USDExporter.ExportFile exportPath exportOptions:exportOptions
        
        local stage = pyUsd.Stage.Open(exportPath)
        
        -- Verify only the baked material was exported
        local bakedMatPrim = stage.GetPrimAtPath("/mtl/BakedMat")
        assert_true (bakedMatPrim.IsValid()) message:"Baked material should be exported."
        assert_true (bakedMatPrim.IsA(pyUsdShade.Material)) message:"Baked material should be a UsdShade.Material."
        
        -- Verify original material was NOT exported
        local originalMatPrim = stage.GetPrimAtPath("/mtl/OriginalMat")
        assert_false (originalMatPrim.IsValid()) message:"Original material should NOT be exported with #baked style."
        
        local shellMatPrim = stage.GetPrimAtPath("/mtl/TestShell")
        assert_false (shellMatPrim.IsValid()) message:"Shell material should not be exported."
        
        -- Verify geometry is bound to BakedMat material
        local boxPrim = stage.GetPrimAtPath("/TestBox")
        assert_true (boxPrim.IsValid()) message:"Box geometry should be exported."
        validate_material_binding boxPrim "/mtl/BakedMat" undefined
    ),

    function test_shell_material_export_original_only = (
        -- Test exporting Shell Material with Original export style
        local b = Box name:"TestBox"
        local shellMtl = create_shell_material "TestShell" "OriginalMat" "BakedMat"
        b.material = shellMtl
        
        local exportOptions = USDExporter.CreateOptions()
        exportOptions.FileFormat = #ascii
        exportOptions.RootPrimPath = "/"
        exportOptions.ShellMtlExportStyle = #original
        
        local exportPath = output_prefix + "shell_original_only.usda"
        USDExporter.ExportFile exportPath exportOptions:exportOptions
        
        local stage = pyUsd.Stage.Open(exportPath)
        
        -- Verify only the original material was exported
        local originalMatPrim = stage.GetPrimAtPath("/mtl/OriginalMat")
        assert_true (originalMatPrim.IsValid()) message:"Original material should be exported."
        assert_true (originalMatPrim.IsA(pyUsdShade.Material)) message:"Original material should be a UsdShade.Material."
        
        -- Verify baked material was NOT exported
        local bakedMatPrim = stage.GetPrimAtPath("/mtl/BakedMat")
        assert_false (bakedMatPrim.IsValid()) message:"Baked material should NOT be exported with #original style."
        
        local shellMatPrim = stage.GetPrimAtPath("/mtl/TestShell")
        assert_false (shellMatPrim.IsValid()) message:"Shell material should not be exported."
        
        -- Verify geometry is bound to OriginalMat material
        local boxPrim = stage.GetPrimAtPath("/TestBox")
        assert_true (boxPrim.IsValid()) message:"Box geometry should be exported."
        validate_material_binding boxPrim "/mtl/OriginalMat" undefined
    ),

    function test_shell_material_export_both = (
        -- Test exporting Shell Material with Both export style (with USD purposes)
        local b = Box name:"TestBox"
        local shellMtl = create_shell_material "TestShell" "OriginalMat" "BakedMat"
        b.material = shellMtl
        
        local exportOptions = USDExporter.CreateOptions()
        exportOptions.FileFormat = #ascii
        exportOptions.RootPrimPath = "/"
        exportOptions.ShellMtlExportStyle = #both
        
        local exportPath = output_prefix + "shell_both_materials.usda"
        USDExporter.ExportFile exportPath exportOptions:exportOptions
        
        local stage = pyUsd.Stage.Open(exportPath)
        
        -- Verify both materials were exported
        local originalMatPrim = stage.GetPrimAtPath("/mtl/OriginalMat")
        assert_true (originalMatPrim.IsValid()) message:"Original material should be exported."
        assert_true (originalMatPrim.IsA(pyUsdShade.Material)) message:"Original material should be a UsdShade.Material."
        
        local bakedMatPrim = stage.GetPrimAtPath("/mtl/BakedMat")
        assert_true (bakedMatPrim.IsValid()) message:"Baked material should be exported."
        assert_true (bakedMatPrim.IsA(pyUsdShade.Material)) message:"Baked material should be a UsdShade.Material."
        
        -- Verify geometry has purpose-specific material bindings
        local boxPrim = stage.GetPrimAtPath("/TestBox")
        assert_true (boxPrim.IsValid()) message:"Box geometry should be exported."
        
        -- Check for full purpose binding (original material for render)
        validate_material_binding boxPrim "/mtl/OriginalMat" pyUsdShade.Tokens.full
        
        -- Check for preview purpose binding (baked material for viewport)
        validate_material_binding boxPrim "/mtl/BakedMat" pyUsdShade.Tokens.preview
    ),

    function test_shell_material_empty = (
        -- Test Shell Material with no materials assigned
        local b = Box name:"TestBox"
        local shellMtl = Shell_Material name:"EmptyShell"
        -- Leave both materials empty
        b.material = shellMtl
        
        local exportOptions = USDExporter.CreateOptions()
        exportOptions.FileFormat = #ascii
        exportOptions.RootPrimPath = "/"
        exportOptions.ShellMtlExportStyle = #both
        
        local exportPath = output_prefix + "shell_empty.usda"
        USDExporter.ExportFile exportPath exportOptions:exportOptions
        
        -- Just making sure we didn't crash...
    ),

    function test_shell_material_with_multi_objects = (
        -- Test Shell Material assigned to multiple objects
        local box1 = Box name:"Box1" pos:[0,0,0]
        local box2 = Box name:"Box2" pos:[10,0,0]
        local s = Sphere name:"Sphere1" pos:[20,0,0]
        
        local shellMtl = create_shell_material "MultiObjShell" "OriginalMat" "BakedMat"
        
        -- Assign shell material to all objects
        box1.material = shellMtl
        box2.material = shellMtl
        s.material = shellMtl
        
        local exportOptions = USDExporter.CreateOptions()
        exportOptions.FileFormat = #ascii
        exportOptions.RootPrimPath = "/"
        exportOptions.ShellMtlExportStyle = #both
        
        local exportPath = output_prefix + "shell_multi_objects.usda"
        USDExporter.ExportFile exportPath exportOptions:exportOptions
        
        local stage = pyUsd.Stage.Open(exportPath)
        
        -- Verify both materials were exported
        local originalMatPrim = stage.GetPrimAtPath("/mtl/OriginalMat")
        assert_true (originalMatPrim.IsValid()) message:"Original material should be exported."
        
        local bakedMatPrim = stage.GetPrimAtPath("/mtl/BakedMat")
        assert_true (bakedMatPrim.IsValid()) message:"Baked material should be exported."
        
        -- Verify all objects have purpose-specific bindings
        local objects = #("Box1", "Box2", "Sphere1")
        for objName in objects do (
            local objPrim = stage.GetPrimAtPath("/" + objName)
            assert_true (objPrim.IsValid()) message:(objName + " should be exported.")
            
            -- Check full purpose binding (original material)
            validate_material_binding objPrim "/mtl/OriginalMat" pyUsdShade.Tokens.full
            
            -- Check preview purpose binding (baked material)
            validate_material_binding objPrim "/mtl/BakedMat" pyUsdShade.Tokens.preview
        )
    ),

    function test_shell_material_option_validation = (
        -- Test that Shell Material export style option validation works
        local exportOptions = USDExporter.CreateOptions()
        
        -- Test valid values
        exportOptions.ShellMtlExportStyle = #baked
        assert_equal #baked exportOptions.ShellMtlExportStyle message:"Should accept #baked value."
        
        exportOptions.ShellMtlExportStyle = #original
        assert_equal #original exportOptions.ShellMtlExportStyle message:"Should accept #original value."
        
        exportOptions.ShellMtlExportStyle = #both
        assert_equal #both exportOptions.ShellMtlExportStyle message:"Should accept #both value."
        
        -- Test invalid value throws exception
        local exceptionThrown = false
        try (
            exportOptions.ShellMtlExportStyle = #invalid
        )
        catch (
            exceptionThrown = true
        )
        assert_true exceptionThrown message:"Should throw exception for invalid Shell Material export style value."
    ),

    function test_shell_material_with_nested_materials = (
        -- Test Shell Material where sub-materials are complex (e.g., Multi/Sub-Object)
        local b = Box name:"TestBox"
        local shellMtl = Shell_Material name:"ComplexShell"
        
        -- Create a Multi/Sub-Object material as the baked material
        local multiMtl = Multimaterial name:"BakedMulti"
        multiMtl.materialList[1] = PhysicalMaterial name:"SubMat1"
        multiMtl.materialList[1].base_color = color 255 0 0
        multiMtl.materialList[2] = PhysicalMaterial name:"SubMat2"
        multiMtl.materialList[2].base_color = color 0 255 0
        multiMtl.count = 2
        
        -- Simple material as original
        local originalMtl = PhysicalMaterial name:"SimpleOriginal"
        originalMtl.base_color = color 0 0 255
        
        shellMtl.originalMaterial = originalMtl
        shellMtl.bakedMaterial = multiMtl
        b.material = shellMtl
        
        local exportOptions = USDExporter.CreateOptions()
        exportOptions.FileFormat = #ascii
        exportOptions.RootPrimPath = "/"
        exportOptions.ShellMtlExportStyle = #both
        
        local exportPath = output_prefix + "shell_nested_materials.usda"
        USDExporter.ExportFile exportPath exportOptions:exportOptions
        
        local stage = pyUsd.Stage.Open(exportPath)
        
        -- Verify all materials were exported
        local originalMatPrim = stage.GetPrimAtPath("/mtl/SimpleOriginal")
        assert_true (originalMatPrim.IsValid()) message:"Original material should be exported."
        
        local multiMatPrim = stage.GetPrimAtPath("/mtl/BakedMulti")
        assert_false (multiMatPrim.IsValid()) message:"Multi/Sub-Object baked material should not be exported."
        
        local subMat1Prim = stage.GetPrimAtPath("/mtl/SubMat1")
        assert_true (subMat1Prim.IsValid()) message:"Sub-material 1 should be exported."
        
        local subMat2Prim = stage.GetPrimAtPath("/mtl/SubMat2")
        assert_true (subMat2Prim.IsValid()) message:"Sub-material 2 should be exported."
        
        local shellMatPrim = stage.GetPrimAtPath("/mtl/ComplexShell")
        assert_true (shellMatPrim.IsValid()) message:"Shell material should be exported. It holds the mapping for Multisubs bindings."
        
        -- Verify GeomSubset bindings with purposes
        local boxPrim = stage.GetPrimAtPath("/TestBox")
        assert_true (boxPrim.IsValid()) message:"Box geometry should be exported."
        
        -- Check that main geometry has no direct binding (should be None)
        local bindingAPI = pyUsdShade.MaterialBindingAPI(boxPrim)
        local directBinding = bindingAPI.GetDirectBinding()
        local boundMaterial = directBinding.GetMaterial()
		local boundPrim = boundMaterial.GetPrim()
        assert_false (boundPrim.IsValid()) message:"Main geometry should have no direct material binding."
        
        -- Verify GeomSubsets have purpose-specific bindings
        local subsetNames = #("_1_", "_2_", "_3_", "_4_", "_5_", "_6_")
        local expectedMatIDs = #(1, 2, 3, 4, 5, 6)  -- Based on face material ID assignments
        
        for i = 1 to subsetNames.count do (
            local subsetName = subsetNames[i]
            local expectedMatID = expectedMatIDs[i]
            local subsetPath = "/TestBox/" + subsetName
            local subsetPrim = stage.GetPrimAtPath(subsetPath)
            
            assert_true (subsetPrim.IsValid()) message:("GeomSubset " + subsetName + " should exist.")
            assert_true (subsetPrim.IsA(pyUsdGeom.Subset)) message:("GeomSubset " + subsetName + " should be a UsdGeomSubset.")
            
            local subsetBindingAPI = pyUsdShade.MaterialBindingAPI(subsetPrim)
            
            -- Check full purpose binding (original material)
            local fullBinding = subsetBindingAPI.GetDirectBinding(pyUsdShade.Tokens.full)
            local fullMaterial = fullBinding.GetMaterial()
			local fullPrim = fullMaterial.GetPrim()
            if fullPrim.IsValid() then (
                local fullMatPath = fullPrim.GetPath()
                local expectedFullPath = pySdf.Path("/mtl/ComplexShell/ComplexShell_Original_Set_1_MatID_" + expectedMatID as string)
                assert_equal expectedFullPath fullMatPath message:("GeomSubset " + subsetName + " should have correct full purpose binding to MatID " + expectedMatID as string + ".")
            ) else (
                assert_true false message:("GeomSubset " + subsetName + " should have full purpose binding.")
            )
            
            -- Check preview purpose binding (baked material)
            local previewBinding = subsetBindingAPI.GetDirectBinding(pyUsdShade.Tokens.preview)
            local previewMaterial = previewBinding.GetMaterial()
			local previewPrim = previewMaterial.GetPrim()
            if previewPrim.IsValid() then (
                local previewMatPath = previewPrim.GetPath()
                local expectedPreviewPath = pySdf.Path("/mtl/ComplexShell/ComplexShell_Baked_Set_1_MatID_" + expectedMatID as string)
                assert_equal expectedPreviewPath previewMatPath message:("GeomSubset " + subsetName + " should have correct preview purpose binding to MatID " + expectedMatID as string + ".")
            ) else (
                assert_true false message:("GeomSubset " + subsetName + " should have preview purpose binding.")
            )
        )
    ),

    function teardown = (
        -- Clean up any test files
        deleteTestFiles(output_prefix + "*.*")
    ),

    Tests = #(
        test_shell_material_export_baked_only,
        test_shell_material_export_original_only,
        test_shell_material_export_both,
        test_shell_material_empty,
        test_shell_material_with_multi_objects,
        test_shell_material_option_validation,
        test_shell_material_with_nested_materials
    )
)

runUsdTestFixture shell_mtl_shader_writer_test (getThisScriptFilename())
