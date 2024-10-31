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

import usd_test_helpers
import maxUsd
import pymxs
from pymxs import runtime as rt

from pxr import Usd, UsdGeom

class TranslationUtilsDummyWriter(maxUsd.PrimWriter):
    def __init__(self, PrimWriter, *args, **kwargs):
        super(TranslationUtilsDummyWriter, self).__init__(PrimWriter, *args, **kwargs)
        self._objectFames = []
        self._objectFrameIdx = 0
        
    def Write(self, prim, applyOffset, time):
        try: 
            opts = self.GetExportArgs()
            
            if (time.IsFirstFrame()):
                nodeHandle = self.GetNodeHandle()
                self._objectFrames = maxUsd.TranslationUtils.GetKeyFramesFromValidityInterval(nodeHandle, opts)
                # See comments in TestPythonTranslationUtils.test_get_keyframes_from_val_interval()
                rt.assert_equal(str([(0.0,0.0), (1.0,1.0), (2.0,2.0), (3.0,3.0), (4.0,4.0), (5.0,5.0), (6.0,6.0), (7.0,7.0), (8.0,8.0), (9.0,9.0), (10.0,10.0), (20.0,20.0), (21.0,21.0), (22.0,22.0), (23.0,23.0), (24.0,24.0), (25.0,25.0), (26.0,26.0), (27.0,27.0), (28.0,28.0), (29.0,29.0), (30.0,30.0)]), str(self._objectFrames))
            
            self._objectFrames[self._objectFrameIdx]
            
            rt.assert_equal(str(self._objectFrames[self._objectFrameIdx]), str((time.GetMaxTime(), time.GetUsdTime().GetValue())))
            self._objectFrameIdx = self._objectFrameIdx + 1
            return True
        except Exception as e:
            rt.assert_true(False, message="\nFailed to test GetKeyFramesFromValidityInterval()!")
            print('\nWrite() - Error: %s' % str(e))

    @classmethod
    def CanExport(cls, nodeHandle, exportArgs):
        node = rt.maxOps.getNodeByHandle(nodeHandle)
        if rt.classOf(node) == rt.Sphere:
            return maxUsd.PrimWriter.ContextSupport.Supported
        return maxUsd.PrimWriter.ContextSupport.Unsupported

class TestPythonTranslationUtils(unittest.TestCase):

    def setUp(self):
        rt.resetMaxFile(rt.Name("noprompt"))
        self.output_prefix = usd_test_helpers.standard_output_prefix("PYHTON_TRANSLATION_UTILS_TEST_")

    def tearDown(self):
        pass

    def test_get_keyframes_from_val_interval(self):
        maxUsd.PrimWriter.Unregister(TranslationUtilsDummyWriter, "TranslationUtilsDummyWriter")
        maxUsd.PrimWriter.Register(TranslationUtilsDummyWriter, "TranslationUtilsDummyWriter")
        # Create an animated sphere

        sphereNode = rt.Sphere()
        with pymxs.animate(True):
            with pymxs.attime(10):
                sphereNode.radius = 50
            with pymxs.attime(20):
                sphereNode.radius = 50
            with pymxs.attime(30):
                sphereNode.radius = 100    
                
        # We've got 5 meaningful intervals : 
        # Object doesn't change in [-infinity, 0]
        # Object changes every frame in [0,10]
        # Object doesn't change in [10, 20]
        # Object changes every frame in [20, 30]
        # Object doesn't change in [30, infinity]
        
        # We therefor expect these frames to be exported : 
        # [0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30]
        test_usd_file_path = self.output_prefix + "test_get_keyframes_from_val_interval.usda"

        export_options = rt.UsdExporter.CreateOptions()
        export_options.FileFormat = rt.Name("ascii")
        export_options.TimeMode = rt.Name("AnimationRange")
        export_options.StartFrame = -5
        export_options.EndFrame = 35
        ret = rt.USDExporter.ExportFile(test_usd_file_path, exportOptions=export_options)
        
        self.assertTrue(ret)
        
        maxUsd.PrimWriter.Unregister(TranslationUtilsDummyWriter, "TranslationUtilsDummyWriter")

rt.clearListener()
def run_tests():
    return unittest.TextTestRunner(stream=sys.stdout, verbosity=2).run(unittest.TestLoader().loadTestsFromTestCase(TestPythonTranslationUtils))

if __name__ == '__main__':
    run_tests()
