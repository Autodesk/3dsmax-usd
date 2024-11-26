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
pyVt = python.import("pxr.Vt")

output_prefix = pathConfig.appendPath (getDir #temp) "/IMPORT_PRIMVAR_TEST_"
deleteTestFiles(output_prefix + "*.*")

struct import_primvar_test
(  
    private 
    importOptions = USDImporter.CreateOptions(),
    exportOptions = USDExporter.CreateOptions(),
    
    public 
    function setup =
    (
        resetmaxfile #noprompt
        exportOptions.FileFormat = #ascii
        exportOptions.RootPrimPath = "/"
    ),
    
    -- Make sure that defaults are properly reset after a call to SetPrimvarChannelMappingDefaults()
    function test_set_default_primvar_channels = (
        importOptions.SetPrimvarChannelMapping "displayOpacity" undefined
        importOptions.SetPrimvarChannelMapping "vertexColor" undefined
        importOptions.SetPrimvarChannelMapping "mapShading" undefined
        importOptions.SetPrimvarChannelMapping "st" undefined
        for i = 1 to 99 do 
        (
            local primvar = "map" + i as string
            importOptions.SetPrimvarChannelMapping primvar undefined
        )
                
        -- Reset to defaults 
        importOptions.SetPrimvarChannelMappingDefaults()
        
        -- Now make sure the defaults were properly reset.        
        assert_equal -2 (importOptions.GetPrimvarChannel "displayOpacity") message:"The displayOpacity mapping was not properly reset."
        assert_equal -1 (importOptions.GetPrimvarChannel "mapShading") message:"The mapShading mapping was not properly reset."
        assert_equal 0 (importOptions.GetPrimvarChannel "vertexColor") message:"The vertexColor mapping was not properly reset"
        -- Other mapped channels.
        for i = 1 to 99 do 
        (
            local primvar = "map" + i as string
            assert_equal i (importOptions.GetPrimvarChannel primvar) message:("Incorrectly reset primvar mapping for :" + primvar)
        )
        
        assert_equal 1 (importOptions.GetPrimvarChannel "st") message:"The st mapping was not properly reset."
        assert_equal 1 (importOptions.GetPrimvarChannel "uv") message:"The uv mapping was not properly reset."
        for i = 0 to 98 do 
        (
            local primvar = "st" + i as string
            assert_equal (i+1) (importOptions.GetPrimvarChannel primvar) message:("Incorrectly reset primvar mapping for :" + primvar)
            primvar = "st_" + i as string
            assert_equal (i+1) (importOptions.GetPrimvarChannel primvar) message:("Incorrectly reset primvar mapping for :" + primvar)
            primvar = "uv" + i as string
            assert_equal (i+1) (importOptions.GetPrimvarChannel primvar) message:("Incorrectly reset primvar mapping for :" + primvar)
            primvar = "uv_" + i as string
            assert_equal (i+1) (importOptions.GetPrimvarChannel primvar) message:("Incorrectly reset primvar mapping for :" + primvar)
        )
    ),
    
    function test_get_primvar_channel = (
        importOptions.ClearMappedPrimvars()
        local primvarName = "foo"
        local primvarChannel = 1
        importOptions.SetPrimvarChannelMapping primvarName primvarChannel
        assert_equal primvarChannel (importOptions.GetPrimvarChannel primvarName)
        
        unmapped_channel = importOptions.GetPrimvarChannel "noMapping"
        assert_undefined unmapped_channel message:"Mapping not undefined when attempting to get a mapping which doesn't exist."

        exceptionThrown = false
        try (
           importOptions.GetPrimvarChannel "invalid:primvar:name"
        )
        catch 
        (
            format "Caught expected exception : %\n" (getCurrentException())
            exceptionThrown = true
        )
        assert_true exceptionThrown message:"No exception was thrown when attempting get mapping using an invalid primvar name."

        exceptionThrown = false
        try (
           importOptions.GetPrimvarChannel undefined
        )
        catch 
        (
            format "Caught expected exception : %\n" (getCurrentException())
            exceptionThrown = true
        )
        assert_true exceptionThrown message:"No exception was thrown when attempting get mapping using an undefined primvar name."
    ),
    
    -- Test that exceptions are thrown when attempting to configure bad values.
    function test_set_bad_mapping = (
        local exceptionThrown = false
        try (
            importOptions.SetPrimvarChannelMapping "invalidChannel" -10
        )
        catch 
        (
            format "Caught expected exception : %\n" (getCurrentException())
            exceptionThrown = true
        )
        assert_true exceptionThrown message:"No exception was thrown when attempting to configure an invalid channel."

        exceptionThrown = false
        try (
           importOptions.SetPrimvarChannelMapping "invalid:primvar:name" 1
        )
        catch 
        (
            format "Caught expected exception : %\n" (getCurrentException())
            exceptionThrown = true
        )
        assert_true exceptionThrown message:"No exception was thrown when attempting set an invalid primvar name."

        exceptionThrown = false
        try (
           importOptions.SetPrimvarChannelMapping undefined 1
        )
        catch 
        (
            format "Caught expected exception : %\n" (getCurrentException())
            exceptionThrown = true
        )
        assert_true exceptionThrown message:"No exception was thrown when attempting set an undefined primvar name."
    ),
    
    -- Test the IsMappedPrimvar() function. It returns true if a primvar is currently mapped to a channel.
    function test_is_mapped_primvar = (
        importOptions.SetPrimvarChannelMapping "somePrimvar" 1
        importOptions.SetPrimvarChannelMapping "someIgnoredPrimvar" undefined
        assert_true (importOptions.IsMappedPrimvar "somePrimvar")
        assert_true (importOptions.IsMappedPrimvar "someIgnoredPrimvar")
        assert_false (importOptions.IsMappedPrimvar "someOtherPrimvar")

        exceptionThrown = false
        try (
           importOptions.IsMappedPrimvar undefined
        )
        catch 
        (
            format "Caught expected exception : %\n" (getCurrentException())
            exceptionThrown = true
        )
        assert_true exceptionThrown message:"No exception was thrown when attempting set an undefined primvar name."
    ),
    
    -- Test the GetMappedPrimvars() function. It returns the list of all currently mapped primvars.
    function test_get_mapped_primvars = (
        importOptions.ClearMappedPrimvars() 
        importOptions.SetPrimvarChannelMapping "foo" 1
        importOptions.SetPrimvarChannelMapping "bar" 2
        importOptions.SetPrimvarChannelMapping "baz" 3
        local primvars = importOptions.GetMappedPrimvars() 
        -- The primvar names are returned in alphabetical order...
        assert_equal "bar" primvars[1]
        assert_equal "baz" primvars[2]
        assert_equal "foo" primvars[3]
    ),
    
    -- Test explicitely disabling the import of a primvar.
    function test_disable_channel_import = (
         -- Test using channel 2, which typically uninitialized.
        local channelId = 2
        local primvarName = "foo"
        local primName = "box"
        local b = box()
        b.name = primName
        convertToPoly b
        polyop.defaultMapFaces b channelId
        
        local numMapFaces = polyop.getNumMapFaces b channelId
        local numMapVerts = polyop.getNumMapVerts b channelId
        
        exportOptions.SetChannelPrimvarMapping channelId primvarName
        local exportPath = output_prefix + "disable_channel_import.usda"
        USDExporter.ExportFile exportPath exportOptions:exportOptions

        -- Now reset the max scene and try to reimport the primvar to channel 2.
        resetMaxFile #noprompt
        importOptions.ClearMappedPrimvars()
        importOptions.SetPrimvarChannelMapping primvarName channelId
        USDImporter.ImportFile exportPath importOptions:importOptions
        
        local reimportedBox = getNodeByName primName        
        assert_true (polyop.getMapsupport reimportedBox channelId)
        assert_equal numMapFaces (polyop.getNumMapFaces reimportedBox channelId)
        assert_equal numMapVerts (polyop.getNumMapVerts reimportedBox channelId)
        
        -- Now disable the primvar and make sure it is no longer imported.
        resetMaxFile #noprompt
        importOptions.SetPrimvarChannelMapping primvarName undefined
        USDImporter.ImportFile exportPath importOptions:importOptions
        local reimportedBox = getNodeByName primName
        assert_false (polyop.getMapsupport reimportedBox channelId)

        exportOptions.Reset()
    ),
    
    -- Make sure that channel "holes" are left empty, i.e. if you import to 1,3,5 - channels 2 and 4 remain uninitialized.
    function test_import_channel_holes = (
        local primName = "box"
        local b = box()
        b.name = primName
        convertToPoly b
        
        -- No map on 2 & 4.
        polyop.defaultMapFaces b 1
        polyop.defaultMapFaces b 3
        polyop.defaultMapFaces b 5
        
        local exportPath = output_prefix + "import_channel_holes.usda"
        USDExporter.ExportFile exportPath exportOptions:exportOptions

        -- Now reset the max scene and try to reimport.
        resetMaxFile #noprompt
        importOptions.Reset()
        USDImporter.ImportFile exportPath importOptions:importOptions
        local reimportedBox = getNodeByName primName
        
        assert_true (polyop.getMapsupport reimportedBox 1)
        assert_false (polyop.getMapsupport reimportedBox 2)
        assert_true (polyop.getMapsupport reimportedBox 3)
        assert_false (polyop.getMapsupport reimportedBox 4)
        assert_true (polyop.getMapsupport reimportedBox 5)
        assert_false (polyop.getMapsupport reimportedBox 6)
    ),
    
    -- Test that textcoord type primvars are used as fallback for the main UV channel if it is not
    -- explicitely mapped.
    function test_unknown_primvars_imported = (
        local nodeName = "box"
        local b = box()
        b.name = nodeName
        convertToPoly b
        polyop.defaultMapFaces b 2
        exportOptions.SetChannelPrimvarMapping 1 undefined -- disable regular UVs
        
        local exportPathFloatPrimvar = output_prefix + "unknown_primvar_floatPrimvar.usda"
        exportOptions.SetChannelPrimvarMapping 2 "foo" type:#float3Array
        USDExporter.ExportFile exportPathFloatPrimvar exportOptions:exportOptions

        local exportPathFoo = output_prefix + "unknown_primvar_foo.usda"
        exportOptions.SetChannelPrimvarMapping 2 "bar" type:#texcoord2fArray
        USDExporter.ExportFile exportPathFoo exportOptions:exportOptions

        local exportPathBar = output_prefix + "unknown_primvar_bar.usda"
        exportOptions.SetChannelPrimvarMapping 2 "baz" type:#texcoord3fArray
        USDExporter.ExportFile exportPathBar exportOptions:exportOptions

        -- A float3Array primvar will not be used as fallback for UVs.
        resetMaxFile #noprompt
        USDImporter.ImportFile exportPathFloatPrimvar importOptions:importOptions
        local reimportedBox = getNodeByName nodeName
        assert_false (polyop.getMapsupport reimportedBox 1)
        
        -- A Texcoord2fArray primvar will be used as fallback for UVs.
        resetMaxFile #noprompt
        USDImporter.ImportFile exportPathFoo importOptions:importOptions
        local reimportedBox = getNodeByName nodeName
        assert_true (polyop.getMapsupport reimportedBox 1)
        
        -- A Texcoord3fArray primvar will be used as fallback for UVs.
        resetMaxFile #noprompt
        USDImporter.ImportFile exportPathBar importOptions:importOptions
        local reimportedBox = getNodeByName nodeName
        assert_true (polyop.getMapsupport reimportedBox 1)

        exportOptions.Reset()
    ),
    
    -- Intergration test, validation is tested in more depth in gtests.
    function test_primvar_with_invalid_data_not_imported = (
        local b = box()
        local nodeName = "box"
        b.name = nodeName
        convertToPoly b
        polyop.defaultMapFaces b 2
        
        local primvarName = "foo"
        exportOptions.SetChannelPrimvarMapping 2 primvarName type:#float3Array
        
        local exportPath = output_prefix + "test_primvar_invalid.usda"
        USDExporter.ExportFile exportPath exportOptions:exportOptions

        -- Primvar is valid... it should reimport.
        importOptions.SetPrimvarChannelMapping primvarName 2
        resetMaxFile #noprompt
        USDImporter.ImportFile exportPath importOptions:importOptions
        
        local reimportedBox = getNodeByName nodeName
        assert_true (polyop.getMapsupport reimportedBox 2)        
        
        -- Set invalid indices.
        stage = pyUsd.Stage.Open(exportPath)
        local boxPrim = stage.GetPrimAtPath("/" + nodeName)
        assert_true (pyUsd.Prim.IsValid(boxPrim)) message:"The cube was not properly exported to the usd stage."
        local primvarApi = pyUsdGeom.PrimvarsAPI(boxPrim)
        assert_true (primvarApi.HasPrimvar(primvarName)) message:("Missing primvar on export : " + primvarName)
        local primvar = primvarApi.GetPrimvar(primvarName)
        indices = pyVt.IntArray(#(999, 999, 999))
        primvar.SetIndices(indices)
        (stage.GetRootLayer()).Save()
        
        -- Primvar is now invalid... it should no longer import.
        importOptions.SetPrimvarChannelMapping primvarName 2
        resetMaxFile #noprompt
        USDImporter.ImportFile exportPath importOptions:importOptions
        reimportedBox = getNodeByName nodeName
        assert_false (polyop.getMapsupport reimportedBox 1)
        
        exportOptions.Reset()
    ),
    
    -- Test that primvars of dimension 4 are still imported, but with the last value cropped.
    function test_import_primvar_4 = (
        local b = box()
        local nodeName = "box"
        b.name = nodeName
        local exportPath = output_prefix + "primvar_4.usda"
        USDExporter.ExportFile exportPath exportOptions:exportOptions
        stage = pyUsd.Stage.Open(exportPath)
        local boxPrim = stage.GetPrimAtPath("/" + nodeName)
        
        -- Create the primvar.
        local primvarName = "foo"        
        local primvarApi = pyUsdGeom.PrimvarsAPI(boxPrim)        
        local foo = primvarApi.CreatePrimvar primvarName pySdf.ValueTypeNames.Float4Array pyUsdGeom.Tokens.constant
        foo.Set((pyVt.Vec4fArray (pyGf.Vec4f 1 2 3 4)))
        stage.Save()
        
        -- Reimport, make sure the XYZ values are still imported.
        local targetChannel = 2
        importOptions.SetPrimvarChannelMapping primvarName targetChannel
        resetMaxFile #noprompt
        USDImporter.ImportFile exportPath importOptions:importOptions
        reimportedBox = getNodeByName nodeName
        assert_point3_equal (point3 1.f 2.f 3.f) (polyop.getMapVert reimportedBox targetChannel 1)
    ),
    
    -- Test the ImportUnmappedPrimvars option, which will attempt to load unmapped primvars into 
    -- the most appropriate channels
    function test_import_unmapped_primvars = (
        local b = box()
        local nodeName = "box"
        b.name = nodeName
        local exportPath = output_prefix + "test_import_unmapped_primvars.usda"
        USDExporter.ExportFile exportPath exportOptions:exportOptions
        stage = pyUsd.Stage.Open(exportPath)
        local boxPrim = stage.GetPrimAtPath("/" + nodeName)
        
        -- Test with the option turned on.
        importOptions.ImportUnmappedPrimvars = true
        -- Create a Float2fArray primvar.
        local primvarName = "bbb"
        local primvarApi = pyUsdGeom.PrimvarsAPI(boxPrim)
        local foo = primvarApi.CreatePrimvar primvarName pySdf.ValueTypeNames.Float2Array pyUsdGeom.Tokens.constant
        foo.Set((pyVt.Vec2fArray (pyGf.Vec2f 1 2)))
        stage.Save()
        resetMaxFile #noprompt
        USDImporter.ImportFile exportPath importOptions:importOptions
        reimportedBox = getNodeByName nodeName
        assert_true (polyop.getMapSupport reimportedBox 1) message:"The main UV primvar should have been inferred."
        assert_point3_equal (point3 1.f 2.f 0.f) (polyop.getMapVert reimportedBox 1 1)  message:"The UVs were not properly set from the inferred UV primvar."
        
        -- Test with the option turned off.
        importOptions.ImportUnmappedPrimvars = false
        resetMaxFile #noprompt
        USDImporter.ImportFile exportPath importOptions:importOptions
        reimportedBox = getNodeByName nodeName
        assert_false (polyop.getMapSupport reimportedBox 1) message:"The main UV primvar should not have been inferred with the option turned off."
        
        -- Test that alphabetical order is used when multiple primvar can be inferred as main UVs.
        -- Create a Double2Array primvar.
        importOptions.ImportUnmappedPrimvars = true
        local primvarName = "aaa"
        local primvarApi = pyUsdGeom.PrimvarsAPI(boxPrim)
        local foo = primvarApi.CreatePrimvar primvarName pySdf.ValueTypeNames.Double2Array pyUsdGeom.Tokens.constant
        foo.Set((pyVt.Vec2dArray (pyGf.Vec2d 3 4)))
        stage.Save()
        resetMaxFile #noprompt
        USDImporter.ImportFile exportPath importOptions:importOptions
        reimportedBox = getNodeByName nodeName
        assert_true (polyop.getMapSupport reimportedBox 1) message:"The main UV primvar should have been inferred."
        assert_point3_equal (point3 3.f 4.f 0.f) (polyop.getMapVert reimportedBox 1 1) message:"The UVs were not properly set from the inferred UV primvar."
        
        -- Test that texcoord type primvars are prioritized.
        -- Create a TexCoord2fArray primvar...
        local primvarName = "zzz" -- Make sure we dont get selected based on alphabetical order...
        local primvarApi = pyUsdGeom.PrimvarsAPI(boxPrim)
        local foo = primvarApi.CreatePrimvar primvarName pySdf.ValueTypeNames.TexCoord2fArray pyUsdGeom.Tokens.constant
        foo.Set((pyVt.Vec2fArray (pyGf.Vec2f 5 6)))
        stage.Save()
        resetMaxFile #noprompt
        USDImporter.ImportFile exportPath importOptions:importOptions
        reimportedBox = getNodeByName nodeName
        assert_true (polyop.getMapSupport reimportedBox 1) message:"The main UV primvar should have been inferred."
        assert_point3_equal (point3 5.f 6.f 0.f) (polyop.getMapVert reimportedBox 1 1) message:"The UVs were not properly set from the inferred UV primvar."
            
        -- Make sure TexCoord3fArray primvars are prioritized...
        local primvarName = "yyy"
        local primvarApi = pyUsdGeom.PrimvarsAPI(boxPrim)
        local foo = primvarApi.CreatePrimvar primvarName pySdf.ValueTypeNames.TexCoord3fArray pyUsdGeom.Tokens.constant
        foo.Set((pyVt.Vec3fArray (pyGf.Vec3f 7 8 9)))
        stage.Save()
        importOptions.ImportUnmappedPrimvars = true
        -- Explicit mapping for the other texcoord primvar to avoid it being picked up first.
        importOptions.SetPrimvarChannelMapping "zzz" 10
        resetMaxFile #noprompt
        USDImporter.ImportFile exportPath importOptions:importOptions
        reimportedBox = getNodeByName nodeName
        assert_true (polyop.getMapSupport reimportedBox 1) message:"The main UV primvar should have been inferred."
        assert_point3_equal (point3 7.f 8.f 9.f) (polyop.getMapVert reimportedBox 1 1) message:"The UVs were not properly set from the inferred UV primvar."
            
        -- Make sure all the unmapped primvars are imported
        importOptions.ClearMappedPrimvars()
        resetMaxFile #noprompt
        USDImporter.ImportFile exportPath importOptions:importOptions
        reimportedBox = getNodeByName nodeName
        -- Inferred from type as main uv : TexCoord2fArray
        assert_point3_equal (point3 5.f 6.f 0.f) (polyop.getMapVert reimportedBox 1 1) message:"Incorrect primvar imported to channel 1."
        -- Ordered from type : TexCoord3fArray
        assert_point3_equal (point3 7.f 8.f 9.f) (polyop.getMapVert reimportedBox 2 1) message:"Incorrect primvar imported to channel 2."
        -- Ordered alphabetically... 
        --- aaa 
        assert_point3_equal (point3 3.f 4.f 0.f) (polyop.getMapVert reimportedBox 3 1) message:"Incorrect primvar imported to channel 3."
        --- bbb
        assert_point3_equal (point3 1.f 2.f 0.f) (polyop.getMapVert reimportedBox 4 1) message:"Incorrect primvar imported to channel 4."
    ),
    
    function test_channel_info_setup = (
        local b = box()
        local nodeName = "box"
        b.name = nodeName
        convertToPoly b
        polyop.defaultMapFaces b 0
        polyop.defaultMapFaces b 1
        polyop.defaultMapFaces b 2
        polyop.defaultMapFaces b 3
                
        exportOptions.SetChannelPrimvarMapping 0 "aaa" type:#color3fArray
        exportOptions.SetChannelPrimvarMapping 1 "bbb" type:#texcoord3fArray
        exportOptions.SetChannelPrimvarMapping 2 "ccc" type:#float3Array
        exportOptions.SetChannelPrimvarMapping 3 "ddd" type:#float3Array
        
        local exportPath = output_prefix + "test_channel_info_setup.usda"
        USDExporter.ExportFile exportPath exportOptions:exportOptions

        resetMaxFile #noprompt
        USDImporter.ImportFile exportPath importOptions:importOptions
        
        local reimportedBox = getNodeByName nodeName
        
        -- Unfortunately the ChannelInfo interface only has a method to set the name, 
        -- not retrieve it... so find it manually from the user properties.
        assert_equal "aaa" (getUserProp reimportedBox "MapChannel:0")
        assert_equal "bbb" (getUserProp reimportedBox "MapChannel:1")
        assert_equal "ccc" (getUserProp reimportedBox "MapChannel:2")
        assert_equal "ddd" (getUserProp reimportedBox "MapChannel:3")        
    ),
    
    function teardown = (
    ),

    Tests = #(test_set_default_primvar_channels,
        test_get_primvar_channel,
        test_set_bad_mapping,
        test_disable_channel_import,
        test_unknown_primvars_imported,
        test_is_mapped_primvar, 
        test_get_mapped_primvars,
        test_import_channel_holes,
        test_primvar_with_invalid_data_not_imported,
        test_import_primvar_4,
        test_import_unmapped_primvars,
        test_channel_info_setup)
)

runUsdTestFixture import_primvar_test (getThisScriptFilename())