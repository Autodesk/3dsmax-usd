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

pyUsd = python.import("pxr.Usd")
pyUsdUtils = python.import("pxr.UsdUtils")
pyUsdShade = python.import("pxr.UsdShade")
pyGf = python.import("pxr.Gf")

output_prefix = pathConfig.appendPath (getDir #temp) "/CREATE_REFERENCE_TEST_"
deleteTestFiles(output_prefix + "*.*")

struct create_reference_test
(  
    private 
    initialUnits,
    initialScale,
    
    public
    function setup =
    (
        initialUnits = units.SystemType
        initialScale = units.SystemScale        
        resetMaxFile #noprompt
    ),
    
    function test_set_root_layer = (
        -- Create a usd file to reference in.
        box()
        local exportPath = output_prefix + "box.usd"
        local exportOptions = USDExporter.CreateOptions()
        exportOptions.FileFormat = #ascii
        USDExporter.ExportFile exportPath exportOptions:exportOptions
        resetMaxFile #noprompt
        
        local usdReference = USDStageObject()
        assert_defined usdReference

        -- test SetRootLayer default stageMask
        usdReference.SetRootLayer exportPath
        assert_equal exportPath usdReference.FilePath message:"FilePath should be set on the stage object."
        assert_equal "/" usdReference.StageMask message:"StageMask should be set on the stage object to the root '/'."
        
        -- test SetRootLayer with specified stageMask
        local testStageMask = "/something"
        usdReference.SetRootLayer exportPath stageMask:testStageMask
        assert_equal exportPath usdReference.FilePath message:"FilePath should be set on the stage object."
        assert_equal testStageMask usdReference.StageMask message:"StageMask should be set on the stage object."
        assert_equal (Box3 [-6.09115,-8.86275,0] [6.09115,8.86275,0]) usdReference.boundingbox message:"Bounding box of usdReference should be the bounding box of the stage"
    ),
    
    function test_cache_id = (
        if (USDStageObject == undefined) do (
            print("Skipping create_reference_test.test() as MaxUsdObjects.dlo plugin not available.")
            return -1
        )

        -- Create a usd file to reference in.
        box()
        local exportPath = output_prefix + "box.usd"
        local exportOptions = USDExporter.CreateOptions()
        USDExporter.ExportFile exportPath exportOptions:exportOptions
        resetMaxFile #noprompt
        
        -- Test that a cache id is assigned correctly with a initialized stage.        
        local usdReference = USDStageObject()
        assert_equal -1 usdReference.CacheId message:"An unitialized stage object should hold the default/invalid cache id."
        try (
            usdReference.SetRootLayer ""
        )
        catch 
        (
            -- catch exception and continue with testing the the cacheId 
        )
        assert_equal -1 usdReference.CacheId message:"An unitialized stage object should hold the default/invalid cache id."
        usdReference.SetRootLayer exportPath
        assert_not_equal -1 usdReference.CacheId message:"CacheId should be set on the stage object."
        
        -- Test cache ids with multiple stages.
        local usdReference2 = USDStageObject()
        usdReference2.SetRootLayer exportPath
        assert_not_equal -1 usdReference2.CacheId message:"CacheId should be set on the stage object."
        local usdReference3 = USDStageObject()
        usdReference3.SetRootLayer exportPath
        assert_not_equal -1 usdReference3.CacheId message:"CacheId should be set on the stage object."
        assert_not_equal usdReference2.CacheId usdReference3.CacheId message:"The assigned stage cache ids should be unique."
        
        -- Test SetRootLayer stageMask valid layer + valid mask
        usdReference3.SetRootLayer exportPath stageMask:"/"
        assert_not_equal -1 usdReference3.CacheId message:"CacheId should be set on the stage object."

        -- Test SetRootLayer stageMask valid layer + invalid mask
        usdReference3.SetRootLayer exportPath stageMask:"/bad"
        assert_not_equal -1 usdReference3.CacheId message:"CacheId should be set on the stage object."
        
        -- Test SetRootLayer stageMask invalid layer + valid mask
        local badExportPath = output_prefix + "bad.usd"
        usdReference3.SetRootLayer badExportPath stageMask:"/"
        assert_equal -1 usdReference3.CacheId message:"An unitialized stage object should hold the default/invalid cache id."

        -- Test SetRootLayer stageMask invalid layer + invalid mask
        usdReference3.SetRootLayer badExportPath stageMask:"/bad"
        assert_equal -1 usdReference3.CacheId message:"An unitialized stage object should hold the default/invalid cache id."

        -- test SetRootLayer same data, same cacheId
        usdReference3.SetRootLayer exportPath stageMask:"/"
        local cacheIdFirst = usdReference3.CacheId
        usdReference3.SetRootLayer exportPath stageMask:"/"
        local cacheIdSecond = usdReference3.CacheId
        assert_equal cacheIdFirst cacheIdSecond message:"CacheId value should be the same if setting same data for SetRootLayer calls."
    ),

    function test_icon_display = (
        if (USDStageObject == undefined) do (
            print("Skipping create_reference_test.test() as MaxUsdObjects.dlo plugin not available.")
            return -1
        )

        -- Create a usd file to reference in.
        box pos:[10,10,10]
        local exportPath = output_prefix + "boxIcon.usd"
        local exportOptions = USDExporter.CreateOptions()
        USDExporter.ExportFile exportPath exportOptions:exportOptions
        resetMaxFile #noprompt

        -- Test that an empty stage has an icon by default.        
        local usdReference = USDStageObject()
        assert_true usdReference.showIcon message:"Icon should be on by default"
        assert_equal (Box3 [-6.09115,-8.86275,0] [6.09115,8.86275,0]) usdReference.boundingbox message:"Bounding box of empty stage should be the bounding box of the icon"
        usdReference.iconScale = 2
        assert_equal (Box3 [-12.1823,-17.7255,0] [12.1823,17.7255,0]) usdReference.boundingbox message:"Bounding box of empty stage should be the bounding box of the icon"
        usdReference.showIcon = false
        assert_equal (Box3 [1e+30,1e+30,1e+30] [-1e+30,-1e+30,-1e+30]) usdReference.boundingbox message:"Bounding box of empty stage with a disabled icon should be an invalid bounding box"

        usdReference.SetRootLayer exportPath
        assert_equal (Box3 [-2.5,-2.5,10] [22.5,22.5,35]) usdReference.boundingbox message:"Bounding box of should be the bounding box of the stage"
        usdReference.showIcon = true
        assert_equal (Box3 [-12.1823,-17.7255,0] [22.5,22.5,35]) usdReference.boundingbox message:"Bounding box of should be the bounding box of the stage and the icon"
    ),
    
    -- Test various behaviors dealing with the UsdPreviewSurface materials which can be generated for the purpose 
    -- of offline rendering.
    function test_usdpreviewsurface_assignment = (
        -- Create a stage, using a root layer which hold some materials.
        local stageObject = USDStageObject name:"stageFright"        
        local testDataDir = getFilenamePath (getThisScriptFilename())
        local sampleFile = (testDataDir + @"data\stage_get_usd_materials.usda")
        stageObject.SetRootLayer sampleFile
           
        -- Make sure the multi-sub carrying UsdPreviewSurfaceMaterials is generated/returned via GetUsdPreviewSurfaceMaterials().
        local multiSub = stageObject.GetUsdPreviewSurfaceMaterials()
        assert_defined multiSub
        assert_undefined stageObject.material -- We only return the material, should still be unassigned on the node.
        assert_equal (#("/stage_get_usd_materials/Materials/Material__25", "displayColor", "/stage_get_usd_materials/Materials/Material__26", "/stage_get_usd_materials/Materials/Material__38") as string) (multiSub.names as string)
    
        local stageCache = pyUsdUtils.StageCache.Get()
        local stage = stageCache.Find(pyUsd.StageCache.Id.FromLongInt(stageObject.cacheId))
        
        -- Validate the diffuseColor of the material, just to make sure the multi-sub is populated correctly.
        local shaderPrim = stage.GetPrimAtPath("/stage_get_usd_materials/Materials/Material__25/Material__25")
        local usdPreviewSurface = pyUsdShade.Shader(shaderPrim)
        local diffuseColorInput = usdPreviewSurface.GetInput("diffuseColor")
        local usdDiffuseColor = diffuseColorInput.Get()
                    
        local maxDiffuseColor = multiSub.materialList[1].diffusecolor
        local r = (maxDiffuseColor.r/255)
        local g = (maxDiffuseColor.g/255)    
        local b = (maxDiffuseColor.b/255)
        assert_point3_equal (Point3 usdDiffuseColor[1] usdDiffuseColor[2] usdDiffuseColor[3]) (Point3  r g b)
        
        -- Now modify the source data in USD, and verify that the 3dsMax material is updated accordingly.
        local usdDiffuseColor = pyGf.Vec3f 0 0 1
        diffuseColorInput.Set(usdDiffuseColor)
        
        -- Force refresh of the fallback material.
        stageObject.GetUsdPreviewSurfaceMaterials()
        
        local maxDiffuseColor = multiSub.materialList[1].diffusecolor
        local r = (maxDiffuseColor.r/255)
        local g = (maxDiffuseColor.g/255)    
        local b = (maxDiffuseColor.b/255)
        assert_point3_equal (Point3 usdDiffuseColor[1] usdDiffuseColor[2] usdDiffuseColor[3]) (Point3  r g b)
        
        -- Override the first material.
        local overrideMtl = MaxUsdPreviewSurface()
        overrideMtl.diffuseColor = green
        local originalUsdMtl = multiSub.materialList[1]        
        multiSub.materialList[1] = overrideMtl
        
        -- Make sure calling GetUsdPreviewSurfaceMaterials with sync = false  doesnt wipe the overriden material.
        local multiSub2 = stageObject.GetUsdPreviewSurfaceMaterials sync:false
        assert_equal multiSub2.materialList[1] overrideMtl
        assert_equal multiSub multiSub2 -- still the same multimaterial
        
        -- With sync = true, the multimaterial is fully updated from the usd source, wiping any overrides.
        local multiSub3 = stageObject.GetUsdPreviewSurfaceMaterials sync:true
        assert_equal multiSub3.materialList[1] originalUsdMtl
        assert_equal multiSub multiSub3 -- still the same material
        
        -- Next, test that the UsdPreviewSurface materials are reconnected to their usd source when loaded from disk.
        -- Apply the multi-sub to the node and save the scene...
        stageObject.material = multiSub
        local maxSceneSavePath = output_prefix + "usdPreviewSurfaceAssigned.max"
        saveMaxFile maxSceneSavePath quiet:true
        
        -- Load the scene from disk...
        loadMaxFile maxSceneSavePath
        local loadedStageObject = $stageFright
                
        -- Modify some property of the source material and make validate that our material changed as well, 
        -- proving that the connection to the source data was made.
        local stage = stageCache.Find(pyUsd.StageCache.Id.FromLongInt(loadedStageObject.cacheId))
        local shaderPrim = stage.GetPrimAtPath("/stage_get_usd_materials/Materials/Material__25/Material__25")
        local usdPreviewSurface = pyUsdShade.Shader(shaderPrim)
        local diffuseColorInput = usdPreviewSurface.GetInput("diffuseColor")
        local usdDiffuseColor = pyGf.Vec3f 1 0 1
            
        diffuseColorInput.Set(usdDiffuseColor)
        
        -- Force refresh of the fallback material.
        loadedStageObject.GetUsdPreviewSurfaceMaterials()
            
        local r = loadedStageObject.material.materialList[1].diffuseColor.r/255
        local g = loadedStageObject.material.materialList[1].diffuseColor.g/255
        local b = loadedStageObject.material.materialList[1].diffuseColor.b/255
        assert_point3_equal (Point3 usdDiffuseColor[1] usdDiffuseColor[2] usdDiffuseColor[3]) (Point3  r g b)
        loadedStageObject.material.materialList[1].diffuseColor        
    ),
    
    -- Tests the fix for https://jira.autodesk.com/browse/EMSUSD-585
    -- The issue was that on load, when we attempt to "reconnect" the UsdPreviewSurface
    -- materials to the source data, we did not expect that a user may have emptied
    -- out of slot of the submtl, and were then crashing on a nullptr.
    function test_empty_submtl_crash_fix = (
        -- Create a stage, using a root layer which hold some materials.
        local stageObject = USDStageObject name:"stageFright"        
        local testDataDir = getFilenamePath (getThisScriptFilename())
        local sampleFile = (testDataDir + @"data\stage_get_usd_materials.usda")
        stageObject.SetRootLayer sampleFile
           
        local multiSub = stageObject.GetUsdPreviewSurfaceMaterials()
        stageObject.material = multiSub
        
        -- Set the first submtl empty
        multiSub.materialList[1] = undefined
        
        local maxSceneSavePath = output_prefix + "test_empty_submtl_crash_fix.max"
        saveMaxFile maxSceneSavePath quiet:true
        
        -- Load the scene from disk...
        loadMaxFile maxSceneSavePath
        local loadedStageObject = $stageFright
        
        assert_undefined loadedStageObject.material.materialList[1]
        assert_defined loadedStageObject.material.materialList[2]
    ),
    
    function test_computed_bounding_box = (
        -- Make sure any current unit setup will not impact the test.
        units.SystemType = #Centimeters
        units.SystemScale = 1
        
        local stageObject = USDStageObject name:"movingCube"
        
        local testDataDir = getFilenamePath (getThisScriptFilename())
        local sampleFile = (testDataDir + @"data\animated_cube_bbox.usda")
        stageObject.SetRootLayer sampleFile
            
        -- Disable the icon for now, it influences the bounding box.
        stageObject.ShowIcon = false
        
        -- The scene is a 10x10 box, centered in x and y.
        
        -- Test at time 0, using the original anim range: 
        at time 0 assert_equal [-5,-5,0] stageObject.min
        at time 0 assert_equal [5,5,10] stageObject.max
        
        -- Test at time 50, using the original anim range (the box moved up 10 units).
        at time 50 assert_equal [-5,-5,10] stageObject.min
        at time 50 assert_equal [5,5,20] stageObject.max
        
        -- Setup the timeline so that the new start is at frame 50.
        stageObject.AnimationMode = 1 -- CustomStartAndSpeed
        stageObject.CustomAnimationStartFrame = 50
        
        -- This tests both that we are getting the bbox at the right time, but also that any
        -- cached value for that frame was properly cleared.
        at time 50 assert_equal [-5,-5,0] stageObject.min
        at time 50 assert_equal [5,5,10] stageObject.max
        
        -- Re-enable the icon, make sure it is included in the bounding box appropriately.
        -- Notice only X and Y dimensions are impacted, as the icon is "flat".
        stageObject.ShowIcon = true
        at time 50 assert_equal [-6.09115,-8.86275,0] stageObject.min
        at time 50 assert_equal [6.09115,8.86275,10] stageObject.max

        -- Scale the icon and make sure the bounding box is still correct.
        stageObject.IconScale = 2.0
        at time 50 assert_equal [-12.1823,-17.7255,0] stageObject.min
        at time 50 assert_equal [12.1823,17.7255,10] stageObject.max
    ),    

    function test_primvar_mapping_api = (
        -- Create a stage, using a root layer which hold some materials.
        local stageObject = USDStageObject name:"stageFright"        
        local testDataDir = getFilenamePath (getThisScriptFilename())
        local sampleFile = (testDataDir + @"data\unmappedpv.usda")
        stageObject.SetRootLayer sampleFile
        
        -- Code used under the hood is the same as for import options, no need to 
        -- test the defaults precisely. High level is enough.
        local allMapped = stageObject.GetMappedPrimvars()
        assert_equal 500 allMapped.count
        assert_equal "displayOpacity" allMapped[1]
        
        -- Test modifying mapping from existing
        assert_equal 1 (stageObject.GetPrimvarChannel "st")
        stageObject.SetPrimvarChannelMapping "st" 50
        assert_equal 50 (stageObject.GetPrimvarChannel "st")
        stageObject.SetPrimvarChannelMapping "st" undefined
        assert_equal undefined (stageObject.GetPrimvarChannel "st")
        
        -- Add a completely new mapping
        assert_equal undefined (stageObject.GetPrimvarChannel "aye")
        stageObject.SetPrimvarChannelMapping "aye" 25
        assert_equal 25 (stageObject.GetPrimvarChannel "aye")
        
        -- Test 'IsMappedPrimvar()'
        assert_true (stageObject.IsMappedPrimvar "aye")
        assert_false (stageObject.IsMappedPrimvar "ney")
        
        -- So now we have one more total!
        allMapped = stageObject.GetMappedPrimvars()
        assert_equal 501 allMapped.count
        
        -- Test clear all mappings.
        stageObject.ClearMappedPrimvars()
        allMapped = stageObject.GetMappedPrimvars()
        assert_equal 0 allMapped.count
        
        -- Reset defaults...
        stageObject.SetPrimvarChannelMappingDefaults()
        local allMapped = stageObject.GetMappedPrimvars()
        assert_equal 500 allMapped.count
        assert_equal "displayOpacity" allMapped[1]
    ),
    
    function test_primvar_mapping_texture_channel = (
        -- Create a stage, using a root layer which holds some materials.
        local stageObject = USDStageObject name:"stageFright"        
        local testDataDir = getFilenamePath (getThisScriptFilename())
        local sampleFile = (testDataDir + @"data\unmappedpv.usda")
        stageObject.SetRootLayer sampleFile
            
        local mat = stageObject.GetUsdPreviewSurfaceMaterials sync:true
        
        -- The geometry in the stage uses two primvars, "foo" and "bar".
        -- By default, there are no mappings for these, so the generated textures will 
        -- default to using channel 1.
            
        -- The first material has an identity texture transform, will load as BitmapTexture.
        assert_equal 1 (mat.materiallist[1].diffusecolor_map.coords.mapChannel)
        -- The first material has a non-identity texture transform, will load an OSL Uberbitmap.
        assert_equal 1 (mat.materiallist[2].diffusecolor_map.sourceMap.UVSet)
        
        -- Now properly map those primvars to some max channels.    
        stageObject.SetPrimvarChannelMapping "foo" 5
        stageObject.SetPrimvarChannelMapping "bar" 10
        
        -- Materials are updated on redraw.
        ForceCompleteRedraw()
            
        local mat = stageObject.GetUsdPreviewSurfaceMaterials sync:true
        assert_equal 5 (mat.materiallist[1].diffusecolor_map.coords.mapChannel)
        assert_equal 10 (mat.materiallist[2].diffusecolor_map.sourceMap.UVSet)
        
        -- Change the mapping.
        stageObject.SetPrimvarChannelMapping "foo" 66
        stageObject.SetPrimvarChannelMapping "bar" 77
        
        -- Force the material to update...
        ForceCompleteRedraw()
        
        -- Validate!
        local mat = stageObject.GetUsdPreviewSurfaceMaterials sync:true
        assert_equal 66 (mat.materiallist[1].diffusecolor_map.coords.mapChannel)
        assert_equal 77 (mat.materiallist[2].diffusecolor_map.sourceMap.UVSet)
    ),
    
    function test_save_load_of_primvar_mapping = (
        -- Create a stage, using a root layer which holds some materials.
        local stageObject = USDStageObject name:"stageFright"        
        local testDataDir = getFilenamePath (getThisScriptFilename())
        local sampleFile = (testDataDir + @"data\unmappedpv.usda")
        stageObject.SetRootLayer sampleFile
            
        -- Setup some primvar mapping that we will save to the .max     
        stageObject.ClearMappedPrimvars()
        stageObject.SetPrimvarChannelMapping "foo" 5
        stageObject.SetPrimvarChannelMapping "bar" 10
        stageObject.SetPrimvarChannelMapping "baz" undefined -- explicit mapping to nothing
    
        local maxSceneSavePath = output_prefix + "test_save_load_of_primvar_mapping.max"
        saveMaxFile maxSceneSavePath quiet:true
                    
        -- Load the scene from disk...
        loadMaxFile maxSceneSavePath
        local loadedStageObject = $stageFright
        
        -- Make sure the primvar mapping is as we saved it.
        local allMapped = loadedStageObject.GetMappedPrimvars()
        assert_array_equal #("bar", "baz", "foo") allMapped
        
        assert_equal 5 (loadedStageObject.GetPrimvarChannel "foo")
        assert_equal 10 (loadedStageObject.GetPrimvarChannel "bar")
        assert_equal undefined (loadedStageObject.GetPrimvarChannel "baz")
    ),
    
    -- Simple test that load a USD file containing a materialX reference.
    -- This caused the stage loading to hang Max prior to fix : MAXX-73770
    -- So nothing to compare here really, just testing the test can finish
    function test_materialX_reference = (

        local usdReference = USDStageObject()
        assert_defined usdReference
        
        local testDataDir = getFilenamePath (getThisScriptFilename())
        local sampleMtlXFile = (testDataDir + @"data\simpleMtlxRef.usda")
        
        usdReference.SetRootLayer sampleMtlXFile
    ),
    
    function test_copied_stage_loaded = (
        -- Make sure any current unit setup will not impact the test.
        units.SystemType = #Centimeters
        units.SystemScale = 1
        
        local stageObject = USDStageObject name:"movingCube"
        
        local testDataDir = getFilenamePath (getThisScriptFilename())
        local sampleFile = (testDataDir + @"data\animated_cube_bbox.usda")
        stageObject.SetRootLayer sampleFile
            
        -- Disable the icon for now, it influences the bounding box.
        stageObject.ShowIcon = false
        
        -- The scene is a 10x10 box, centered in x and y.
        
        assert_equal [-5,-5,0] stageObject.min
        assert_equal [5,5,10] stageObject.max
        
        -- copy the stage
        stageCopy = copy stageObject
        stageCopy.position = [10,10,10]
        
        assert_equal [5,5,10] stageCopy.min
        assert_equal [15,15,20] stageCopy.max        
    ),        
    
    function teardown = (
        units.SystemType = initialUnits
        units.SystemScale = initialScale
    ),

    Tests = #(test_set_root_layer,
        test_cache_id,
        test_icon_display,
        test_usdpreviewsurface_assignment,
        test_computed_bounding_box,
        test_primvar_mapping_api,
        test_primvar_mapping_texture_channel,
        test_save_load_of_primvar_mapping,
        test_materialX_reference,
        test_copied_stage_loaded,
        test_empty_submtl_crash_fix
        )
)

runUsdTestFixture create_reference_test(getThisScriptFilename())