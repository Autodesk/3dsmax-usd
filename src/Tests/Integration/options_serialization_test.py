#
# Copyright 2024 Autodesk
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
import pymxs
import sys
import os
import glob
import json

import maxUsd
from pxr import Usd, Sdf

RT = pymxs.runtime

mxs = pymxs.runtime
NOPROMPT = mxs.Name("noprompt")
test_dir = os.path.dirname(__file__)


class TestOptionsSerialization(unittest.TestCase):
    def setUp(self):
        rt.resetMaxFile(rt.Name("noprompt"))
        
    #This test validates the serialization/deserialization of the export options
    def test_export_options(self):
        
        maxVer = rt.maxversion()
        
        #Test Python interface
        pyOpt = maxUsd.USDSceneBuilderOptions()
        
        pyOpt.SetTranslateMeshes(False)
        pyOpt.SetTranslateShapes(False)
        pyOpt.SetTranslateLights(False)
        pyOpt.SetTranslateCameras(False)
        pyOpt.SetTranslateMaterials(False)
        pyOpt.SetTranslateSkin(True)
        pyOpt.SetTranslateMorpher(True)
        pyOpt.SetUsdStagesAsReferences(False)
        pyOpt.SetTranslateHidden(False)
        pyOpt.SetUseUSDVisibility(True)
        pyOpt.SetAllowNestedGprims(True)
        pyOpt.SetUseProgressBar(False)
        pyOpt.SetUseLastResortUSDPreviewSurfaceWriter(False)
        if (maxVer[0] > 26000):
            pyOpt.SetMtlSwitcherExportStyle(maxUsd.MtlSwitcherExportStyle.ActiveMaterialOnly)
        
        rootPrim = "/foo/bar"
        pyOpt.SetRootPrimPath(rootPrim)
        pyOpt.SetOpenInUsdview(True)
        
        chasers = {"chaser1", "chaser2"}
        pyOpt.SetChaserNames(chasers)
        
        allChaserArgsDict = {"chaser1":{"param1":"a", "param2":"b"}}
        pyOpt.SetAllChaserArgs(allChaserArgsDict)
        
        pyOpt.SetChannelPrimvarType(1, maxUsd.PrimvarType.Color3fArray)
        pyOpt.SetChannelPrimvarName(1, "foo")
        pyOpt.SetChannelPrimvarAutoExpandType(1, True)
        pyOpt.SetBakeObjectOffsetTransform(False)
        pyOpt.SetPreserveEdgeOrientation(True)
        pyOpt.SetMeshFormat(maxUsd.MeshFormat.PolyMesh)
        pyOpt.SetNormalsMode(maxUsd.NormalsMode.AsAttribute)
        
        pyOpt.SetTimeMode(maxUsd.TimeMode.AnimationRange)
        pyOpt.SetStartFrame(10)
        pyOpt.SetEndFrame(10)
        pyOpt.SetSamplesPerFrame(5)
        
        allMaterialConversions = {'foo', 'bar'}
        pyOpt.SetAllMaterialConversions(allMaterialConversions)
        
        pyOpt.SetMaterialLayerPath("MyMaterials.usda")
        pyOpt.SetUseSeparateMaterialLayer(True)
        pyOpt.SetMaterialPrimPath("MyMaterials")

        pyOpt.SetBonesPrimName("MyBones")
        pyOpt.SetAnimationsPrimName("MyAnimations")
        
        allChaserArgsDict = {"chaser1":{"param1":"a", "param2":"b"}}
        pyOpt.SetAllChaserArgs(allChaserArgsDict)
        
        pyOpt.SetUpAxis(maxUsd.UpAxis.Y)
        pyOpt.SetFileFormat(maxUsd.FileFormat.ASCII)
        pyOpt.SetLogLevel(maxUsd.LogLevel.Error)
        logPath = "C:\\foo\\bar.log"
        pyOpt.SetLogPath(logPath)
        
        pyS = pyOpt.Serialize()
        pyOptLoaded = maxUsd.USDSceneBuilderOptions(pyS)        
        
        self.assertTrue(pyOptLoaded.GetTranslateMorpher())
        self.assertTrue(pyOptLoaded.GetTranslateSkin())
        self.assertFalse(pyOptLoaded.GetTranslateMaterials())
        self.assertFalse(pyOptLoaded.GetTranslateCameras())
        self.assertFalse(pyOptLoaded.GetTranslateLights())
        self.assertFalse(pyOptLoaded.GetTranslateShapes())
        self.assertFalse(pyOptLoaded.GetTranslateMeshes())
        self.assertFalse(pyOptLoaded.GetUsdStagesAsReferences())
        self.assertFalse(pyOptLoaded.GetTranslateHidden())
        self.assertTrue(pyOptLoaded.GetUseUSDVisibility())
        self.assertTrue(pyOptLoaded.GetAllowNestedGprims())        
        self.assertEqual(rootPrim, pyOptLoaded.GetRootPrimPath())
        self.assertTrue(pyOptLoaded.GetOpenInUsdview())
        self.assertFalse(pyOptLoaded.GetUseProgressBar())
        self.assertFalse(pyOptLoaded.GetUseLastResortUSDPreviewSurfaceWriter())
        
        self.assertEqual(chasers, pyOptLoaded.GetChaserNames())
        self.assertEqual(allChaserArgsDict, pyOptLoaded.GetAllChaserArgs())
        self.assertEqual(maxUsd.PrimvarType.Color3fArray, pyOptLoaded.GetChannelPrimvarType(1))
        self.assertEqual("foo", pyOptLoaded.GetChannelPrimvarName(1))
        self.assertTrue(pyOptLoaded.GetChannelPrimvarAutoExpandType(1))
        self.assertEqual(maxUsd.MeshFormat.PolyMesh, pyOptLoaded.GetMeshFormat())
        self.assertEqual(maxUsd.NormalsMode.AsAttribute, pyOptLoaded.GetNormalsMode())
        self.assertEqual(maxUsd.TimeMode.AnimationRange, pyOptLoaded.GetTimeMode())
        self.assertEqual(10, pyOptLoaded.GetStartFrame())
        self.assertEqual(10, pyOptLoaded.GetEndFrame())
        self.assertEqual(5, pyOptLoaded.GetSamplesPerFrame())
        self.assertEqual(allMaterialConversions, pyOptLoaded.GetAllMaterialConversions())
        self.assertEqual("MyMaterials.usda", pyOptLoaded.GetMaterialLayerPath())
        self.assertTrue(pyOptLoaded.GetUseSeparateMaterialLayer())
        self.assertEqual("MyMaterials", pyOptLoaded.GetMaterialPrimPath())
        self.assertEqual("MyBones", pyOptLoaded.GetBonesPrimName())
        self.assertEqual("MyAnimations", pyOptLoaded.GetAnimationsPrimName())
        
        self.assertEqual(maxUsd.UpAxis.Y, pyOptLoaded.GetUpAxis())
        self.assertEqual(maxUsd.FileFormat.ASCII, pyOptLoaded.GetFileFormat())
        self.assertEqual(maxUsd.LogLevel.Error, pyOptLoaded.GetLogLevel())
        self.assertEqual(logPath, pyOptLoaded.GetLogPath())
        
        if (maxVer[0] > 26000):
            self.assertEqual(maxUsd.MtlSwitcherExportStyle.ActiveMaterialOnly, pyOptLoaded.GetMtlSwitcherExportStyle())
        
    def test_import_options(self):
        
        pyOpt = maxUsd.MaxSceneBuilderOptions()
        pyOpt.SetStageMaskPaths(['/root'])
        pyOpt.SetStageInitialLoadSet(Usd.Stage.LoadNone)
        pyOpt.SetPreferredMaterial("fooMaterial")
        pyOpt.SetMetaData({1,2,3})
        
        pyOpt.SetImportUnmappedPrimvars(False)
        pyOpt.SetPrimvarChannel('st1', 3)
        pyOpt.SetUseProgressBar(False)
        
        chasers = {"chaser1", "chaser2"}
        pyOpt.SetChaserNames(chasers)
        
        allChaserArgsDict = {"chaser1":{"param1":"a", "param2":"b"}}
        pyOpt.SetAllChaserArgs(allChaserArgsDict)
        
        contexts = {"context1", "context2"}
        pyOpt.SetContextNames(contexts)
        
        shadingModes = [{"materialConversion": "UsdPreviewSurface","mode": "useRegistry"}, {"materialConversion": "test", "mode": "me"}]
        pyOpt.SetShadingModes(shadingModes)
        
        pyS = pyOpt.Serialize()
        pyOptLoaded = maxUsd.MaxSceneBuilderOptions(pyS)
        self.assertEqual([Sdf.Path('/root')], pyOptLoaded.GetStageMaskPaths())
        self.assertEqual(Usd.Stage.LoadNone, pyOptLoaded.GetStageInitialLoadSet())
        self.assertEqual("fooMaterial", pyOptLoaded.GetPreferredMaterial())
        self.assertEqual({1,2,3}, pyOptLoaded.GetMetaData())
        
        self.assertFalse(pyOptLoaded.GetImportUnmappedPrimvars())
        self.assertEqual(3, pyOptLoaded.GetPrimvarChannel('st1'))
        self.assertFalse(pyOptLoaded.GetUseProgressBar())
        
        self.assertEqual(chasers, pyOptLoaded.GetChaserNames())
        self.assertEqual(allChaserArgsDict, pyOptLoaded.GetAllChaserArgs())
        self.assertEqual(contexts, pyOptLoaded.GetContextNames())
        
        self.assertEqual(shadingModes, pyOptLoaded.GetShadingModes())

def run_tests():
    return unittest.TextTestRunner(stream=sys.stdout, verbosity=2).run(unittest.TestLoader().loadTestsFromTestCase(TestOptionsSerialization))

if __name__ == "__main__":
    from importlib import reload
    import usd_utils
    reload(usd_utils)
    usd_utils.get_config_data(update=True)
    mxs.clearListener()
    run_tests()
    