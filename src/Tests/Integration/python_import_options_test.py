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
class TestMaxSceneBuilderOptions(unittest.TestCase):

    def setUp(self):
        rt.resetMaxFile(rt.Name("noprompt"))
        
    def tearDown(self):
        pass

    def test_options(self):

        options = maxUsd.MaxSceneBuilderOptions()

        # Test all getters and setters. 
        self.assertEqual(1, len(options.GetStageMaskPaths()))
        self.assertEqual('/', options.GetStageMaskPaths()[0])
        options.SetStageMaskPaths(['/root'])
        self.assertEqual("/root", options.GetStageMaskPaths()[0])
        
        self.assertEqual(0.0, options.GetStartTimeCode())
        self.assertEqual(0.0, options.GetEndTimeCode())
        options.SetStartTimeCode(5.0)
        self.assertEqual(5.0, options.GetStartTimeCode())

        options.SetEndTimeCode(7.0)
        self.assertEqual(7.0, options.GetEndTimeCode())
        
        self.assertEqual(Usd.Stage.LoadAll, options.GetStageInitialLoadSet())
        options.SetStageInitialLoadSet(Usd.Stage.LoadNone)
        self.assertEqual(Usd.Stage.LoadNone, options.GetStageInitialLoadSet())
        
        self.assertEqual(rt.GetDir(rt.Name("temp")) + "\\MaxUsdImport.log", options.GetLogPath())
        logPath = "C:\\foo\\bar.log"
        options.SetLogPath(logPath)
        self.assertEqual(logPath, options.GetLogPath())        
        
        self.assertEqual(maxUsd.LogLevel.Off, options.GetLogLevel())
        options.SetLogLevel(maxUsd.LogLevel.Error)
        self.assertEqual(maxUsd.LogLevel.Error, options.GetLogLevel())

        self.assertTrue(options.GetTranslateMaterials())
        self.assertEqual(1, len(options.GetShadingModes()))
        self.assertEqual('useRegistry', options.GetShadingModes()[0]['mode'])
        self.assertEqual('UsdPreviewSurface', options.GetShadingModes()[0]['materialConversion'])

        self.assertEqual('none', options.GetPreferredMaterial())
        options.SetPreferredMaterial('physicalMaterial')
        self.assertEqual('physicalMaterial', options.GetPreferredMaterial())
        
        self.assertEqual({0, 1, 2}, options.GetMetaData())
        options.SetMetaData({1,2,3})
        self.assertEqual({1, 2, 3}, options.GetMetaData())
        
        self.assertTrue(options.GetImportUnmappedPrimvars())
        options.SetImportUnmappedPrimvars(False)
        self.assertFalse(options.GetImportUnmappedPrimvars())
        
        self.assertTrue(options.IsMappedPrimvar('st1'))
        self.assertEqual(2, options.GetPrimvarChannel('st1'))
        options.SetPrimvarChannel('st1', 3)
        self.assertEqual(3, options.GetPrimvarChannel('st1'))

        self.assertEqual("set()", str(options.GetChaserNames()))
        chasers = {"chaser1", "chaser2"}
        options.SetChaserNames(chasers)
        self.assertEqual(chasers, options.GetChaserNames())

        self.assertTrue(options.GetUseProgressBar())
        options.SetUseProgressBar(False)
        self.assertFalse(options.GetUseProgressBar())

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

        self.assertEqual("set()", str(options.GetContextNames()))
        contexts = {"context1", "context2"}
        options.SetContextNames(contexts)
        self.assertEqual(contexts, options.GetContextNames())

        # Test setting defaults.
        options.SetDefaults()
        
        self.assertEqual(1, len(options.GetStageMaskPaths()))
        self.assertEqual('/', options.GetStageMaskPaths()[0])
        self.assertEqual(0.0, options.GetStartTimeCode())
        self.assertEqual(0.0, options.GetEndTimeCode())
        self.assertEqual(Usd.Stage.LoadAll, options.GetStageInitialLoadSet())
        self.assertEqual(rt.GetDir(rt.Name("temp")) + "\\MaxUsdImport.log", options.GetLogPath())
        self.assertEqual(maxUsd.LogLevel.Off, options.GetLogLevel())
        self.assertTrue(options.GetTranslateMaterials())
        self.assertEqual(1, len(options.GetShadingModes()))
        self.assertEqual('useRegistry', options.GetShadingModes()[0]['mode'])
        self.assertEqual('UsdPreviewSurface', options.GetShadingModes()[0]['materialConversion'])
        self.assertEqual('none', options.GetPreferredMaterial())
        self.assertEqual({0, 1, 2}, options.GetMetaData())
        self.assertTrue(options.IsMappedPrimvar('st1'))
        self.assertTrue(options.GetImportUnmappedPrimvars())
        self.assertEqual(2, options.GetPrimvarChannel('st1'))
        self.assertEqual("set()", str(options.GetChaserNames()))
        self.assertEqual("set()", str(options.GetContextNames()))        
        self.assertEqual("{}", str(options.GetAllChaserArgs()))
        self.assertTrue(options.GetUseProgressBar())
        
        # Test copy construction
        options.SetStageMaskPaths(["/foo"])
        optionsCopy = maxUsd.MaxSceneBuilderOptions(options)
        self.assertEqual("/foo", optionsCopy.GetStageMaskPaths()[0])
        optionsCopy.SetStageMaskPaths(["/bar", "/maid"])
        self.assertEqual("/foo", options.GetStageMaskPaths()[0])
        self.assertEqual("/bar", optionsCopy.GetStageMaskPaths()[0])
        self.assertEqual("/maid", optionsCopy.GetStageMaskPaths()[1])
        
        
rt.clearListener()
def run_tests():
    return unittest.TextTestRunner(stream=sys.stdout, verbosity=2).run(unittest.TestLoader().loadTestsFromTestCase(TestMaxSceneBuilderOptions))

if __name__ == '__main__':
    run_tests()