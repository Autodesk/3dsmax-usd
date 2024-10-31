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
pyGf = python.import("pxr.Gf")
pySdf = python.import("pxr.Sdf")
pyUsdMaterialReader = python.import("usd_material_reader")
pyShaderReader = python.import("shaderReader")

output_prefix = pathConfig.appendPath (getDir #temp) "/EXPORT_TEXTURE_MAP_CHANNELS_TEST_"
deleteTestFiles(output_prefix + "*.*")

struct export_texture_map_channels_test
(
    private
    _oslbitmapfile = "",
    _uberbitmapfile = "",

    public
    function setup =
    (
        resetmaxfile #noprompt
        maxver = maxversion()
        if maxver[1] >= 26000 then (  -- 3ds Max 2024 and up
            _oslbitmapfile = "OSLBitmap2.osl"
            _uberbitmapfile = "UberBitmap2.osl"
        ) else (
            _oslbitmapfile = "OSLBitmap.osl"
            _uberbitmapfile = "UberBitmap.osl"
        )
        pyShaderReader._material_import_options["texture_target_id"] = _uberbitmapfile
    ),
    function get_primvar_st_input stage bitmap_path = (
        local usd_bitmap = pyUsdShade.Shader.Get stage bitmap_path
        local st_attr_input = usd_bitmap.GetInput("st")
        local primvar_reader_outputs = st_attr_input.GetValueProducingAttributes()
        local st_src_prim = primvar_reader_outputs[1].GetPrim()

        local usd_primvar_reader = pyUsdShade.Shader(st_src_prim)
        local varname_input = usd_primvar_reader.GetInput("varname")
        local varname_value_attributes = varname_input.GetValueProducingAttributes()
        local primvar_name = varname_value_attributes[1].Get()
        return primvar_name
    ),
    function build_test_box map_channel pos:[0,0,0] name:"testBox"= (
        local testBox = Box name:name pos:pos width:10 length:10 height:10
        local clear_uvw = UVW_Mapping_Clear()
        addModifier testBox clear_uvw

        local uvw_modifier = Uvwmap()
        uvw_modifier.maptype = 4 -- box mapping
        uvw_modifier.mapChannel = map_channel
        addModifier testBox uvw_modifier

        local uvw_unwrap = UVWUnwrap()
        addModifier testBox uvw_unwrap

        max modify mode
        uvw_unwrap.unwrap.setMapChannel map_channel
        uvw_unwrap.flattenMapNoParams()
        convertTo testBox Editable_Poly
        return testBox
    ),
    function test_export_texture_map_channels = (
        local export_path = (output_prefix + "test_export_texture_map_channels.usda")
        local log_path = (output_prefix + "test_export_texture_map_channels.log")
        local map_channel = 3 -- non default value
        local map_primvar_name = "testMapChannel"
        local test_texture_filename = "uvwunwrap\\uv_checker.png"

        -- build the boxes
        local testBox = build_test_box map_channel name:"testBox"
        local testBox_osl = build_test_box map_channel name:"testBox_OSL" pos:[0, 20, 0]
        local testBox_osl_2 = build_test_box map_channel name:"testBox_OSL_2" pos:[0, 40, 0]

        -- materials
        testBox.material = PhysicalMaterial()
        testBox_osl.material = PhysicalMaterial()
        testBox_osl_2.material = PhysicalMaterial()

        -- standard bitmap
        local bitmap_texture = Bitmaptexture fileName:test_texture_filename
        bitmap_texture.coordinates.mapChannel = map_channel
        testBox.material.base_color_map = bitmap_texture

        -- uberbitmap.osl
        osl_texture = OSLMap()
        osl_texture.name = "UberBitmap"
        max_root = symbolicPaths.getPathValue("$max")
        osl_texture.OSLPath = (max_root + "/OSL/" + _uberbitmapfile)
        osl_texture.OSLAutoUpdate = True
        osl_texture.filename = test_texture_filename
        osl_texture.UVSet = map_channel
        multi_channel_map = MultiOutputChannelTexmapToTexmap()
        multi_channel_map.sourceMap = osl_texture
        out_channel_index = 1 -- rgb
        multi_channel_map.outputChannelIndex = out_channel_index
        multi_channel_map.name = osl_texture.name + "rgb"
        testBox_osl.material.base_color_map = multi_channel_map

        -- oslbitmap.osl
        osl_texture_2 = OSLMap()
        osl_texture_2.name = "OslBitmap"
        max_root = symbolicPaths.getPathValue("$max")
        osl_texture_2.OSLPath = (max_root + "/OSL/" + _oslbitmapfile)
        osl_texture_2.OSLAutoUpdate = True
        osl_texture_2.filename = test_texture_filename

        pos_map = OSLMap()
        pos_osl_path = (max_root + "/OSL/GetUVW.osl")
        pos_map.OSLPath = pos_osl_path
        pos_map.OSLAutoUpdate = True
        osl_texture_2.Pos_map = pos_map
        pos_map.UVSet = map_channel

        multi_channel_map = MultiOutputChannelTexmapToTexmap()
        multi_channel_map.sourceMap = osl_texture_2
        out_channel_index = 1 -- rgb
        multi_channel_map.outputChannelIndex = out_channel_index
        multi_channel_map.name = osl_texture.name + "rgb"
        testBox_osl_2.material.base_color_map = multi_channel_map


        -- export
        local export_options = USDExporter.CreateOptions()
        export_options.SetChannelPrimvarMapping map_channel map_primvar_name type:#texCoord2fArray autoExpandType:False
        export_options.FileFormat = #ascii
        export_options.RootPrimPath = "/"
        export_options.LogPath = log_path
        export_options.LogLevel = #warn
        res = USDExporter.exportFile export_path exportOptions:export_options contentSource:#nodelist nodeList:#(testBox, testBox_osl, testBox_osl_2)
        assert_true res
        -- test output
        local stage = pyUsd.Stage.Open(export_path)
        local bitmap_path = pySdf.Path("/mtl/Bitmaptexture/Bitmaptexture")
        local OSLbitmap_path = pySdf.Path("/mtl/OslBitmap/OslBitmap")
        local uberbitmap_path = pySdf.Path("/mtl/UberBitmap/UberBitmap")

        local usd_primvar_name = get_primvar_st_input stage bitmap_path
        local usd_primvar_name_osl = get_primvar_st_input stage OSLbitmap_path
        local usd_primvar_name_uber = get_primvar_st_input stage uberbitmap_path

        local export_options_primvar_name = export_options.GetChannelPrimvarName(map_channel)
        assert_equal map_primvar_name export_options_primvar_name
        assert_equal map_primvar_name usd_primvar_name
        assert_equal map_primvar_name usd_primvar_name_osl
        assert_equal map_primvar_name usd_primvar_name_uber

        local importOptions = USDImporter.CreateOptions()
        importOptions.SetPrimvarChannelMapping map_primvar_name map_channel

        -- test import uberbitmap
        resetmaxfile #noprompt
        -- Use caps to make sure the osl name is not case sensitive.
        pyShaderReader._material_import_options["texture_target_id"] = _uberbitmapfile

        USDImporter.ImportFile export_path importOptions:importOptions
        local imp_cube = $testBox
        local imp_material = imp_cube.material
        local imp_texture = imp_material.diffuseColor_map
        assert_equal map_channel imp_texture.sourceMap.UVSet

        -- test import with standard bitmaps
        resetmaxfile #noprompt
        pyShaderReader._material_import_options["texture_target_id"] = "Bitmaptexture"

        USDImporter.ImportFile export_path importOptions:importOptions
        local imp_cube = $testBox
        local imp_material = imp_cube.material
        local imp_texture = imp_material.diffuseColor_map
        assert_equal map_channel imp_texture.coordinates.mapChannel

        -- test import with oslbitmap
        resetmaxfile #noprompt
        pyShaderReader._material_import_options["texture_target_id"] = _oslbitmapfile

        USDImporter.ImportFile export_path importOptions:importOptions
        local imp_cube = $testBox
        local imp_material = imp_cube.material
        local imp_texture = imp_material.diffuseColor_map
        assert_equal map_channel imp_texture.sourceMap.pos_map.UVSet
    ),
    function test_unmapped_primvar_export = (
        -- Log an error and skip export of texture
        local export_path = (output_prefix + "test_unmapped_primvar_export.usda")
        local log_path = (output_prefix + "test_unmapped_primvar_export.log")
        local map_channel = 3 -- non default value
        local map_primvar_name = undefined
        local test_texture_filename = "uvwunwrap\\uv_checker.png"

        -- build the box
        local testBox = build_test_box map_channel name:"testBox"

        -- materials
        testBox.material = PhysicalMaterial()

        -- standard bitmap
        local bitmap_texture = Bitmaptexture fileName:test_texture_filename
        bitmap_texture.coordinates.mapChannel = map_channel
        testBox.material.base_color_map = bitmap_texture

        -- export
        local export_options = USDExporter.CreateOptions()
        export_options.SetChannelPrimvarMapping map_channel map_primvar_name type:#texCoord2fArray autoExpandType:False
        export_options.FileFormat = #ascii
        export_options.RootPrimPath = "/"
        export_options.LogPath = log_path
        export_options.LogLevel = #warn
        res = USDExporter.exportFile export_path exportOptions:export_options contentSource:#nodelist nodeList:#(testBox, testBox_osl)
        assert_true res

        -- test the log output
        local fileStream = openFile log_path
        assert_defined fileStream message:("Could not open file '" + log_path + "'!")
        local infoMessage = "using a channel not mapped to a primvar"
        assert_defined (skipToString fileStream infoMessage) message:("Could not read line '" + infoMessage + "'!")
        close fileStream

        -- test output
        local stage = pyUsd.Stage.Open(export_path)
        local bitmap_path = pySdf.Path("/Materials/Bitmaptexture")
        local usd_bitmap = stage.GetPrimAtPath bitmap_path
        assert_false (usd_bitmap.IsValid())
    ),
    function test_unmapped_primvar_import = (
        -- Should log an error, fallback to channel 1
        local export_path = (output_prefix + "test_unmapped_primvar_import.usda")
        local log_path = (output_prefix + "test_unmapped_primvar_import.log")
        local map_channel = 3 -- non default value
        local map_channel_fallback = 1
        local map_primvar_name = "fuzz"
        local test_texture_filename = "uvwunwrap\\uv_checker.png"

        -- build the box
        local testBox = build_test_box map_channel name:"testBox"

        -- materials
        testBox.material = PhysicalMaterial()

        -- standard bitmap
        local bitmap_texture = Bitmaptexture fileName:test_texture_filename
        bitmap_texture.coordinates.mapChannel = map_channel
        testBox.material.base_color_map = bitmap_texture

        -- export
        local export_options = USDExporter.CreateOptions()
        export_options.SetChannelPrimvarMapping map_channel map_primvar_name type:#texCoord2fArray autoExpandType:False
        export_options.FileFormat = #ascii
        export_options.RootPrimPath = "/"
        res = USDExporter.exportFile export_path exportOptions:export_options contentSource:#nodelist nodeList:#(testBox, testBox_osl)
        assert_true res

        -- test output
        local stage = pyUsd.Stage.Open(export_path)
        local bitmap_path = pySdf.Path("/mtl/Bitmaptexture")
        local usd_bitmap = stage.GetPrimAtPath bitmap_path
        assert_true (usd_bitmap.IsValid())

        -- test import
        resetmaxfile #noprompt
        local importOptions = USDImporter.CreateOptions()
        importOptions.ClearMappedPrimvars() -- now no primvars are mapped
        importOptions.ImportUnmappedPrimvars = False
        importOptions.LogPath = log_path
        importOptions.LogLevel = #warn

        USDImporter.ImportFile export_path importOptions:importOptions
        local imp_cube = $testBox
        local imp_material = imp_cube.material
        local imp_texture = imp_material.diffuseColor_map
        assert_equal map_channel_fallback imp_texture.sourceMap.UVSet

        -- test the log output
        local fileStream = openFile log_path
        assert_defined fileStream message:("Could not open file '" + log_path + "'!")
        local infoMessage = "Channel is not mapped for primvar name:"
        assert_defined (skipToString fileStream infoMessage) message:("Could not read line '" + infoMessage + "'!")
        close fileStream
    ),
    function teardown = (
        pyShaderReader._material_import_options["texture_target_id"] = _uberbitmapfile
    ),

    Tests = #(
        test_export_texture_map_channels,
        test_unmapped_primvar_import,
        test_unmapped_primvar_export
    )
)

runUsdTestFixture export_texture_map_channels_test (getThisScriptFilename())