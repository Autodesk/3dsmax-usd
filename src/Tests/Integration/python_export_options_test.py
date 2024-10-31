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
import sys, os

import maxUsd
import pymxs
from pymxs import runtime as rt

from pxr import Usd, UsdGeom, Sdf, Plug

# Test class.
class TestUSDSceneBuilderOptions(unittest.TestCase):

    def setUp(self):
        rt.resetMaxFile(rt.Name("noprompt"))
        
    def tearDown(self):
        pass

    def test_options(self):
                
        options = maxUsd.USDSceneBuilderOptions()
        
        # Test all getters and setters.
        self.assertEqual(maxUsd.ContentSource.RootNode, options.GetContentSource())
        options.SetContentSource(maxUsd.ContentSource.Selection)
        self.assertEqual(maxUsd.ContentSource.Selection, options.GetContentSource())
        options.SetContentSource(maxUsd.ContentSource.NodeList)
        self.assertEqual(maxUsd.ContentSource.NodeList, options.GetContentSource())
        
        self.assertEqual("useRegistry", options.GetShadingMode())
        options.SetShadingMode("foo")
        self.assertEqual("foo", options.GetShadingMode())
        
        self.assertTrue(options.GetTranslateMeshes())
        options.SetTranslateMeshes(False)
        self.assertFalse(options.GetTranslateMeshes())
        
        self.assertTrue(options.GetTranslateShapes())
        options.SetTranslateShapes(False)
        self.assertFalse(options.GetTranslateShapes())
        
        self.assertTrue(options.GetTranslateLights())
        options.SetTranslateLights(False)
        self.assertFalse(options.GetTranslateLights())
        
        self.assertTrue(options.GetTranslateCameras())
        options.SetTranslateCameras(False)
        self.assertFalse(options.GetTranslateCameras())
        
        self.assertTrue(options.GetTranslateMaterials())
        options.SetTranslateMaterials(False)
        self.assertFalse(options.GetTranslateMaterials())

        self.assertFalse(options.GetTranslateSkin())
        options.SetTranslateSkin(True)
        self.assertTrue(options.GetTranslateSkin())

        self.assertFalse(options.GetTranslateMorpher())
        options.SetTranslateMorpher(True)
        self.assertTrue(options.GetTranslateMorpher())
        
        self.assertEqual("{'UsdPreviewSurface'}", str(options.GetAllMaterialConversions()))
        allMaterialConversions = {'foo', 'bar'}
        options.SetAllMaterialConversions(allMaterialConversions)
        self.assertEqual(allMaterialConversions, options.GetAllMaterialConversions())

        self.assertTrue(options.GetUsdStagesAsReferences())
        options.SetUsdStagesAsReferences(False)
        self.assertFalse(options.GetUsdStagesAsReferences())
        
        self.assertTrue(options.GetTranslateHidden())
        options.SetTranslateHidden(False)
        self.assertFalse(options.GetTranslateHidden())
        
        self.assertFalse(options.GetUseUSDVisibility())
        options.SetUseUSDVisibility(True)
        self.assertTrue(options.GetUseUSDVisibility())
        
        self.assertFalse(options.GetAllowNestedGprims())
        options.SetAllowNestedGprims(True)
        self.assertTrue(options.GetAllowNestedGprims())

        self.assertEqual(maxUsd.FileFormat.Binary, options.GetFileFormat())
        options.SetFileFormat(maxUsd.FileFormat.ASCII)
        self.assertEqual(maxUsd.FileFormat.ASCII, options.GetFileFormat())
        
        self.assertEqual(maxUsd.NormalsMode.AsPrimvar, options.GetNormalsMode())
        options.SetNormalsMode(maxUsd.NormalsMode.AsAttribute)
        self.assertEqual(maxUsd.NormalsMode.AsAttribute, options.GetNormalsMode())        
        
        self.assertEqual(maxUsd.MeshFormat.FromScene, options.GetMeshFormat())
        options.SetMeshFormat(maxUsd.MeshFormat.PolyMesh)
        self.assertEqual(maxUsd.MeshFormat.PolyMesh, options.GetMeshFormat())

        self.assertTrue(options.GetBakeObjectOffsetTransform())
        options.SetBakeObjectOffsetTransform(False)
        self.assertFalse(options.GetBakeObjectOffsetTransform())

        self.assertFalse(options.GetPreserveEdgeOrientation())
        options.SetPreserveEdgeOrientation(True)
        self.assertTrue(options.GetPreserveEdgeOrientation())
                        
        self.assertEqual(options.GetChannelPrimvarType(1), maxUsd.PrimvarType.TexCoord2fArray)
        options.SetChannelPrimvarType(1, maxUsd.PrimvarType.Color3fArray)
        self.assertEqual(options.GetChannelPrimvarType(1), maxUsd.PrimvarType.Color3fArray)

        self.assertEqual(options.GetChannelPrimvarName(1), "st")
        options.SetChannelPrimvarName(1, "foo")
        self.assertEqual(options.GetChannelPrimvarName(1), "foo")

        self.assertEqual(options.GetChannelPrimvarAutoExpandType(1), False)
        options.SetChannelPrimvarAutoExpandType(1, True)
        self.assertEqual(options.GetChannelPrimvarAutoExpandType(1), True)

        self.assertEqual(maxUsd.TimeMode.CurrentFrame, options.GetTimeMode())
        options.SetTimeMode(maxUsd.TimeMode.AnimationRange)
        self.assertEqual(maxUsd.TimeMode.AnimationRange, options.GetTimeMode())

        self.assertEqual(0, options.GetStartFrame())
        options.SetStartFrame(10)
        self.assertEqual(10, options.GetStartFrame())

        self.assertEqual(0, options.GetEndFrame())
        options.SetEndFrame(10)
        self.assertEqual(10, options.GetEndFrame())
        
        self.assertEqual(1, options.GetSamplesPerFrame())
        options.SetSamplesPerFrame(5)
        self.assertEqual(5, options.GetSamplesPerFrame())

        self.assertEqual(maxUsd.UpAxis.Z, options.GetUpAxis())
        options.SetUpAxis(maxUsd.UpAxis.Y)
        self.assertEqual(maxUsd.UpAxis.Y, options.GetUpAxis())

        self.assertEqual("/root", str(options.GetRootPrimPath()))
        rootPrim = "/foo/bar"
        options.SetRootPrimPath(rootPrim)
        self.assertEqual(rootPrim, str(options.GetRootPrimPath()))
        
        self.assertTrue("MaxUsdExport.log" in options.GetLogPath())
        logPath = "C:\\foo\\bar.log"
        options.SetLogPath(logPath)
        self.assertEqual(logPath, options.GetLogPath())        
        
        self.assertEqual(maxUsd.LogLevel.Off, options.GetLogLevel())
        options.SetLogLevel(maxUsd.LogLevel.Error)
        self.assertEqual(maxUsd.LogLevel.Error, options.GetLogLevel())
        
        self.assertFalse(options.GetOpenInUsdview())        
        options.SetOpenInUsdview(True)
        self.assertTrue(options.GetOpenInUsdview())
        
        self.assertEqual("set()", str(options.GetChaserNames()))
        chasers = {"chaser1", "chaser2"}
        options.SetChaserNames(chasers)
        self.assertEqual(chasers, options.GetChaserNames())
        
        self.assertEqual("set()", str(options.GetContextNames()))
        contexts = {"context1", "context2"}
        options.SetContextNames(contexts)
        self.assertEqual(contexts, options.GetContextNames())
        
        # We have 2 overloads for SetAllChaserArgs, one taking in a dict, the other a list.
        self.assertEqual("{}", str(options.GetAllChaserArgs()))
        # Using dictionnary: 
        allChaserArgsDict = {"chaser1":{"param1":"a", "param2":"b"}}
        options.SetAllChaserArgs(allChaserArgsDict)
        self.assertEqual(allChaserArgsDict, options.GetAllChaserArgs())
        # Using list:
        allChaserArgsList = ["chaser1", "param1", "1", "chaser1", "param2", "1"]
        options.SetAllChaserArgs(allChaserArgsList)
        self.assertEqual({'chaser1': {'param1': '1', 'param2': '1'}}, options.GetAllChaserArgs())
        
        self.assertTrue(options.GetUseProgressBar())
        options.SetUseProgressBar(False)
        self.assertFalse(options.GetUseProgressBar())

        self.assertEqual("<filename>_mtl.usda", options.GetMaterialLayerPath())
        options.SetMaterialLayerPath("MyMaterials.usda")
        self.assertEqual("MyMaterials.usda", options.GetMaterialLayerPath())

        self.assertFalse(options.GetUseSeparateMaterialLayer())
        options.SetUseSeparateMaterialLayer(True)
        self.assertTrue(options.GetUseSeparateMaterialLayer())

        self.assertEqual("mtl", options.GetMaterialPrimPath())
        options.SetMaterialPrimPath("MyMaterials")
        self.assertEqual("MyMaterials", options.GetMaterialPrimPath())
        
        self.assertTrue(options.GetUseLastResortUSDPreviewSurfaceWriter())
        options.SetUseLastResortUSDPreviewSurfaceWriter(False)
        self.assertFalse(options.GetUseLastResortUSDPreviewSurfaceWriter())
        
        # Test setting defaults.
        options.SetDefaults()
        
        self.assertEqual(maxUsd.ContentSource.RootNode, options.GetContentSource())
        self.assertEqual("useRegistry", options.GetShadingMode())
        self.assertTrue(options.GetTranslateMeshes())
        self.assertTrue(options.GetTranslateShapes())
        self.assertTrue(options.GetTranslateLights())
        self.assertTrue(options.GetTranslateCameras())
        self.assertTrue(options.GetTranslateMaterials())
        self.assertFalse(options.GetTranslateSkin())
        self.assertFalse(options.GetTranslateMorpher())
        self.assertEqual("{'UsdPreviewSurface'}", str(options.GetAllMaterialConversions()))
        self.assertTrue(options.GetUsdStagesAsReferences())
        self.assertTrue(options.GetTranslateHidden())
        self.assertFalse(options.GetUseUSDVisibility())
        self.assertFalse(options.GetAllowNestedGprims())
        self.assertEqual(maxUsd.FileFormat.Binary, options.GetFileFormat())
        self.assertEqual(maxUsd.NormalsMode.AsPrimvar, options.GetNormalsMode())
        self.assertEqual(maxUsd.MeshFormat.FromScene, options.GetMeshFormat())
        self.assertTrue(options.GetBakeObjectOffsetTransform())
        self.assertFalse(options.GetPreserveEdgeOrientation())
        self.assertEqual(options.GetChannelPrimvarType(1), maxUsd.PrimvarType.TexCoord2fArray)
        self.assertEqual(options.GetChannelPrimvarName(1), "st")
        self.assertEqual(options.GetChannelPrimvarAutoExpandType(1), False)
        self.assertEqual(maxUsd.TimeMode.CurrentFrame, options.GetTimeMode())
        self.assertEqual(0, options.GetStartFrame())
        self.assertEqual(0, options.GetEndFrame())
        self.assertEqual(1, options.GetSamplesPerFrame())
        self.assertEqual(maxUsd.UpAxis.Z, options.GetUpAxis())
        self.assertEqual("/root", str(options.GetRootPrimPath()))
        self.assertTrue("MaxUsdExport.log" in options.GetLogPath())
        self.assertEqual(maxUsd.LogLevel.Off, options.GetLogLevel())
        self.assertFalse(options.GetOpenInUsdview())        
        self.assertEqual("set()", str(options.GetChaserNames()))
        self.assertEqual("set()", str(options.GetContextNames()))        
        self.assertEqual("{}", str(options.GetAllChaserArgs()))
        self.assertTrue(options.GetUseProgressBar())
        self.assertEqual("<filename>_mtl.usda", options.GetMaterialLayerPath())
        self.assertFalse(options.GetUseSeparateMaterialLayer())
        self.assertEqual("mtl", options.GetMaterialPrimPath())
        self.assertTrue(options.GetUseLastResortUSDPreviewSurfaceWriter())
        
        # Test copy construction
        options.SetRootPrimPath("/foo")
        optionsCopy = maxUsd.USDSceneBuilderOptions(options)
        self.assertEqual("/foo", str(optionsCopy.GetRootPrimPath()))
        optionsCopy.SetRootPrimPath("/bar")
        self.assertEqual("/foo", str(options.GetRootPrimPath()))
        self.assertEqual("/bar", str(optionsCopy.GetRootPrimPath()))
        
        
rt.clearListener()
def run_tests():
    return unittest.TextTestRunner(stream=sys.stdout, verbosity=2).run(unittest.TestLoader().loadTestsFromTestCase(TestUSDSceneBuilderOptions))

if __name__ == '__main__':
    run_tests()