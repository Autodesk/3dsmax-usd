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

from pxr import Usd
import usd_test_helpers

mxs = pymxs.runtime
NOPROMPT = mxs.Name("noprompt")
test_dir = os.path.dirname(__file__)


class TestMaterialOpacity(unittest.TestCase):
    def setUp(self):
        self.output_prefix = usd_test_helpers.standard_output_prefix("TestMaterialOpacity_")
        usd_test_helpers.load_usd_plugins()
        mxs.resetMaxFile(NOPROMPT)


    def test_opacity_physical_mat(self):
        """Test opacity for physcial mat works as expected."""
        test_usd_file_path = self.output_prefix + "test_opacity_physical_mat.usda"
        mxs.loadMaxFile(os.path.join(test_dir, "data", "phy-mat-leaf-cutout.max"), quiet=True)
        
        # create meshes
        test_node = mxs.getNodeByName("phy-mat-leaf001")
        test_node_trans = mxs.getNodeByName("phy-mat-trans")
        # create material for mesh        
        # set material settings
        # export
        export_options = mxs.USDExporter.CreateOptions()
        export_options.FileFormat = mxs.Name("ascii")
        export_options.RootPrimPath = "/"
        ret = mxs.USDExporter.ExportFile(
            test_usd_file_path,
            exportOptions=export_options,
            contentSource=mxs.Name("nodeList"),
            nodeList=[test_node, test_node_trans],
        )
        self.assertTrue(ret)

        # test
        stage = Usd.Stage.Open(test_usd_file_path)
        self.assertIsInstance(stage, Usd.Stage)

        usd_mat_path = "/mtl/phys_cutout_mat/phys_cutout_mat"
        usd_test_helpers.check_expected_usd_mat_value(self, stage, usd_mat_path, "opacityThreshold", 0.5)

        trans_mat_path = "/mtl/phys_transparent_mat/phys_transparent_mat"
        usd_test_helpers.check_expected_usd_mat_value(self, stage, trans_mat_path, "opacityThreshold", None)


class TestMaterialTransparency(unittest.TestCase):
    def setUp(self):
        self.output_prefix = usd_test_helpers.standard_output_prefix("TestMaterialTransparency_")

        mxs.resetMaxFile(NOPROMPT)
        usd_test_helpers.load_usd_plugins()

    def test_opacity_physical_mat(self):
        """Test opacity for physcial mat works as expected."""
        test_usd_file_path = self.output_prefix + "phys_transparency_mat.usda"
        
        # create meshes
        test_trans = mxs.Box(name="phys_trans_cube")
        # create material for mesh
        mat_name = "physical_trans_mat"
        test_trans.material = mxs.PhysicalMaterial(name=mat_name)
        # set material settings
        transparency_value = 0.9
        test_trans.material.transparency = transparency_value
        
        # export
        export_options = mxs.USDExporter.CreateOptions()
        export_options.FileFormat = mxs.Name("ascii")
        export_options.RootPrimPath = "/"
        ret = mxs.USDExporter.ExportFile(
            test_usd_file_path,
            exportOptions=export_options,
            contentSource=mxs.Name("nodeList"),
            nodeList=[test_trans],
        )
        self.assertTrue(ret)

        # test
        stage = Usd.Stage.Open(test_usd_file_path)
        self.assertIsInstance(stage, Usd.Stage)

        usd_mat_path = f"/mtl/{mat_name}/{mat_name}"
        usd_test_helpers.check_expected_usd_mat_value(self, stage, usd_mat_path, "opacity", 1 - transparency_value)



def run_tests():
    test_cases = (TestMaterialOpacity, TestMaterialTransparency)
    test_runner = unittest.TextTestRunner(stream=sys.stdout, verbosity=2)
    suite = unittest.TestSuite()
    for test_class in test_cases:
        tests = unittest.defaultTestLoader.loadTestsFromTestCase(test_class)
        suite.addTests(tests)
    
    return test_runner.run(suite)


if __name__ == "__main__":
    from importlib import reload
    import usd_utils
    reload(usd_utils)
    reload(usd_material_writer)
    reload(usd_material_reader)
    usd_utils.get_config_data(update=True)
    mxs.clearListener()
    run_tests()
    