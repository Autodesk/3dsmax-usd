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
pySdf = python.import("pxr.Sdf")
pyGf = python.import("pxr.Gf")
    
output_prefix = pathConfig.appendPath (getDir #temp) "/MAP_CHANNEL_EXPORT_TEST_"
deleteTestFiles(output_prefix + "*.*")

struct map_channel_export_test
(  
    private 
    exportOptions = USDExporter.CreateOptions(),
    
    public 
    function setup =
    (
        resetmaxfile #noprompt
        exportOptions.FileFormat = #ascii
        exportOptions.RootPrimPath = "/"
    ),
    
    -- Make sure that defaults are properly reset after a call to SetChannelPrimvarMappingDefaults()
    function test_set_defaults_mapchannel_output = (
        -- Set some non default mappings.
        for i = -2 to 99 do 
        (
            exportOptions.SetChannelPrimvarMapping i ("test" + (i+2) as string) type:#float3Array
        )
        
        -- reset to defaults 
        exportOptions.SetChannelPrimvarMappingDefaults()
        
        -- Now make sure the defaults were properly reset.
        
        -- Alpha 
        assert_equal "displayOpacity" (exportOptions.GetChannelPrimvarName -2) message:"The alpha channel primvar name was not properly reset."
        assert_equal #floatArray (exportOptions.GetChannelPrimvarType -2) message:"The alpha channel primvar type was not properly reset."
        
        -- Shading
        assert_equal "mapShading" (exportOptions.GetChannelPrimvarName -1) message:"The shading channel primvar name was not properly reset."
        assert_equal #color3fArray (exportOptions.GetChannelPrimvarType -1) message:"The shading channel primvar type was not properly reset."
        
        -- Vertex color
        assert_equal "vertexColor" (exportOptions.GetChannelPrimvarName 0) message:"The vertex color channel primvar name was not properly reset."
        assert_equal #color3fArray (exportOptions.GetChannelPrimvarType 0) message:"The vertex color channel primvar type was not properly reset."
            
        -- Other mapped channels.
        for i = 1 to 99 do 
        (
            local expected = "st"
            if i > 1 do 
            (
                expected = expected + (i-1) as string
            )
            assert_equal (exportOptions.GetChannelPrimvarName i) expected message:("The primvar name was not properly reset for channel " + i as string)
            assert_equal (exportOptions.GetChannelPrimvarType i) #texCoord2fArray message:("The primvar type was not properly reset for channel " + i as string)
            assert_false (exportOptions.GetChannelPrimvarAutoExpandType i) message:("The primvar autoExpand was not properly reset for channel " + i as string)
        )
    ),
 
    -- Test that exceptions are thrown when attempting to configure a channel with bad values.
    function test_bad_mapchannel_output = (
        local exceptionThrown = false
        try (
            exportOptions.SetChannelPrimvarMapping -3 "invalid_channel" type:#float3Array
        )
        catch 
        (
            exceptionThrown = true
        )
        assert_true exceptionThrown message:"No exception was thrown when attempting to configure an invalid channel."

        exceptionThrown = false
        try (
            exportOptions.SetChannelPrimvarMapping 0 "invalid:name" type:#float3Array
        )
        catch 
        (
            exceptionThrown = true
        )
        assert_true exceptionThrown message:"No exception was thrown when attempting set an invalid primvar name."        
    ),
   
    -- Test all primvar types on output.
    function test_channel_mapping_config = (
        
        local types = #(#floatArray, #float2Array, #float3Array, #color3fArray, #texcoord2fArray, #texcoord3fArray)
        local type_names = #("float[]", "float2[]", "float3[]", "color3f[]", "texCoord2f[]", "texCoord3f[]")
        local b = box()
        b.name = "box"
        convertToPoly b
        
        for i = 1 to 6 do (
            exportOptions.SetChannelPrimvarMapping i ("test" + i as string) type:types[i] autoExpandType:false
            polyop.defaultMapFaces b i
        )
        
        local exportPath = output_prefix + "channel_mapping_types.usda"
        USDExporter.ExportFile exportPath exportOptions:exportOptions
        local stage = pyUsd.Stage.Open(exportPath)
        
        local usdBox = stage.GetPrimAtPath("/" + b.name)
        assert_true (pyUsd.Prim.IsValid(usdBox)) message:"The box was not exported to a USD prim as expected."
        assert_true (usdBox.IsA(pyUsdGeom.Mesh)) message:"The box prim is not a mesh."
    
        for i = 1 to 6 do (
            local name = ("test" + i as string)            
            local primvarApi = pyUsdGeom.PrimvarsAPI(usdBox)
            assert_true (primvarApi.HasPrimvar(name)) message:("Missing primvar on export : " + name)
            local primvar = primvarApi.GetPrimvar(name)
            assert_equal type_names[i] (primvar.GetTypeName() as string) message:("Found primvar with the wrong type, " + name + " should be " + type_names[i])
        )
    ),    

    -- Integration test, interpolation schemes are tested in depth in MappedAttributeBuilderTests.
    function test_uv_interpolation_schemes = (
        -- Cylinder -> face varying
        local c = Cylinder()
        c.name = "cylinder"
        c.mapCoords = true -- generate basic uvs
        
        -- Plane -> Vertex 
        local p = plane()
        p.name = "plane"
        p.mapCoords = true -- generate basic uvs
        
        local exportPath = output_prefix + "test_uv_interpolation_schemes.usda"
        USDExporter.ExportFile exportPath exportOptions:exportOptions
        local stage = pyUsd.Stage.Open(exportPath)
        
        local usdCylinder = stage.GetPrimAtPath("/" + c.name)
    
        assert_true (pyUsd.Prim.IsValid(usdCylinder)) message:"The cylinder was not exported to a USD prim as expected."
        assert_true (usdCylinder.IsA(pyUsdGeom.Mesh)) message:"The cylinder prim is not a mesh."
        
        local mainUvsName = exportOptions.GetChannelPrimvarName 1
            
        local primvarApi = pyUsdGeom.PrimvarsAPI(usdCylinder)
        assert_true (primvarApi.HasPrimvar(mainUvsName))            
        local st = primvarApi.GetPrimvar(mainUvsName)
        assert_equal "faceVarying" (st.GetInterpolation())

        local usdPlane = stage.GetPrimAtPath("/" + p.name)
        assert_true (pyUsd.Prim.IsValid(usdPlane)) message:"Thecylinder was not exported to a USD prim as expected."
        assert_true (usdPlane.IsA(pyUsdGeom.Mesh)) message:"The cylinder prim is not a mesh."
        
        primvarApi = pyUsdGeom.PrimvarsAPI(usdPlane)
        assert_true (primvarApi.HasPrimvar(mainUvsName))
        st = primvarApi.GetPrimvar(mainUvsName)
        assert_equal "vertex" (st.GetInterpolation())
    ),
    
    -- Unless autoExpandType is set to false by the user via script, we check if the data "fits". For example, if "W" (Z) is used
    -- in a channel, but the target primvar type is not 3 dimensional (floatArray, float2array or texcoord2fArray...) 
    -- we would loose information. In those cases, we bump the type to the higher dimension type required to avoid this loss.
    function data_type_bump_test = (
        local uvPrimvarName = "uvs"
        -- Start with autoExpandType:true
        exportOptions.SetChannelPrimvarMapping 1 uvPrimvarName type:#texcoord2fArray autoExpandType:true
        assert_true (exportOptions.GetChannelPrimvarAutoExpandType 1)
        
        local t = teapot()
        t.mapCoords = true -- generate basic uvs
        
        local exportPath = output_prefix + "data_type_bump_test.usda"
        USDExporter.ExportFile exportPath exportOptions:exportOptions
        local stage = pyUsd.Stage.Open(exportPath)
        
        -- The main UV channel should have been exported as a texcoord2fArray, as W is not used.
        local usdTeapot = stage.GetPrimAtPath("/" + t.name)
        assert_true (pyUsd.Prim.IsValid(usdTeapot)) message:"The teapot was not exported to a USD prim as expected."
        assert_true (usdTeapot.IsA(pyUsdGeom.Mesh)) message:"The teapot prim is not a mesh."
        local primvarApi = pyUsdGeom.PrimvarsAPI(usdTeapot)
        assert_true (primvarApi.HasPrimvar(uvPrimvarName)) message:"The main UV channel was not exported to the expected primvar."
        local primvar = primvarApi.GetPrimvar(uvPrimvarName)
        assert_equal "texCoord2f[]" (primvar.GetTypeName() as string)
        
        -- Now add an offset in W...now the type should be bumped to a texcoord3fArray, to avoid any loss.
        local uvwXformMod = uvw_xform()
        uvwXformMod.W_Offset = 10
        addModifier t uvwXformMod
        exportPath = output_prefix + "data_type_bump_test_w_offset.usda"
        USDExporter.ExportFile exportPath exportOptions:exportOptions
        stage = pyUsd.Stage.Open(exportPath)
        usdTeapot = stage.GetPrimAtPath("/" + t.name)
        assert_true (pyUsd.Prim.IsValid(usdTeapot)) message:"The teapot was not exported to a USD prim as expected."
        assert_true (usdTeapot.IsA(pyUsdGeom.Mesh)) message:"The teapot prim is not a mesh."
        primvarApi = pyUsdGeom.PrimvarsAPI(usdTeapot)
        assert_true (primvarApi.HasPrimvar(uvPrimvarName)) message:"The main UV channel was not exported to the expected primvar."
        primvar = primvarApi.GetPrimvar(uvPrimvarName)
        assert_equal "texCoord3f[]" (primvar.GetTypeName() as string)
        
        -- Now try forcing the type, W should now be ignored and we should get back to a #texcoord2fArray
        exportOptions.SetChannelPrimvarMapping 1 uvPrimvarName type:#texcoord2fArray autoExpandType:false
        assert_false (exportOptions.GetChannelPrimvarAutoExpandType 1)            
        exportPath = output_prefix + "data_type_bump_test_force_type.usda"
        USDExporter.ExportFile exportPath exportOptions:exportOptions
        stage = pyUsd.Stage.Open(exportPath)
        usdTeapot = stage.GetPrimAtPath("/" + t.name)
        assert_true (pyUsd.Prim.IsValid(usdTeapot)) message:"The teapot was not exported to a USD prim as expected."
        assert_true (usdTeapot.IsA(pyUsdGeom.Mesh)) message:"The teapot prim is not a mesh."
        primvarApi = pyUsdGeom.PrimvarsAPI(usdTeapot)
        assert_true (primvarApi.HasPrimvar(uvPrimvarName)) message:"The main UV channel was not exported to the expected primvar."
        primvar = primvarApi.GetPrimvar(uvPrimvarName)
        assert_equal "texCoord2f[]" (primvar.GetTypeName() as string)
    ),
    
    -- Test that the export of channels to primvars can be disabled per-channel.
    function test_exclude_channel_from_export = (
        local t = teapot()
        t.mapCoords = true -- generate basic uvs
        
        local targetName = exportOptions.GetChannelPrimvarName 1
        -- Disable the export of channel 1
        exportOptions.SetChannelPrimvarMapping 1 undefined
        assert_undefined (exportOptions.GetChannelPrimvarName 1)
        
        local exportPath = output_prefix + "test_exclude_channel_from_export_channel1_off.usda"
        USDExporter.ExportFile exportPath exportOptions:exportOptions
        local stage = pyUsd.Stage.Open(exportPath)
        usdTeapot = stage.GetPrimAtPath("/" + t.name)
        assert_true (pyUsd.Prim.IsValid(usdTeapot)) message:"The teapot was not exported to a USD prim as expected."
        assert_true (usdTeapot.IsA(pyUsdGeom.Mesh)) message:"The teapot prim is not a mesh."
        local primvarApi = pyUsdGeom.PrimvarsAPI(usdTeapot)
        assert_false (primvarApi.HasPrimvar(targetName)) message:"The main UV channel was disabled and should not have been exported."
        
        -- Now renable the export of channel one...
        exportOptions.SetChannelPrimvarMapping 1 targetName
        exportPath = output_prefix + "test_exclude_channel_from_export_channel1_on.usda"
        USDExporter.ExportFile exportPath exportOptions:exportOptions
        stage = pyUsd.Stage.Open(exportPath)
        usdTeapot = stage.GetPrimAtPath("/" + t.name)
        assert_true (pyUsd.Prim.IsValid(usdTeapot)) message:"The teapot was not exported to a USD prim as expected."
        assert_true (usdTeapot.IsA(pyUsdGeom.Mesh)) message:"The teapot prim is not a mesh."
        primvarApi = pyUsdGeom.PrimvarsAPI(usdTeapot)
        targetName = exportOptions.GetChannelPrimvarName 1
        assert_true (primvarApi.HasPrimvar(targetName)) message:"The main UV channel was not exported to the expected primvar."
    ),
    
    function test_channel_config_getters_validation = (
        local exportOptions = USDExporter.CreateOptions()
        local exceptionThrown = false
        try (
            exportOptions.GetChannelPrimvarName -3
        )
        catch 
        (
            exceptionThrown = true
        )
        assert_true exceptionThrown message:"No exception was thrown when attempting to get the primvar name of an unmapped/invalid channel."
        
        local exceptionThrown = false
        try (
            exportOptions.GetChannelPrimvarType -3
        )
        catch 
        (
            exceptionThrown = true
        )
        assert_true exceptionThrown message:"No exception was thrown when attempting to get the primvar type of an unmapped/invalid channel."
        
        local exceptionThrown = false
        try (
            exportOptions.GetChannelPrimvarAutoExpandType -3
        )
        catch 
        (
            exceptionThrown = true
        )
        assert_true exceptionThrown message:"No exception was thrown when attempting to get the autoExpandType of an unmapped/invalid channel."
    ),
    
    function teardown = (
    ),

    Tests = #(test_set_defaults_mapchannel_output,
        test_bad_mapchannel_output, 
        test_uv_interpolation_schemes,
        test_channel_mapping_config,
        data_type_bump_test,
        test_exclude_channel_from_export,
        test_channel_config_getters_validation)
)

runUsdTestFixture map_channel_export_test (getThisScriptFilename())