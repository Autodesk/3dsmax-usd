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
pyUsdLux = python.import("pxr.UsdLux")
pyGf = python.import("pxr.Gf")

output_prefix = pathConfig.appendPath (getDir #temp) "/EXPORT_USD_STAGE_AS_REFERENCE_TEST_"
deleteTestFiles(output_prefix + "*.*")

struct export_usd_stage_as_reference_test
(  
    private 
    exportOptions = USDExporter.CreateOptions(),
    initialUnits, 
    initialScale,
    
    public 
    function setup =
    (
        resetmaxfile #noprompt
        exportOptions.FileFormat = #ascii
        
        initialUnits = units.SystemType
        initialScale = units.SystemScale
    
        units.SystemType = #inches
        units.SystemScale = 1.0
    ),

    -- Simple case, stage has the same axis/unis and a single prim at the root, set as default Prim.
    function export_stage_as_reference_test = (
        -- Create a simple layer to bring in as a USD stage.
        local stage = pyUsd.Stage.CreateInMemory()     
        local sphere = pyUsdGeom.Sphere.Define stage "/sphere"
        stage.SetDefaultPrim (sphere.GetPrim())
        
        --  Set up units so that we dont need to adjust anything when referencing this stage.
        pyUsdGeom.SetStageMetersPerUnit stage 0.0254
        pyUsdGeom.SetStageUpAxis stage "Z"
            
        local layerName = "simple_layer_sphere"
        local layerPath = output_prefix + layerName + ".usda"
        stage.Export layerPath
        
        -- Create a USD Stage Object from the layer.
        stage = UsdStageObject name:"UsdStageObject"
        stage.SetRootLayer layerPath
            
        -- Export the scene to USD.
        local exportOptions = USDExporter.CreateOptions()
        exportOptions.FileFormat = #ascii
        exportOptions.RootPrimPath = "/foo" -- For simplicity.
        exportOptions.UsdStagesAsReferences = true    
            
        local exportPath = output_prefix + "export_stage_as_reference_test.usd"
        USDExporter.ExportFile exportPath exportOptions:exportOptions
            
        -- Validate that the reference was created as expected.
        local exporterLayerStage = pyUsd.Stage.Open exportPath
        
        local sphereRef = exporterLayerStage.GetPrimAtPath (exportOptions.RootPrimPath + "/" + stage.name + "/" + (sphere.GetPrim()).GetName())
        assert_true (sphereRef.IsValid())
        assert_true (sphereRef.IsA(pyUsdGeom.Sphere))
            
        xformable = pyUsdGeom.Xformable(sphereRef)
        assert_false ((xformable.GetXformOpOrderAttr()).HasAuthoredValue())
            
        -- Make sure the reference is using a relative path. We look at the ascii directly as 
        -- there is no way from the API. Path should be using forward slashes, posix style, so that 
        -- it works on mac & linux.
        local foundRelativeRef = false
        fs = openFile exportPath
        while (not eof fs and not foundRelativeRef) do(
            fileLine = readline fs
            if (fileLine != "" ) do (
                foundRelativeRef  = ((findString fileLine ("prepend references = @./"+ (filenameFromPath layerPath))) != undefined)
            )            
        )
        close fs
        assert_true foundRelativeRef message:"Reference was not setup using a relative path"
    ),
    
    -- Test that a USD Stage pointing to a bad/missing usd layer, behaves correctly.
    function export_null_stage_as_ref = (
        -- Create a USD file to reference.
        stage = UsdStageObject name:"UsdStageObject"
        stage.SetRootLayer  "C:/does/not/exist.usda"
        
        local exportOptions = USDExporter.CreateOptions()
        exportOptions.FileFormat = #ascii
        exportOptions.RootPrimPath = "/foo" -- For simplicity.
        exportOptions.UsdStagesAsReferences = true
        
        -- Run the export.
        local exportPath = output_prefix + "export_null_stage_as_ref.usd"
        USDExporter.ExportFile exportPath exportOptions:exportOptions
        
        local exporterLayerStage = pyUsd.Stage.Open exportPath
        
        -- A prim is created for the Stage object, but it didnt reference anything.
        local stageObject = exporterLayerStage.GetPrimAtPath (exportOptions.RootPrimPath + "/" + stage.name)
        assert_true (stageObject.IsValid())            
        assert_equal 0 ((stageObject.GetChildren()).Count)
    ),
    
    -- Test that a USD Stage Object with a layer that has no suitable prims for referencing is not exported as a reference.
    function export_stage_no_target_prim = (
        -- Create an empty layer.
        local stage = pyUsd.Stage.CreateInMemory()     
        
        --  Set up units so that we dont need to adjust anything when referencing this stage.
        pyUsdGeom.SetStageMetersPerUnit stage 0.0254
        pyUsdGeom.SetStageUpAxis stage "Z"
            
        local layerName = "empty_layer"
        local layerPath = output_prefix + layerName + ".usda"
        stage.Export layerPath
        
        -- Create a USD Stage Object from the layer.
        stage = UsdStageObject name:"UsdStageObject"
        stage.SetRootLayer layerPath
            
        -- Export the scene to USD.
        local exportOptions = USDExporter.CreateOptions()
        exportOptions.FileFormat = #ascii
        exportOptions.RootPrimPath = "/foo" -- For simplicity.
        exportOptions.UsdStagesAsReferences = true    
            
        local exportPath = output_prefix + "export_stage_no_target_prim.usd"
        USDExporter.ExportFile exportPath exportOptions:exportOptions
            
        -- A prim is created for the Stage object, but it didnt reference anything.
        local exporterLayerStage = pyUsd.Stage.Open exportPath
        
        local stageObject = exporterLayerStage.GetPrimAtPath (exportOptions.RootPrimPath + "/" + stage.name)
        assert_true (stageObject.IsValid())
        assert_equal 0 ((stageObject.GetChildren()).Count)
    ),    
    
    -- Test that an object offset applied to USD Stage object is correctly handled.
    function export_stage_object_offset = (
        -- Create a simple layer to bring in as a USD stage.
        local stage = pyUsd.Stage.CreateInMemory()     
        local sphere = pyUsdGeom.Sphere.Define stage "/sphere"
        stage.SetDefaultPrim (sphere.GetPrim())
        
        -- Set the default units/up axis. Only want to test the object offset transform.
        pyUsdGeom.SetStageMetersPerUnit stage 0.0254
        pyUsdGeom.SetStageUpAxis stage "Z"
        
        local layerName = "simple_layer_sphere_max_config"
        local layerPath = output_prefix + layerName + ".usda"
        stage.Export layerPath
        
        -- Create a USD Stage object from the layer.
        stage = UsdStageObject name:"UsdStageObject"
        stage.SetRootLayer layerPath
            
        -- Specify an offset for the stage.
        stage.objectoffsetpos = [0,0,10]
        
        -- Export the scene to USD.
        local exportOptions = USDExporter.CreateOptions()
        exportOptions.FileFormat = #ascii
        exportOptions.RootPrimPath = "/foo" -- For simplicity.
        exportOptions.UsdStagesAsReferences = true    
            
        local exportPath = output_prefix + "export_stage_object_offset.usd"
        USDExporter.ExportFile exportPath exportOptions:exportOptions
            
        -- Validate that the USD Stage was split into an xform and the reference, because an object offset is applied,
        -- the stage object was split into 2 prims (xform + object)
        local exporterLayerStage = pyUsd.Stage.Open exportPath
        
        local sphereRef = exporterLayerStage.GetPrimAtPath (exportOptions.RootPrimPath + "/" + stage.name + "/" + stage.name + "_Layer/" + (sphere.GetPrim()).GetName())
        assert_true (sphereRef.IsValid())
        assert_true (sphereRef.IsA(pyUsdGeom.Sphere))
            
        local objectXformPrim = exporterLayerStage.GetPrimAtPath (exportOptions.RootPrimPath + "/" + stage.name + "/" + stage.name + "_Layer")
        objectXformable = pyUsdGeom.Xformable(objectXformPrim)
        objectTransform = objectXformable.GetLocalTransformation()
            
        local scaleOffset = scaleMatrix stage.objectOffsetScale
        local rotationOffset = stage.objectOffsetRot as matrix3
        local translationOffset = transMatrix stage.objectOffsetPos
        local offsetTransform = scaleOffset * rotationOffset * translationOffset
            
        -- Check that the object offset is correctly applyed.
        assert_true (compareUsdAndMaxTransforms objectTransform offsetTransform)
    ),
    
    -- Test that a USD Stage Object using a layer that has different up-axis config is correctly handled.
    function export_stage_with_different_units_and_axis = (
        -- Create a simple layer to bring in as a USD stage.
        local stage = pyUsd.Stage.CreateInMemory()     
        local sphere = pyUsdGeom.Sphere.Define stage "/sphere"
        stage.SetDefaultPrim (sphere.GetPrim())
        
        -- Setup the units/axis in a way that will require an adjustment.
        pyUsdGeom.SetStageMetersPerUnit stage 1 
        pyUsdGeom.SetStageUpAxis stage "Y"
        
        local layerName = "simple_layer_mod_upaxis_and_units"
        local layerPath = output_prefix + layerName + ".usda"
        stage.Export layerPath
        
        -- Create a USD Stage object from the layer.
        stage = UsdStageObject name:"UsdStageObject"
        stage.SetRootLayer layerPath
                
        -- Export the scene to USD.
        local exportOptions = USDExporter.CreateOptions()
        exportOptions.FileFormat = #ascii
        exportOptions.RootPrimPath = "/foo" -- For simplicity.
        exportOptions.UsdStagesAsReferences = true    
            
        local exportPath = output_prefix + "export_stage_object_offset.usd"
        USDExporter.ExportFile exportPath exportOptions:exportOptions
            
        -- Validate that the USD Stage was split into an xform and the reference, because the transform required
        -- to adjust for the unit/axis setup must not be inherited.
        local exporterLayerStage = pyUsd.Stage.Open exportPath
        
        local sphereRef = exporterLayerStage.GetPrimAtPath (exportOptions.RootPrimPath + "/" + stage.name + "/" + stage.name + "_Layer/" + (sphere.GetPrim()).GetName())
        assert_true (sphereRef.IsValid())
        assert_true (sphereRef.IsA(pyUsdGeom.Sphere))
            
        local objectXformPrim = exporterLayerStage.GetPrimAtPath (exportOptions.RootPrimPath + "/" + stage.name + "/" + stage.name + "_Layer")
        objectXformable = pyUsdGeom.Xformable(objectXformPrim)
        local localTransform = objectXformable.GetLocalTransformation()
        
        -- Meters to inches
        local expectedTransform = (pyGf.Matrix4d 39.370079 0 0 0 0 0 39.370079 0 0 -39.370079 0 0 0 0 0 1) 
            
        -- Check that the object offset is correctly applied.
        assert_true (compareUsdAndMaxTransforms localTransform expectedTransform)
    ),
    
    -- Test that a USD Stage Object with an object offset AND using a layer that has different up-axis config is correctly handled.
    -- (Testing both together, because the order of transforms matters)
    function export_stage_with_object_offset_and_different_units_and_axis = (
        -- Create a simple layer to bring in as a USD stage.
        local stage = pyUsd.Stage.CreateInMemory()     
        local sphere = pyUsdGeom.Sphere.Define stage "/sphere"
        stage.SetDefaultPrim (sphere.GetPrim())
        
        -- Setup the units/axis in a way that will require an adjustment.
        pyUsdGeom.SetStageMetersPerUnit stage 1 
        pyUsdGeom.SetStageUpAxis stage "Y"
        
        local layerName = "simple_layer_mod_upaxis_and_units_offset"
        local layerPath = output_prefix + layerName + ".usda"
        stage.Export layerPath
        
        -- Create a USD Stage object from the layer.
        stage = UsdStageObject name:"UsdStageObject"
        stage.SetRootLayer layerPath
        
        -- Specify an offset for the stage.            
        stage.objectoffsetpos = [0,0,100]    
            
        -- Export the scene to USD.
        local exportOptions = USDExporter.CreateOptions()
        exportOptions.FileFormat = #ascii
        exportOptions.RootPrimPath = "/foo" -- For simplicity.
        exportOptions.UsdStagesAsReferences = true    
            
        local exportPath = output_prefix + "export_stage_with_object_offset_and_different_units_and_axis.usd"
        USDExporter.ExportFile exportPath exportOptions:exportOptions
            
        -- Validate that the USD Stage was split into an xform and the reference, because the transform required
        -- to adjust for the unit/axis setup must not be inherited.
        local exporterLayerStage = pyUsd.Stage.Open exportPath
        
        local sphereRef = exporterLayerStage.GetPrimAtPath (exportOptions.RootPrimPath + "/" + stage.name + "/" + stage.name + "_Layer/" + (sphere.GetPrim()).GetName())
        assert_true (sphereRef.IsValid())
        assert_true (sphereRef.IsA(pyUsdGeom.Sphere))
        
        local objectXformPrim = exporterLayerStage.GetPrimAtPath (exportOptions.RootPrimPath + "/" + stage.name + "/" + stage.name + "_Layer")
        objectXformable = pyUsdGeom.Xformable(objectXformPrim)
            
        local localTransform = objectXformable.GetLocalTransformation()
        
        -- Meters to inches + offset of 10
        local expectedTransform = (pyGf.Matrix4d 39.370079 0 0 0 0 0 39.370079 0 0 -39.370079 0 0 0 0 100 1) 
            
        -- Check that the object offset is correctly applied.
        assert_true (compareUsdAndMaxTransforms localTransform expectedTransform)

    ),
    
    -- Test exporting a USD Stage using a layer that has multiple root Prims.
    function export_stage_with_multi_roots = (
        -- Create a simple with more than one "root" prim.
        local stage = pyUsd.Stage.CreateInMemory()     
        local sphere = pyUsdGeom.Sphere.Define stage "/sphere"
        local sphere2 = pyUsdGeom.Sphere.Define stage "/sphere2"
        -- Purposefully set the second sphere as default, to make sure we arent just 
        -- targetting the first prim in the file.
        stage.SetDefaultPrim (sphere2.GetPrim())
        
        pyUsdGeom.SetStageMetersPerUnit stage 0.0254
        pyUsdGeom.SetStageUpAxis stage "Z"
            
        local layerName = "multiroots_layer"
        local layerPath = output_prefix + layerName + ".usda"
        stage.Export layerPath
        
        -- Create a USD Stage object from the layer.
        stage = UsdStageObject name:"UsdStageObject"
        stage.SetRootLayer layerPath
            
        -- Export the scene to USD.
        local exportOptions = USDExporter.CreateOptions()
        exportOptions.FileFormat = #ascii
        exportOptions.RootPrimPath = "/foo" -- For simplicity.
        exportOptions.UsdStagesAsReferences = true    
            
        local exportPath = output_prefix + "export_stage_with_multi_roots.usd"
        USDExporter.ExportFile exportPath exportOptions:exportOptions
            
        -- Validate that the reference was created as expected. Targetting the prim setup as default (/sphere2)
        local exporterLayerStage = pyUsd.Stage.Open exportPath
        
        local sphereRef = exporterLayerStage.GetPrimAtPath (exportOptions.RootPrimPath + "/" + stage.name + "/" + (sphere2.GetPrim()).GetName())
        assert_true (sphereRef.IsValid())
        assert_true (sphereRef.IsA(pyUsdGeom.Sphere))
    ),

    -- Test that is a USD Stage object uses a layer with no default prim, the first suitable prim found is used as reference target.
    function export_stage_with_no_default_prim = (
        -- Create a simple layer with more than one "root" prims.
        local stage = pyUsd.Stage.CreateInMemory()     
        local sphere = pyUsdGeom.Sphere.Define stage "/sphere"
        local sphere2 = pyUsdGeom.Sphere.Define stage "/sphere2"
        
        pyUsdGeom.SetStageMetersPerUnit stage 0.0254
        pyUsdGeom.SetStageUpAxis stage "Z"
            
        local layerName = "no_default_prim"
        local layerPath = output_prefix + layerName + ".usda"
        stage.Export layerPath
        
        -- Create a USD Stage object from the layer.
        stage = UsdStageObject name:"UsdStageObject"
        stage.SetRootLayer layerPath
            
        -- Export the scene to USD.
        local exportOptions = USDExporter.CreateOptions()
        exportOptions.FileFormat = #ascii
        exportOptions.RootPrimPath = "/foo" -- For simplicity.
        exportOptions.UsdStagesAsReferences = true    
            
        local exportPath = output_prefix + "export_stage_with_no_default_prim.usd"
        USDExporter.ExportFile exportPath exportOptions:exportOptions
            
        -- Validate that the reference was created as expected, targetting the first prim found (/sphere)
        local exporterLayerStage = pyUsd.Stage.Open exportPath
        
        local sphereRef = exporterLayerStage.GetPrimAtPath (exportOptions.RootPrimPath + "/" + stage.name + "/" + (sphere.GetPrim()).GetName())
        assert_true (sphereRef.IsValid())
        assert_true (sphereRef.IsA(pyUsdGeom.Sphere))
    ),
    
    function export_stage_with_instanced_data = (
        -- Create a box, instance it, and export to a USD layer.
        local box1Name = "box1"
        local box2Name = "box2"
        
        local box1 = Box name:box1Name pos:[10,0,0] width:10 length:10 height:10
        local box2 = create_clone box1 #instance box2Name
        box2.pos = [-10,0,0]
        local exportOptions = USDExporter.CreateOptions()
        exportOptions.RootPrimPath = "/foo"
        exportOptions.FileFormat = #ascii
        local instanceLayerPath = output_prefix + "instanced_boxes.usd"
        USDExporter.ExportFile instanceLayerPath exportOptions:exportOptions
        
        delete box1
        delete box2
        
        -- Create a USD Stage using it as root layer.
        local stage = UsdStageObject name:"UsdStageObject"
        stage.SetRootLayer instanceLayerPath
        
        -- Rexport that, and make sure instances are there as expected.
        local exportPath = output_prefix + "reexport_usd_layer_with_instanced_data.usd"
        USDExporter.ExportFile exportPath exportOptions:exportOptions

        local stageWithInstances = pyUsd.Stage.Open exportPath
                
        local box1Prim = stageWithInstances.GetPrimAtPath (exportOptions.RootPrimPath + "/" + stage.name + exportOptions.RootPrimPath +  "/" + box1Name)
        assert_true (box1Prim.IsValid())
        assert_true (box1Prim.IsInstance())
        local box2Prim = stageWithInstances.GetPrimAtPath (exportOptions.RootPrimPath + "/" + stage.name + exportOptions.RootPrimPath +  "/" + box2Name)
        assert_true (box2Prim.IsValid())
        assert_true (box2Prim.IsInstance())
    ),
    
    -- Test that is a USD Stage object uses a layer with default prim, do not explicitly refer to the default prim.
    function export_stage_with_default_prim = (
        -- Create a USD stage and define a sphere.
        local stage = pyUsd.Stage.CreateInMemory()
        local sphere = pyUsdGeom.Sphere.Define stage "/sphere"

        -- Set the sphere as the default prim for the stage.
        stage.SetDefaultPrim(sphere.GetPrim())

        -- Set the stage's units and up axis.
        pyUsdGeom.SetStageMetersPerUnit stage 0.0254
        pyUsdGeom.SetStageUpAxis stage "Z"

        -- Export the stage to a USD file.
        local layerName = "with_default_prim"
        local layerPath = output_prefix + layerName + ".usda"
        stage.Export layerPath

        -- Create a USD Stage Object from the exported layer.
        stage = UsdStageObject name:"UsdStageObject"
        stage.SetRootLayer layerPath

        -- Export the new stage with a reference to the default prim.
        local exportOptions = USDExporter.CreateOptions()
        exportOptions.FileFormat = #ascii
        exportOptions.RootPrimPath = "/foo"
        exportOptions.UsdStagesAsReferences = true

        local exportPath = output_prefix + "export_stage_with_default_prim.usd"
        USDExporter.ExportFile exportPath exportOptions:exportOptions

        -- Validate that the reference was created as expected.
        local exporterLayerStage = pyUsd.Stage.Open exportPath
        local sphereRef = exporterLayerStage.GetPrimAtPath (exportOptions.RootPrimPath + "/" + stage.name + "/" + (sphere.GetPrim()).GetName())
        assert_true (sphereRef.IsValid())
        assert_true (sphereRef.IsA(pyUsdGeom.Sphere))

        -- Ensure that no explicit prim path is being referenced since we have a default prim.
        fs = openFile exportPath
        local foundRefWithoutExplicitPrimPath = false
        while (not eof fs and not foundRefWithoutExplicitPrimPath) do (
            fileLine = readline fs
            if (fileLine != "") do (
                -- Check for the reference without the explicit prim path.
                foundRefWithoutExplicitPrimPath = ((findString fileLine ("prepend references = @./" + (filenameFromPath layerPath) + "@")) != undefined)
            )
        )
        close fs
        assert_true foundRefWithoutExplicitPrimPath message:"Expected reference without explicit prim path not found"
    ),

    -- Test that a USD Stage object uses a layer with a stage mask, ensuring the stage mask prim is referenced.
    function export_stage_with_stage_mask = (
        -- Create a simple layer with multiple prims.
        local stage = pyUsd.Stage.CreateInMemory()
        local sphere = pyUsdGeom.Sphere.Define stage "/sphere"
        local sphere2 = pyUsdGeom.Sphere.Define stage "/sphere2"
        stage.SetDefaultPrim(sphere2.GetPrim())

        -- Set stage properties (units, up axis).
        pyUsdGeom.SetStageMetersPerUnit stage 0.0254
        pyUsdGeom.SetStageUpAxis stage "Z"

        -- Export the stage to a USD file.
        local layerName = "with_stage_mask"
        local layerPath = output_prefix + layerName + ".usda"
        stage.Export layerPath

        -- Set up the stage mask for '/sphere'.
        local maskStr = "/sphere"

        -- Create a USD Stage Object and apply the stage mask when setting the root layer.
        stage = UsdStageObject name:"UsdStageObject"
        stage.SetRootLayer layerPath stageMask:maskStr

        -- Export the scene with the stage mask applied.
        local exportOptions = USDExporter.CreateOptions()
        exportOptions.FileFormat = #ascii
        exportOptions.RootPrimPath = "/foo"
        exportOptions.UsdStagesAsReferences = true

        local exportPath = output_prefix + "export_stage_with_stage_mask.usd"
        USDExporter.ExportFile exportPath exportOptions:exportOptions

        -- Validate that only the masked prim '/sphere' is referenced.
        local exporterLayerStage = pyUsd.Stage.Open exportPath
        local sphereRef = exporterLayerStage.GetPrimAtPath (exportOptions.RootPrimPath + "/" + stage.name + "/" + (sphere.GetPrim()).GetName())
        assert_true (sphereRef.IsValid())
        assert_true (sphereRef.IsA(pyUsdGeom.Sphere))

        local sphere2Ref = exporterLayerStage.GetPrimAtPath (exportOptions.RootPrimPath + "/" + stage.name + "/" + (sphere2.GetPrim()).GetName())
        assert_false(sphere2Ref.IsValid())

        -- Check the file for the correct reference with the explicit prim path using the stage mask.
        fs = openFile exportPath
        local foundRelativeRefWithMask = false
        while (not eof fs and not foundRelativeRefWithMask) do (
            fileLine = readline fs
            if (fileLine != "" ) do (
                -- Check for the reference with the explicit prim path for the stage mask.
                foundRelativeRefWithMask = ((findString fileLine ("prepend references = @./" + (filenameFromPath layerPath) + "@</sphere>")) != undefined)
            )
        )
        close fs
        -- Validate that the stage mask was applied in the exported USD file.
        assert_true foundRelativeRefWithMask message:"Reference with stage mask and prim path was not correctly setup."
    ),

    function teardown = (
        units.SystemType = initialUnits
        units.SystemScale = initialScale
    ),

    Tests = #(
        export_stage_as_reference_test,
        export_null_stage_as_ref,
        export_stage_no_target_prim,
        export_stage_object_offset,
        export_stage_with_different_units_and_axis,
        export_stage_with_object_offset_and_different_units_and_axis,
        export_stage_with_multi_roots,
        export_stage_with_no_default_prim,
        export_stage_with_instanced_data,
        export_stage_with_default_prim,
        export_stage_with_stage_mask
    )
)

runUsdTestFixture export_usd_stage_as_reference_test (getThisScriptFilename())