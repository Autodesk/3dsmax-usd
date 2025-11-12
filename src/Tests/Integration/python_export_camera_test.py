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
import unittest
import sys
import os

from pxr import Usd, UsdGeom, Gf

import usd_test_helpers
import pymxs

mxs = pymxs.runtime

class PythonExportCameraTest(unittest.TestCase):
    def setUp(self):
        """Set up test environment."""
        mxs.resetMaxFile(mxs.Name("noprompt"))
        usd_test_helpers.load_usd_plugins()
        # Set up output directory
        self.output_prefix = usd_test_helpers.standard_output_prefix("EXPORT_CAMERA_CURVES_TEST_")

    def _CreateCameraScene(self):
    # Create a basic scene with a target box for the camera
        target_box = mxs.box(height=20, width=20, length=20, pos=mxs.Point3(25, 25, 0))

        # Create a Physical camera (recommended for USD export)
        phys_camera = mxs.Physical(target=target_box, pos=mxs.Point3(0, -45, 25))

        # Enable necessary camera properties for testing
        phys_camera.specify_focus = 1  # Enable custom focus distance
        phys_camera.clip_on = True     # Enable clipping planes
        phys_camera.shutter_offset_enabled = True  # Enable shutter offset
        phys_camera.lens_breathing_amount = 0      # Remove lens breathing for cleaner test
        
        # Create curves-based animation for various physical camera properties
        # This tests the new curves animation feature
        with pymxs.animate(True):
            with pymxs.attime(0):
                phys_camera.focal_length_mm = 35
                phys_camera.fov = 30
                phys_camera.focus_distance = 100
                phys_camera.f_number = 2.8
                phys_camera.exposure_value = 6.0
                phys_camera.exposure_value = 9.0
                phys_camera.horizontal_shift = -10.0
                phys_camera.vertical_shift = -5.0
                phys_camera.zoom_factor = 1.0
                phys_camera.shutter_length_frames = 0.25
                phys_camera.shutter_offset_frames = 0.1
                phys_camera.clip_near = 1.0
                phys_camera.clip_far = 1000.0
            with pymxs.attime(5):
                phys_camera.focal_length_mm = 50
                phys_camera.fov = 45
                phys_camera.focus_distance = 200
                phys_camera.f_number = 5.6
                phys_camera.exposure_value = 12.0
                phys_camera.horizontal_shift = 0.0
                phys_camera.vertical_shift = 10.0
                phys_camera.zoom_factor = 1.5
                phys_camera.shutter_length_frames = 0.5
                phys_camera.shutter_offset_frames = 0.2
                phys_camera.clip_near = 5.0
                phys_camera.clip_far = 1500.0
            with pymxs.attime(10):
                phys_camera.focal_length_mm = 85
                phys_camera.fov = 60
                phys_camera.focus_distance = 300
                phys_camera.f_number = 8.0
                phys_camera.horizontal_shift = 15.0
                phys_camera.vertical_shift = 20.0
                phys_camera.zoom_factor = 2.0
                phys_camera.shutter_length_frames = 0.75
                phys_camera.shutter_offset_frames = 0.3
                phys_camera.clip_near = 10.0
                phys_camera.clip_far = 2000.0

    if Usd.GetVersion() >= (0,24,11):
        def _ValidateUsdCamera(self, usdCamera, expectedPropertiesTuples):
            schemaAttrNames = usdCamera.GetSchemaAttributeNames()
            
            for expectedPropertyTuple in expectedPropertiesTuples:
                (propertyName, expectedValueTuples) = expectedPropertyTuple

                self.assertTrue(propertyName in schemaAttrNames, f"Expected property '{propertyName}' not found in USD camera schema.")

                usdAttr = usdCamera.GetPrim().GetAttribute(propertyName)
                self.assertTrue(usdAttr.IsDefined(), f"USD attribute '{propertyName}' is not defined.")

                attrSpline = usdAttr.GetSpline()
                self.assertIsNotNone(attrSpline, f"USD attribute '{propertyName}' does not have a spline.")

                for expectedValue in expectedValueTuples:
                    knots = attrSpline.GetKnots()
                    self.assertTrue(len(expectedValueTuples) == len(knots), f"Expected value tuple for property '{propertyName}' does not have exactly 2 elements.")

                    (time, expectedValue) = expectedValue
                    self.assertAlmostEqual(knots[time].GetValue(), expectedValue, places=5, msg=f"USD attribute '{propertyName}' spline value at time {time} is not {expectedValue}.")

        def _test_export_camera_properties(self):
            """Test exporting animated camera properties using USD curves."""
            self._CreateCameraScene()

            # Set up export options for curves animation
            export_options = mxs.USDExporter.CreateOptions()
            export_options.FileFormat = mxs.Name("ascii")
            export_options.RootPrimPath = "/"
            export_options.TimeMode = mxs.Name("frameRange")
            export_options.StartFrame = 0
            export_options.EndFrame = 10
            # Use spline animation instead of time samples
            export_options.AnimationType = mxs.Name("Curves")

            # Export the animated camera with curves
            export_path = self.output_prefix + "export_physical_camera_curves.usda"
            result = mxs.USDExporter.ExportFile(export_path, exportOptions=export_options)

            # Verify export was successful
            self.assertTrue(result, "USD export failed")
            self.assertTrue(os.path.exists(export_path), f"Export file {export_path} was not created")

            # Load the USD stage and verify curves data
            stage = Usd.Stage.Open(export_path)
            self.assertIsNotNone(stage, "Failed to open exported USD stage")
            
            # Find the camera prim
            camera_prim_path = "/PhysCamera001"
            camera_prim = stage.GetPrimAtPath(camera_prim_path)
            self.assertTrue(camera_prim.IsValid(), f"Camera prim at {camera_prim_path} is not valid")

            expectedPropertyTuples = [
                ('focalLength', [(0, 13.779528), (5, 19.68504), (10, 33.46457)]),
                ('exposure', [(0, 9), (5, 12)]),
                ('focusDistance', [(0, 100), (5, 200), (10, 300)]),
                ('fStop', [(0, 2.8), (5, 5.6), (10, 8)]),
                ('horizontalAperture', [(0, 35.999996), (5, 18.0), (10, 18.0)]),
                ('horizontalApertureOffset', [(0, 3.5999997), (5, -0), (10, -2.7)]),
                ('shutter:open', [(0, 89.9999966472389), (5, 179.9999932944778), (10, 269.9999966472389)]),
                ('shutter:close', [(0, 314.9999849125751), (5, 629.9999698251502), (10, 944.9999614432475)]),
                ('verticalAperture', [(0, 20.249998), (5, 10.125), (10, 10.125)]),
                ('verticalApertureOffset', [(0, 1.7999998), (5, -1.8000001), (10, -3.6000001)]),
            ]

            # Verify it's a USD Camera
            self.assertTrue(camera_prim.IsA(UsdGeom.Camera), "Exported prim is not a USD Camera")
            usd_camera = UsdGeom.Camera(camera_prim)

            self._ValidateUsdCamera(usd_camera, expectedPropertyTuples)

        def test_export_camera_properties_sample_per_frame(self):
            """Test exporting animated camera properties using USD curves with different frame rate."""

            self._CreateCameraScene()

            mxs.frameRate = 24

            # Set up export options for curves animation
            export_options = mxs.USDExporter.CreateOptions()
            export_options.FileFormat = mxs.Name("ascii")
            export_options.RootPrimPath = "/"
            export_options.TimeMode = mxs.Name("frameRange")
            export_options.StartFrame = 0
            export_options.EndFrame = 10
            # Use spline animation instead of time samples
            export_options.AnimationType = mxs.Name("Curves")

            # Export the animated camera with curves
            export_path = self.output_prefix + "test_export_camera_properties_sample_per_frame.usda"
            result = mxs.USDExporter.ExportFile(export_path, exportOptions=export_options)

            # Verify export was successful
            self.assertTrue(result, "USD export failed")
            self.assertTrue(os.path.exists(export_path), f"Export file {export_path} was not created")

            # Load the USD stage and verify curves data
            stage = Usd.Stage.Open(export_path)
            self.assertIsNotNone(stage, "Failed to open exported USD stage")
            
            # Find the camera prim
            camera_prim_path = "/PhysCamera001"
            camera_prim = stage.GetPrimAtPath(camera_prim_path)
            self.assertTrue(camera_prim.IsValid(), f"Camera prim at {camera_prim_path} is not valid")

            # Verify it's a USD Camera
            self.assertTrue(camera_prim.IsA(UsdGeom.Camera), "Exported prim is not a USD Camera")
            usd_camera = UsdGeom.Camera(camera_prim)

            expectedPropertyTuples = [
                ('focalLength', [(0, 13.779528), (4, 19.68504), (8, 33.46457)]),
                ('exposure', [(0, 9), (4, 12)]),
                ('focusDistance', [(0, 100), (4, 200), (8, 300)]),
                ('fStop', [(0, 2.8), (4, 5.6), (8, 8)]),
                ('horizontalAperture', [(0, 35.999996), (4, 18.0), (8, 18.0)]),
                ('horizontalApertureOffset', [(0, 3.5999997), (4, -0), (8, -2.7)]),
                ('shutter:open', [(0, 57.59999914169314), (4, 115.19999828338628), (8, 172.8000017166137)]),
                ('shutter:close', [(0, 201.59999485015882), (4, 403.19998970031764), (8, 604.7999888420109)]),
                ('verticalAperture', [(0, 20.249998), (4, 10.125), (8, 10.125)]),
                ('verticalApertureOffset', [(0, 1.7999998), (4, -1.8000001), (8, -3.6000001)]),
            ]

            # Verify it's a USD Camera
            self.assertTrue(camera_prim.IsA(UsdGeom.Camera), "Exported prim is not a USD Camera")
            usd_camera = UsdGeom.Camera(camera_prim)

            self._ValidateUsdCamera(usd_camera, expectedPropertyTuples)

def run_tests():
    return unittest.TextTestRunner(stream=sys.stdout, verbosity=2).run(unittest.TestLoader().loadTestsFromTestCase(PythonExportCameraTest))

if __name__ == "__main__":
    from importlib import reload
    import usd_utils
    reload(usd_utils)
    usd_utils.get_config_data(update=True)
    mxs.clearListener()
    run_tests()