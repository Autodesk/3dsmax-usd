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

import maxUsd
import pymxs
import os
from pymxs import runtime as rt

from pxr import Usd, UsdGeom, UsdShade, Sdf

TEST_DIR = os.path.dirname(__file__)
TEST_DATA_DIR = os.path.join(TEST_DIR, "data")
UV_CHECKER_NAME = "uv_checker.png"
UV_CHECKER_PATH = os.path.join(TEST_DATA_DIR, UV_CHECKER_NAME)

class TestPythonMaterialConverter(unittest.TestCase):

    def setUp(self):
        rt.resetMaxFile(rt.Name("noprompt"))

    def tearDown(self):
        pass

    def test_mtl_to_usd(self):
        
        testMaterial = rt.MaxUsdPreviewSurface()
        testMaterial.Name = "usdps"
        checker = rt.BitmapTexture()
        checker.filename = UV_CHECKER_PATH
        
        testMaterial.diffuseColor_map = checker
                
        stage = Usd.Stage.CreateInMemory()
        
        # We wont actually write the stage do disk, but give a fictitious file path for the stage's root layer.
        # We need to know the path of the layer when translating materials, so that we can compute any required
        # relative paths for things like textures and any other referenced asset.
        
        layerFilePath = os.path.join(TEST_DATA_DIR, "dummy.usda")
        options = maxUsd.USDSceneBuilderOptions()
        testMaterialHandle = rt.getHandleByAnim(testMaterial)
        
        usdMaterial = maxUsd.MaterialConverter.ConvertToUSDMaterial(testMaterialHandle, stage, layerFilePath, False, "/mtl/test_mtl_no_binding", options)
        self.assertTrue(usdMaterial.GetPrim().IsDefined())
        
        texturePrim = stage.GetPrimAtPath("/mtl/test_mtl_no_binding/Bitmaptexture/Bitmaptexture")
        self.assertTrue(texturePrim.IsValid())
        inputFile = str(UsdShade.Shader(texturePrim).GetInput("file").Get())
        self.assertEqual("@./" + UV_CHECKER_NAME + "@", inputFile)
        
        spherePath = "/sphere"
        cubePath = "/cube"
        
        spherePrim = UsdGeom.Sphere.Define(stage, spherePath)
        cubePrim = UsdGeom.Cube.Define(stage, cubePath)

        bindings = [spherePath, cubePath]
        boundMtlPath = "/mtl/test_mtl_with_binding"
        usdBoundMaterial = maxUsd.MaterialConverter.ConvertToUSDMaterial(testMaterialHandle, stage, layerFilePath, False, boundMtlPath, options, bindings)
        self.assertTrue(usdBoundMaterial.GetPrim().IsDefined())

        # Validate the bindings we requested.
        for path in bindings:
            prim = stage.GetPrimAtPath(path)
            api = UsdShade.MaterialBindingAPI(prim)
            mtl = api.GetDirectBindingRel().GetTargets()[0]
            self.assertEqual(str(mtl), boundMtlPath)

        # Left around, for debugging.
        # stage.GetRootLayer().Export(layerFilePath)
        # maxUsd.OpenInUsdView(layerFilePath)
        
rt.clearListener()
def run_tests():
    return unittest.TextTestRunner(stream=sys.stdout, verbosity=2).run(unittest.TestLoader().loadTestsFromTestCase(TestPythonMaterialConverter))

if __name__ == '__main__':
    run_tests()
