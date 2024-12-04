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
import sys
import os
import shutil

from importlib import reload
from enum import Enum

from pxr import Usd, UsdGeom, UsdUtils

import usd_test_helpers
import shaderReader
import pymxs

__author__ = r'Autodesk Inc.'
__copyright__ = r'Copyright 2024, Autodesk Inc.'

RT = pymxs.runtime
RT.pluginManager.loadClass(RT.USDImporter)

TEST_DIR = os.path.dirname(__file__)
TEST_DATA_DIR = os.path.join(TEST_DIR, "data")


class TestImportCurves(unittest.TestCase):
    def setUp(self):
        RT.resetMaxFile(RT.Name("noprompt"))

        self.initialUnits = RT.units.SystemType
        self.initialScale = RT.units.SystemScale
        RT.units.SystemType = RT.Name("Centimeters")
        RT.units.SystemScale = 1

        self.output_prefix = usd_test_helpers.standard_output_prefix("IMPORT_CURVES_PYMXS_TEST_")
        usd_test_helpers.load_usd_plugins()

        self.stage = Usd.Stage.CreateInMemory()
        UsdGeom.SetStageUpAxis(self.stage, UsdGeom.Tokens.z)

        self.import_options = RT.USDImporter.CreateOptions()
        self.import_options.LogLevel = RT.Name("warn")

    def tearDown(self):
        RT.units.SystemType = self.initialUnits
        RT.units.SystemScale = self.initialScale

    def import_usd_basis_curve_test(self,
                                    test_usd_file_name,
                                    expected_num_knots,
                                    expected_knot_values_list,
                                    expected_knot_in_vecs_list,
                                    expected_knot_out_vecs_list,
                                    expected_is_closed,
                                    num_objects,
                                    expected_warnings = 0):
        # Setup logging.
        self.import_options.LogPath = "{0}{1}{2}".format(self.output_prefix, test_usd_file_name, ".log")
        if os.path.exists(self.import_options.LogPath):
            os.remove(self.import_options.LogPath)

        stageCache = UsdUtils.StageCache.Get()
        stageCache.Insert(self.stage)
        stageId = (stageCache.GetId(self.stage)).ToLongInt()

        RT.USDImporter.ImportFromCache(stageId, importOptions=self.import_options)
        
        # check logged warnings
        log_file_handle = open(self.import_options.LogPath)
        self.assertEqual(expected_warnings, len(log_file_handle.readlines()))
        log_file_handle.close()

        self.assertEqual(len(RT.objects), num_objects)
        if num_objects == 0:
            return

        max_curve = RT.objects[0]

        self.assertEqual(RT.ClassOf(max_curve), RT.SplineShape)

        num_knots = RT.numKnots(max_curve)
        self.assertEqual(num_knots, expected_num_knots)

        for idx, expected_knot_values  in enumerate(expected_knot_values_list):
            for jdx, expected_knot_value in enumerate(expected_knot_values):
                knot_val = RT.getKnotPoint(max_curve, idx+1, jdx+1)
                self.assertEqual(knot_val[0], expected_knot_value[0])
                self.assertEqual(knot_val[1], expected_knot_value[1])
                self.assertEqual(knot_val[2], expected_knot_value[2])

        for idx, expected_knot_in_vecs in enumerate(expected_knot_in_vecs_list):
            for jdx, expected_knot_in_vec in enumerate(expected_knot_in_vecs):
                knot_in_vec = RT.getInVec(max_curve, idx+1, jdx+1)
                self.assertEqual(knot_in_vec[0], expected_knot_in_vec[0])
                self.assertEqual(knot_in_vec[1], expected_knot_in_vec[1])
                self.assertEqual(knot_in_vec[2], expected_knot_in_vec[2])

        for idx, expected_knot_out_vecs in enumerate(expected_knot_out_vecs_list):
            for jdx, expected_knot_out_vec in enumerate(expected_knot_out_vecs):
                knot_out_vec = RT.getOutVec(max_curve, idx+1, jdx+1)
                self.assertEqual(knot_out_vec[0], expected_knot_out_vec[0])
                self.assertEqual(knot_out_vec[1], expected_knot_out_vec[1])
                self.assertEqual(knot_out_vec[2], expected_knot_out_vec[2])

        is_closed = RT.isClosed(max_curve, 1)
        self.assertEqual(is_closed, expected_is_closed)

    ########### linear ##############
    def test_usd_basis_curve_linear_non_periodic(self):
        some_curve = UsdGeom.BasisCurves.Define(self.stage, '/Curve')

        curvesPoints =  [(0,0,0), (1,1,1), (2,1,1), (3,0,0)]
        curvesType = "linear"
        curvesWrap = "nonperiodic"
        curveVertexCount = [4]
        some_curve.CreatePointsAttr(curvesPoints)
        some_curve.CreateTypeAttr(curvesType)
        some_curve.CreateWrapAttr(curvesWrap)
        some_curve.CreateCurveVertexCountsAttr(curveVertexCount)

        test_usd_file_name = "test_usd_basis_curve_linear_non_periodic"

        self.import_usd_basis_curve_test(test_usd_file_name, 4, [curvesPoints], [curvesPoints], [curvesPoints], False, 1, 0)

    def test_usd_basis_curve_linear_periodic(self):
        some_curve = UsdGeom.BasisCurves.Define(self.stage, '/Curve')

        curvesPoints =  [(0,0,0), (1,1,1), (2,1,1), (3,0,0)]
        curvesType = "linear"
        curvesWrap = "periodic"
        curveVertexCount = [4]
        some_curve.CreatePointsAttr(curvesPoints)
        some_curve.CreateTypeAttr(curvesType)
        some_curve.CreateWrapAttr(curvesWrap)
        some_curve.CreateCurveVertexCountsAttr(curveVertexCount)

        test_usd_file_name = "test_usd_basis_curve_linear_periodic"

        self.import_usd_basis_curve_test(test_usd_file_name, 4, [curvesPoints], [curvesPoints], [curvesPoints], True, 1, 0)

    def test_usd_basis_curve_linear_non_periodic_multiple(self):
        some_curve = UsdGeom.BasisCurves.Define(self.stage, '/Curve')

        curvesPoints =  [(0,0,0), (1,1,1), (2,1,1), (3,0,0), (2,1,1), (3,2,2), (4,2,2), (5,3,3), (6,3,3), (15,15,15)]
        curvesType = "linear"
        curvesWrap = "nonperiodic"
        curveVertexCount = [4, 6]
        some_curve.CreatePointsAttr(curvesPoints)
        some_curve.CreateTypeAttr(curvesType)
        some_curve.CreateWrapAttr(curvesWrap)
        some_curve.CreateCurveVertexCountsAttr(curveVertexCount)

        test_usd_file_name = "test_usd_basis_curve_linear_non_periodic_multiple"

        expected_points = [[(0,0,0), (1,1,1), (2,1,1), (3,0,0)], [(2,1,1), (3,2,2), (4,2,2), (5,3,3), (6,3,3), (15,15,15)]]
        self.import_usd_basis_curve_test(test_usd_file_name, 10, expected_points, expected_points, expected_points, False, 1, 0)

    def test_usd_basis_curve_linear_periodic_multiple(self):
        some_curve = UsdGeom.BasisCurves.Define(self.stage, '/Curve')

        curvesPoints =  [(0,0,0), (1,1,1), (2,1,1), (3,0,0), (2,1,1), (3,2,2), (4,2,2), (5,3,3), (6,3,3), (15,15,15)]
        curvesType = "linear"
        curvesWrap = "periodic"
        curveVertexCount = [4, 6]
        some_curve.CreatePointsAttr(curvesPoints)
        some_curve.CreateTypeAttr(curvesType)
        some_curve.CreateWrapAttr(curvesWrap)
        some_curve.CreateCurveVertexCountsAttr(curveVertexCount)

        test_usd_file_name = "test_usd_basis_curve_linear_periodic_multiple"

        expected_points = [[(0,0,0), (1,1,1), (2,1,1), (3,0,0)], [(2,1,1), (3,2,2), (4,2,2), (5,3,3), (6,3,3), (15,15,15)]]
        self.import_usd_basis_curve_test(test_usd_file_name, 10, expected_points, expected_points, expected_points, True, 1, 0)

    def test_usd_basis_curve_linear_insufficient_points(self):
        some_curve = UsdGeom.BasisCurves.Define(self.stage, '/Curve')

        curvesPoints =  [(0,0,0)]
        curvesType = "linear"

        curveVertexCount = [1]
        some_curve.CreatePointsAttr(curvesPoints)
        some_curve.CreateTypeAttr(curvesType)
        some_curve.CreateCurveVertexCountsAttr(curveVertexCount)

        test_usd_file_name = "test_usd_basis_curve_linear_insufficient_points"

        self.import_usd_basis_curve_test(test_usd_file_name, 0, None, None, None, False, 0, 1)

    ########### cubic ##############
    def test_usd_basis_curve_cubic_bezier_nonperiodic_valid(self):
        some_curve = UsdGeom.BasisCurves.Define(self.stage, '/Curve')

        curvesPoints =  [(0,0,0), (1,1,1), (2,1,1), (3,2,2), (4,2,2), (5,3,3), (6,3,3)]
        curvesType = "cubic"
        curvesBasis = "bezier"
        curvesWrap = "nonperiodic"
        curveVertexCount = [7]
        some_curve.CreatePointsAttr(curvesPoints)
        some_curve.CreateTypeAttr(curvesType)
        some_curve.CreateWrapAttr(curvesWrap)
        some_curve.CreateBasisAttr(curvesBasis)
        some_curve.CreateCurveVertexCountsAttr(curveVertexCount)

        test_usd_file_name = "test_usd_basis_curve_cubic_bezier_nonperiodic_valid"

        expected_points = [[(0,0,0), (3,2,2), (6,3,3)]]
        expected_in_vecs = [[(0,0,0), (2,1,1), (5,3,3)]]
        expected_out_vecs = [[(1,1,1), (4,2,2), (6,3,3)]]
        self.import_usd_basis_curve_test(test_usd_file_name, 3, expected_points, expected_in_vecs, expected_out_vecs, False, 1, 0)


    def test_usd_basis_curve_cubic_bezier_nonperiodic_invalid(self):
        some_curve = UsdGeom.BasisCurves.Define(self.stage, '/Curve')

        curvesPoints =  [(0,0,0), (1,1,1), (2,1,1), (3,2,2), (4,2,2), (5,3,3), (6,3,3), (15,15,15)]
        curvesType = "cubic"
        curvesBasis = "bezier"
        curvesWrap = "nonperiodic"
        curveVertexCount = [8] # invalid amount of verts -- needs to validate ((count - 4) % 3 == 0)
        some_curve.CreatePointsAttr(curvesPoints)
        some_curve.CreateTypeAttr(curvesType)
        some_curve.CreateWrapAttr(curvesWrap)
        some_curve.CreateBasisAttr(curvesBasis)
        some_curve.CreateCurveVertexCountsAttr(curveVertexCount)

        test_usd_file_name = "test_usd_basis_curve_cubic_bezier_nonperiodic_invalid"

        expected_points = [[(0,0,0), (3,2,2), (6,3,3)]]
        expected_in_vecs = [[(0,0,0), (2,1,1), (5,3,3)]]
        expected_out_vecs = [[(1,1,1), (4,2,2), (15,15,15)]]
        self.import_usd_basis_curve_test(test_usd_file_name, 3, expected_points, expected_in_vecs, expected_out_vecs, False, 1, 1)

    def test_usd_basis_curve_cubic_nonbezier(self):
        # this function is to test that non-bezier type functions get converted in the same way as bezier
        some_curve = UsdGeom.BasisCurves.Define(self.stage, '/Curve')

        curvesPoints =  [(0,0,0), (1,1,1), (2,1,1), (3,2,2), (4,2,2), (5,3,3), (6,3,3)]
        curvesType = "cubic"
        curvesBasis = "bspline"
        curveVertexCount = [7]
        some_curve.CreatePointsAttr(curvesPoints)
        some_curve.CreateTypeAttr(curvesType)
        some_curve.CreateBasisAttr(curvesBasis)
        some_curve.CreateCurveVertexCountsAttr(curveVertexCount)

        test_usd_file_name = "test_usd_basis_curve_cubic_nonbezier"

        expected_points = [[(0,0,0), (3,2,2), (6,3,3)]]
        expected_in_vecs = [[(0,0,0), (2,1,1), (5,3,3)]]
        expected_out_vecs = [[(1,1,1), (4,2,2), (6,3,3)]]
        self.import_usd_basis_curve_test(test_usd_file_name, 3, expected_points, expected_in_vecs, expected_out_vecs, False, 1, 0)

    def test_usd_basis_curve_cubic_bezier_periodic_valid(self):
        some_curve = UsdGeom.BasisCurves.Define(self.stage, '/Curve')

        curvesPoints =  [(0,0,0), (1,1,1), (2,1,1), (3,2,2), (4,2,2), (15,3,3)]
        curvesType = "cubic"
        curvesBasis = "bezier"
        curvesWrap = "periodic"
        curveVertexCount = [6]
        some_curve.CreatePointsAttr(curvesPoints)
        some_curve.CreateTypeAttr(curvesType)
        some_curve.CreateWrapAttr(curvesWrap)
        some_curve.CreateBasisAttr(curvesBasis)
        some_curve.CreateCurveVertexCountsAttr(curveVertexCount)

        test_usd_file_name = "test_usd_basis_curve_cubic_bezier_periodic_valid"

        expected_points = [[(0,0,0), (3,2,2)]]
        expected_in_vecs = [[(15,3,3), (2,1,1)]]
        expected_out_vecs = [[(1,1,1), (4,2,2)]]
        self.import_usd_basis_curve_test(test_usd_file_name, 2, expected_points, expected_in_vecs, expected_out_vecs, True, 1, 0)

    def test_usd_basis_curve_cubic_bezier_periodic_invalid_period_valid_last_point(self):
        some_curve = UsdGeom.BasisCurves.Define(self.stage, '/Curve')

        curvesPoints =  [(0,0,0), (1,1,1), (2,1,1), (3,2,2), (4,2,2), (5,3,3), (10,10,10)]
        curvesType = "cubic"
        curvesBasis = "bezier"
        curvesWrap = "periodic"
        curveVertexCount = [7] # invalid amount of verts -- needs to validate ((count % 3) == 0). however, passes ((count - 4) % 3 == 0), so last point is drawn
        some_curve.CreatePointsAttr(curvesPoints)
        some_curve.CreateTypeAttr(curvesType)
        some_curve.CreateWrapAttr(curvesWrap)
        some_curve.CreateBasisAttr(curvesBasis)
        some_curve.CreateCurveVertexCountsAttr(curveVertexCount)

        test_usd_file_name = "test_usd_basis_curve_cubic_bezier_periodic_invalid_period_valid_last_point"

        expected_points = [[(0,0,0), (3,2,2), (10,10,10)]]
        expected_in_vecs = [[(0,0,0), (2,1,1), (5,3,3)]]
        expected_out_vecs = [[(1,1,1), (4,2,2), (10,10,10)]]
        self.import_usd_basis_curve_test(test_usd_file_name, 3, expected_points, expected_in_vecs, expected_out_vecs, False, 1, 1)

    def test_usd_basis_curve_cubic_bezier_periodic_invalid_period_invalid_last_point(self):
        some_curve = UsdGeom.BasisCurves.Define(self.stage, '/Curve')

        curvesPoints =  [(0,0,0), (1,1,1), (2,1,1), (3,2,2), (4,2,2), (5,3,3), (10,10,10), (15,15,15)]
        curvesType = "cubic"
        curvesBasis = "bezier"
        curvesWrap = "periodic"
        curveVertexCount = [8] # invalid amount of verts -- needs to validate ((count % 3) == 0)
        some_curve.CreatePointsAttr(curvesPoints)
        some_curve.CreateTypeAttr(curvesType)
        some_curve.CreateWrapAttr(curvesWrap)
        some_curve.CreateBasisAttr(curvesBasis)
        some_curve.CreateCurveVertexCountsAttr(curveVertexCount)

        test_usd_file_name = "test_usd_basis_curve_cubic_bezier_periodic_invalid_period_invalid_last_point"

        expected_points = [[(0,0,0), (3,2,2), (10,10,10)]]
        expected_in_vecs = [[(0,0,0), (2,1,1), (5,3,3)]]
        expected_out_vecs = [[(1,1,1), (4,2,2), (15,15,15)]]
        self.import_usd_basis_curve_test(test_usd_file_name, 3, expected_points, expected_in_vecs, expected_out_vecs, False, 1, 1)

    def test_usd_basis_curve_cubic_non_periodic_multiple(self):
        some_curve = UsdGeom.BasisCurves.Define(self.stage, '/Curve')

        curvesPoints =  [(0,0,0), (1,1,1), (2,1,1), (3,0,0), (0,0,0), (1,1,1), (2,1,1), (3,2,2), (4,2,2), (5,3,3), (6,3,3)]
        curvesType = "cubic"
        curvesWrap = "nonperiodic"
        curveVertexCount = [4, 7]
        some_curve.CreatePointsAttr(curvesPoints)
        some_curve.CreateTypeAttr(curvesType)
        some_curve.CreateWrapAttr(curvesWrap)
        some_curve.CreateCurveVertexCountsAttr(curveVertexCount)

        test_usd_file_name = "test_usd_basis_curve_cubic_non_periodic_multiple"

        expected_points = [[(0,0,0), (3,0,0)], [(0,0,0), (3,2,2), (6,3,3)]]
        expected_in_vecs = [[(0,0,0), (2,1,1)], [(0,0,0), (2,1,1), (5,3,3)]]
        expected_out_vecs = [[(1,1,1), (3,0,0)], [(1,1,1), (4,2,2), (6,3,3)]]
        self.import_usd_basis_curve_test(test_usd_file_name, 5, expected_points, expected_in_vecs, expected_out_vecs, False, 1, 0)

    def test_usd_basis_curve_cubic_periodic_multiple(self):
        some_curve = UsdGeom.BasisCurves.Define(self.stage, '/Curve')

        curvesPoints =  [(0,0,0), (1,1,1), (2,1,1), (3,2,2), (4,2,2), (5,3,3), (0,0,0), (1,1,1), (2,1,1), (3,2,2), (4,2,2), (5,3,3)]
        curvesType = "cubic"
        curvesWrap = "periodic"
        curveVertexCount = [6, 6]
        some_curve.CreatePointsAttr(curvesPoints)
        some_curve.CreateTypeAttr(curvesType)
        some_curve.CreateWrapAttr(curvesWrap)
        some_curve.CreateCurveVertexCountsAttr(curveVertexCount)

        test_usd_file_name = "test_usd_basis_curve_cubic_periodic_multiple"

        expected_points = [[(0,0,0), (3,2,2)], [(0,0,0), (3,2,2)]]
        expected_in_vecs = [[(5,3,3), (2,1,1)], [(5,3,3), (2,1,1)]]
        expected_out_vecs = [[(1,1,1), (4,2,2)], [(1,1,1), (4,2,2)]]
        self.import_usd_basis_curve_test(test_usd_file_name, 4, expected_points, expected_in_vecs, expected_out_vecs, True, 1, 0)

    def test_usd_basis_curve_cubic_nonperiodic_min_size(self):
        some_curve = UsdGeom.BasisCurves.Define(self.stage, '/Curve')

        curvesPoints =  [(0,0,0), (1,1,1), (2,1,1), (3,0,0)]
        curvesType = "cubic"
        curvesWrap = "nonperiodic"
        curveVertexCount = [4]
        some_curve.CreatePointsAttr(curvesPoints)
        some_curve.CreateTypeAttr(curvesType)
        some_curve.CreateWrapAttr(curvesWrap)
        some_curve.CreateCurveVertexCountsAttr(curveVertexCount)

        test_usd_file_name = "test_usd_basis_curve_cubic_periodic_min_size"

        expected_points = [[(0,0,0), (3,0,0)]]
        expected_in_vecs = [[(0,0,0), (2,1,1)]]
        expected_out_vecs = [[(1,1,1), (3,0,0)]]
        self.import_usd_basis_curve_test(test_usd_file_name, 2, expected_points, expected_in_vecs, expected_out_vecs, False, 1, 0)
        
    def test_usd_basis_curve_cubic_periodic_min_size(self):
        some_curve = UsdGeom.BasisCurves.Define(self.stage, '/Curve')

        curvesPoints =  [(0,0,0), (1,1,1), (2,1,1)]
        curvesType = "cubic"
        curvesWrap = "periodic"
        curveVertexCount = [3]
        some_curve.CreatePointsAttr(curvesPoints)
        some_curve.CreateTypeAttr(curvesType)
        some_curve.CreateWrapAttr(curvesWrap)
        some_curve.CreateCurveVertexCountsAttr(curveVertexCount)

        test_usd_file_name = "test_usd_basis_curve_cubic_periodic_min_size"

        self.import_usd_basis_curve_test(test_usd_file_name, 0, None, None, None, False, 0, 1)

    def test_usd_basis_curve_cubic_insufficient_points(self):
        some_curve = UsdGeom.BasisCurves.Define(self.stage, '/Curve')

        curvesPoints =  [(0,0,0)]
        curvesType = "cubic"

        curveVertexCount = [1]
        some_curve.CreatePointsAttr(curvesPoints)
        some_curve.CreateTypeAttr(curvesType)
        some_curve.CreateCurveVertexCountsAttr(curveVertexCount)

        test_usd_file_name = "test_usd_basis_curve_cubic_insufficient_points"

        self.import_usd_basis_curve_test(test_usd_file_name, 0, None, None, None, False, 0, 1)

    ############## MISC ##############
    def test_usd_basis_curve_missing_count(self):
        some_curve = UsdGeom.BasisCurves.Define(self.stage, '/Curve')

        curvesPoints =  [(0,0,0), (1,1,1), (2,1,1), (3,2,2), (4,2,2), (5,3,3), (10,10,10), (15,15,15)]
        curvesType = "cubic"
        curvesBasis = "bezier"
        curvesWrap = "periodic"
        some_curve.CreatePointsAttr(curvesPoints)
        some_curve.CreateTypeAttr(curvesType)
        some_curve.CreateWrapAttr(curvesWrap)
        some_curve.CreateBasisAttr(curvesBasis)

        test_usd_file_name = "test_usd_basis_curve_missing_count"

        self.import_usd_basis_curve_test(test_usd_file_name, 0, None, None, None, False, 0, 1)

    def test_usd_basis_curve_missing_points(self):
        some_curve = UsdGeom.BasisCurves.Define(self.stage, '/Curve')

        curvesType = "cubic"
        curvesBasis = "bezier"
        curvesWrap = "nonperiodic"
        curveVertexCount = [4]
        some_curve.CreateTypeAttr(curvesType)
        some_curve.CreateWrapAttr(curvesWrap)
        some_curve.CreateBasisAttr(curvesBasis)
        some_curve.CreateCurveVertexCountsAttr(curveVertexCount)

        test_usd_file_name = "test_usd_basis_curve_missing_points"

        self.import_usd_basis_curve_test(test_usd_file_name, 0, None, None, None, False, 0, 1)

    def test_usd_basis_curve_mismatch_points_and_count(self):
        some_curve = UsdGeom.BasisCurves.Define(self.stage, '/Curve')

        curvesPoints =  [(0,0,0), (1,1,1), (2,1,1), (3,2,2), (4,2,2), (5,3,3), (10,10,10), (15,15,15)]
        curvesType = "cubic"
        curvesBasis = "bezier"
        curvesWrap = "nonperiodic"
        curveVertexCount = [4]
        some_curve.CreatePointsAttr(curvesPoints)
        some_curve.CreateTypeAttr(curvesType)
        some_curve.CreateWrapAttr(curvesWrap)
        some_curve.CreateBasisAttr(curvesBasis)
        some_curve.CreateCurveVertexCountsAttr(curveVertexCount)

        test_usd_file_name = "test_usd_basis_curve_mismatch_points_and_count"

        self.import_usd_basis_curve_test(test_usd_file_name, 0, None, None, None, False, 0, 1)

def run_tests():
    return unittest.TextTestRunner(stream=sys.stdout, verbosity=2).run(unittest.TestLoader().loadTestsFromTestCase(TestImportCurves))

if __name__ == "__main__":
    RT.clearListener()
    run_tests()
