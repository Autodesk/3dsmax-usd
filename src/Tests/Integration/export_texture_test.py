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
import usd_material_writer
import usd_material_reader
import pymxs
from pymxs import runtime as rt
import sys
import os
import shutil

from pxr import Usd, UsdShade, Sdf, Gf
import usd_test_helpers
from importlib import reload
reload(usd_test_helpers)
reload(usd_material_reader)
reload(usd_material_writer)

mxs = pymxs.runtime
TEST_DIR = os.path.dirname(__file__)
TEST_DATA_DIR = os.path.join(TEST_DIR, "data")
NOPROMPT = mxs.Name("noprompt")
TEMP_DIR = mxs.getdir(mxs.name("temp"))
UV_CHECKER_PATH = os.path.join(TEST_DATA_DIR, "uv_checker.png")

_material_import_options = {
    "texture_target_id": usd_material_reader._uberbitmapfile,
    "material_target_id": "MaxUsdPreviewSurface"
}

channel_index_map = {
    "rgb": 1,
    "r": 2,
    "g": 3,
    "b": 4,
    "a": 5
}

def create_osl_multi_channel(osl_texture, channel):
    multi_channel_map = mxs.MultiOutputChannelTexmapToTexmap()
    multi_channel_map.sourceMap = osl_texture
    out_channel_index = channel_index_map[channel] # rgb
    multi_channel_map.outputChannelIndex = out_channel_index
    multi_channel_map.name = "{0}:{1}".format(osl_texture.name, channel)
    return multi_channel_map

def create_test_osl_bitmap(osl_name, test_texture_filename, name):
    osl_texture = mxs.OSLMap()
    osl_texture.name = name
    max_root = mxs.symbolicPaths.getPathValue("$max")
    osl_texture.OSLPath = (max_root + "/OSL/" + osl_name)
    osl_texture.OSLAutoUpdate = True
    osl_texture.filename = test_texture_filename
    return osl_texture

def create_test_bitmap(test_texture_filename, name):
    bitmap_texture = mxs.Bitmaptexture(fileName=test_texture_filename)
    bitmap_texture.name = name
    return bitmap_texture

class TestOutputUsdTexture(unittest.TestCase):
    def setUp(self):
        self.output_prefix = usd_test_helpers.standard_output_prefix("EXPORT_TEXTURE_TEST_")
        usd_test_helpers.load_usd_plugins()
        mxs.resetMaxFile(NOPROMPT)
        self.parent_path = Sdf.Path("/tests")
        # Turn off relative paths for testing. The feature is tested explictely.
        usd_material_writer._material_export_options["relative_texture_paths"] = False
        self.export_options = dict(usd_material_writer._material_export_options)
        maxver = rt.maxversion()
        if maxver[0] >= 26000:  # 3ds Max 2024 and up
            self.OslBitmapFile = "OSLBitmap2.osl"
            self.UberBitmapFile = "UberBitmap2.osl"
        else:
            self.OslBitmapFile = "OSLBitmap.osl"
            self.UberBitmapFile = "UberBitmap.osl"

    def tearDown(self):
        usd_material_writer._material_export_options["relative_texture_paths"] = True

    def test_bitmap(self):
        # type: () -> None
        """Test roundtrip of max bitmap texture reader"""
        import_options = dict(_material_import_options)
        import_options['texture_target_id'] = "Bitmaptexture"
        stage = Usd.Stage.CreateInMemory()
        bitmap = create_test_bitmap(UV_CHECKER_PATH, "test_Bitmap")
        self.assertEqual(mxs.classOf(bitmap), mxs.Bitmaptexture)
        texture_sampler, resolved_texture, _, _ = usd_material_writer.output_usd_texture(bitmap, stage, self.parent_path, self.export_options, {}, False)
        self.assertIsInstance(texture_sampler, UsdShade.NodeGraph)
        self.assertIsNotNone(resolved_texture)

        texture_sampler_path = "/tests/test_Bitmap/test_Bitmap"
        usd_test_helpers.test_texture_sampler_ouput(self, stage, texture_sampler_path, UV_CHECKER_PATH)

        mxs.resetMaxFile(NOPROMPT)
        texture_sampler_shader = UsdShade.Shader.Get(stage, texture_sampler_path)
        max_bitmap = usd_material_reader.output_max_texture(texture_sampler_shader, import_options)
        self.assertEqual(mxs.classOf(max_bitmap), mxs.Bitmaptexture)
        self.assertEqual(usd_test_helpers.get_file_path_mxs(max_bitmap.filename), UV_CHECKER_PATH)

    def test_OSL_UberBitmap(self):
        """Test roundtrip of UberBitmap OSL texture reader"""
        import_options = dict(_material_import_options)
        import_options['texture_target_id'] = self.UberBitmapFile

        stage = Usd.Stage.CreateInMemory()
        bitmap = create_test_osl_bitmap(self.UberBitmapFile, UV_CHECKER_PATH, 'test_OSL_UberBitmap')
        osl_path = bitmap.OSLPath
        osl_base_name = os.path.basename(osl_path)
        self.assertEqual(osl_base_name.lower(), self.UberBitmapFile.lower())

        self.assertEqual(mxs.classOf(bitmap), mxs.OSLMap)
        texture_sampler, resolved_texture, _, _ = usd_material_writer.output_usd_texture(bitmap, stage, self.parent_path, self.export_options, {}, False)
        self.assertIsInstance(texture_sampler, UsdShade.NodeGraph)
        self.assertIsNotNone(resolved_texture)

        texture_sampler_path = "/tests/test_OSL_UberBitmap/test_OSL_UberBitmap"
        usd_test_helpers.test_texture_sampler_ouput(self, stage, texture_sampler_path, UV_CHECKER_PATH)

        mxs.resetMaxFile(NOPROMPT)
        texture_sampler_shader = UsdShade.Shader.Get(stage, texture_sampler_path)
        max_bitmap = usd_material_reader.output_max_texture(texture_sampler_shader, import_options)
        self.assertEqual(mxs.classOf(max_bitmap), mxs.OSLMap)
        self.assertEqual(usd_test_helpers.get_file_path_mxs(max_bitmap.filename), UV_CHECKER_PATH)
        self.assertEqual(os.path.basename(max_bitmap.OSLPath), os.path.basename(osl_path))

    def test_OSL_OSLBitmap(self):
        """Test roundtrip of OSLBitmap OSL texture reader"""
        import_options = dict(_material_import_options)
        import_options['texture_target_id'] = self.OslBitmapFile
        test_usd_file_path = self.output_prefix + "test_OSL_OSLBitmap.usda"
        bitmap_name = "test_OSL_OSLBitmap"
        bitmap = create_test_osl_bitmap(self.OslBitmapFile, UV_CHECKER_PATH, bitmap_name)
        osl_path = bitmap.OSLPath
        osl_base_name = os.path.basename(osl_path)
        self.assertEqual(osl_base_name.lower(), self.OslBitmapFile.lower())
        self.assertEqual(mxs.classOf(bitmap), mxs.OSLMap)

        stage = Usd.Stage.CreateInMemory()
        texture_sampler, resolved_texture, _, _ = usd_material_writer.output_usd_texture(bitmap, stage, self.parent_path, self.export_options, {}, False)
        self.assertIsInstance(texture_sampler, UsdShade.NodeGraph)
        self.assertIsNotNone(resolved_texture)

        texture_sampler_path = "/tests/test_OSL_OSLBitmap/test_OSL_OSLBitmap"
        usd_test_helpers.test_texture_sampler_ouput(self, stage, texture_sampler_path, UV_CHECKER_PATH)
        mxs.resetMaxFile(NOPROMPT)
        texture_sampler_shader = UsdShade.Shader.Get(stage, texture_sampler_path)
        max_bitmap = usd_material_reader.output_max_texture(texture_sampler_shader, import_options)
        self.assertEqual(mxs.classOf(max_bitmap), mxs.OSLMap)
        self.assertEqual(usd_test_helpers.get_file_path_mxs(max_bitmap.filename), UV_CHECKER_PATH)
        self.assertEqual(os.path.basename(max_bitmap.OSLPath), os.path.basename(osl_path))

    def test_unsupported_texturenode(self):
        """Test expected behavior of unsupported texture nodes"""
        stage = Usd.Stage.CreateInMemory()
        bitmap = mxs.AdvancedWood()
        texture_sampler, resolved_texture, _, _ = usd_material_writer.output_usd_texture(bitmap, stage, self.parent_path, self.export_options, {}, False)
        # an unsupported bitmap texture should return none
        self.assertIsNone(texture_sampler)
        self.assertIsNone(resolved_texture)

        # self.assertIsNone(uv_reader)
        texture_sampler_path = "/tests/test_unsupported_bitmap"
        prim = stage.GetPrimAtPath(texture_sampler_path)
        # and there bitmap should not export...
        self.assertFalse(prim.IsValid())


    def test_osl_connections(self):
        """Test that specific connected channels round-trip as expected with OSL textures"""
        box = mxs.Box(name="test_box_mat_connections")
        box.material = mxs.PBRMetalRough()
        bitmap = create_test_osl_bitmap(self.UberBitmapFile, UV_CHECKER_PATH, 'test_OSL_connections')
        box.material.base_color_map = create_osl_multi_channel(bitmap, "rgb")
        box.material.metalness_map = create_osl_multi_channel(bitmap, "r")
        box.material.roughness_map = create_osl_multi_channel(bitmap, "g")
        box.material.ao_map = create_osl_multi_channel(bitmap, "b")
        box.material.opacity_map = create_osl_multi_channel(bitmap, "a")
        test_usd_file_path = self.output_prefix + "test_osl_connections.usda"

        export_options = mxs.UsdExporter.CreateOptions()
        export_options.FileFormat = mxs.Name("ascii")
        export_options.RootPrimPath = "/"
        ret = mxs.USDExporter.ExportFile(test_usd_file_path, exportOptions=export_options, contentSource=mxs.Name("nodeList"), nodeList=[box])
        self.assertTrue(ret)

        stage = Usd.Stage.Open(test_usd_file_path)
        stage.Reload()
        self.assertIsInstance(stage, Usd.Stage)
        box_mat_path = "/mtl/box_mat"
        usd_box_mat = UsdShade.Shader.Get(stage, box_mat_path)
        self.assertIsInstance(usd_box_mat, UsdShade.Shader)

        texture_sampler_path = "/mtl/PBRMetalRough/test_OSL_connections/test_OSL_connections"
        texture_sampler = UsdShade.Shader.Get(stage, texture_sampler_path)
        self.assertIsInstance(texture_sampler, UsdShade.Shader)
        usd_test_helpers.test_texture_sampler_ouput(self, stage, texture_sampler_path, usd_test_helpers.get_file_path_mxs(bitmap.filename))

        mxs.resetMaxFile(NOPROMPT)
        import_options = mxs.USDImporter.CreateOptions()
        ret = mxs.USDImporter.ImportFile(test_usd_file_path, importOptions=import_options)
        box = mxs.getnodebyname("test_box_mat_connections")
        import_mat = box.material
        self.assertEqual(mxs.classOf(import_mat), mxs.MaxUsdPreviewSurface)

        diffuseColor_multi_channel_map = box.material.diffuseColor_map
        self.assertEqual(diffuseColor_multi_channel_map.outputChannelIndex, 1) # rgb
        metallic_multi_channel_map = box.material.metallic_map
        self.assertEqual(metallic_multi_channel_map.outputChannelIndex, 2) # r
        roughness_multi_channel_map = box.material.roughness_map
        self.assertEqual(roughness_multi_channel_map.outputChannelIndex, 3) # g
        occlusion_multi_channel_map = box.material.occlusion_map
        self.assertEqual(occlusion_multi_channel_map.outputChannelIndex, 4) # b
        opacity_multi_channel_map = box.material.opacity_map
        self.assertEqual(opacity_multi_channel_map.outputChannelIndex, 5) # a

        diffuseColor_source_map = diffuseColor_multi_channel_map.sourceMap
        self.assertEqual(mxs.classOf(diffuseColor_source_map), mxs.OSLMap)


    def test_bitmaptexture_gamma_1_to_normal_map_usdpreviewsurface_handling(self):
        """Test that bitmap set gamma to 1 associated to normal map export correctly. (MAXX-70937)"""

        sphere_normal = mxs.Sphere(name="test_sphere_mat_normal_with_bitmaptexture")
        sphere_normal.material = mxs.MaxUsdPreviewSurface(name="test_mat_normal_with_bitmaptexture")

        normal_map_tex = os.path.join(TEST_DATA_DIR, 'orangepeel_normal.png')
        normal_bitmap = mxs.BitmapTexture(filename=normal_map_tex)
        
        # The assignment to the normal_map will ensure the gamma is set to 1.0
        sphere_normal.material.normal_map = normal_bitmap

        test_usd_file_path = self.output_prefix + "test_bitmaptexture_gamma_1_to_normal_map_handling.usda"

        export_options = mxs.UsdExporter.CreateOptions()
        export_options.FileFormat = mxs.Name("ascii")
        export_options.RootPrimPath = "/"
        
        ret = mxs.USDExporter.ExportFile(test_usd_file_path, exportOptions=export_options, contentSource=mxs.Name("nodeList"), nodeList=[sphere_normal])
        self.assertTrue(ret)

        stage = Usd.Stage.Open(test_usd_file_path)
        self.assertIsInstance(stage, Usd.Stage)

        usd_mat_path = "/mtl/Bitmaptexture/Bitmaptexture"
        usd_mat_shader = UsdShade.Shader.Get(stage, usd_mat_path)
        shader_inputs = usd_mat_shader.GetInputs()

        is_bias_present_with_preset_value = False
        is_scale_present_with_preset_value = False
        is_sourceColorSpace_present_with_preset_value = False

        for shader_input in shader_inputs:
            if shader_input.GetBaseName() == "bias" and shader_input.Get() == Gf.Vec4f(-1, -1, -1, 0):
                is_bias_present_with_preset_value = True
            if shader_input.GetBaseName() == "scale" and shader_input.Get() == Gf.Vec4f(2, 2, 2, 1):
                is_scale_present_with_preset_value = True
            if shader_input.GetBaseName() == "sourceColorSpace" and shader_input.Get() == "raw":
                is_sourceColorSpace_present_with_preset_value = True

        areCorrectBitmapInputsPresent = is_bias_present_with_preset_value and is_scale_present_with_preset_value and is_sourceColorSpace_present_with_preset_value

        self.assertTrue(areCorrectBitmapInputsPresent)

    def  test_bitmaptexture_gamma_1_to_non_normal_map_usdpreviewsurface_handling(self):
        """Test that bitmap set to gamma 1 associated to non-normal map export correctly. (MAXX-70937)"""

        sphere_roughness = mxs.Sphere(name="test_sphere_roughness_map_with_bitmaptexture")
        sphere_roughness.material = mxs.MaxUsdPreviewSurface(name="test_roughness_map_with_bitmaptexture")

        roughness_map_tex = os.path.join(TEST_DATA_DIR, 'orangepeel_normal.png') #doesn't matter what type of file we use
        roughness_map = mxs.BitmapTexture(filename=roughness_map_tex)
        roughness_map.bitmap.gamma = 1.0

        sphere_roughness.material.roughness_map = roughness_map

        test_usd_file_path = self.output_prefix + "test_bitmaptexture_gamma_1_to_non_normal_map_handling.usda"

        export_options = mxs.UsdExporter.CreateOptions()
        export_options.FileFormat = mxs.Name("ascii")
        export_options.RootPrimPath = "/"
        
        ret = mxs.USDExporter.ExportFile(test_usd_file_path, exportOptions=export_options, contentSource=mxs.Name("nodeList"), nodeList=[sphere_roughness])
        self.assertTrue(ret)

        stage = Usd.Stage.Open(test_usd_file_path)
        self.assertIsInstance(stage, Usd.Stage)

        usd_mat_path = "/mtl/Bitmaptexture/Bitmaptexture"
        usd_mat_shader = UsdShade.Shader.Get(stage, usd_mat_path)
        shader_inputs = usd_mat_shader.GetInputs()

        is_bias_present_with_preset_value = False
        is_scale_present_with_preset_value = False
        is_sourceColorSpace_present_with_preset_value = False

        for shader_input in shader_inputs:
            if shader_input.GetBaseName() == "bias" and shader_input.Get() == Gf.Vec4f(-1, -1, -1, 0):
                is_bias_present_with_preset_value = True
            if shader_input.GetBaseName() == "scale" and shader_input.Get() == Gf.Vec4f(2, 2, 2, 1):
                is_scale_present_with_preset_value = True
            if shader_input.GetBaseName() == "sourceColorSpace" and shader_input.Get() == "raw":
                is_sourceColorSpace_present_with_preset_value = True

        areCorrectBitmapInputsPresent = is_sourceColorSpace_present_with_preset_value and not is_bias_present_with_preset_value and not is_scale_present_with_preset_value

        self.assertTrue(areCorrectBitmapInputsPresent)

    def test_oslbitmap_gamma_1_to_normal_map_usdpreviewsurface_handling(self):
        """Test that oslbitmap set gamma to 1 associated to normal map export correctly. (MAXX-70937)"""

        sphere_normal = mxs.Sphere(name="test_sphere_mat_normal_with_OSLBitmap")
        sphere_normal.material = mxs.MaxUsdPreviewSurface(name="test_mat_normal_with_OSLBitmap")

        normal_map_tex = os.path.join(TEST_DATA_DIR, 'orangepeel_normal.png')
        normal_map_name = 'test_OSLBitmap_gamma_1_to_normal'
        osl_normal_bitmap = create_test_osl_bitmap(self.OslBitmapFile, normal_map_tex, normal_map_name)

        osl_normal_bitmap.AutoGamma = 0
        osl_normal_bitmap.ManualGamma = 1.0

        multi_channel_map = mxs.MultiOutputChannelTexmapToTexmap()
        multi_channel_map.sourceMap = osl_normal_bitmap
        out_channel_index = channel_index_map["rgb"]
        multi_channel_map.outputChannelIndex = out_channel_index
        multi_channel_map.name = "{0}:{1}".format(normal_map_name, "rgb")

        sphere_normal.material.normal_map = multi_channel_map

        test_usd_file_path = self.output_prefix + "test_oslbitmap_gamma_1_to_normal_map_handling.usda"

        export_options = mxs.UsdExporter.CreateOptions()
        export_options.FileFormat = mxs.Name("ascii")
        export_options.RootPrimPath = "/"
        
        ret = mxs.USDExporter.ExportFile(test_usd_file_path, exportOptions=export_options, contentSource=mxs.Name("nodeList"), nodeList=[sphere_normal])
        self.assertTrue(ret)

        stage = Usd.Stage.Open(test_usd_file_path)
        self.assertIsInstance(stage, Usd.Stage)

        usd_mat_path = "/mtl/test_OSLBitmap_gamma_1_to_normal/test_OSLBitmap_gamma_1_to_normal"
        usd_mat_shader = UsdShade.Shader.Get(stage, usd_mat_path)
        shader_inputs = usd_mat_shader.GetInputs()

        is_bias_present_with_preset_value = False
        is_scale_present_with_preset_value = False
        is_sourceColorSpace_present_with_preset_value = False

        for shader_input in shader_inputs:
            if shader_input.GetBaseName() == "bias" and shader_input.Get() == Gf.Vec4f(-1, -1, -1, 0):
                is_bias_present_with_preset_value = True
            if shader_input.GetBaseName() == "scale" and shader_input.Get() == Gf.Vec4f(2, 2, 2, 1):
                is_scale_present_with_preset_value = True
            if shader_input.GetBaseName() == "sourceColorSpace" and shader_input.Get() == "raw":
                is_sourceColorSpace_present_with_preset_value = True

        areCorrectBitmapInputsPresent = is_bias_present_with_preset_value and is_scale_present_with_preset_value and is_sourceColorSpace_present_with_preset_value

        self.assertTrue(areCorrectBitmapInputsPresent)

    def  test_oslbitmap_gamma_1_to_non_normal_map_usdpreviewsurface_handling(self):
        """Test that oslbitmap set to gamma 1 associated to non-normal map export correctly. (MAXX-70937)"""

        sphere_roughness = mxs.Sphere(name="test_sphere_roughness_map")
        sphere_roughness.material = mxs.MaxUsdPreviewSurface(name="test_roughness_map")

        roughness_map_tex = os.path.join(TEST_DATA_DIR, 'orangepeel_normal.png') #doesn't matter what type of file we use
        roughnesss_map_name = 'test_OSLBitmap_gamma_1_to_non_normal'
        osl_roughness_bitmap = create_test_osl_bitmap(self.OslBitmapFile, roughness_map_tex, roughnesss_map_name)

        osl_roughness_bitmap.AutoGamma = 0
        osl_roughness_bitmap.ManualGamma = 1.0

        multi_channel_map = mxs.MultiOutputChannelTexmapToTexmap()
        multi_channel_map.sourceMap = osl_roughness_bitmap
        out_channel_index = channel_index_map["rgb"]
        multi_channel_map.outputChannelIndex = out_channel_index
        multi_channel_map.name = "{0}:{1}".format(roughnesss_map_name, "rgb")

        sphere_roughness.material.roughness_map = multi_channel_map

        test_usd_file_path = self.output_prefix + "test_oslbitmap_gamma_1_to_non_normal_map_handling.usda"

        export_options = mxs.UsdExporter.CreateOptions()
        export_options.FileFormat = mxs.Name("ascii")
        export_options.RootPrimPath = "/"
        
        ret = mxs.USDExporter.ExportFile(test_usd_file_path, exportOptions=export_options, contentSource=mxs.Name("nodeList"), nodeList=[sphere_roughness])
        self.assertTrue(ret)

        stage = Usd.Stage.Open(test_usd_file_path)
        self.assertIsInstance(stage, Usd.Stage)

        usd_mat_path = "/mtl/test_OSLBitmap_gamma_1_to_non_normal/test_OSLBitmap_gamma_1_to_non_normal"
        usd_mat_shader = UsdShade.Shader.Get(stage, usd_mat_path)
        shader_inputs = usd_mat_shader.GetInputs()

        is_bias_present_with_preset_value = False
        is_scale_present_with_preset_value = False
        is_sourceColorSpace_present_with_preset_value = False

        for shader_input in shader_inputs:
            if shader_input.GetBaseName() == "bias" and shader_input.Get() == Gf.Vec4f(-1, -1, -1, 0):
                is_bias_present_with_preset_value = True
            if shader_input.GetBaseName() == "scale" and shader_input.Get() == Gf.Vec4f(2, 2, 2, 1):
                is_scale_present_with_preset_value = True
            if shader_input.GetBaseName() == "sourceColorSpace" and shader_input.Get() == "raw":
                is_sourceColorSpace_present_with_preset_value = True

        areCorrectBitmapInputsPresent = is_sourceColorSpace_present_with_preset_value and not is_bias_present_with_preset_value and not is_scale_present_with_preset_value

        self.assertTrue(areCorrectBitmapInputsPresent)

    def test_normal_map_handling(self):
        """Test that normal maps export, bump maps do not. (MAXX-59913)"""

        box_bump = mxs.Box(name="test_box_mat_bump") # this has a bump map
        box_bump.material = mxs.PhysicalMaterial(name="test_mat_bump")
        box_normal = mxs.Box(name="test_box_mat_normal") # this has a normal map
        box_normal.material = mxs.PhysicalMaterial(name="test_mat_normal")
        normal_map_tex = os.path.join(TEST_DATA_DIR, 'fabric_normal.png')
        bump_map_tex = os.path.join(TEST_DATA_DIR, 'fabric_bump.jpg')
        normal_bitmap = mxs.BitmapTexture(filename=normal_map_tex)
        bump_bitmap = mxs.BitmapTexture(filename=bump_map_tex)
        box_bump.material.bump_map = bump_bitmap
        box_normal.material.bump_map = mxs.Normal_Bump()
        box_normal.material.bump_map.normal_map = normal_bitmap

        test_usd_file_path = self.output_prefix + "test_normal_map_handling.usda"

        export_options = mxs.UsdExporter.CreateOptions()
        export_options.FileFormat = mxs.Name("ascii")
        export_options.RootPrimPath = "/"
        
        ret = mxs.USDExporter.ExportFile(test_usd_file_path, exportOptions=export_options, contentSource=mxs.Name("nodeList"), nodeList=[box_bump, box_normal])
        self.assertTrue(ret)

        stage = Usd.Stage.Open(test_usd_file_path)
        self.assertIsInstance(stage, Usd.Stage)
        
        bump_mat_path = "/mtl/test_mat_bump/test_mat_bump"
        usd_bump_mat = UsdShade.Shader.Get(stage, bump_mat_path)
        self.assertIsInstance(usd_bump_mat, UsdShade.Shader)
        normal_input = usd_bump_mat.GetInput("normal")
        # THE BELOW WILL CRASH BEWARE!
        #self.assertFalse(normal_input.HasConnectedSource())
        # DO THIS INSTEAD
        self.assertFalse(normal_input.IsInput(normal_input.GetAttr()), msg=test_usd_file_path)

        normal_mat_path = "/mtl/test_mat_normal/test_mat_normal"
        usd_normal_mat = UsdShade.Shader.Get(stage, normal_mat_path)
        self.assertIsInstance(usd_normal_mat, UsdShade.Shader)

        # GetInput() Return the requested input if it exists.
        normal_input_2 = usd_normal_mat.GetInput("normal")
        # GetAttr() Explicit UsdAttribute extractor.
        # IsInput() Test whether a given UsdAttribute represents a valid Input,
        # which implies that creating a UsdShadeInput from the attribute will succeed.
        input_valid = normal_input_2.IsInput(normal_input_2.GetAttr())
        self.assertTrue(input_valid, msg=test_usd_file_path)
        # HasConnectedSource() Returns true if and only if the shading attribute is currently connected to at least one valid (defined) source.
        has_connected_source = normal_input_2.HasConnectedSource()
        self.assertTrue(has_connected_source)
        source, sourceName, _ = normal_input_2.GetConnectedSource()
        attr = source.GetOutput(sourceName).GetAttr()
        target = attr.GetPrim()
        self.assertTrue(target.IsValid())
        texture_reader = UsdShade.NodeGraph.Get(stage,target.GetPath().AppendChild(target.GetName()))
        file_attr = texture_reader.GetInput('file')
        file_path = file_attr.Get().path
        self.assertTrue(os.path.samefile(file_path, normal_bitmap.filename))

    def test_absolute_and_relative_paths(self):
        # type: () -> None
        """Test roundtrip of max bitmap texture reader"""
        log_path = self.output_prefix + "test_relative_paths.log"
        data_texture_filename = os.path.join(TEST_DATA_DIR, "uv_checker.png")
        test_texture_filename = os.path.join(TEMP_DIR, (self.output_prefix + "uv_checker.png"))
        shutil.copyfile(data_texture_filename, test_texture_filename)

        material_export_options = dict(self.export_options)

        # create a bitmap texture
        bitmap_texture = mxs.Bitmaptexture(fileName=test_texture_filename)

        # export to usd relative
        test_usd_file_path = self.output_prefix + "test_relative_paths.usda"
        material_export_options["usd_filename"] = test_usd_file_path
        material_export_options["relative_texture_paths"] = True
        stage = Usd.Stage.CreateNew(test_usd_file_path)
        usd_texture_sampler, resolved_texture, uv_texture, _ = usd_material_writer.output_usd_texture(bitmap_texture, stage, self.parent_path, material_export_options, {}, False)
        stage.Save()
        self.assertIsInstance(usd_texture_sampler, UsdShade.NodeGraph)
        self.assertIsInstance(uv_texture, UsdShade.Shader)
        self.assertIsNotNone(resolved_texture)

        # check that relative paths are correct: relative
        file_attr = uv_texture.GetInput('file')
        file_attr_val = file_attr.Get()
        # path and resolved path are different, one is abs and one is not
        self.assertTrue(os.path.exists(file_attr_val.resolvedPath))
        self.assertFalse(file_attr_val.path == file_attr_val.resolvedPath)
        self.assertFalse(os.path.isabs(file_attr_val.path))
        self.assertTrue((str(file_attr_val.path)).startswith("./")) #for the Default resolve to always anchor to the exported layer.

        # export to usd absolute
        test_usd_file_path = self.output_prefix + "test_absolute_paths.usda"
        material_export_options["usd_filename"] = test_usd_file_path
        material_export_options["relative_texture_paths"] = False
        stage = Usd.Stage.CreateNew(test_usd_file_path)
        usd_texture_sampler, resolved_texture, uv_texture, _ = usd_material_writer.output_usd_texture(bitmap_texture, stage, self.parent_path, material_export_options, {}, False)
        stage.Save()
        self.assertIsInstance(usd_texture_sampler, UsdShade.NodeGraph)
        self.assertIsInstance(uv_texture, UsdShade.Shader)
        self.assertIsNotNone(resolved_texture)

        # check that absolute paths are correct
        file_attr = uv_texture.GetInput('file')
        file_attr_val = file_attr.Get()
        # path and resolved path are the same absolute path
        self.assertTrue(os.path.exists(file_attr_val.resolvedPath))
        self.assertTrue(os.path.normpath(file_attr_val.path.lower()) == file_attr_val.resolvedPath.lower())
        self.assertTrue(os.path.isabs(file_attr_val.path))
        # Make sure the path is using forward slashes (so things work on unix/mac)
        self.assertTrue(test_texture_filename.lower().replace("\\","/") == file_attr_val.path.lower())

def run_tests():
    return unittest.TextTestRunner(stream=sys.stdout, verbosity=2).run(unittest.TestLoader().loadTestsFromTestCase(TestOutputUsdTexture))


if __name__ == "__main__":
    from importlib import reload
    reload(usd_material_writer)
    reload(usd_material_reader)
    mxs.clearListener()
    run_tests()
