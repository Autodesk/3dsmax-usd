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

from pxr import Usd, UsdShade, Sdf
import usd_test_helpers

mxs = pymxs.runtime
NOPROMPT = mxs.Name("noprompt")
test_dir = os.path.dirname(__file__)


class TestMaterialExportCases(unittest.TestCase):
    def setUp(self):
        self.output_prefix = usd_test_helpers.standard_output_prefix("EXPORT_MATERIAL_CASES_")
        usd_test_helpers.load_usd_plugins()
        mxs.resetMaxFile(NOPROMPT)

    def test_roughness_cases(self):
        """Test export of special cases for roughness"""
        # create meshes
        rough_inv_box = mxs.Box(name="box_roughness_inv")
        pbrmetal_box = mxs.Box(name="box_pbrmetal")
        pbrspec_box = mxs.Box(name="box_pbrspec")

        # create material for mesh
        rough_inv_box.material = mxs.PhysicalMaterial(name="roughness_inv_mat")
        pbrmetal_box.material = mxs.PBRMetalRough(name="pbrmetal_gloss_mat")
        pbrspec_box.material = mxs.PBRSpecGloss(name="pbrspec_rough_mat")

        # set material settings
        USE_ROUGHNESS = 2
        USE_GLOSINESS = 1
        roughness_val = 0.333
        rough_inv_box.material.roughness_inv = True
        rough_inv_box.material.roughness = roughness_val
        pbrmetal_box.material.useGlossiness = USE_GLOSINESS
        pbrmetal_box.material.roughness = roughness_val
        pbrspec_box.material.useGlossiness = USE_ROUGHNESS # default is glosiness
        pbrspec_box.material.glossiness = roughness_val

        # export
        test_usd_file_path = self.output_prefix + "test_roughness_inv.usda"
        export_options = mxs.USDExporter.CreateOptions()
        export_options.FileFormat = mxs.Name("ascii")
        export_options.RootPrimPath = "/"
        ret = mxs.USDExporter.ExportFile(
            test_usd_file_path,
            exportOptions=export_options,
            contentSource=mxs.Name("nodeList"),
            nodeList=[rough_inv_box,pbrmetal_box,pbrspec_box],
        )
        msg = test_usd_file_path
        self.assertTrue(ret)
        # test
        stage = Usd.Stage.Open(test_usd_file_path)
        self.assertIsInstance(stage, Usd.Stage)

        usd_mat_path = "/mtl/roughness_inv_mat/roughness_inv_mat"
        usd_mat_shader = UsdShade.Shader.Get(stage, usd_mat_path)
        self.assertIsInstance(usd_mat_shader, UsdShade.Shader)
        roughness_input = usd_mat_shader.GetInput("roughness")
        self.assertTrue(roughness_input)
        self.assertAlmostEqual(roughness_input.Get(), 1 - roughness_val, msg=msg)

        usd_test_helpers.check_expected_usd_mat_value(
            self,
            stage,
            "/mtl/pbrmetal_gloss_mat/pbrmetal_gloss_mat",
            "roughness",
            1 - roughness_val,
        )
        usd_test_helpers.check_expected_usd_mat_value(
            self,
            stage,
            "/mtl/pbrspec_rough_mat/pbrspec_rough_mat",
            "roughness",
            roughness_val,
        )


def run_tests():
    return unittest.TextTestRunner(stream=sys.stdout, verbosity=2).run(
        unittest.TestLoader().loadTestsFromTestCase(TestMaterialExportCases)
    )


if __name__ == "__main__":
    from importlib import reload
    import usd_utils
    reload(usd_utils)
    reload(usd_material_writer)
    reload(usd_material_reader)
    usd_utils.get_config_data(update=True)
    mxs.clearListener()
    run_tests()