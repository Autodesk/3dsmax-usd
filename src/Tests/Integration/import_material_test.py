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
import pymxs
from pymxs import runtime as RT

import unittest
import sys
import os
import json
import glob

from pxr import Kind, Sdf, Usd, UsdGeom, UsdShade, Vt
#import shaderReader
import usd_test_helpers

conversion_test_file_location = RT.symbolicPaths.getPathValue("$userTools")

class TestMaterialImport(unittest.TestCase):

    def setUp(self):
        RT.resetMaxFile(RT.Name("noprompt"))
        self.output_prefix = usd_test_helpers.standard_output_prefix("IMPORT_MATERIAL_PYMXS_TEST_")
        usd_test_helpers.load_usd_plugins()

        self.stage = Usd.Stage.CreateInMemory()
        UsdGeom.SetStageUpAxis(self.stage, UsdGeom.Tokens.y)

        self.root = UsdGeom.Xform.Define(self.stage, "/root")
        Usd.ModelAPI(self.root).SetKind(Kind.Tokens.component)
        self.import_options = RT.USDImporter.CreateOptions()
        self.import_options.PreferredMaterial = "maxUsdPreviewSurface"

    def tearDown(self) -> None:
        self.import_options.PreferredMaterial = "maxUsdPreviewSurface"

    def test_usd_preview_surface_values_to_physical_material(self):
        billboard = UsdGeom.Mesh.Define(self.stage, "/root/billboard")
        bbmaterial = UsdShade.Material.Define(self.stage, '/root/billboardMat')
        pbrShader = UsdShade.Shader.Define(self.stage, '/root/billboardMat/PBRShader')

        roughness_value = 0.4
        metallic_value = 0.12
        diffuseColor_value = (111.0, 222.0, 111.0)
        opacity_value = 0.77

        pbrShader.CreateIdAttr("UsdPreviewSurface")
        pbrShader.CreateInput("roughness", Sdf.ValueTypeNames.Float).Set(roughness_value)
        pbrShader.CreateInput("metallic", Sdf.ValueTypeNames.Float).Set(metallic_value)
        pbrShader.CreateInput("diffuseColor", Sdf.ValueTypeNames.Color3f).Set((diffuseColor_value[0]/255.0, diffuseColor_value[1]/255.0, diffuseColor_value[2]/255.0))
        pbrShader.CreateInput("opacity", Sdf.ValueTypeNames.Float).Set(opacity_value)

        bbmaterial.CreateSurfaceOutput().ConnectToSource(pbrShader.ConnectableAPI(), "surface")

        UsdShade.MaterialBindingAPI(billboard).Bind(bbmaterial)
        test_name = "test_usd_preview_surface_values_to_physical_material"
        test_usd_file_path = "{0}{1}.usda".format(self.output_prefix, test_name)
        self.import_options.LogPath = "{0}{1}_LOG.txt".format(self.output_prefix, test_name)
        self.import_options.LogLevel = RT.Name("warn")

        self.stage.GetRootLayer().Export(test_usd_file_path)

        self.import_options.PreferredMaterial = "physicalMaterial"
        RT.USDImporter.ImportFile(test_usd_file_path, importOptions=self.import_options)

        self.assertEqual(len(RT.sceneMaterials), 1)

        max_material = RT.sceneMaterials[0]
        self.assertEqual(RT.ClassOf(max_material), RT.PhysicalMaterial)
        self.assertEqual(max_material.name, "billboardMat")

        with self.subTest():
            self.assertAlmostEqual(max_material.roughness, roughness_value)

        with self.subTest():
            self.assertAlmostEqual(max_material.metalness, metallic_value)

        with self.subTest():
            self.assertEqual((max_material.Base_Color.R, max_material.Base_Color.G, max_material.Base_Color.B), diffuseColor_value)

        with self.subTest():
            self.assertAlmostEqual(max_material.Transparency, 1 - opacity_value)


    def test_usd_preview_surface_values_to_pbr_metal_rough(self):
        billboard = UsdGeom.Mesh.Define(self.stage, "/root/billboard")
        bbmaterial = UsdShade.Material.Define(self.stage, '/root/billboardMat')
        pbrShader = UsdShade.Shader.Define(self.stage, '/root/billboardMat/PBRShader')

        roughness_value = 0.4
        metallic_value = 0.12
        diffuseColor_value = (111.0, 222.0, 111.0)
        emissiveColor_value = (123.0, 124.0, 125.0)

        pbrShader.CreateIdAttr("UsdPreviewSurface")
        pbrShader.CreateInput("roughness", Sdf.ValueTypeNames.Float).Set(roughness_value)
        pbrShader.CreateInput("metallic", Sdf.ValueTypeNames.Float).Set(metallic_value)
        pbrShader.CreateInput("diffuseColor", Sdf.ValueTypeNames.Color3f).Set((diffuseColor_value[0]/255.0, diffuseColor_value[1]/255.0, diffuseColor_value[2]/255.0))
        pbrShader.CreateInput("emissiveColor", Sdf.ValueTypeNames.Color3f).Set((emissiveColor_value[0]/255.0, emissiveColor_value[1]/255.0, emissiveColor_value[2]/255.0))

        bbmaterial.CreateSurfaceOutput().ConnectToSource(pbrShader.ConnectableAPI(), "surface")

        UsdShade.MaterialBindingAPI(billboard).Bind(bbmaterial)

        test_usd_file_path = self.output_prefix + "test_usd_preview_surface_values_to_pbr_metal_rough.usda"
        self.stage.GetRootLayer().Export(test_usd_file_path)

        self.import_options.PreferredMaterial = "pbrMetalRough"
        RT.USDImporter.ImportFile(test_usd_file_path,importOptions=self.import_options)

        self.assertEqual(len(RT.sceneMaterials), 1)

        max_material = RT.sceneMaterials[0]

        self.assertEqual(RT.ClassOf(max_material), RT.PBRMetalRough)

        with self.subTest():
            self.assertAlmostEqual(max_material.roughness, roughness_value)

        with self.subTest():
            self.assertAlmostEqual(max_material.metalness, metallic_value)

        with self.subTest():
            self.assertEqual((max_material.basecolor.R, max_material.basecolor.G, max_material.basecolor.B), diffuseColor_value)

        with self.subTest():
            self.assertEqual((max_material.emit_Color.R, max_material.emit_Color.G, max_material.emit_Color.B), emissiveColor_value)


    def test_usd_preview_surface_values_to_max_usd_preview_surface(self):
        billboard = UsdGeom.Mesh.Define(self.stage, "/root/billboard")
        bbmaterial = UsdShade.Material.Define(self.stage, '/root/billboardMat')
        pbrShader = UsdShade.Shader.Define(self.stage, '/root/billboardMat/PBRShader')

        roughness_value = 0.4
        metallic_value = 0.12
        diffuseColor_value = (111.0, 222.0, 111.0)
        emissiveColor_value = (123.0, 124.0, 125.0)
        specularColor_value = (44.0, 22.0, 123.0)
        opacity_value = 0.77
        clearcoat_value = 0.543
        clearcoatRoughness_value = 0.213
        ior_value = 1.2
        opacityThreshold_value = 0.44
        occlusion_value = 0.85
        displacement_value = 0.53

        pbrShader.CreateIdAttr("UsdPreviewSurface")
        pbrShader.CreateInput("roughness", Sdf.ValueTypeNames.Float).Set(roughness_value)
        pbrShader.CreateInput("metallic", Sdf.ValueTypeNames.Float).Set(metallic_value)
        pbrShader.CreateInput("diffuseColor", Sdf.ValueTypeNames.Color3f).Set((diffuseColor_value[0]/255.0, diffuseColor_value[1]/255.0, diffuseColor_value[2]/255.0))
        pbrShader.CreateInput("emissiveColor", Sdf.ValueTypeNames.Color3f).Set((emissiveColor_value[0]/255.0, emissiveColor_value[1]/255.0, emissiveColor_value[2]/255.0))
        pbrShader.CreateInput("specularColor", Sdf.ValueTypeNames.Color3f).Set((specularColor_value[0]/255.0, specularColor_value[1]/255.0, specularColor_value[2]/255.0))
        pbrShader.CreateInput("opacity", Sdf.ValueTypeNames.Float).Set(opacity_value)
        pbrShader.CreateInput("clearcoat", Sdf.ValueTypeNames.Float).Set(clearcoat_value)
        pbrShader.CreateInput("clearcoatRoughness", Sdf.ValueTypeNames.Float).Set(clearcoatRoughness_value)
        pbrShader.CreateInput("ior", Sdf.ValueTypeNames.Float).Set(ior_value)
        pbrShader.CreateInput("opacityThreshold", Sdf.ValueTypeNames.Float).Set(opacityThreshold_value)
        pbrShader.CreateInput("occlusion", Sdf.ValueTypeNames.Float).Set(occlusion_value)
        pbrShader.CreateInput("displacement", Sdf.ValueTypeNames.Float).Set(displacement_value)

        bbmaterial.CreateSurfaceOutput().ConnectToSource(pbrShader.ConnectableAPI(), "surface")

        UsdShade.MaterialBindingAPI(billboard).Bind(bbmaterial)

        test_usd_file_path = self.output_prefix + "test_usd_preview_surface_values_to_max_usd_preview_surface.usda"
        self.stage.GetRootLayer().Export(test_usd_file_path)

        self.import_options.PreferredMaterial = "maxUsdPreviewSurface"
        RT.USDImporter.ImportFile(test_usd_file_path,importOptions=self.import_options)

        self.assertEqual(len(RT.sceneMaterials), 1)

        max_material = RT.sceneMaterials[0]

        self.assertEqual(RT.ClassOf(max_material), RT.MaxUsdPreviewSurface)

        with self.subTest():
            self.assertAlmostEqual(max_material.roughness, roughness_value)

        with self.subTest():
            self.assertAlmostEqual(max_material.metallic, metallic_value)

        with self.subTest():
            self.assertEqual((max_material.diffuseColor.R, max_material.diffuseColor.G, max_material.diffuseColor.B), diffuseColor_value)

        with self.subTest():
            self.assertEqual((max_material.emissiveColor.R, max_material.emissiveColor.G, max_material.emissiveColor.B), emissiveColor_value)

        with self.subTest():
            self.assertEqual((max_material.specularColor.R, max_material.specularColor.G, max_material.specularColor.B), specularColor_value)

        with self.subTest():
            self.assertAlmostEqual(max_material.opacity, opacity_value)

        with self.subTest():
            self.assertAlmostEqual(max_material.clearcoat, clearcoat_value)

        with self.subTest():
            self.assertAlmostEqual(max_material.clearcoatRoughness, clearcoatRoughness_value)

        with self.subTest():
            self.assertAlmostEqual(max_material.ior, ior_value)

        with self.subTest():
            self.assertAlmostEqual(max_material.opacityThreshold, opacityThreshold_value)

        with self.subTest():
            self.assertAlmostEqual(max_material.occlusion, occlusion_value)

        with self.subTest():
            self.assertAlmostEqual(max_material.displacement, displacement_value)


    def test_full_geomsubset_to_submat(self):
        pyramid = UsdGeom.Mesh.Define(self.stage, "/root/pyramid")
        pyramid.CreatePointsAttr([(-10, 0, -10), (10, 0, -10), (-10, 0, 10), (10, 0, 10), (0, 20, 0)])
        pyramid.CreateFaceVertexCountsAttr([3, 3, 3, 3, 4])
        pyramid.CreateFaceVertexIndicesAttr([0, 1, 4, 0, 2, 4, 3, 1, 4, 3, 2, 4, 0, 1, 3, 2])
        pyramidBindingAPI = UsdShade.MaterialBindingAPI(pyramid)

        bbmaterial = UsdShade.Material.Define(self.stage, '/root/billboardMat')

        pbrShader = UsdShade.Shader.Define(self.stage, '/root/billboardMat/PBRShader')
        pbrShader.CreateIdAttr("UsdPreviewSurface")
        pbrShader.CreateInput("roughness", Sdf.ValueTypeNames.Float).Set(0.4)
        pbrShader.CreateInput("metallic", Sdf.ValueTypeNames.Float).Set(0.0)

        bbmaterial.CreateSurfaceOutput().ConnectToSource(pbrShader.ConnectableAPI(), "surface")

        for i in range(0, 5):
            indices = Vt.IntArray([i])
            subset_name = "_sub_{0}".format(i)
            geom_subset = pyramidBindingAPI.CreateMaterialBindSubset(subset_name, indices)
            UsdShade.MaterialBindingAPI(geom_subset.GetPrim()).Bind(bbmaterial)

        test_usd_file_path = self.output_prefix + "test_full_geomsubset_to_submat.usda"
        self.stage.GetRootLayer().Export(test_usd_file_path)

        self.import_options.PreferredMaterial = "maxUsdPreviewSurface"
        RT.USDImporter.ImportFile(test_usd_file_path,importOptions=self.import_options)

        self.assertEqual(len(RT.sceneMaterials), 1)
        self.assertEqual(RT.ClassOf(RT.sceneMaterials[0]), RT.Multimaterial)
        self.assertEqual(len(RT.sceneMaterials[0].names), 5)
        for i in range(0, 5):
            subset_name = "_sub_{0}".format(i)
            self.assertEqual(RT.sceneMaterials[0].names[i], subset_name)
            self.assertEqual(RT.sceneMaterials[0][i].name, "billboardMat")
            self.assertEqual(RT.ClassOf(RT.sceneMaterials[0][i]), RT.MaxUsdPreviewSurface)
            self.assertAlmostEqual(RT.sceneMaterials[0][i].roughness, 0.4)
            self.assertAlmostEqual(RT.sceneMaterials[0][i].metallic, 0.0)

    def test_incomplete_geomsubset_to_submat(self):
        pyramid = UsdGeom.Mesh.Define(self.stage, "/root/pyramid")
        pyramid.CreatePointsAttr([(-10, 0, -10), (10, 0, -10), (-10, 0, 10), (10, 0, 10), (0, 20, 0)])
        pyramid.CreateFaceVertexCountsAttr([3, 3, 3, 3, 4])
        pyramid.CreateFaceVertexIndicesAttr([0, 1, 4, 0, 2, 4, 3, 1, 4, 3, 2, 4, 0, 1, 3, 2])
        pyramidBindingAPI = UsdShade.MaterialBindingAPI(pyramid)

        bbmaterial = UsdShade.Material.Define(self.stage, '/root/billboardMat')

        pbrShader = UsdShade.Shader.Define(self.stage, '/root/billboardMat/PBRShader')
        pbrShader.CreateIdAttr("UsdPreviewSurface")
        pbrShader.CreateInput("roughness", Sdf.ValueTypeNames.Float).Set(0.4)
        pbrShader.CreateInput("metallic", Sdf.ValueTypeNames.Float).Set(0.0)

        bbmaterial.CreateSurfaceOutput().ConnectToSource(pbrShader.ConnectableAPI(), "surface")

        for i in range(0, 5):
            indices = Vt.IntArray([i])
            subset_name = "_sub_{0}".format(i)
            geom_subset = pyramidBindingAPI.CreateMaterialBindSubset(subset_name, indices)
            if i > 0:
                UsdShade.MaterialBindingAPI(geom_subset.GetPrim()).Bind(bbmaterial)

        test_usd_file_path = self.output_prefix + "test_full_geomsubset_to_submat.usda"
        self.stage.GetRootLayer().Export(test_usd_file_path)

        self.import_options.PreferredMaterial = "maxUsdPreviewSurface"
        RT.USDImporter.ImportFile(test_usd_file_path,importOptions=self.import_options)

        self.assertEqual(len(RT.sceneMaterials), 1)
        self.assertEqual(RT.ClassOf(RT.sceneMaterials[0]), RT.Multimaterial)
        self.assertEqual(len(RT.sceneMaterials[0].names), 5)
        for i in range(0, 5):
            subset_name = "_sub_{0}".format(i)
            self.assertEqual(RT.sceneMaterials[0].names[i], subset_name)
            if i == 0:
                self.assertEqual(RT.sceneMaterials[0][i].name, "displayColor")
                self.assertEqual(RT.ClassOf(RT.sceneMaterials[0][i]), RT.PhysicalMaterial) # default mat
            else:
                self.assertEqual(RT.sceneMaterials[0][i].name, "billboardMat")
                self.assertEqual(RT.ClassOf(RT.sceneMaterials[0][i]), RT.MaxUsdPreviewSurface)
                self.assertAlmostEqual(RT.sceneMaterials[0][i].roughness, 0.4)
                self.assertAlmostEqual(RT.sceneMaterials[0][i].metallic, 0.0)

    def test_single_geomsubset_to_submat(self):
        pyramid = UsdGeom.Mesh.Define(self.stage, "/root/pyramid")
        pyramid.CreatePointsAttr([(-10, 0, -10), (10, 0, -10), (-10, 0, 10), (10, 0, 10), (0, 20, 0)])
        pyramid.CreateFaceVertexCountsAttr([3, 3, 3, 3, 4])
        pyramid.CreateFaceVertexIndicesAttr([0, 1, 4, 0, 2, 4, 3, 1, 4, 3, 2, 4, 0, 1, 3, 2])
        pyramidBindingAPI = UsdShade.MaterialBindingAPI(pyramid)

        bbmaterial = UsdShade.Material.Define(self.stage, '/root/billboardMat')
        pbrShader = UsdShade.Shader.Define(self.stage, '/root/billboardMat/PBRShader')

        pbrShader.CreateIdAttr("UsdPreviewSurface")
        pbrShader.CreateInput("roughness", Sdf.ValueTypeNames.Float).Set(0.4)
        pbrShader.CreateInput("metallic", Sdf.ValueTypeNames.Float).Set(0.0)

        bbmaterial.CreateSurfaceOutput().ConnectToSource(pbrShader.ConnectableAPI(), "surface")

        indices = Vt.IntArray([2])
        subset_name = "_sub_{0}".format(2)
        geom_subset = pyramidBindingAPI.CreateMaterialBindSubset(subset_name, indices)
        UsdShade.MaterialBindingAPI(geom_subset.GetPrim()).Bind(bbmaterial)

        test_usd_file_path = self.output_prefix + "test_single_geomsubset_to_submat.usda"
        self.stage.GetRootLayer().Export(test_usd_file_path)

        self.import_options.PreferredMaterial = "maxUsdPreviewSurface"
        RT.USDImporter.ImportFile(test_usd_file_path,importOptions=self.import_options)

        self.assertEqual(len(RT.sceneMaterials), 1)
        self.assertEqual(RT.ClassOf(RT.sceneMaterials[0]), RT.Multimaterial)
        self.assertEqual(RT.sceneMaterials[0].numsubs, 1)
        self.assertEqual(RT.sceneMaterials[0].names[0], subset_name)
        self.assertEqual(RT.sceneMaterials[0][0].name, "billboardMat")
        self.assertEqual(RT.ClassOf(RT.sceneMaterials[0][0]), RT.MaxUsdPreviewSurface)
        self.assertAlmostEqual(RT.sceneMaterials[0][0].roughness, 0.4)
        self.assertAlmostEqual(RT.sceneMaterials[0][0].metallic, 0.0)


    def test_usd_preview_surface_bitmaps_to_physical_material(self):
        test_bitmap_path = os.path.join(os.path.dirname(__file__), "USDLogoDocs.png")

        billboard = UsdGeom.Mesh.Define(self.stage, "/root/billboard")
        bbmaterial = UsdShade.Material.Define(self.stage, '/root/billboardMat')
        pbrShader = UsdShade.Shader.Define(self.stage, '/root/billboardMat/PBRShader')

        pbrShader.CreateIdAttr("UsdPreviewSurface")

        bbmaterial.CreateSurfaceOutput().ConnectToSource(pbrShader.ConnectableAPI(), "surface")

        stReader = UsdShade.Shader.Define(self.stage, '/TexModel/boardMat/stReader')
        stReader.CreateIdAttr('UsdPrimvarReader_float2')

        diffuseTextureSampler = UsdShade.Shader.Define(self.stage,'/TexModel/boardMat/diffuseTexture')
        diffuseTextureSampler.CreateIdAttr('UsdUVTexture')
        diffuseTextureSampler.CreateInput('file', Sdf.ValueTypeNames.Asset).Set(test_bitmap_path)
        diffuseTextureSampler.CreateInput("st", Sdf.ValueTypeNames.Float2).ConnectToSource(stReader.ConnectableAPI(), 'result')
        diffuseTextureSampler.CreateOutput('rgb', Sdf.ValueTypeNames.Float3)
        pbrShader.CreateInput("diffuseColor", Sdf.ValueTypeNames.Color3f).ConnectToSource(diffuseTextureSampler.ConnectableAPI(), 'rgb')

        stInput = bbmaterial.CreateInput('frame:stPrimvarName', Sdf.ValueTypeNames.Token)
        stInput.Set('st')

        stReader.CreateInput('varname',Sdf.ValueTypeNames.Token).ConnectToSource(stInput)

        roughnessTextureSampler = UsdShade.Shader.Define(self.stage,'/TexModel/boardMat/roughnessTexture')
        roughnessTextureSampler.CreateInput('file', Sdf.ValueTypeNames.Asset).Set(test_bitmap_path)
        pbrShader.CreateInput("roughness", Sdf.ValueTypeNames.Color3f).ConnectToSource(roughnessTextureSampler.ConnectableAPI(), 'rgb')

        metalnessTextureSampler = UsdShade.Shader.Define(self.stage,'/TexModel/boardMat/metalnessTexture')
        metalnessTextureSampler.CreateInput('file', Sdf.ValueTypeNames.Asset).Set(test_bitmap_path)
        pbrShader.CreateInput("metallic", Sdf.ValueTypeNames.Color3f).ConnectToSource(metalnessTextureSampler.ConnectableAPI(), 'rgb')

        normalTextureSampler = UsdShade.Shader.Define(self.stage,'/TexModel/boardMat/normalTexture')
        normalTextureSampler.CreateInput('file', Sdf.ValueTypeNames.Asset).Set(test_bitmap_path)
        pbrShader.CreateInput("normal", Sdf.ValueTypeNames.Color3f).ConnectToSource(normalTextureSampler.ConnectableAPI(), 'rgb')

        occlusionTextureSampler = UsdShade.Shader.Define(self.stage,'/TexModel/boardMat/occlusionTexture')
        occlusionTextureSampler.CreateInput('file', Sdf.ValueTypeNames.Asset).Set(test_bitmap_path)
        pbrShader.CreateInput("occlusion", Sdf.ValueTypeNames.Color3f).ConnectToSource(occlusionTextureSampler.ConnectableAPI(), 'rgb')

        UsdShade.MaterialBindingAPI(billboard).Bind(bbmaterial)

        test_usd_file_path = self.output_prefix + "test_usd_preview_surface_bitmaps_to_physical_material.usda"
        self.stage.GetRootLayer().Export(test_usd_file_path)

        self.import_options.PreferredMaterial = "physicalMaterial"
        RT.USDImporter.ImportFile(test_usd_file_path,importOptions=self.import_options)

        self.assertEqual(len(RT.sceneMaterials), 1)

        max_material = RT.sceneMaterials[0]

        self.assertEqual(RT.ClassOf(max_material), RT.PhysicalMaterial)

        with self.subTest():
            self.assertTrue(hasattr(max_material, "Base_Color_map") and os.path.abspath(test_bitmap_path).lower() == os.path.abspath(max_material.Base_Color_map.sourceMap.filename).lower())

        with self.subTest():
            self.assertTrue(hasattr(max_material, "roughness_map") and os.path.abspath(test_bitmap_path).lower() == os.path.abspath(max_material.roughness_map.sourceMap.filename).lower())

        with self.subTest():
            self.assertTrue(hasattr(max_material, "metalness_map") and os.path.abspath(test_bitmap_path).lower() == os.path.abspath(max_material.metalness_map.sourceMap.filename).lower())

        with self.subTest():
            self.assertTrue(hasattr(max_material, "bump_map") and os.path.abspath(test_bitmap_path).lower() == os.path.abspath(max_material.bump_map.sourceMap.filename).lower())

        with self.subTest():
            # ao_map does not exist for max physical material
            self.assertFalse(hasattr(max_material, "ao_map"))

    def test_usd_preview_surface_bitmaps_to_PBR_metal_rough(self):
        test_bitmap_path = os.path.join(os.path.dirname(__file__), "USDLogoDocs.png")

        billboard = UsdGeom.Mesh.Define(self.stage, "/root/billboard")
        bbmaterial = UsdShade.Material.Define(self.stage, '/root/billboardMat')
        pbrShader = UsdShade.Shader.Define(self.stage, '/root/billboardMat/PBRShader')

        pbrShader.CreateIdAttr("UsdPreviewSurface")

        bbmaterial.CreateSurfaceOutput().ConnectToSource(pbrShader.ConnectableAPI(), "surface")

        stReader = UsdShade.Shader.Define(self.stage, '/TexModel/boardMat/stReader')
        stReader.CreateIdAttr('UsdPrimvarReader_float2')

        diffuseTextureSampler = UsdShade.Shader.Define(self.stage,'/TexModel/boardMat/diffuseTexture')
        diffuseTextureSampler.CreateIdAttr('UsdUVTexture')
        diffuseTextureSampler.CreateInput('file', Sdf.ValueTypeNames.Asset).Set(test_bitmap_path)
        diffuseTextureSampler.CreateInput("st", Sdf.ValueTypeNames.Float2).ConnectToSource(stReader.ConnectableAPI(), 'result')
        diffuseTextureSampler.CreateOutput('rgb', Sdf.ValueTypeNames.Float3)
        pbrShader.CreateInput("diffuseColor", Sdf.ValueTypeNames.Color3f).ConnectToSource(diffuseTextureSampler.ConnectableAPI(), 'rgb')

        stInput = bbmaterial.CreateInput('frame:stPrimvarName', Sdf.ValueTypeNames.Token)
        stInput.Set('st')

        stReader.CreateInput('varname',Sdf.ValueTypeNames.Token).ConnectToSource(stInput)

        roughnessTextureSampler = UsdShade.Shader.Define(self.stage,'/TexModel/boardMat/roughnessTexture')
        roughnessTextureSampler.CreateInput('file', Sdf.ValueTypeNames.Asset).Set(test_bitmap_path)
        pbrShader.CreateInput("roughness", Sdf.ValueTypeNames.Color3f).ConnectToSource(roughnessTextureSampler.ConnectableAPI(), 'rgb')

        metalnessTextureSampler = UsdShade.Shader.Define(self.stage,'/TexModel/boardMat/metalnessTexture')
        metalnessTextureSampler.CreateInput('file', Sdf.ValueTypeNames.Asset).Set(test_bitmap_path)
        pbrShader.CreateInput("metallic", Sdf.ValueTypeNames.Color3f).ConnectToSource(metalnessTextureSampler.ConnectableAPI(), 'rgb')

        normalTextureSampler = UsdShade.Shader.Define(self.stage,'/TexModel/boardMat/normalTexture')
        normalTextureSampler.CreateInput('file', Sdf.ValueTypeNames.Asset).Set(test_bitmap_path)
        pbrShader.CreateInput("normal", Sdf.ValueTypeNames.Color3f).ConnectToSource(normalTextureSampler.ConnectableAPI(), 'rgb')

        occlusionTextureSampler = UsdShade.Shader.Define(self.stage,'/TexModel/boardMat/occlusionTexture')
        occlusionTextureSampler.CreateInput('file', Sdf.ValueTypeNames.Asset).Set(test_bitmap_path)
        pbrShader.CreateInput("occlusion", Sdf.ValueTypeNames.Color3f).ConnectToSource(occlusionTextureSampler.ConnectableAPI(), 'rgb')

        emissiveColorTextureSampler = UsdShade.Shader.Define(self.stage,'/TexModel/boardMat/emissiveColorTexture')
        emissiveColorTextureSampler.CreateInput('file', Sdf.ValueTypeNames.Asset).Set(test_bitmap_path)
        pbrShader.CreateInput("emissiveColor", Sdf.ValueTypeNames.Color3f).ConnectToSource(emissiveColorTextureSampler.ConnectableAPI(), 'rgb')

        opacityTextureSampler = UsdShade.Shader.Define(self.stage,'/TexModel/boardMat/opacityTexture')
        opacityTextureSampler.CreateInput('file', Sdf.ValueTypeNames.Asset).Set(test_bitmap_path)
        pbrShader.CreateInput("opacity", Sdf.ValueTypeNames.Color3f).ConnectToSource(opacityTextureSampler.ConnectableAPI(), 'rgb')


        UsdShade.MaterialBindingAPI(billboard).Bind(bbmaterial)

        test_usd_file_path = self.output_prefix + "test_usd_preview_surface_bitmaps_to_PBR_metal_rough.usda"
        self.stage.GetRootLayer().Export(test_usd_file_path)

        self.import_options.PreferredMaterial = "pbrMetalRough"
        RT.USDImporter.ImportFile(test_usd_file_path,importOptions=self.import_options)

        self.assertEqual(len(RT.sceneMaterials), 1)

        max_material = RT.sceneMaterials[0]

        self.assertEqual(RT.ClassOf(max_material), RT.PBRMetalRough)

        with self.subTest():
            self.assertTrue(hasattr(max_material, "Base_Color_map") and os.path.abspath(test_bitmap_path).lower() == os.path.abspath(max_material.Base_Color_map.sourceMap.filename).lower())

        with self.subTest():
            self.assertTrue(hasattr(max_material, "roughness_map") and os.path.abspath(test_bitmap_path).lower() == os.path.abspath(max_material.roughness_map.sourceMap.filename).lower())

        with self.subTest():
            self.assertTrue(hasattr(max_material, "metalness_map") and os.path.abspath(test_bitmap_path).lower() == os.path.abspath(max_material.metalness_map.sourceMap.filename).lower())

        with self.subTest():
            self.assertTrue(hasattr(max_material, "norm_map") and os.path.abspath(test_bitmap_path).lower() == os.path.abspath(max_material.norm_map.sourceMap.filename).lower())

        with self.subTest():
            self.assertTrue(hasattr(max_material, "ao_map") and os.path.abspath(test_bitmap_path).lower() == os.path.abspath(max_material.ao_map.sourceMap.filename).lower())

        with self.subTest():
            self.assertTrue(hasattr(max_material, "emit_color_map") and os.path.abspath(test_bitmap_path).lower() == os.path.abspath(max_material.emit_color_map.sourceMap.filename).lower())

        with self.subTest():
            self.assertTrue(hasattr(max_material, "opacity_map") and os.path.abspath(test_bitmap_path).lower() == os.path.abspath(max_material.opacity_map.sourceMap.filename).lower())

    def test_usd_preview_surface_bitmaps_to_max_usd_preview_surface(self):
        test_bitmap_path = os.path.join(os.path.dirname(__file__), "USDLogoDocs.png")

        billboard = UsdGeom.Mesh.Define(self.stage, "/root/billboard")
        bbmaterial = UsdShade.Material.Define(self.stage, '/root/billboardMat')
        pbrShader = UsdShade.Shader.Define(self.stage, '/root/billboardMat/PBRShader')

        pbrShader.CreateIdAttr("UsdPreviewSurface")

        bbmaterial.CreateSurfaceOutput().ConnectToSource(pbrShader.ConnectableAPI(), "surface")

        stReader = UsdShade.Shader.Define(self.stage, '/TexModel/boardMat/stReader')
        stReader.CreateIdAttr('UsdPrimvarReader_float2')

        diffuseTextureSampler = UsdShade.Shader.Define(self.stage,'/TexModel/boardMat/diffuseTexture')
        diffuseTextureSampler.CreateIdAttr('UsdUVTexture')
        diffuseTextureSampler.CreateInput('file', Sdf.ValueTypeNames.Asset).Set(test_bitmap_path)
        diffuseTextureSampler.CreateInput("st", Sdf.ValueTypeNames.Float2).ConnectToSource(stReader.ConnectableAPI(), 'result')
        diffuseTextureSampler.CreateOutput('rgb', Sdf.ValueTypeNames.Float3)
        pbrShader.CreateInput("diffuseColor", Sdf.ValueTypeNames.Color3f).ConnectToSource(diffuseTextureSampler.ConnectableAPI(), 'rgb')

        stInput = bbmaterial.CreateInput('frame:stPrimvarName', Sdf.ValueTypeNames.Token)
        stInput.Set('st')

        stReader.CreateInput('varname',Sdf.ValueTypeNames.Token).ConnectToSource(stInput)

        roughnessTextureSampler = UsdShade.Shader.Define(self.stage,'/TexModel/boardMat/roughnessTexture')
        roughnessTextureSampler.CreateInput('file', Sdf.ValueTypeNames.Asset).Set(test_bitmap_path)
        pbrShader.CreateInput("roughness", Sdf.ValueTypeNames.Color3f).ConnectToSource(roughnessTextureSampler.ConnectableAPI(), 'rgb')

        metalnessTextureSampler = UsdShade.Shader.Define(self.stage,'/TexModel/boardMat/metalnessTexture')
        metalnessTextureSampler.CreateInput('file', Sdf.ValueTypeNames.Asset).Set(test_bitmap_path)
        pbrShader.CreateInput("metallic", Sdf.ValueTypeNames.Color3f).ConnectToSource(metalnessTextureSampler.ConnectableAPI(), 'rgb')

        normalTextureSampler = UsdShade.Shader.Define(self.stage,'/TexModel/boardMat/normalTexture')
        normalTextureSampler.CreateInput('file', Sdf.ValueTypeNames.Asset).Set(test_bitmap_path)
        pbrShader.CreateInput("normal", Sdf.ValueTypeNames.Color3f).ConnectToSource(normalTextureSampler.ConnectableAPI(), 'rgb')

        occlusionTextureSampler = UsdShade.Shader.Define(self.stage,'/TexModel/boardMat/occlusionTexture')
        occlusionTextureSampler.CreateInput('file', Sdf.ValueTypeNames.Asset).Set(test_bitmap_path)
        pbrShader.CreateInput("occlusion", Sdf.ValueTypeNames.Color3f).ConnectToSource(occlusionTextureSampler.ConnectableAPI(), 'rgb')

        specularColorTextureSampler = UsdShade.Shader.Define(self.stage,'/TexModel/boardMat/specularColorTexture')
        specularColorTextureSampler.CreateInput('file', Sdf.ValueTypeNames.Asset).Set(test_bitmap_path)
        pbrShader.CreateInput("specularColor", Sdf.ValueTypeNames.Color3f).ConnectToSource(specularColorTextureSampler.ConnectableAPI(), 'rgb')

        emissiveColorTextureSampler = UsdShade.Shader.Define(self.stage,'/TexModel/boardMat/emissiveColorTexture')
        emissiveColorTextureSampler.CreateInput('file', Sdf.ValueTypeNames.Asset).Set(test_bitmap_path)
        pbrShader.CreateInput("emissiveColor", Sdf.ValueTypeNames.Color3f).ConnectToSource(emissiveColorTextureSampler.ConnectableAPI(), 'rgb')

        opacityTextureSampler = UsdShade.Shader.Define(self.stage,'/TexModel/boardMat/opacityTexture')
        opacityTextureSampler.CreateInput('file', Sdf.ValueTypeNames.Asset).Set(test_bitmap_path)
        pbrShader.CreateInput("opacity", Sdf.ValueTypeNames.Color3f).ConnectToSource(opacityTextureSampler.ConnectableAPI(), 'rgb')

        displacementTextureSampler = UsdShade.Shader.Define(self.stage,'/TexModel/boardMat/displacementTexture')
        displacementTextureSampler.CreateInput('file', Sdf.ValueTypeNames.Asset).Set(test_bitmap_path)
        pbrShader.CreateInput("displacement", Sdf.ValueTypeNames.Color3f).ConnectToSource(displacementTextureSampler.ConnectableAPI(), 'rgb')

        iorTextureSampler = UsdShade.Shader.Define(self.stage,'/TexModel/boardMat/iorTexture')
        iorTextureSampler.CreateInput('file', Sdf.ValueTypeNames.Asset).Set(test_bitmap_path)
        pbrShader.CreateInput("ior", Sdf.ValueTypeNames.Color3f).ConnectToSource(iorTextureSampler.ConnectableAPI(), 'rgb')

        clearcoatTextureSampler = UsdShade.Shader.Define(self.stage,'/TexModel/boardMat/clearcoatTexture')
        clearcoatTextureSampler.CreateInput('file', Sdf.ValueTypeNames.Asset).Set(test_bitmap_path)
        pbrShader.CreateInput("clearcoat", Sdf.ValueTypeNames.Color3f).ConnectToSource(clearcoatTextureSampler.ConnectableAPI(), 'rgb')

        clearcoatRoughnessTextureSampler = UsdShade.Shader.Define(self.stage,'/TexModel/boardMat/clearcoatRoughnessTexture')
        clearcoatRoughnessTextureSampler.CreateInput('file', Sdf.ValueTypeNames.Asset).Set(test_bitmap_path)
        pbrShader.CreateInput("clearcoatRoughness", Sdf.ValueTypeNames.Color3f).ConnectToSource(clearcoatRoughnessTextureSampler.ConnectableAPI(), 'rgb')


        UsdShade.MaterialBindingAPI(billboard).Bind(bbmaterial)

        test_usd_file_path = self.output_prefix + "test_usd_preview_surface_bitmaps_to_max_usd_preview_surface.usda"
        self.stage.GetRootLayer().Export(test_usd_file_path)

        self.import_options.PreferredMaterial = "maxUsdPreviewSurface"

        RT.USDImporter.ImportFile(test_usd_file_path,importOptions=self.import_options)

        self.assertEqual(len(RT.sceneMaterials), 1)

        max_material = RT.sceneMaterials[0]

        self.assertEqual(RT.ClassOf(max_material), RT.MaxUsdPreviewSurface)

        with self.subTest():
            self.assertTrue(hasattr(max_material, "diffuseColor_map") and os.path.abspath(test_bitmap_path).lower() == os.path.abspath(max_material.diffuseColor_map.sourceMap.filename).lower())

        with self.subTest():
            self.assertTrue(hasattr(max_material, "roughness_map") and os.path.abspath(test_bitmap_path).lower() == os.path.abspath(max_material.roughness_map.sourceMap.filename).lower())

        with self.subTest():
            self.assertTrue(hasattr(max_material, "metallic_map") and os.path.abspath(test_bitmap_path).lower() == os.path.abspath(max_material.metallic_map.sourceMap.filename).lower())

        with self.subTest():
            self.assertTrue(hasattr(max_material, "specularColor_map") and os.path.abspath(test_bitmap_path).lower() == os.path.abspath(max_material.specularColor_map.sourceMap.filename).lower())

        with self.subTest():
            self.assertTrue(hasattr(max_material, "normal_map") and os.path.abspath(test_bitmap_path).lower() == os.path.abspath(max_material.normal_map.sourceMap.filename).lower())

        with self.subTest():
            self.assertTrue(hasattr(max_material, "occlusion_map") and os.path.abspath(test_bitmap_path).lower() == os.path.abspath(max_material.occlusion_map.sourceMap.filename).lower())

        with self.subTest():
            self.assertTrue(hasattr(max_material, "emissiveColor_map") and os.path.abspath(test_bitmap_path).lower() == os.path.abspath(max_material.emissiveColor_map.sourceMap.filename).lower())

        with self.subTest():
            self.assertTrue(hasattr(max_material, "opacity_map") and os.path.abspath(test_bitmap_path).lower() == os.path.abspath(max_material.opacity_map.sourceMap.filename).lower())

        with self.subTest():
            self.assertTrue(hasattr(max_material, "displacement_map") and os.path.abspath(test_bitmap_path).lower() == os.path.abspath(max_material.displacement_map.sourceMap.filename).lower())

        with self.subTest():
            self.assertTrue(hasattr(max_material, "ior_map") and os.path.abspath(test_bitmap_path).lower() == os.path.abspath(max_material.ior_map.sourceMap.filename).lower())

        with self.subTest():
            self.assertTrue(hasattr(max_material, "clearcoat_map") and os.path.abspath(test_bitmap_path).lower() == os.path.abspath(max_material.clearcoat_map.sourceMap.filename).lower())

        with self.subTest():
            self.assertTrue(hasattr(max_material, "clearcoatRoughness_map") and os.path.abspath(test_bitmap_path).lower() == os.path.abspath(max_material.clearcoatRoughness_map.sourceMap.filename).lower())

    def test_usd_preview_surface_texture_map_from_graph(self):
        
        # This test makes sure that we support connected "NodeGraph" in UsdPreviewSurface map inputs.
        # Instead of connecting a UsdUVTexture directly, we connect a NodeGraph, itself wrapping
        # another graph (to make sure we look recursively until we find an actual map), which then connects
        # to the texture.
        
        test_bitmap_path = os.path.join(os.path.dirname(__file__), "USDLogoDocs.png")

        cube = UsdGeom.Mesh.Define(self.stage, "/root/billboard")
        cubeMaterial = UsdShade.Material.Define(self.stage, '/root/cubeMaterial')
        shader = UsdShade.Shader.Define(self.stage, '/root/cubeMaterial/UsdPreviewSurface')

        shader.CreateIdAttr("UsdPreviewSurface")

        cubeMaterial.CreateSurfaceOutput().ConnectToSource(shader.ConnectableAPI(), "surface")

        # Create the two node graphs.
        graph1 = UsdShade.NodeGraph.Define(self.stage, '/root/cubeMaterial/g1')
        graph2 = UsdShade.NodeGraph.Define(self.stage, '/root/cubeMaterial/g1/g2')
        
        stReader = UsdShade.Shader.Define(self.stage, '/root/cubeMaterial/g1/g2/stReader')
        stReader.CreateIdAttr('UsdPrimvarReader_float2')

        # Create the actual texture map, all the way down the hierarchy.
        diffuseTextureSampler = UsdShade.Shader.Define(self.stage,'/root/cubeMaterial/g1/g2/diffuseTexture')
        diffuseTextureSampler.CreateIdAttr('UsdUVTexture')
        diffuseTextureSampler.CreateInput('file', Sdf.ValueTypeNames.Asset).Set(test_bitmap_path)
        diffuseTextureSampler.CreateInput("st", Sdf.ValueTypeNames.Float2).ConnectToSource(stReader.ConnectableAPI(), 'result')
        diffuseTextureSampler.CreateOutput('rgb', Sdf.ValueTypeNames.Float3)
        
        stInput = cubeMaterial.CreateInput('frame:stPrimvarName', Sdf.ValueTypeNames.Token)
        stInput.Set('st')

        stReader.CreateInput('varname',Sdf.ValueTypeNames.Token).ConnectToSource(stInput)
        
        # Wire up the texture output...
        # UsdPreviewSurface <-- Graph1:rgb <-- Graph2:rgb <-- diffuseTextureSampler:result
        graph2_output = graph2.CreateOutput("rgb", Sdf.ValueTypeNames.Float3)
        graph2_output.ConnectToSource(diffuseTextureSampler.ConnectableAPI(), "result")
        graph1_output = graph1.CreateOutput("rgb", Sdf.ValueTypeNames.Float3)
        graph1_output.ConnectToSource(graph2.ConnectableAPI(), "rgb")
        
        shader.CreateInput("diffuseColor", Sdf.ValueTypeNames.Color3f).ConnectToSource(graph1.ConnectableAPI(), 'rgb')
        
        UsdShade.MaterialBindingAPI(cube).Bind(cubeMaterial)

        test_usd_file_path = self.output_prefix + "test_usd_preview_surface_texture_map_from_graph.usda"
        self.stage.GetRootLayer().Export(test_usd_file_path)
        
        reloader = Usd.Stage.Open(test_usd_file_path)
        reloader.Reload()

        self.import_options.PreferredMaterial = "maxUsdPreviewSurface"

        RT.USDImporter.ImportFile(test_usd_file_path,importOptions=self.import_options)

        # Make sure the importer resolved the texture correctly from the shader node graph.
        self.assertEqual(len(RT.sceneMaterials), 1)
        max_material = RT.sceneMaterials[0]
        self.assertEqual(RT.ClassOf(max_material), RT.MaxUsdPreviewSurface)
        self.assertTrue(hasattr(max_material, "diffuseColor_map") and os.path.abspath(test_bitmap_path).lower() == os.path.abspath(max_material.diffuseColor_map.sourceMap.filename).lower())

    def test_unbound_material_import(self):
        test_bitmap_path = os.path.join(os.path.dirname(__file__), "USDLogoDocs.png")

        bbmaterial = UsdShade.Material.Define(self.stage, '/root/billboardMat')
        pbrShader = UsdShade.Shader.Define(self.stage, '/root/billboardMat/PBRShader')

        pbrShader.CreateIdAttr("UsdPreviewSurface")

        bbmaterial.CreateSurfaceOutput().ConnectToSource(pbrShader.ConnectableAPI(), "surface")

        diffuseTextureSampler = UsdShade.Shader.Define(self.stage,'/TexModel/boardMat/diffuseTexture')
        diffuseTextureSampler.CreateIdAttr('UsdUVTexture')
        diffuseTextureSampler.CreateInput('file', Sdf.ValueTypeNames.Asset).Set(test_bitmap_path)
        pbrShader.CreateInput("diffuseColor", Sdf.ValueTypeNames.Color3f).ConnectToSource(diffuseTextureSampler.ConnectableAPI(), 'rgb')

        test_usd_file_path = self.output_prefix + "test_unbound_material_import.usda"
        self.stage.GetRootLayer().Export(test_usd_file_path)

        RT.USDImporter.ImportFile(test_usd_file_path,importOptions=self.import_options)

        self.assertEqual(len(RT.sceneMaterials), 0)

    def test_invalid_texture_name_material(self):
        billboard = UsdGeom.Mesh.Define(self.stage, "/root/billboard")
        bbmaterial = UsdShade.Material.Define(self.stage, '/root/billboardMat')
        pbrShader = UsdShade.Shader.Define(self.stage, '/root/billboardMat/PBRShader')

        pbrShader.CreateIdAttr("UsdPreviewSurface")

        bbmaterial.CreateSurfaceOutput().ConnectToSource(pbrShader.ConnectableAPI(), "surface")

        diffuseTextureSampler = UsdShade.Shader.Define(self.stage,'/TexModel/boardMat/diffuseTexture')
        diffuseTextureSampler.CreateIdAttr('UsdUVTexture')
        diffuseTextureSampler.CreateInput('file', Sdf.ValueTypeNames.Asset).Set("USDLogoDocsFake.png")
        pbrShader.CreateInput("diffuseColor", Sdf.ValueTypeNames.Color3f).ConnectToSource(diffuseTextureSampler.ConnectableAPI(), 'rgb')

        UsdShade.MaterialBindingAPI(billboard).Bind(bbmaterial)

        test_usd_file_path = self.output_prefix + "test_invalid_texture_name_material.usda"
        self.stage.GetRootLayer().Export(test_usd_file_path)

        RT.USDImporter.ImportFile(test_usd_file_path,importOptions=self.import_options)

        self.assertEqual(len(RT.sceneMaterials), 1)

        max_material = RT.sceneMaterials[0]

        self.assertEqual(max_material.diffuseColor_map.sourceMap.filename, '')


def run_tests():
    return unittest.TextTestRunner(stream=sys.stdout, verbosity=2).run(unittest.TestLoader().loadTestsFromTestCase(TestMaterialImport))

if __name__ == '__main__':
    run_tests()
