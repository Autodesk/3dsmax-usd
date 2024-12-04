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
import sys
import os
import glob

from pxr import Usd, UsdShade
import usd_test_helpers

mxs = pymxs.runtime
NOPROMPT = mxs.Name("noprompt")
test_dir = os.path.dirname(__file__)


class TestMaterialSwitcher(unittest.TestCase):
    def setUp(self):
        self.output_prefix = usd_test_helpers.standard_output_prefix("TestMaterialSwitcher_")

        mxs.resetMaxFile(NOPROMPT)
        usd_test_helpers.load_usd_plugins()

        # create meshes
        self.test_trans = mxs.Box(name="cube")
        # create material for mesh
        self.materialSwitcherName = "MaterialSwitcherName"
        self.test_trans.material = mxs.Material_Switcher()
        self.test_trans.material.name = self.materialSwitcherName
        self.test_trans.material.materialList[0] = mxs.PhysicalMaterial()
        self.test_trans.material.materialList[0].name = "MaterialName1"
        self.test_trans.material.names[0] = "Material1"
        self.test_trans.material.materialList[1] = mxs.PhysicalMaterial()
        self.test_trans.material.materialList[1].name = "MaterialName2"
        self.test_trans.material.names[1] = "Material2"

    def test_as_variant_sets_material_export_style(self):
        """Test material switcher export as variant sets"""
        test_usd_file_path = self.output_prefix + "asVariantSets_mat.usda"
        
        # export
        export_options = mxs.USDExporter.CreateOptions()
        export_options.FileFormat = mxs.Name("ascii")
        export_options.MtlSwitcherExportStyle = mxs.Name("asVariantSets")
        export_options.RootPrimPath = "/"
        ret = mxs.USDExporter.ExportFile(
            test_usd_file_path,
            exportOptions=export_options,
            contentSource=mxs.Name("nodeList"),
            nodeList=[self.test_trans],
        )
        self.assertTrue(ret)

        # test
        stage = Usd.Stage.Open(test_usd_file_path)
        self.assertIsInstance(stage, Usd.Stage)
        
        cubePrim = stage.GetPrimAtPath("/cube")
        self.assertTrue(Usd.Prim.IsValid(cubePrim))

        materialRootPath = export_options.RootPrimPath + export_options.MaterialPrimPath
        materialSwitcherNamePath = materialRootPath + "/" + self.test_trans.material.name
        materialName1Path = materialRootPath + "/" + self.test_trans.material.materialList[0].name
        materialName2Path = materialRootPath + "/" + self.test_trans.material.materialList[1].name

        self.assertTrue(UsdShade.MaterialBindingAPI(cubePrim).GetDirectBindingRel().HasAuthoredTargets())
        bindingTargets = UsdShade.MaterialBindingAPI(cubePrim).GetDirectBindingRel().GetTargets()
        self.assertEqual(1, len(bindingTargets))
        self.assertEqual(materialSwitcherNamePath, bindingTargets[0])

        mtlSwitcherPrim = stage.GetPrimAtPath(materialSwitcherNamePath)
        self.assertTrue(Usd.Prim.IsValid(mtlSwitcherPrim))
        self.assertTrue(mtlSwitcherPrim.GetVariantSets().HasVariantSet("shadingVariant"))
        materialVariantSet = mtlSwitcherPrim.GetVariantSets().GetVariantSet("shadingVariant")
        
        materialVariantSetNames = materialVariantSet.GetVariantNames()
        self.assertEqual(2, len(materialVariantSetNames))
        self.assertEqual(self.test_trans.material.materialList[0].name, materialVariantSetNames[0])
        self.assertEqual(self.test_trans.material.materialList[1].name, materialVariantSetNames[1])        
        activeMaterial = self.test_trans.material.GetActiveMaterial()
        self.assertEqual(activeMaterial.name, materialVariantSet.GetVariantSelection())
        
        # all materials are exported
        materialRootPrim = stage.GetPrimAtPath(materialRootPath)
        materialRootChildren = materialRootPrim.GetChildren()
        self.assertEqual(3, len(materialRootChildren))
        self.assertEqual(materialSwitcherNamePath, materialRootChildren[0].GetPath())
        self.assertEqual(materialName1Path, materialRootChildren[1].GetPath())
        self.assertEqual(materialName2Path, materialRootChildren[2].GetPath())

    def test_active_material_export_style(self):
        """Test material switcher export active material"""
        test_usd_file_path = self.output_prefix + "active_mat.usda"
        
        # export
        export_options = mxs.USDExporter.CreateOptions()
        export_options.FileFormat = mxs.Name("ascii")
        export_options.MtlSwitcherExportStyle = mxs.Name("activeMaterial")
        export_options.RootPrimPath = "/"
        ret = mxs.USDExporter.ExportFile(
            test_usd_file_path,
            exportOptions=export_options,
            contentSource=mxs.Name("nodeList"),
            nodeList=[self.test_trans],
        )
        self.assertTrue(ret)

        # test
        stage = Usd.Stage.Open(test_usd_file_path)
        self.assertIsInstance(stage, Usd.Stage)
        
        cubePrim = stage.GetPrimAtPath("/cube")
        self.assertTrue(Usd.Prim.IsValid(cubePrim))
        materialRootPath = export_options.RootPrimPath + export_options.MaterialPrimPath
        materialSwitcherNamePath = materialRootPath + "/" + self.test_trans.material.name
        materialName1Path = materialRootPath + "/" + self.test_trans.material.materialList[0].name
        materialName2Path = materialRootPath + "/" + self.test_trans.material.materialList[1].name

        self.assertTrue(UsdShade.MaterialBindingAPI(cubePrim).GetDirectBindingRel().HasAuthoredTargets())
        bindingTargets = UsdShade.MaterialBindingAPI(cubePrim).GetDirectBindingRel().GetTargets()
        self.assertEqual(1, len(bindingTargets))
        self.assertEqual(materialSwitcherNamePath, bindingTargets[0])
        
        mtlSwitcherPrim = stage.GetPrimAtPath(materialSwitcherNamePath)
        self.assertTrue(Usd.Prim.IsValid(mtlSwitcherPrim))
        self.assertFalse(mtlSwitcherPrim.GetVariantSets().HasVariantSet("shadingVariant"))

        # only active material is exported with the MtlSwitcher material
        materialRootPrim = stage.GetPrimAtPath(materialRootPath)
        materialRootChildren = materialRootPrim.GetChildren()
        self.assertEqual(2, len(materialRootChildren))
        self.assertEqual(materialSwitcherNamePath, materialRootChildren[0].GetPath())
        self.assertEqual(materialName1Path, materialRootChildren[1].GetPath())
        
    def test_as_variant_sets_material_export_style_with_one_material_switch(self):
        """Test material switcher export as variant sets when only one material switch is defined"""
        test_usd_file_path = self.output_prefix + "asVariantSetsOneMaterialSwitch_mat.usda"

        mxs.delete(self.test_trans)
        # create meshes
        test_sphere = mxs.Sphere(name="sphere")
        # create material for mesh
        materialSwitcherSingleMatName = "MaterialSwitcherSingleMatName"
        test_sphere.material = mxs.Material_Switcher()
        test_sphere.material.name = self.materialSwitcherName
        test_sphere.material.materialList[0] = mxs.PhysicalMaterial()
        test_sphere.material.materialList[0].name = "MaterialName1"
        test_sphere.material.names[0] = "MaterialSingle"

        # export
        export_options = mxs.USDExporter.CreateOptions()
        export_options.FileFormat = mxs.Name("ascii")
        export_options.MtlSwitcherExportStyle = mxs.Name("asVariantSets")
        export_options.RootPrimPath = "/"
        ret = mxs.USDExporter.ExportFile(
            test_usd_file_path,
            exportOptions=export_options,
            contentSource=mxs.Name("nodeList"),
            nodeList=[test_sphere],
        )
        self.assertTrue(ret)

        # test
        stage = Usd.Stage.Open(test_usd_file_path)
        self.assertIsInstance(stage, Usd.Stage)
        
        spherePrim = stage.GetPrimAtPath("/sphere")
        self.assertTrue(Usd.Prim.IsValid(spherePrim))
        materialRootPath = export_options.RootPrimPath + export_options.MaterialPrimPath
        materialSwitcherNamePath = materialRootPath + "/" + test_sphere.material.name
        materialName1Path = materialRootPath + "/" + test_sphere.material.materialList[0].name

        self.assertTrue(UsdShade.MaterialBindingAPI(spherePrim).GetDirectBindingRel().HasAuthoredTargets())
        bindingTargets = UsdShade.MaterialBindingAPI(spherePrim).GetDirectBindingRel().GetTargets()
        self.assertEqual(1, len(bindingTargets))
        self.assertEqual(materialSwitcherNamePath, bindingTargets[0])
        
        mtlSwitcherPrim = stage.GetPrimAtPath(materialSwitcherNamePath)
        self.assertTrue(Usd.Prim.IsValid(mtlSwitcherPrim))
        self.assertFalse(mtlSwitcherPrim.GetVariantSets().HasVariantSet("shadingVariant"))

        # only active material is exported with the MtlSwitcher material
        materialRootPrim = stage.GetPrimAtPath(materialRootPath)
        materialRootChildren = materialRootPrim.GetChildren()
        self.assertEqual(2, len(materialRootChildren))
        self.assertEqual(materialSwitcherNamePath, materialRootChildren[0].GetPath())
        self.assertEqual(materialName1Path, materialRootChildren[1].GetPath())

    def test_as_variant_sets_material_export_style_with_no_material_switch(self):
        """Test material switcher export as variant sets when no material switch is defined"""
        test_usd_file_path = self.output_prefix + "asVariantSetsNoMaterialSwitch_mat.usda"

        mxs.delete(self.test_trans)
        # create meshes
        test_sphere = mxs.Sphere(name="sphere")
        # create material for mesh
        materialSwitcherNoMatName = "MaterialSwitcherNoMatName"
        test_sphere.material = mxs.Material_Switcher()
        test_sphere.material.name = self.materialSwitcherName

        # export
        export_options = mxs.USDExporter.CreateOptions()
        export_options.FileFormat = mxs.Name("ascii")
        export_options.MtlSwitcherExportStyle = mxs.Name("asVariantSets")
        export_options.RootPrimPath = "/"
        ret = mxs.USDExporter.ExportFile(
            test_usd_file_path,
            exportOptions=export_options,
            contentSource=mxs.Name("nodeList"),
            nodeList=[test_sphere],
        )
        self.assertTrue(ret)

        # test
        stage = Usd.Stage.Open(test_usd_file_path)
        self.assertIsInstance(stage, Usd.Stage)
        
        spherePrim = stage.GetPrimAtPath("/sphere")
        self.assertTrue(Usd.Prim.IsValid(spherePrim))

        materialRootPath = export_options.RootPrimPath + export_options.MaterialPrimPath
        materialSwitcherNamePath = materialRootPath + "/" + test_sphere.material.name

        self.assertFalse(UsdShade.MaterialBindingAPI(spherePrim).GetDirectBindingRel().HasAuthoredTargets())
        
        mtlSwitcherPrim = stage.GetPrimAtPath(materialSwitcherNamePath)
        self.assertFalse(Usd.Prim.IsValid(mtlSwitcherPrim))
        
        # no material exported
        materialRootPath = export_options.RootPrimPath + "/" + export_options.MaterialPrimPath
        materialRootPrim = stage.GetPrimAtPath(materialRootPath)
        self.assertFalse(materialRootPrim.IsValid())

    def test_as_variant_sets_material_export_style_with_two_material_targets(self):
        """Test material switcher export as variant sets when only one material switch is defined, with two material targets"""
        test_usd_file_path = self.output_prefix + "asVariantSetsTwoMaterialTargets_mat.usda"

        mxs.delete(self.test_trans)
        # create meshe
        test_sphere = mxs.Sphere(name="sphere")
        # create materials for mesh
        test_sphere.material = mxs.Material_Switcher()
        test_sphere.material.name = self.materialSwitcherName
        test_sphere.material.materialList[0] = mxs.MaterialXMaterial()
        test_sphere.material.materialList[0].name = "MaterialName1"
        test_sphere.material.names[0] = "Material1"
        test_sphere.material.materialList[1] = mxs.MaterialXMaterial()
        test_sphere.material.materialList[1].name = "MaterialName2"
        test_sphere.material.names[1] = "Material2"

        # Load a materialX doc into the Materials.
        mtlxDoc = os.path.dirname(os.path.abspath(__file__)) + "\\data\\Iberian_Blue_Ceramic_Tiles_1k_8b\\Iberian_Blue_Ceramic_Tiles.mtlx"
        test_sphere.material.materialList[0].importMaterial(mtlxDoc)
        test_sphere.material.materialList[1].importMaterial(mtlxDoc)

        # export
        export_options = mxs.USDExporter.CreateOptions()
        export_options.FileFormat = mxs.Name("ascii")
        export_options.MtlSwitcherExportStyle = mxs.Name("asVariantSets")
        export_options.RootPrimPath = "/"
        materialTargets = mxs.Array("UsdPreviewSurface", "MaterialX")
        export_options.AllMaterialTargets = materialTargets
        ret = mxs.USDExporter.ExportFile(
            test_usd_file_path,
            exportOptions=export_options,
            contentSource=mxs.Name("nodeList"),
            nodeList=[test_sphere],
        )
        self.assertTrue(ret)

        # test
        stage = Usd.Stage.Open(test_usd_file_path)
        self.assertIsInstance(stage, Usd.Stage)

        spherePrim = stage.GetPrimAtPath("/sphere")
        self.assertTrue(Usd.Prim.IsValid(spherePrim))
        materialRootPath = export_options.RootPrimPath + export_options.MaterialPrimPath
        materialSwitcherNamePath = materialRootPath + "/" + test_sphere.material.name
        materialName1Path = materialRootPath + "/" + test_sphere.material.materialList[0].name
        materialName2Path = materialRootPath + "/" + test_sphere.material.materialList[1].name

        self.assertTrue(UsdShade.MaterialBindingAPI(spherePrim).GetDirectBindingRel().HasAuthoredTargets())
        bindingTargets = UsdShade.MaterialBindingAPI(spherePrim).GetDirectBindingRel().GetTargets()
        self.assertEqual(1, len(bindingTargets))
        self.assertEqual(materialSwitcherNamePath, bindingTargets[0])
        
        mtlSwitcherPrim = stage.GetPrimAtPath(materialSwitcherNamePath)
        self.assertTrue(Usd.Prim.IsValid(mtlSwitcherPrim))
        self.assertTrue(mtlSwitcherPrim.GetVariantSets().HasVariantSet("shadingVariant"))

        materialVariantSet = mtlSwitcherPrim.GetVariantSets().GetVariantSet("shadingVariant")
        materialVariantSetNames = materialVariantSet.GetVariantNames()
        self.assertEqual(2, len(materialVariantSetNames))
        self.assertEqual(test_sphere.material.materialList[0].name, materialVariantSetNames[0])
        self.assertEqual(test_sphere.material.materialList[1].name, materialVariantSetNames[1])        
        activeMaterial = test_sphere.material.GetActiveMaterial()
        self.assertEqual(activeMaterial.name, materialVariantSet.GetVariantSelection())

        materialRootPrim = stage.GetPrimAtPath(materialRootPath)
        materialRootChildren = materialRootPrim.GetChildren()
        self.assertEqual(3, len(materialRootChildren))
        self.assertEqual(materialSwitcherNamePath, materialRootChildren[0].GetPath())
        self.assertEqual(materialName1Path, materialRootChildren[1].GetPath())
        self.assertEqual(materialName2Path, materialRootChildren[2].GetPath())

        # Validate that Material1 was properly exported
        # MaterialX UsdShade node graph
        mtlxTargetPrim = stage.GetPrimAtPath(materialName1Path + "/" + "MaterialX")
        self.assertTrue(mtlxTargetPrim.IsValid())
        # UsdPreviewSurface UsdShade node graph
        uspsTargetPrim = stage.GetPrimAtPath(materialName1Path + "/" + "UsdPreviewSurface")
        self.assertTrue(uspsTargetPrim.IsValid())

        # Validate that Material2 was properly exported
        # MaterialX UsdShade node graph
        mtlxTargetPrim = stage.GetPrimAtPath(materialName2Path + "/" + "MaterialX")
        self.assertTrue(mtlxTargetPrim.IsValid())
        # UsdPreviewSurface UsdShade node graph
        uspsTargetPrim = stage.GetPrimAtPath(materialName2Path + "/" + "UsdPreviewSurface")
        self.assertTrue(uspsTargetPrim.IsValid())
    
    def test_active_material_with_multisub(self):
        """Test material switcher export active material when a multi/sub-object material is the active material"""
        test_usd_file_path = self.output_prefix + "activeMaterialWithMultisub.usda"

        mxs.delete(self.test_trans)
        
        # create mesh
        #box has 6 mat IDs [1-6]
        test_box = mxs.Box(name="box")
        
        multiMat = mxs.Multimaterial()
        multiMat.name = "MultiMat"
        multiMat.numsubs = 3
        switcherMat = mxs.Material_Switcher()
        switcherMat.name = "Switcher"
        physMatOne = mxs.PhysicalMaterial()
        physMatOne.name = "Material1"
        physMatTwo = mxs.PhysicalMaterial()
        physMatTwo.name = "Material2"
        physMatThree = mxs.PhysicalMaterial()
        physMatThree.name = "Material3"
        
        #leave [0] empty on purpose
        multiMat.materialList[0] = mxs.undefined
        multiMat.materialList[1] = physMatOne
        multiMat.materialList[2] = physMatTwo
        switcherMat.materialList[0] = multiMat
        switcherMat.materialList[1] = physMatThree
        
        test_box.mat = switcherMat
        
        export_options = mxs.USDExporter.CreateOptions()
        export_options.FileFormat = mxs.Name("ascii")
        export_options.MtlSwitcherExportStyle = mxs.Name("activeMaterial")
        export_options.RootPrimPath = "/"
        ret = mxs.USDExporter.ExportFile(
            test_usd_file_path,
            exportOptions=export_options
        )
        self.assertTrue(ret)
        
        stage = Usd.Stage.Open(test_usd_file_path)
        self.assertIsInstance(stage, Usd.Stage)

        materialRootPath = export_options.RootPrimPath + export_options.MaterialPrimPath
        materialSwitcherNamePath = materialRootPath + "/" + test_box.material.name
        materialName1Path = materialRootPath + "/" + test_box.material.materialList[0].materialList[1].name
        materialName2Path = materialRootPath + "/" + test_box.material.materialList[0].materialList[2].name
        materialName3Path = materialRootPath + "/" + test_box.material.materialList[1].name
        
        boxPrim = stage.GetPrimAtPath("/box")
        self.assertTrue(Usd.Prim.IsValid(boxPrim))
        children = boxPrim.GetChildren()
        id = 1
        for child in children:
            if child.GetTypeName() == "GeomSubset" :
                bindingTargets = UsdShade.MaterialBindingAPI(child).GetDirectBindingRel().GetTargets()
                self.assertEqual(1, len(bindingTargets))
                refMat = stage.GetPrimAtPath(bindingTargets[0]).GetChildren()
                if (id % 3) == 1:
                    self.assertEqual([], refMat)
                if (id % 3) == 2:
                    self.assertEqual("Material1", refMat[0].GetName())
                if (id % 3) == 0:
                    self.assertEqual("Material2", refMat[0].GetName())
                id = id + 1
                
        mtlSwitcherPrim = stage.GetPrimAtPath(materialSwitcherNamePath)
        self.assertTrue(Usd.Prim.IsValid(mtlSwitcherPrim))
        self.assertFalse(mtlSwitcherPrim.GetVariantSets().HasVariantSet("shadingVariant"))
        
        #make sure that physMatTwo was exported
        physMatTwoPrim = stage.GetPrimAtPath(materialName2Path)
        self.assertTrue(Usd.Prim.IsValid(physMatTwoPrim))
        
        #make sure that physMatThree was NOT exported
        physMatThreePrim = stage.GetPrimAtPath(materialName3Path)
        self.assertFalse(Usd.Prim.IsValid(physMatThreePrim))
    
    def test_variant_set_with_multisub(self):
        """Test material switcher export as variant sets when a multi/sub-object material is one of the variants"""
        test_usd_file_path = self.output_prefix + "variantSetWithMultisub.usda"
        
        mxs.delete(self.test_trans)
        
        # create meshes
        #box has 6 mat IDs [1-6]
        test_box = mxs.Box(name="box")
        #sphere has only 1 mat ID: 2
        test_sphere = mxs.Sphere(name="sphere")
        
        multiMat = mxs.Multimaterial()
        multiMat.name = "MultiMat"
        multiMat.numsubs = 3
        switcherMat = mxs.Material_Switcher()
        switcherMat.name = "Switcher"
        physMatOne = mxs.PhysicalMaterial()
        physMatOne.name = "Material1"
        physMatTwo = mxs.PhysicalMaterial()
        physMatTwo.name = "Material2"
        
        #leave [0] empty on purpose
        multiMat.materialList[0] = mxs.undefined
        multiMat.materialList[1] = physMatOne
        multiMat.materialList[2] = physMatTwo
        switcherMat.materialList[0] = multiMat
        switcherMat.materialList[1] = physMatOne
        
        test_box.mat = switcherMat
        test_sphere.mat = switcherMat
        
        export_options = mxs.USDExporter.CreateOptions()
        export_options.FileFormat = mxs.Name("ascii")
        export_options.MtlSwitcherExportStyle = mxs.Name("asVariantSets")
        export_options.RootPrimPath = "/"
        ret = mxs.USDExporter.ExportFile(
            test_usd_file_path,
            exportOptions=export_options
        )
        self.assertTrue(ret)
        
        stage = Usd.Stage.Open(test_usd_file_path)
        self.assertIsInstance(stage, Usd.Stage)

        materialRootPath = export_options.RootPrimPath + export_options.MaterialPrimPath
        materialSwitcherNamePath = materialRootPath + "/" + test_sphere.material.name
        materialName1Path = materialRootPath + "/" + test_sphere.material.materialList[0].materialList[1].name
        materialName2Path = materialRootPath + "/" + test_sphere.material.materialList[0].materialList[2].name

        spherePrim = stage.GetPrimAtPath("/sphere")
        self.assertTrue(Usd.Prim.IsValid(spherePrim))
        self.assertTrue(UsdShade.MaterialBindingAPI(spherePrim).GetDirectBindingRel().HasAuthoredTargets())
        bindingTargets = UsdShade.MaterialBindingAPI(spherePrim).GetDirectBindingRel().GetTargets()
        self.assertEqual(1, len(bindingTargets))
        refMat = stage.GetPrimAtPath(bindingTargets[0]).GetChildren()
        self.assertEqual("Material1", refMat[0].GetName())
        
        boxPrim = stage.GetPrimAtPath("/box")
        self.assertTrue(Usd.Prim.IsValid(boxPrim))
        children = boxPrim.GetChildren()
        id = 1
        for child in children:
            if child.GetTypeName() == "GeomSubset" :
                bindingTargets = UsdShade.MaterialBindingAPI(child).GetDirectBindingRel().GetTargets()
                self.assertEqual(1, len(bindingTargets))
                refMat = stage.GetPrimAtPath(bindingTargets[0]).GetChildren()
                if (id % 3) == 1:
                    self.assertEqual([], refMat)
                if (id % 3) == 2:
                    self.assertEqual("Material1", refMat[0].GetName())
                if (id % 3) == 0:
                    self.assertEqual("Material2", refMat[0].GetName())
                id = id + 1
                
        mtlSwitcherPrim = stage.GetPrimAtPath(materialSwitcherNamePath)
        self.assertTrue(Usd.Prim.IsValid(mtlSwitcherPrim))
        self.assertTrue(mtlSwitcherPrim.GetVariantSets().HasVariantSet("shadingVariant"))

        materialVariantSet = mtlSwitcherPrim.GetVariantSets().GetVariantSet("shadingVariant")
        materialVariantSetNames = materialVariantSet.GetVariantNames()
        self.assertEqual(2, len(materialVariantSetNames))
        self.assertEqual("Material1", materialVariantSetNames[0])
        self.assertEqual("MultiMat", materialVariantSetNames[1])
        activeMaterial = test_sphere.material.GetActiveMaterial()
        self.assertEqual(activeMaterial.name, materialVariantSet.GetVariantSelection())
        
        #make sure that physMatTwo was exported
        physMatTwoPrim = stage.GetPrimAtPath(materialName2Path)
        self.assertTrue(Usd.Prim.IsValid(physMatTwoPrim))
        
    def test_variant_set_with_multisub_and_switcher(self):
        """Test material switcher export as variant sets when a multi/sub-object material is one of the variants, which itself has a Material Switcher"""
        test_usd_file_path = self.output_prefix + "variantSetWithMultisubAndSwitcher.usda"
        
        mxs.delete(self.test_trans)
        
        # create meshes
        #box has 6 mat IDs [1-6]
        test_box = mxs.Box(name="box")
        
        multiMat = mxs.Multimaterial()
        multiMat.name = "MultiMat"
        multiMat.numsubs = 3
        switcherMat = mxs.Material_Switcher()
        switcherMat.name = "Switcher"
        nestedSwitcherMat = mxs.Material_Switcher()
        nestedSwitcherMat.name = "NestedSwitcher"
        physMatOne = mxs.PhysicalMaterial()
        physMatOne.name = "Material1"
        physMatTwo = mxs.PhysicalMaterial()
        physMatTwo.name = "Material2"
        physMatThree = mxs.PhysicalMaterial()
        physMatThree.name = "Material3"
        physMatFour = mxs.PhysicalMaterial()
        physMatFour.name = "Material4"
        
        test_box.mat = switcherMat
        
        switcherMat.materialList[0] = multiMat
        switcherMat.materialList[1] = physMatOne
        #leave [0] empty on purpose
        multiMat.materialList[0] = mxs.undefined
        multiMat.materialList[1] = nestedSwitcherMat
        multiMat.materialList[2] = physMatTwo
        nestedSwitcherMat.materialList[0] = physMatThree
        nestedSwitcherMat.materialList[1] = physMatFour 
        
        export_options = mxs.USDExporter.CreateOptions()
        export_options.FileFormat = mxs.Name("ascii")
        export_options.MtlSwitcherExportStyle = mxs.Name("asVariantSets")
        export_options.RootPrimPath = "/"
        ret = mxs.USDExporter.ExportFile(
            test_usd_file_path,
            exportOptions=export_options
        )
        self.assertTrue(ret)
        
        stage = Usd.Stage.Open(test_usd_file_path)
        self.assertIsInstance(stage, Usd.Stage)

        materialRootPath = export_options.RootPrimPath + export_options.MaterialPrimPath
        materialSwitcherNamePath = materialRootPath + "/" + test_box.material.name
        nestedSwitcherMatPath = materialRootPath + "/" + nestedSwitcherMat.name
        materialName1Path = materialRootPath + "/" + physMatOne.name
        materialName2Path = materialRootPath + "/" + physMatTwo.name
        materialName3Path = materialRootPath + "/" + physMatThree.name
        materialName4Path = materialRootPath + "/" + physMatFour.name
        
        boxPrim = stage.GetPrimAtPath("/box")
        self.assertTrue(Usd.Prim.IsValid(boxPrim))
        children = boxPrim.GetChildren()
        id = 1
        for child in children:
            if child.GetTypeName() == "GeomSubset" :
                bindingTargets = UsdShade.MaterialBindingAPI(child).GetDirectBindingRel().GetTargets()
                self.assertEqual(1, len(bindingTargets))
                refMat = stage.GetPrimAtPath(bindingTargets[0]).GetChildren()
                if (id % 3) == 1:
                    self.assertEqual([], refMat)
                if (id % 3) == 2:
                    # Material 3 is the active material in the Nested Switcher
                    self.assertEqual("Material3", refMat[0].GetName())
                if (id % 3) == 0:
                    self.assertEqual("Material2", refMat[0].GetName())
                id = id + 1
        
        # Validate the Switcher assigned to the box
        mtlSwitcherPrim = stage.GetPrimAtPath(materialSwitcherNamePath)
        self.assertTrue(Usd.Prim.IsValid(mtlSwitcherPrim))
        self.assertTrue(mtlSwitcherPrim.GetVariantSets().HasVariantSet("shadingVariant"))

        materialVariantSet = mtlSwitcherPrim.GetVariantSets().GetVariantSet("shadingVariant")
        materialVariantSetNames = materialVariantSet.GetVariantNames()
        self.assertEqual(2, len(materialVariantSetNames))
        self.assertEqual(physMatOne.name, materialVariantSetNames[0])
        self.assertEqual(multiMat.name, materialVariantSetNames[1])
        activeMaterial = test_box.material.GetActiveMaterial()
        self.assertEqual(activeMaterial.name, materialVariantSet.GetVariantSelection())
        
        # Make sure that all other physical materials were exported
        physMatTwoPrim = stage.GetPrimAtPath(materialName2Path)
        self.assertTrue(Usd.Prim.IsValid(physMatTwoPrim))
        physMatThreePrim = stage.GetPrimAtPath(materialName3Path)
        self.assertTrue(Usd.Prim.IsValid(physMatThreePrim))
        physMatFourPrim = stage.GetPrimAtPath(materialName4Path)
        self.assertTrue(Usd.Prim.IsValid(physMatFourPrim))
        
        # Now inspect the NestedSwitcher
        NestedSwitcherPrim = stage.GetPrimAtPath(nestedSwitcherMatPath)
        self.assertTrue(Usd.Prim.IsValid(NestedSwitcherPrim))
        self.assertTrue(NestedSwitcherPrim.GetVariantSets().HasVariantSet("shadingVariant"))

        materialVariantSet = NestedSwitcherPrim.GetVariantSets().GetVariantSet("shadingVariant")
        materialVariantSetNames = materialVariantSet.GetVariantNames()
        self.assertEqual(2, len(materialVariantSetNames))
        self.assertEqual(physMatThree.name, materialVariantSetNames[0])
        self.assertEqual(physMatFour.name, materialVariantSetNames[1])
        activeMaterial = nestedSwitcherMat.GetActiveMaterial()
        self.assertEqual(activeMaterial.name, materialVariantSet.GetVariantSelection())

    def test_variant_set_with_multisub_instanced_obj(self):
        """Test material switcher export as variant sets when a multi/sub-object material assigned to an instanced object"""
        test_usd_file_path = self.output_prefix + "variantSetWithMultisubInstanced.usda"
        
        mxs.delete(self.test_trans)
        
        # create mesh
        #box has 6 mat IDs [1-6]
        test_box = mxs.Box(name="originalBox")
        
        multiMat = mxs.Multimaterial()
        multiMat.name = "MultiMat"
        multiMat.numsubs = 2
        switcherMat = mxs.Material_Switcher()
        switcherMat.name = "Switcher"
        physMatOne = mxs.PhysicalMaterial()
        physMatOne.name = "Material1"
        physMatTwo = mxs.PhysicalMaterial()
        physMatTwo.name = "Material2"
        
        multiMat.materialList[0] = physMatOne
        multiMat.materialList[1] = physMatTwo
        switcherMat.materialList[0] = multiMat
        switcherMat.materialList[1] = physMatOne
        
        test_box.mat = physMatOne
        
        instanced_box = mxs.Instance(test_box)
        instanced_box.mat = switcherMat
        instanced_box.name = "instancedBox"
        
        export_options = mxs.USDExporter.CreateOptions()
        export_options.FileFormat = mxs.Name("ascii")
        export_options.MtlSwitcherExportStyle = mxs.Name("asVariantSets")
        export_options.RootPrimPath = "/"
        ret = mxs.USDExporter.ExportFile(
            test_usd_file_path,
            exportOptions=export_options
        )
        self.assertTrue(ret)
        
        stage = Usd.Stage.Open(test_usd_file_path)
        self.assertIsInstance(stage, Usd.Stage)
        
        materialRootPath = export_options.RootPrimPath + export_options.MaterialPrimPath
        materialSwitcherNamePath = materialRootPath + "/" + instanced_box.material.name
        materialName1Path = materialRootPath + "/" + instanced_box.material.materialList[0].materialList[0].name
        materialName2Path = materialRootPath + "/" + instanced_box.material.materialList[0].materialList[1].name
        
        instancedBoxPrim = stage.GetPrimAtPath("/instancedBox")
        self.assertTrue(Usd.Prim.IsValid(instancedBoxPrim))
        #Instancing should've been broken because geomSubsets are being "over"ed
        self.assertFalse(instancedBoxPrim.IsInstance())
        children = instancedBoxPrim.GetChildren()
        id = 1
        for child in children:
            if child.GetTypeName() == "GeomSubset" :
                bindingTargets = UsdShade.MaterialBindingAPI(child).GetDirectBindingRel().GetTargets()
                self.assertEqual(1, len(bindingTargets))
                refMat = stage.GetPrimAtPath(bindingTargets[0]).GetChildren()
                if (id % 2) == 1:
                    self.assertEqual("Material1", refMat[0].GetName())
                if (id % 2) == 0:
                    self.assertEqual("Material2", refMat[0].GetName())
                switcherPlaceHolderPrim = stage.GetPrimAtPath(materialSwitcherNamePath + "/Switcher_Set_1_MatID_" + id)
                self.assertTrue(Usd.Prim.IsValid(switcherPlaceHolderPrim))
                id = id + 1
        
        

def run_tests():
    return unittest.TextTestRunner(stream=sys.stdout, verbosity=2).run(unittest.TestLoader().loadTestsFromTestCase(TestMaterialSwitcher))

if __name__ == "__main__":
    from importlib import reload
    import usd_utils
    reload(usd_utils)
    reload(usd_material_writer)
    reload(usd_material_reader)
    usd_utils.get_config_data(update=True)
    mxs.clearListener()
    run_tests()
    