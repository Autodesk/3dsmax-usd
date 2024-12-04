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

from pxr import Usd, UsdGeom, UsdShade, Sdf, Kind, Vt

import usd_test_helpers
import shaderReader
import pymxs

__author__ = r'Autodesk Inc.'
__copyright__ = r'Copyright 2021, Autodesk Inc.'

RT = pymxs.runtime
RT.pluginManager.loadClass(RT.USDImporter)

TEST_DIR = os.path.dirname(__file__)
TEST_DATA_DIR = os.path.join(TEST_DIR, "data")
UV_CHECKER_PATH = os.path.join(TEST_DATA_DIR, "uv_checker.png")


class TestImportTextureTransform(unittest.TestCase):
    def setUp(self):
        RT.resetMaxFile(RT.Name("noprompt"))
        self.output_prefix = usd_test_helpers.standard_output_prefix("IMPORT_TEXTURE_TRANSFORM_PYMXS_TEST_")
        usd_test_helpers.load_usd_plugins()

        self.stage = Usd.Stage.CreateInMemory()
        UsdGeom.SetStageUpAxis(self.stage, UsdGeom.Tokens.y)

        self.import_options = RT.USDImporter.CreateOptions()
        self.import_options.LogLevel = RT.Name("warn")

        self.create_usd_preview_surface_with_usd_transform_2d()

    # Creates a basic UsdPreviewSurface material and UsdUVTexture shader with
    # UsdTransform2d transform inputs, attached to a cube mesh
    def create_usd_preview_surface_with_usd_transform_2d(self):
        #create material container
        some_mat = UsdShade.Material.Define(self.stage, '/some_mat')

        # create UsdPreviewSurface material shader
        some_shader = UsdShade.Shader.Define(self.stage, '/some_mat/UsdPreviewSurface')
        some_shader.CreateIdAttr("UsdPreviewSurface")
        some_mat.CreateSurfaceOutput().ConnectToSource(some_shader.ConnectableAPI(), "surface")


        # create UsdUVTexture shader texture reader
        texture_sampler = UsdShade.Shader.Define(self.stage, '/uv_checker_bitmap')
        texture_sampler.CreateIdAttr("UsdUVTexture")
        texture_sampler.CreateInput("file", Sdf.ValueTypeNames.Asset).Set(UV_CHECKER_PATH)

        self.wrap_s_input = texture_sampler.CreateInput("wrapS", Sdf.ValueTypeNames.Token)
        self.wrap_t_input = texture_sampler.CreateInput("wrapT", Sdf.ValueTypeNames.Token)

        texture_sampler.CreateOutput('rgb', Sdf.ValueTypeNames.Float3)
        some_shader.CreateInput("diffuseColor", Sdf.ValueTypeNames.Color3f).ConnectToSource(texture_sampler.ConnectableAPI(), 'rgb')

        # create UsdPrimvarReader_float2 shader texture reader
        uv_reader = UsdShade.Shader.Define(self.stage, '/uv_checker_bitmap/PrimvarReader_map1')
        uv_reader.CreateIdAttr("UsdPrimvarReader_float2")

        input_frame_primvar = some_mat.CreateInput("frame:map1", Sdf.ValueTypeNames.Token)
        input_frame_primvar.Set('map1')
        input_varname = uv_reader.CreateInput("varname", Sdf.ValueTypeNames.Token)
        input_varname.ConnectToSource(input_frame_primvar)

        st_input = texture_sampler.CreateInput("st", Sdf.ValueTypeNames.Float2)


        # create UsdTransform2d shader to support texture transforms (offset,translate,rotate)
        transform_2d_prim = UsdShade.Shader.Define(self.stage, '/uv_checker_bitmap/TextureTransform_map1')
        transform_2d_prim.CreateIdAttr("UsdTransform2d")

        self.scale_input = transform_2d_prim.CreateInput("scale", Sdf.ValueTypeNames.Float2)
        self.rotation_input = transform_2d_prim.CreateInput("rotation", Sdf.ValueTypeNames.Float)
        self.translation_input = transform_2d_prim.CreateInput("translation", Sdf.ValueTypeNames.Float2)

        transform_input = transform_2d_prim.CreateInput("in", Sdf.ValueTypeNames.Float2)
        transform_input.ConnectToSource(uv_reader.ConnectableAPI(), "result")
        st_input.ConnectToSource(transform_2d_prim.ConnectableAPI(), "result")

        # create basic cube
        cubePrim = UsdGeom.Cube.Define(self.stage, '/test_cube')

        cubePrim = UsdGeom.Mesh.Define(self.stage, "/test_cube")
        cubePrim.CreatePointsAttr([(-12.5, -12.5, 0), (12.5, -12.5, 0), (-12.5, 12.5, 0), (12.5, 12.5, 0), (-12.5, -12.5, 25), (12.5, -12.5, 25), (-12.5, 12.5, 25), (12.5, 12.5, 25)])
        cubePrim.CreateFaceVertexCountsAttr([4, 4, 4, 4, 4, 4])
        cubePrim.CreateFaceVertexIndicesAttr([0, 2, 3, 1, 4, 5, 7, 6, 0, 1, 5, 4, 1, 3, 7, 5, 3, 2, 6, 7, 2, 0, 4, 6])
        cubePrim.CreateExtentAttr([(-12.5, -12.5, 0), (12.5, 12.5, 25)])
        
        texCoords = UsdGeom.PrimvarsAPI(cubePrim).CreatePrimvar("map1", 
                                            Sdf.ValueTypeNames.TexCoord2fArray, 
                                            UsdGeom.Tokens.faceVarying)
        texCoords.Set([(1, 0), (1, 1), (0, 1), (0, 0), (0, 0), (1, 0), (1, 1), (0, 1), (0, 0), (1, 0), (1, 1), (0, 1), (0, 0), (1, 0), (1, 1), (0, 1), (0, 0), (1, 0), (1, 1), (0, 1), (0, 0), (1, 0), (1, 1), (0, 1)])

        normals = UsdGeom.PrimvarsAPI(cubePrim).CreatePrimvar("normals", Sdf.ValueTypeNames.Float3Array, UsdGeom.Tokens.faceVarying)
        normals.Set([(0, 0, -1), (0, 0, -1), (0, 0, -1), (0, 0, -1), (0, 0, 1), (0, 0, 1), (0, 0, 1), (0, 0, 1), (0, -1, 0), (0, -1, 0), (0, -1, 0), (0, -1, 0), (1, 0, 0), (1, 0, 0), (1, 0, 0), (1, 0, 0), (0, 1, 0), (0, 1, 0), (0, 1, 0), (0, 1, 0), (-1, 0, 0), (-1, 0, 0), (-1, 0, 0), (-1, 0, 0)])

        # bind the material to the cube
        binding = UsdShade.MaterialBindingAPI.Apply(cubePrim.GetPrim()).Bind(some_mat)

    def import_usd_preview_surface_with_usd_transform_2d_transform_test(self,
                                                                        test_usd_file_name,
                                                                        scale_usd_value, 
                                                                        rotation_usd_value, 
                                                                        translation_usd_value,
                                                                        wrap_s_usd_value,
                                                                        wrap_t_usd_value, 
                                                                        expected_max_offset_value,
                                                                        expected_max_scale_value,
                                                                        expected_max_tiling_value,
                                                                        expected_max_rotate_value,
                                                                        expected_max_rotaxis_value,
                                                                        expected_max_rotcenter_value,
                                                                        expected_max_wrap_mode_value,
                                                                        expected_warnings = 0):
        # type: () -> None
        """Test UsdPreviewSurface with UsdUVTexture texture transform import for the given configuration."""

        # Setup logging.
        self.import_options.LogPath = "{0}{1}{2}".format(self.output_prefix, test_usd_file_name, ".log")
        if os.path.exists(self.import_options.LogPath):
            os.remove(self.import_options.LogPath)

        test_usd_file_path = "{0}{1}{2}".format(self.output_prefix, test_usd_file_name, ".usda")

        if scale_usd_value is not None:
            self.scale_input.Set(scale_usd_value)

        if rotation_usd_value is not None:
            self.rotation_input.Set(rotation_usd_value)

        if translation_usd_value is not None:
            self.translation_input.Set(translation_usd_value)

        if wrap_s_usd_value is not None:
            self.wrap_s_input.Set(wrap_s_usd_value)

        if wrap_t_usd_value is not None:
            self.wrap_t_input.Set(wrap_t_usd_value)

        self.stage.GetRootLayer().Export(test_usd_file_path)
        
        self.import_options.PreferredMaterial = "maxUsdPreviewSurface"

        maxver = RT.maxversion()
        if maxver[0] >= 26000:  # 3ds Max 2024 and up
            shaderReader._material_import_options["texture_target_id"] = "UberBitmap2.osl"
        else:
            shaderReader._material_import_options["texture_target_id"] = "UberBitmap.osl"

        RT.USDImporter.ImportFile(test_usd_file_path,importOptions=self.import_options)

        self.assertEqual(len(RT.sceneMaterials), 1)
        max_material = RT.sceneMaterials[0]

        self.assertEqual(RT.ClassOf(max_material), RT.MaxUsdPreviewSurface)

        self.assertIsNotNone(max_material.diffuseColor_map)
        diffuseColorSourceMap = max_material.diffuseColor_map.sourceMap

        self.assertEqual(RT.ClassOf(diffuseColorSourceMap), RT.OSLMap)
        if maxver[0] >= 26000:  # 3ds Max 2024 and up
            self.assertEqual(diffuseColorSourceMap.OSLShaderName, "UberBitmap2b")
        else:
            self.assertEqual(diffuseColorSourceMap.OSLShaderName, "UberBitmap2")

        self.assertAlmostEqual(diffuseColorSourceMap.offset[0], expected_max_offset_value[0], places=3)
        self.assertAlmostEqual(diffuseColorSourceMap.offset[1], expected_max_offset_value[1], places=3)
        self.assertAlmostEqual(diffuseColorSourceMap.offset[2], expected_max_offset_value[2], places=3)

        self.assertAlmostEqual(diffuseColorSourceMap.scale, expected_max_scale_value, places=3)

        self.assertAlmostEqual(diffuseColorSourceMap.tiling[0], expected_max_tiling_value[0], places=3)
        self.assertAlmostEqual(diffuseColorSourceMap.tiling[1], expected_max_tiling_value[1], places=3)
        self.assertAlmostEqual(diffuseColorSourceMap.tiling[2], expected_max_tiling_value[2], places=3)
        
        self.assertAlmostEqual(diffuseColorSourceMap.Rotate, expected_max_rotate_value, places=3)
        
        self.assertAlmostEqual(diffuseColorSourceMap.RotAxis[0], expected_max_rotaxis_value[0], places=3)
        self.assertAlmostEqual(diffuseColorSourceMap.RotAxis[1], expected_max_rotaxis_value[1], places=3)
        self.assertAlmostEqual(diffuseColorSourceMap.RotAxis[2], expected_max_rotaxis_value[2], places=3)

        self.assertAlmostEqual(diffuseColorSourceMap.RotCenter[0], expected_max_rotcenter_value[0], places=3)
        self.assertAlmostEqual(diffuseColorSourceMap.RotCenter[1], expected_max_rotcenter_value[1], places=3)
        self.assertAlmostEqual(diffuseColorSourceMap.RotCenter[2], expected_max_rotcenter_value[2], places=3)

        self.assertEqual(diffuseColorSourceMap.WrapMode, expected_max_wrap_mode_value)        

        # Finaly, check warnings.
        log_file_handle = open(self.import_options.LogPath)
        self.assertEqual(expected_warnings, len(log_file_handle.readlines()))
        log_file_handle.close()

    def test_usd_preview_surface_texture_to_OSL_ubermap_scaling(self):
        
        # Test scale
        self.import_usd_preview_surface_with_usd_transform_2d_transform_test(
            "test_usd_preview_surface_texture_to_OSL_ubermap_scaling",
            (15.0, 15.0), 
            0.0, 
            (0.0, 0.0),
            None,
            None, 
            (0.0, 0.0, 0.0),
            0.067,
            (1.0, 1.0, 1.0),
            0.0,
            (0.0, 0.0, 1.0),
            (0.5, 0.5, 0.0),
            "periodic")

    def test_usd_preview_surface_texture_to_OSL_ubermap_negative_scaling(self):
        # Test negative scale
        self.import_usd_preview_surface_with_usd_transform_2d_transform_test(
            "test_usd_preview_surface_texture_to_OSL_ubermap_negative_scaling",
            (-15.0, -15.0), 
            0.0, 
            (0.0, 0.0),
            None,
            None,
            (0.0, 0.0, 0.0),
            -0.067,
            (1.0, 1.0, 1.0),
            0.0,
            (0.0, 0.0, 1.0),
            (0.5, 0.5, 0.0),
            "periodic")

    def test_usd_preview_surface_texture_to_OSL_ubermap_non_uniform_scaling(self):
        # Test non-uniform scale
        self.import_usd_preview_surface_with_usd_transform_2d_transform_test(
            "test_usd_preview_surface_texture_to_OSL_ubermap_non_uniform_scaling",
            (15.0, 25.0), 
            0.0, 
            (0.0, 0.0), 
            None,
            None,
            (0.0, 0.0, 0.0),
            1.0,
            (15.0, 25.0, 0.0),
            0.0,
            (0.0, 0.0, 1.0),
            (0.5, 0.5, 0.0),
            "periodic")

    def test_usd_preview_surface_texture_to_OSL_ubermap_zero_scaling(self):
        # Test zero value scale
        self.import_usd_preview_surface_with_usd_transform_2d_transform_test(
            "test_usd_preview_surface_texture_to_OSL_ubermap_zero_scaling",
            (0.0, 0.0), 
            0.0, 
            (0.0, 0.0), 
            None,
            None,
            (float("inf"), float("inf"), 0.0),
            float("inf"),
            (1.0, 1.0, 1.0),
            0.0,
            (0.0, 0.0, 1.0),
            (0.5, 0.5, 0.0),
            "periodic")

    def test_usd_preview_surface_texture_to_OSL_ubermap_non_uniform_scaling_with_zero(self):
        # Test non-uniform scale with zero value
        self.import_usd_preview_surface_with_usd_transform_2d_transform_test(
            "test_usd_preview_surface_texture_to_OSL_ubermap_non_uniform_scaling_with_zero",
            (15.0, 0.0), 
            0.0, 
            (0.0, 0.0), 
            None,
            None,
            (0.0, float("inf"), 0.0),
            1.0,
            (15.0, float("inf"), 0.0),
            0.0,
            (0.0, 0.0, 1.0),
            (0.5, 0.5, 0.0),
            "periodic")

    def test_usd_preview_surface_texture_to_OSL_ubermap_rotation(self):
        # Test rotation
        self.import_usd_preview_surface_with_usd_transform_2d_transform_test(
            "test_usd_preview_surface_texture_to_OSL_ubermap_rotation",
            (1.0, 1.0), 
            30.0, 
            (0.0, 0.0), 
            None,
            None,
            (0.0, 0.0, 0.0),
            1.0,
            (1.0, 1.0, 1.0),
            30.0,
            (0.0, 0.0, 1.0),
            (0.0, 0.0, 0.0),
            "periodic")

    def test_usd_preview_surface_texture_to_OSL_ubermap_negative_rotation(self):
        # Test negative rotation
        self.import_usd_preview_surface_with_usd_transform_2d_transform_test(
            "test_usd_preview_surface_texture_to_OSL_ubermap_negative_rotation",
            (1.0, 1.0), 
            -30.0, 
            (0.0, 0.0),
            None,
            None, 
            (0.0, 0.0, 0.0),
            1.0,
            (1.0, 1.0, 1.0),
            -30.0,
            (0.0, 0.0, 1.0),
            (0.0, 0.0, 0.0),
            "periodic")

    def test_usd_preview_surface_texture_to_OSL_ubermap_rotation_with_non_uniform_scale(self):
        # Test rotation with non-uniform scale
        self.import_usd_preview_surface_with_usd_transform_2d_transform_test(
            "test_usd_preview_surface_texture_to_OSL_ubermap_rotation_with_non_uniform_scale",
            (4.0, 2.0), 
            30.0, 
            (0.0, 0.0), 
            None,
            None,
            (0.0, 0.0, 0.0),
            1.0,
            (4.0, 2.0, 0.0),
            30.0,
            (0.0, 0.0, 1.0),
            (0.0, 0.0, 0.0),
            "periodic",
            1)

    def test_usd_preview_surface_texture_to_OSL_ubermap_translation(self):
        # Test translation
        self.import_usd_preview_surface_with_usd_transform_2d_transform_test(
            "test_usd_preview_surface_texture_to_OSL_ubermap_translation",
            (1.0, 1.0), 
            0.0, 
            (23.4, 11.57), 
            None,
            None,
            (-23.4, -11.57, 0.0),
            1.0,
            (1.0, 1.0, 1.0),
            0.0,
            (0.0, 0.0, 1.0),
            (0.5, 0.5, 0.0),
            "periodic")

    def test_usd_preview_surface_texture_to_OSL_ubermap_negative_translation(self):
        # Test negative translation values
        self.import_usd_preview_surface_with_usd_transform_2d_transform_test(
            "test_usd_preview_surface_texture_to_OSL_ubermap_negative_translation",
            (1.0, 1.0), 
            0.0, 
            (-23.4, -11.57), 
            None,
            None,
            (23.4, 11.57, 0.0),
            1.0,
            (1.0, 1.0, 1.0),
            0.0,
            (0.0, 0.0, 1.0),
            (0.5, 0.5, 0.0),
            "periodic")

    def test_usd_preview_surface_texture_to_OSL_ubermap_all_transforms(self):
        # Test all transforms
        self.import_usd_preview_surface_with_usd_transform_2d_transform_test(
            "test_usd_preview_surface_texture_to_OSL_ubermap_all_transforms",
            (4.0, 4.0), 
            30.0, 
            (23.4, 11.57), 
            None,
            None,
            (-6.512, 0.42, 0.0),
            0.25,
            (1.0, 1.0, 1.0),
            30.0,
            (0.0, 0.0, 1.0),
            (0.0, 0.0, 0.0),
            "periodic")

    def test_usd_preview_surface_texture_to_OSL_ubermap_none_values_on_transforms(self):
        # Test all transforms
        self.import_usd_preview_surface_with_usd_transform_2d_transform_test(
            "test_usd_preview_surface_texture_to_OSL_ubermap_none_values_on_transforms",
            None, 
            None, 
            None, 
            None,
            None,
            (0.0, 0.0, 0.0),
            1.0,
            (1.0, 1.0, 1.0),
            0.0,
            (0.0, 0.0, 1.0),
            (0.5, 0.5, 0.0),
            "periodic")

    def test_usd_preview_surface_texture_to_OSL_ubermap_wrap_mode_black(self):
        # Test black wrap mode
        self.import_usd_preview_surface_with_usd_transform_2d_transform_test(
            "test_usd_preview_surface_texture_to_OSL_ubermap_wrap_mode_black",
            None, 
            None, 
            None, 
            "black",
            "black",
            (0.0, 0.0, 0.0),
            1.0,
            (1.0, 1.0, 1.0),
            0.0,
            (0.0, 0.0, 1.0),
            (0.5, 0.5, 0.0),
            "black")

    def test_usd_preview_surface_texture_to_OSL_ubermap_wrap_mode_clamp(self):
        # Test clamp wrap mode
        self.import_usd_preview_surface_with_usd_transform_2d_transform_test(
            "test_usd_preview_surface_texture_to_OSL_ubermap_wrap_mode_clamp",
            None, 
            None, 
            None, 
            "clamp",
            "clamp",
            (0.0, 0.0, 0.0),
            1.0,
            (1.0, 1.0, 1.0),
            0.0,
            (0.0, 0.0, 1.0),
            (0.5, 0.5, 0.0),
            "clamp")

    def test_usd_preview_surface_texture_to_OSL_ubermap_wrap_mode_repeat(self):
        # Test repeat wrap mode
        self.import_usd_preview_surface_with_usd_transform_2d_transform_test(
            "test_usd_preview_surface_texture_to_OSL_ubermap_wrap_mode_repeat",
            None, 
            None, 
            None, 
            "repeat",
            "repeat",
            (0.0, 0.0, 0.0),
            1.0,
            (1.0, 1.0, 1.0),
            0.0,
            (0.0, 0.0, 1.0),
            (0.5, 0.5, 0.0),
            "periodic")

    def test_usd_preview_surface_texture_to_OSL_ubermap_wrap_mode_mirror(self):
        # Test mirror wrap mode
        self.import_usd_preview_surface_with_usd_transform_2d_transform_test(
            "test_usd_preview_surface_texture_to_OSL_ubermap_wrap_mode_mirror",
            None, 
            None, 
            None, 
            "mirror",
            "mirror",
            (0.0, 0.0, 0.0),
            1.0,
            (1.0, 1.0, 1.0),
            0.0,
            (0.0, 0.0, 1.0),
            (0.5, 0.5, 0.0),
            "mirror")

    def test_usd_preview_surface_texture_to_OSL_ubermap_wrap_mode_useMetadata(self):
        # Test useMetadata wrap mode
        self.import_usd_preview_surface_with_usd_transform_2d_transform_test(
            "test_usd_preview_surface_texture_to_OSL_ubermap_wrap_mode_useMetadata",
            None, 
            None, 
            None, 
            "useMetadata",
            "useMetadata",
            (0.0, 0.0, 0.0),
            1.0,
            (1.0, 1.0, 1.0),
            0.0,
            (0.0, 0.0, 1.0),
            (0.5, 0.5, 0.0),
            "periodic",
            1)

    def test_usd_preview_surface_texture_to_OSL_ubermap_wrap_mode_non_uniform(self):
        # Test non-uniform wrap mode
        self.import_usd_preview_surface_with_usd_transform_2d_transform_test(
            "test_usd_preview_surface_texture_to_OSL_ubermap_wrap_mode_non_uniform",
            None, 
            None, 
            None, 
            "black",
            "mirror",
            (0.0, 0.0, 0.0),
            1.0,
            (1.0, 1.0, 1.0),
            0.0,
            (0.0, 0.0, 1.0),
            (0.5, 0.5, 0.0),
            "periodic",
            1)

    def test_usd_preview_surface_texture_to_OSL_ubermap_wrap_mode_only_s(self):
        # Test only wrapS wrap mode defined
        self.import_usd_preview_surface_with_usd_transform_2d_transform_test(
            "test_usd_preview_surface_texture_to_OSL_ubermap_wrap_mode_only_s",
            None, 
            None, 
            None, 
            "black",
            None,
            (0.0, 0.0, 0.0),
            1.0,
            (1.0, 1.0, 1.0),
            0.0,
            (0.0, 0.0, 1.0),
            (0.5, 0.5, 0.0),
            "black")

    def test_usd_preview_surface_texture_to_OSL_ubermap_wrap_mode_only_t(self):
        # Test only wrapT wrap mode defined
        self.import_usd_preview_surface_with_usd_transform_2d_transform_test(
            "test_usd_preview_surface_texture_to_OSL_ubermap_wrap_mode_only_t",
            None, 
            None, 
            None, 
            None,
            "black",
            (0.0, 0.0, 0.0),
            1.0,
            (1.0, 1.0, 1.0),
            0.0,
            (0.0, 0.0, 1.0),
            (0.5, 0.5, 0.0),
            "black")

    def test_usd_preview_surface_texture_to_OSL_ubermap_all_inputs(self):
        # Test all inputs
        self.import_usd_preview_surface_with_usd_transform_2d_transform_test(
            "test_usd_preview_surface_texture_to_OSL_ubermap_all_inputs",
            (4.0, 4.0), 
            30.0, 
            (23.4, 11.57), 
            "black",
            "black",
            (-6.512, 0.42, 0.0),
            0.25,
            (1.0, 1.0, 1.0),
            30.0,
            (0.0, 0.0, 1.0),
            (0.0, 0.0, 0.0),
            "black")

def run_tests():
    return unittest.TextTestRunner(stream=sys.stdout, verbosity=2).run(unittest.TestLoader().loadTestsFromTestCase(TestImportTextureTransform))

if __name__ == "__main__":
    RT.clearListener()
    run_tests()
