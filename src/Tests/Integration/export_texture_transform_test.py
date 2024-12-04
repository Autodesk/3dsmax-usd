#
# Copyright 2023 Autodesk
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
import unittest
import sys
import os
import shutil

from importlib import reload
from enum import Enum

from pxr import Usd, UsdShade, Sdf, Ar

import usd_test_helpers
import usd_material_writer
import pymxs

__author__ = r'Autodesk Inc.'
__copyright__ = r'Copyright 2021, Autodesk Inc.'

RT = pymxs.runtime
RT.pluginManager.loadClass(RT.USDExporter)

TEST_DIR = os.path.dirname(__file__)
TEST_DATA_DIR = os.path.join(TEST_DIR, "data")
UV_CHECKER_PATH = os.path.join(TEST_DATA_DIR, "uv_checker.png")

class BitmapType(Enum):
    BITMAP_TEXTURE = 1
    OSL_BITMAP = 2
    OSL_UBERBITMAP = 3

# Various wrapping modes that we support, either from BitmapTexture or OSL bitmaps.
class WrapModes(Enum):
    REPEAT_S = 1
    REPEAT_T = 2
    REPEAT_ST = 3
    MIRROR_S = 4
    MIRROR_T = 5
    MIRROR_ST = 6
    MIRROR_S_REPEAT_T = 7
    REPEAT_S_MIRROR_T = 8
    CLAMP = 9
    DEFAULT = 10
    BLACK = 11

channel_index_map = {
    "rgb": 1,
    "r": 2,
    "g": 3,
    "b": 4,
    "a": 5
}

# Builds a Bitmap texture with wrapping and transform parameters.
def create_texture_bitmap(file_name, name, wrap_mode, tiling, offset, rotate, real_world_scale, real_size):
    test_bitmap = RT.Bitmaptexture(fileName=file_name, name=name)
    test_bitmap.coords.U_Tiling = tiling[0]
    test_bitmap.coords.V_Tiling = tiling[1]
    test_bitmap.coords.U_Offset = offset[0]
    test_bitmap.coords.V_Offset = offset[1]
    test_bitmap.coords.U_Angle = rotate[0]
    test_bitmap.coords.V_Angle = rotate[1]
    test_bitmap.coords.W_Angle = rotate[2]

    if wrap_mode == WrapModes.REPEAT_S:
        test_bitmap.coords.U_Tile = True
        test_bitmap.coords.V_Tile = False
        test_bitmap.coords.V_Mirror = False
    elif wrap_mode == WrapModes.REPEAT_T:
        test_bitmap.coords.U_Tile = False
        test_bitmap.coords.U_Mirror = False
        test_bitmap.coords.V_Tile = True
    elif wrap_mode == WrapModes.MIRROR_S:
        test_bitmap.coords.U_Mirror = True
        test_bitmap.coords.V_Mirror = False
        test_bitmap.coords.V_Tile = False
    elif wrap_mode == WrapModes.MIRROR_T:
        test_bitmap.coords.U_Mirror = False
        test_bitmap.coords.U_Tile = False
        test_bitmap.coords.V_Mirror = True
    elif wrap_mode == WrapModes.REPEAT_ST:
        test_bitmap.coords.U_Tile = True
        test_bitmap.coords.V_Tile = True
    elif wrap_mode == WrapModes.MIRROR_ST:
        test_bitmap.coords.U_Mirror = True
        test_bitmap.coords.V_Mirror = True
    elif wrap_mode == WrapModes.MIRROR_S_REPEAT_T:
        test_bitmap.coords.U_Mirror = True
        test_bitmap.coords.V_Tile = True
    elif wrap_mode == WrapModes.REPEAT_S_MIRROR_T:
        test_bitmap.coords.U_Tile = True
        test_bitmap.coords.V_Mirror = True
    elif wrap_mode == WrapModes.BLACK:
        test_bitmap.coords.U_Tile = False
        test_bitmap.coords.U_Mirror = False
        test_bitmap.coords.V_Tile = False
        test_bitmap.coords.V_Mirror = False

    if real_world_scale:
        test_bitmap.coords.realWorldScale  = real_world_scale
        test_bitmap.coords.realWorldWidth = real_size[0]
        test_bitmap.coords.realWorldHeight = real_size[1]
    return test_bitmap

# Builds an OSL multi-channel map.
# Used to map out individual channels or RGB so we can connect them to a material.
def create_osl_multi_channel(osl_texture, channel):
    multi_channel_map = RT.MultiOutputChannelTexmapToTexmap()
    multi_channel_map.sourceMap = osl_texture
    out_channel_index = channel_index_map[channel] # rgb
    multi_channel_map.outputChannelIndex = out_channel_index
    multi_channel_map.name = "{0}:{1}".format(osl_texture.name, channel)
    return multi_channel_map

# Builds an OSL uberbitmap with wrapping and transform parameters.
def create_osl_uberbitmap(file_name, name, wrap_mode, tiling, scale, real_size, offset, rotate, rot_center, rot_axis, real_world_scale):
    osl_texture = RT.OSLMap()
    osl_texture.name = name
    osl_texture.OSLAutoUpdate = True
    max_root = RT.symbolicPaths.getPathValue("$max")
    
    maxver = RT.maxversion()
    if maxver[0] >= 26000:  # 3ds Max 2024 and up
        osl_texture.OSLPath = (max_root + "/OSL/UberBitmap2.osl")
    else:
        osl_texture.OSLPath = (max_root + "/OSL/UberBitmap.osl")

    osl_texture.filename = file_name
    osl_texture.UVSet = 1

    osl_texture.RealWorld = real_world_scale
    osl_texture.Scale = scale
    osl_texture.Tiling = RT.Point3(tiling[0], tiling[1], 1.0)
    osl_texture.RealWidth = real_size[0]
    osl_texture.RealHeight = real_size[1]

    osl_texture.Offset = RT.Point3(offset[0], offset[1], 1.0)

    osl_texture.RotAxis = RT.Point3(rot_axis[0], rot_axis[1], rot_axis[2])
    osl_texture.RotCenter = RT.Point3(rot_center[0], rot_center[1], rot_center[2])
    osl_texture.Rotate = rotate[2]

    if wrap_mode == WrapModes.DEFAULT:
        osl_texture.WrapMode = "default"
    elif wrap_mode == WrapModes.BLACK:
        osl_texture.WrapMode = "black"
    elif wrap_mode == WrapModes.CLAMP:
        osl_texture.WrapMode = "clamp"
    elif wrap_mode == WrapModes.REPEAT_ST:
        osl_texture.WrapMode = "periodic"
    elif wrap_mode == WrapModes.MIRROR_ST:
        osl_texture.WrapMode = "mirror"

    return osl_texture

# Builds a simple OSLBitmap with a GetUVW position map.
def create_osl_bitmap(file_name, name, wrap_mode, scale):
    osl_texture = RT.OSLMap()
    osl_texture.name = name
    osl_texture.OSLAutoUpdate = True
    max_root = RT.symbolicPaths.getPathValue("$max")
    maxver = RT.maxversion()
    if maxver[0] >= 26000:  # 3ds Max 2024 and up
        osl_texture.OSLPath = (max_root + "/OSL/OSLBitmap2.osl")
    else:
        osl_texture.OSLPath = (max_root + "/OSL/OSLBitmap.osl")

    osl_texture.filename = file_name
    osl_texture.Scale = scale

    if wrap_mode == WrapModes.DEFAULT:
        osl_texture.WrapMode = "default"
    elif wrap_mode == WrapModes.BLACK:
        osl_texture.WrapMode = "black"
    elif wrap_mode == WrapModes.CLAMP:
        osl_texture.WrapMode = "clamp"
    elif wrap_mode == WrapModes.REPEAT_ST:
        osl_texture.WrapMode = "periodic"
    elif wrap_mode == WrapModes.MIRROR_ST:
        osl_texture.WrapMode = "mirror"

    # Need a pos map to set the UV channel.
    uvw_channel_map = RT.OSLMap()
    uvw_channel_map.name = "uvw_map"
    uvw_channel_map.OSLAutoUpdate = True
    uvw_channel_map.OSLPath = (max_root + "/OSL/GetUVW.osl")
    uvw_channel_map.UVSet = 1
    osl_texture.Pos_map = uvw_channel_map
    return osl_texture

class TestExportTextureTransform(unittest.TestCase):
    def setUp(self):
        self.output_prefix = usd_test_helpers.standard_output_prefix("EXPORT_TEXTURE_XFORM_")
        self.data_texture_filename = os.path.join(TEST_DATA_DIR, "uv_checker.png")
        self.export_options = RT.UsdExporter.CreateOptions()
        self.export_options.FileFormat = RT.Name("ascii")
        self.export_options.RootPrimPath = "/"
        self.export_options.LogLevel = RT.Name("warn")

        usd_test_helpers.load_usd_plugins()
        RT.resetMaxFile(RT.Name("noprompt"))
        self.parent_path = Sdf.Path("/tests")
        box = RT.Box(name="test_box")
        self.box = box

        box.mapcoords = True
        box.material = RT.PhysicalMaterial()

    def is_connected(self, target_shader, input_name, source_shader):
        _input = target_shader.GetInput(input_name)
        self.assertTrue(_input.HasConnectedSource())
        valid_connections, _ = _input.GetConnectedSources()
        self.assertEqual(len(valid_connections), 1)
        self.assertEqual(valid_connections[0].source.GetPath(), source_shader.GetPath())


    def bitmap_transform_test(self, test_name, bitmap_type, wrap_mode, real_world_scale, tiling, scale, real_size, offset, rotate, rot_center, rot_axis, expected_s_wrap, expected_t_wrap, expected_scale, expected_translate, expected_rotate, expected_warnings = 0):
        # type: () -> None
        """Test bitmap transform export for the given configuration."""

        # Setup logging.
        self.export_options.LogPath = "{0}{1}{2}".format(self.output_prefix, test_name, ".log")
        if os.path.exists(self.export_options.LogPath):
            os.remove(self.export_options.LogPath)

        test_bitmap_name = "uv_checker_bitmap"

        # Build the map from the given type / config.
        if bitmap_type == BitmapType.BITMAP_TEXTURE:
            test_bitmap = create_texture_bitmap(self.data_texture_filename, test_bitmap_name, wrap_mode, tiling, offset, rotate, real_world_scale, real_size)
        elif bitmap_type == BitmapType.OSL_BITMAP:
            osl_map = create_osl_bitmap(self.data_texture_filename, test_bitmap_name, wrap_mode, scale)
            test_bitmap = create_osl_multi_channel(osl_map, "rgb")
        elif bitmap_type == BitmapType.OSL_UBERBITMAP:
            osl_map = create_osl_uberbitmap(self.data_texture_filename, test_bitmap_name, wrap_mode, tiling, scale, real_size, offset, rotate, rot_center, rot_axis, real_world_scale)
            test_bitmap = create_osl_multi_channel(osl_map, "rgb")

        # Connect it to our material.
        self.box.material.base_color_map = test_bitmap

        # Export to usd.
        test_usd_file_path = "{0}{1}{2}".format(self.output_prefix, test_name, ".usda")

        ret = RT.USDExporter.ExportFile(test_usd_file_path, exportOptions=self.export_options, contentSource=RT.Name("nodeList"), nodeList=[self.box])
        self.assertTrue(ret)

        # Test that a transform2d was created and connected:
        # primvarReader.outputs:result -> transform2d.outputs:result -> uv_checker.inputs:st
        stage = Usd.Stage.Open(test_usd_file_path)
        self.assertIsInstance(stage, Usd.Stage)
        
        texture_node_graph_path = "/mtl/{0}".format(test_bitmap_name)
        texture_node_graph = stage.GetPrimAtPath(texture_node_graph_path)
        self.assertTrue(texture_node_graph.IsValid())
        self.assertTrue(texture_node_graph.IsA(UsdShade.NodeGraph))
        
        uv_checker_path = "/mtl/{0}/{1}".format(test_bitmap_name, test_bitmap_name)
        uv_checker_prim = stage.GetPrimAtPath(uv_checker_path)
        self.assertTrue(uv_checker_prim.IsValid())
        self.assertTrue(uv_checker_prim.IsA(UsdShade.Shader))
        usd_uv_checker = UsdShade.Shader(uv_checker_prim)
        self.assertEqual(usd_uv_checker.GetShaderId(), "UsdUVTexture")

        usd_primvar_reader_path = "/mtl/{0}/PrimvarReader_st".format(test_bitmap_name)
        usd_primvar_reader_prim = stage.GetPrimAtPath(usd_primvar_reader_path)
        self.assertTrue(usd_primvar_reader_prim.IsValid())
        self.assertTrue(usd_primvar_reader_prim.IsA(UsdShade.Shader))
        usd_primvar_reader = UsdShade.Shader(usd_primvar_reader_prim)
        self.assertEqual(usd_primvar_reader.GetShaderId(), "UsdPrimvarReader_float2")

        # The final case "(scale == 1.0 and (expected_scale[0] != 1.0 or expected_scale[1] != 1.0)" is to cover
        # the test_export_bitmap_texture_wrap_modes() cases, which will create UsdTransform2d in certain cases
        if scale != 1.0 or offset[0] != 0.0 or offset[1] != 0.0 or tiling[0] != 1.0 or tiling[1] != 1.0 or rotate[0] != 0.0 or rotate[1] != 0.0 or rotate[2] != 0.0 or real_size[0] != 1.0 or real_size[1] != 1.0 or (scale == 1.0 and (expected_scale[0] != 1.0 or expected_scale[1] != 1.0)):
            usd_transform_2d_path = "/mtl/{0}/TextureTransform_st".format(test_bitmap_name)
            usd_transform_2d_prim = stage.GetPrimAtPath(usd_transform_2d_path)
            self.assertTrue(usd_transform_2d_prim.IsValid())
            self.assertTrue(usd_transform_2d_prim.IsA(UsdShade.Shader))
            usd_transform_2d = UsdShade.Shader(usd_transform_2d_prim)
            self.assertEqual(usd_transform_2d.GetShaderId(), "UsdTransform2d")

            self.is_connected(usd_uv_checker, "st", usd_transform_2d)
            self.is_connected(usd_transform_2d, "in", usd_primvar_reader)

            # Validate UsdTransform2d inputs values.

            # scale...
            scale_input = usd_transform_2d.GetInput('scale')
            scale_value = scale_input.Get()

            if scale_value  == None:
                self.assertEqual(None, expected_scale)
            else:
                self.assertAlmostEqual(expected_scale[0], scale_value[0], places=5)
                self.assertAlmostEqual(expected_scale[1], scale_value[1], places=5)

            # Translation...
            translation_input = usd_transform_2d.GetInput('translation')
            translation_value = translation_input.Get()

            if translation_value == None:
                self.assertEqual(None, expected_translate)
            else:
                self.assertAlmostEqual(expected_translate[0], translation_value[0], places=5)
                self.assertAlmostEqual(expected_translate[1], translation_value[1], places=5)

            # Rotation...
            rotation_input = usd_transform_2d.GetInput('rotation')
            rotation_value = rotation_input.Get()

            if rotation_value == None:
                self.assertEqual(None, expected_rotate)
            else:
                self.assertAlmostEqual(expected_rotate, rotation_value, places=5)

            # Finaly, check warnings.
            log_file_handle = open(self.export_options.LogPath)
            self.assertGreaterEqual(expected_warnings, len(log_file_handle.readlines()))
            log_file_handle.close()
        else:
            self.is_connected(usd_uv_checker, "st", usd_primvar_reader)

        # Make sure we get the correct wrapping mode applied.
        self.assertEqual(usd_uv_checker.GetInput("wrapS").Get(), expected_s_wrap)
        self.assertEqual(usd_uv_checker.GetInput("wrapT").Get(), expected_t_wrap)

    def test_export_bitmap_texture_scale(self):
        # Bitmap texture, scale only.
        self.bitmap_transform_test("bitmap_texture_scale",
            BitmapType.BITMAP_TEXTURE,
            WrapModes.REPEAT_ST,
            False, # Use world scale
            (0.5, 0.2), # tiling
            None, # Scale, not supported for bitmap texture.
            (1.0,1.0), # Real size
            (0.0, 0.0), # Offset
            (0.0,0.0,0.0), # Rotation
            None, # Rotation center
            None, # Rotation axis.
            # Expected results in USD :
            "repeat", #wrapS
            "repeat", #wrapT
            (0.5, 0.2), # Scale
            (0.25, 0.4), # Translation
            None) # Rotation

    def test_export_bitmap_texture_scale_negative_u(self):
        # Test special handling of negative scales. In the UVGen class, negative scaling is implemented
        # through rotations, but we make sure to export negative scales to USD instead, as only Z rotations 
        # can be exported.
    
        # Bitmap texture, negative U scale.
        self.bitmap_transform_test("bitmap_texture_scale_negative_u",
            BitmapType.BITMAP_TEXTURE,
            WrapModes.REPEAT_ST,
            False, # Use world scale
            (-0.5, 0.2), # tiling
            None, # Scale, not supported for bitmap texture.
            (1.0,1.0), # Real size
            (0.0, 0.0), # Offset
            (0.0,0.0,0.0), # Rotation
            None, # Rotation center
            None, # Rotation axis.
            # Expected results in USD :
            "repeat", #wrapS
            "repeat", #wrapT
            (-0.5, 0.2), # Scale
            (0.75, 0.4), # Translation
            None) # Rotation

    def test_export_bitmap_texture_scale_negative_v(self):        
        # Bitmap texture, negative V scale.
        self.bitmap_transform_test("bitmap_texture_scale_negative_v",
            BitmapType.BITMAP_TEXTURE,
            WrapModes.REPEAT_ST,
            False, # Use world scale
            (0.5, -0.2), # tiling
            None, # Scale, not supported for bitmap texture.
            (1.0,1.0), # Real size
            (0.0, 0.0), # Offset
            (0.0,0.0,0.0), # Rotation
            None, # Rotation center
            None, # Rotation axis.
            # Expected results in USD :
            "repeat", #wrapS
            "repeat", #wrapT
            (0.5, -0.2), # Scale
            (0.25, 0.6), # Translation
            None) # Rotation

    def test_export_bitmap_texture_scale_negative_u_v(self):            
        # Bitmap texture, negative U and V scale.
        self.bitmap_transform_test("bitmap_texture_scale_negative_u_v",
            BitmapType.BITMAP_TEXTURE,
            WrapModes.REPEAT_ST,
            False, # Use world scale
            (-0.5, -0.2), # tiling
            None, # Scale, not supported for bitmap texture.
            (1.0,1.0), # Real size
            (0.0, 0.0), # Offset
            (0.0,0.0,0.0), # Rotation
            None, # Rotation center
            None, # Rotation axis.
            # Expected results in USD :
            "repeat", #wrapS
            "repeat", #wrapT
            (-0.5, -0.2), # Scale
            (0.75, 0.6), # Translation
            None) # Rotation

    def test_export_bitmap_texture_world_scale(self):
        # Bitmap texture, real world scale
        self.bitmap_transform_test("bitmap_texture_world_scale",
            BitmapType.BITMAP_TEXTURE,
            WrapModes.REPEAT_ST,
            True, # Use world scale
            (1.0, 1.0), # Tiling
            None, # Scale
            (15.0, 10.0), # Real size
            (0.0, 0.0), # Offset
            (0.0,0.0,0.0), # Rotation
            None, # Rotation center
            None, # Rotation axis
            # Expected results in USD :
            "repeat", #wrapS
            "repeat", #wrapT
            (0.066666, 0.1), # Scale
            None, # Translation
            None) # Rotation

    def test_export_bitmap_texture_offset(self):
        # Bitmap texture, offset
        self.bitmap_transform_test("bitmap_texture_offset",
            BitmapType.BITMAP_TEXTURE,
            WrapModes.REPEAT_ST,
            False, # Use world scale
            (1.0, 1.0), # Tiling
            None, # Scale
            (1.0, 1.0), # Real size
            (0.3, 0.7), # Offset
            (0.0,0.0,0.0), # Rotation
            None, # Rotation center
            None,
            # Expected results in USD :
            "repeat", #wrapS
            "repeat", #wrapT
            None, # Scale
            (-0.3, -0.7), # Translation
            None) # Rotation

    def test_export_bitmap_texture_rotation(self):
        # Bitmap texture, rotation
        self.bitmap_transform_test("bitmap_texture_rotation",
            BitmapType.BITMAP_TEXTURE,
            WrapModes.REPEAT_ST,
            False, # use world scale
            (1.0, 1.0), # Tiling
            None, # Scale
            (1.0, 1.0), # Real size
            (0.0, 0.0), # Offset
            (0.0,0.0,45.0), # Rotation
            None, # Rotation center
            None, # Rotation axis
            # Expected results in USD :
            "repeat", #wrapS
            "repeat", #wrapT
            None, # Scale
            (0.5, -0.207106), # Translation
            45.0) # Rotation

    def test_export_bitmap_texture_unsupported_rotation(self):
        # Bitmap texture, usupported rotation.
        self.bitmap_transform_test("bitmap_texture_unsupported_rotation",
            BitmapType.BITMAP_TEXTURE,
            WrapModes.REPEAT_ST,
            False, # Use world scale
            (1.0, 1.0), # Tiling
            None, # Scale
            (1.0, 1.0), # Real size
            (0.0, 0.0), # Offset
            (0.0, 45.0, 0.0), # Rotation
            None, # Rotation center
            None, # Rotatin axis
            # Expected results in USD :
            "repeat", #wrapS
            "repeat", #wrapT
            None, # Scale
            (-0.207106, 0.0), # Translation
            None, # Rotation
            1) # Expect a warning!

    def test_export_bitmap_texture_rotation_with_nonuniform_scale_with_rotation(self):
        # Bitmap texture, non-uniform scaling + rotation.
        self.bitmap_transform_test("bitmap_texture_rotation_with_nonuniform_scale_with_rotation",
            BitmapType.BITMAP_TEXTURE,
            WrapModes.REPEAT_ST,
            False, # Use world scale
            (1.0, 2.0), # Tiling
            None, # Scale
            (1.0, 1.0), # Real size
            (0.0, 0.0), # Offset
            (0.0, 0.0, 45.0), # Rotation
            None, # Rotation center
            None, # Rotation axis
            # Expected results in USD :
            "repeat", #wrapS
            "repeat", #wrapT
            (1.0, 2.0), # Scale
            (0.5, -0.914213), # Translation
            45.0, # Rotation
            1) # Expect a warning!

    def test_export_bitmap_texture_all_transforms(self):
        # Bitmap texture, all transforms.
        self.bitmap_transform_test("bitmap_texture_all_transforms",
            # Max bitmap config
            BitmapType.BITMAP_TEXTURE,
            WrapModes.REPEAT_ST,
            False, # Use world scale
            (0.5, 0.5), # Tiling
            None, # Scale
            (1.0, 1.0), # Real size
            (0.4, 0.8), # Offset
            (0.0,0.0,30.0), # Rotation
            None, # Rotation center
            None, # Rotation axis
            # Expected results in USD :
            "repeat", #wrapS
            "repeat", #wrapT
            (0.5, 0.5), # Scale
            (0.435288, -0.287916), # Translation
            30.0) # Rotation

    def test_export_bitmap_texture_wrap_modes(self):
        # (wrap mode, expected wrapS, expected wrapT, expected scale S, expected scale U)
        wrap_test_cases = [(WrapModes.REPEAT_ST, "repeat", "repeat", 1.0, 1.0),
                           (WrapModes.REPEAT_S, "repeat", "black", 1.0, 1.0),
                           (WrapModes.REPEAT_T, "black", "repeat", 1.0, 1.0),
                           (WrapModes.MIRROR_ST, "mirror", "mirror", 2.0, 2.0),
                           (WrapModes.MIRROR_S, "mirror", "black", 2.0, 1.0),
                           (WrapModes.MIRROR_T, "black", "mirror", 1.0, 2.0),
                           (WrapModes.MIRROR_S_REPEAT_T, "mirror", "repeat", 2.0, 1.0),
                           (WrapModes.REPEAT_S_MIRROR_T, "repeat", "mirror", 1.0, 2.0),
                           (WrapModes.BLACK, "black", "black", 1.0, 1.0)]

        for case in wrap_test_cases:
            self.bitmap_transform_test("bitmap_texture_wrap_test_" + str(case[0]) ,
                BitmapType.BITMAP_TEXTURE,
                case[0],
                False, # Use world scale
                (1.0, 1.0), # Tiling
                1.0, # Scale
                (1.0, 1.0), # Real size
                (0.0, 0.0), # Offset
                (0.0, 0.0, 0.0), # Rotation
                None, # Rotation center
                None, # Rotation axis
                # Expected results in USD :
                case[1], #wrapS
                case[2], #wrapT
                (case[3], case[4]), # Scale
                None, # Translation
                None) # Rotation


    def test_export_osl_uberbitmap_scaling(self):
        # OSLUberbitmap, scale.
        self.bitmap_transform_test("osl_uberbitmap_scaling",
            BitmapType.OSL_UBERBITMAP,
            WrapModes.REPEAT_ST,
            False, # Use world scale
            (0.5, 0.2), # Tiling
            2.0, # Scale
            (1.0,1.0), # Real size
            (0.0, 0.0), # Offset
            (0.0,0.0,0.0), # Rotation
            (0.5,0.5,0.0), # Rotation center
            (0.0,0.0,1.0), # Rotation axis
            # Expected results in USD :
            "repeat", #wrapS
            "repeat", #wrapT
            (0.25, 0.1), # Scale
            None, # Translation
            None) # Rotation

    def test_export_osl_uberbitmap_real_world_scale(self):
        # OSLUberbitmap, real world scale.
        self.bitmap_transform_test("osl_uberbitmap_real_world_scale",
            BitmapType.OSL_UBERBITMAP,
            WrapModes.REPEAT_ST,
            True, # Use world scale
            (1.0, 1.0), # Tiling
            1.0, # Scale
            (10.0, 15.0), # Real size
            (0.0, 0.0), # Offset
            (0.0, 0.0, 0.0), # Rotation
            (0.5,0.5,0.0), # Rotation center
            (0.0,0.0,1.0), # Rotation axis
            # Expected results in USD :
            "repeat", #wrapS
            "repeat", #wrapT
            (0.1, 0.066666), # Scale
            (0.0, 0.0), # Translation
            None) # Rotation

    def test_export_osl_uberbitmap_offset(self):
        # OSLUberbitmap, offset.
        self.bitmap_transform_test("osl_uberbitmap_offset",
            BitmapType.OSL_UBERBITMAP,
            WrapModes.REPEAT_ST,
            False, # Use world scale
            (1.0, 1.0), # Tiling
            1.0, # Scale
            (1.0, 1.0), # Real size
            (0.15, 0.85), # Offset
            (0.0,0.0,0.0), # Rotation
            (0.5,0.5,0.0), # Rotation center
            (0.0,0.0,1.0), # Rotation axis.
            # Expected results in USD :
            "repeat", #wrapS
            "repeat", #wrapT
            None, # Scale
            (-0.15, -0.85), # Translation
            None) # Rotation

    def test_export_osl_uberbitmap_rotation(self):
        # OSLUberbitmap, rotation.
        self.bitmap_transform_test("osl_uberbitmap_rotation",
            BitmapType.OSL_UBERBITMAP,
            WrapModes.REPEAT_ST,
            False, # Use world scale
            (1.0, 1.0), # Tiling
            1.0, # Scale
            (1.0, 1.0), # Real size
            (0.0, 0.0), # Offset
            (0.0,0.0,30.0), # Rotation
            (0.7,0.7,0.0), # Rotation center
            (0.0,0.0,1.0), # Rotation axis
            # Expected results in USD :
            "repeat", #wrapS
            "repeat", #wrapT
            None, # Scale
            (0.443782, -0.256217), # Translation
            30.0) # Rotation

    def test_export_osl_uberbitmap_rotation_on_non_z_axis(self):
        # OSLUberbitmap, rotation around arbitrary axis (other than Z) should be ignored.
        self.bitmap_transform_test("osl_uberbitmap_rotation_on_non_z_axis",
            BitmapType.OSL_UBERBITMAP,
            WrapModes.REPEAT_ST,
            False, # Use world scale
            (1.0, 1.0), # Tiling
            1.0, # Scale
            (1.0, 1.0), # Real size
            (0.0, 0.0), # Offset
            (0.0,0.0,30.0), # Rotation
            (0.7,0.7,0.0), # Rotation center
            (1.0,1.0,1.0), # Rotation axis
            # Expected results in USD :
            "repeat", #wrapS
            "repeat", #wrapT
            None, # Scale
            None, # Translation
            30.0, # Rotation
            1) # Expect a warning!

    def test_export_osl_uberbitmap_all_transforms(self):
        # OSLUberbitmap, all transforms.
        self.bitmap_transform_test("osl_uberbitmap_all_transforms",
            BitmapType.OSL_UBERBITMAP,
            WrapModes.REPEAT_ST,
            False, # Use world scale
            (1.5, 1.5), # Tiling
            0.1, # Scale
            (1.0, 1.0), # Real size
            (0.2, 0.4), # Offset
            (0.0,0.0,30.0), # Rotation
            (0.1,0.3,0.0), # Rotation center
            (0.0,0.0,1.0), # Rotation axis
            # Expected results in USD :
            "repeat", #wrapS
            "repeat", #wrapT
            (15.0, 15.0), # scale
            (2.852885, -6.843266), # translation
            30.0) # rotation

    def test_export_osl_bitmap_scale(self):
        # OslBitmap, scale.
        self.bitmap_transform_test("osl_bitmap_scale",
            BitmapType.OSL_BITMAP,
            WrapModes.REPEAT_ST,
            None, # Use world scale
            None, # Tiling
            8.0, # Scale
            None, # Real size
            None, # Offset
            None, # Rotation
            None, # Rotation center
            None,
            # Expected results in USD :
            "repeat", #wrapS
            "repeat", #wrapT
            (0.125, 0.125), # Scale
            None, # Translation
            None, # Rotation
            1) # One warning for the Pos_map input that we need to set the UVW channel.

    def test_export_osl_bitmap_wrap_modes(self):
        osl_maps = [BitmapType.OSL_UBERBITMAP, BitmapType.OSL_BITMAP]

        # (WrapMode, expected wrapS, expected wrapT)
        wrap_test_cases = [(WrapModes.REPEAT_ST, "repeat", "repeat"),
                           (WrapModes.MIRROR_ST, "mirror", "mirror"),
                           (WrapModes.CLAMP, "clamp", "clamp"),
                           (WrapModes.DEFAULT, "black", "black"),
                           (WrapModes.BLACK, "black", "black")]
        for map_type in osl_maps:
            for case in wrap_test_cases:
                self.bitmap_transform_test("osl_"+ str(map_type) +"_wrap_test_" + str(case[0]),
                    map_type,
                    case[0], # Wrap mode
                    False, # Use world scale
                    (1.0, 1.0), # Tiling
                    0.10, # Scale
                    (1.0, 1.0), # Real size
                    (0.0, 0.0), # Offset
                    (0.0,0.0,0.0), # Rotation
                    (0.5,0.5,0.0), # Rotation center
                    (0.0,0.0,1.0), # Rotation axis
                    # Expected results in USD :
                    case[1], #wrapS
                    case[2], #wrapT
                    (10.0, 10.0), # scale
                    (0.0, 0.0) if map_type == BitmapType.OSL_UBERBITMAP else None, # Translation
                    None if map_type == BitmapType.OSL_UBERBITMAP else None, # Rotation
                    0 if map_type == BitmapType.OSL_UBERBITMAP  else 1) # Warning because Pos_map is not fully supported for OSLBitmap but needed to set the UVW channel.

def run_tests():
    return unittest.TextTestRunner(stream=sys.stdout, verbosity=2).run(unittest.TestLoader().loadTestsFromTestCase(TestExportTextureTransform))

if __name__ == "__main__":
    reload(usd_material_writer)
    RT.clearListener()
    run_tests()