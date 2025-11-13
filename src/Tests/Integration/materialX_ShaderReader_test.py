#
# Copyright 2025 Autodesk
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
import os
import tempfile
import unittest

from pymxs import runtime as rt


class TestMaterialXShaderReader(unittest.TestCase):

    def setUp(self):
        rt.resetMaxFile(rt.Name("noprompt"))
        self.script_dir = os.path.dirname(os.path.abspath(__file__))
        self.output_prefix = os.path.join(tempfile.gettempdir(), "PYTHON_MTLX_SHADER_READER_TEST_")

    def tearDown(self):
        pass

    def validate_materialx_material(self, material_instance, expected_mtlx_path):
        """
        Validate that we have the correct MaterialX instance based on 3ds Max version
        and that the MaterialX file reference is correct.
        """
        # Check 3ds Max version to determine which MaterialX class to validate
        max_version = rt.maxVersion()
        is_2024_or_higher = max_version[0] >= 26000  # 3ds Max 2024 version number

        # Validate that we have the correct MaterialX instance based on version
        if is_2024_or_higher:
            self.assertTrue(rt.isKindOf(material_instance, rt.MaterialXMat),
                           "Expected MaterialXMat instance for 3ds Max 2024+")
        else:
            self.assertTrue(rt.isKindOf(material_instance, rt.MaterialXMaterial),
                           "Expected MaterialXMaterial instance for 3ds Max pre-2024")

        # Validate that the MaterialX file reference is correct
        actual_mtlx_path = material_instance.MaterialXFile
        self.assertIsNotNone(actual_mtlx_path, "Expected MaterialX file path to not be undefined")
        self.assertNotEqual("", actual_mtlx_path, "Expected MaterialX file path to not be empty")

        # Compare the file names (not full paths since they may be relative/absolute)
        expected_filename = os.path.basename(expected_mtlx_path)
        actual_filename = os.path.basename(actual_mtlx_path)
        self.assertEqual(expected_filename, actual_filename,
                        f"Expected MaterialX file reference to be '{expected_filename}' but got '{actual_filename}'")

    def test_import_iberian_blue_ceramic_material(self):
        #Test importing the iberianBlueCeramic material from the USD file
        import_path = os.path.join(self.script_dir, "data", "Iberian_Blue_Ceramic_Tiles_1k_8b", "iberianBlueCeramicTilesRef.usda")
        expected_mtlx_path = os.path.join(self.script_dir, "data", "Iberian_Blue_Ceramic_Tiles_1k_8b", "Iberian_Blue_Ceramic_Tiles.mtlx")

        # Verify the test files exist
        self.assertTrue(os.path.exists(import_path), f"USD test file not found: {import_path}")
        self.assertTrue(os.path.exists(expected_mtlx_path), f"MaterialX test file not found: {expected_mtlx_path}")

        # Create import options
        import_options = rt.USDImporter.CreateOptions()

        # Import the USD file
        result = rt.USDImporter.ImportFile(import_path, importOptions=import_options)
        self.assertTrue(result, "USD import should succeed")

        # Validate that we have the expected geometry
        sphere_obj = rt.getNodeByName("Sphere001")
        self.assertIsNotNone(sphere_obj, "Expected 'Sphere001' object to be imported")

        # Validate that the material was applied to the object
        self.assertIsNotNone(sphere_obj.material, "Expected 'Sphere001' to have a material assigned")

        # Validate the MaterialX material properties
        self.validate_materialx_material(sphere_obj.material, expected_mtlx_path)

        # Validate that the material name is correct
        self.assertEqual("Material__25", sphere_obj.material.name, "Expected material name to be 'Material__25'")

    def test_import_iberian_blue_ceramic_flat_material(self):
        #Test importing a MaterialX file that should fail
        import_path = os.path.join(self.script_dir, "data", "Iberian_Blue_Ceramic_Tiles_1k_8b", "iberianBlueCeramicTilesFlat.usda")
        import_options = rt.USDImporter.CreateOptions()

        # The USD import itself will succeed, but the MaterialX reader should fail internally
        # and not create a material
        result = rt.USDImporter.ImportFile(import_path, importOptions=import_options)
        self.assertTrue(result, "USD import should succeed even when MaterialX reader fails")

        # Validate that we have the expected geometry (this should still work)
        sphere_obj = rt.getNodeByName("Sphere001")
        self.assertIsNotNone(sphere_obj, "Expected 'Sphere001' object to be imported")

        # The material should be undefined because the MaterialX reader failed
        self.assertIsNone(sphere_obj.material, "Expected material to be undefined when MaterialX reader fails")


def run_tests():
    """Function called by MaxScript to run the tests"""
    loader = unittest.TestLoader()
    suite = loader.loadTestsFromTestCase(TestMaterialXShaderReader)
    runner = unittest.TextTestRunner(verbosity=2)
    result = runner.run(suite)
    return result
