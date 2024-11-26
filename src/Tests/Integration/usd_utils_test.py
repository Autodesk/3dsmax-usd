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
import usd_utils
import sys
import maxUsd
import re

class TestUtils(unittest.TestCase):
    # Relative path tests
    _start = "c:\\foo"
    _path = "c:\\foo\\bar\\baz"
    _expected = "bar\\baz"
    _path_bad = "d:\\baz"

    def test_normal(self):
        rel_path = usd_utils.safe_relpath(self._path, self._start)
        self.assertEqual(rel_path, self._expected)

    def test_degenerate(self):
        rel_path = usd_utils.safe_relpath(self._path_bad, self._start)
        self.assertEqual(rel_path, self._path_bad)
        
    # Math utils
    def test_normalize_angle(self):
        self.assertEqual(0, usd_utils.normalize_angle(0))
        self.assertEqual(45, usd_utils.normalize_angle(45))
        self.assertEqual(180, usd_utils.normalize_angle(180))
        self.assertEqual(-45, usd_utils.normalize_angle(-45))
        self.assertEqual(180, usd_utils.normalize_angle(-180))
        self.assertEqual(170, usd_utils.normalize_angle(-190))
        self.assertEqual(-170, usd_utils.normalize_angle(190))
        self.assertEqual(10, usd_utils.normalize_angle(370))
        self.assertEqual(-10, usd_utils.normalize_angle(-370))
        self.assertEqual(180, usd_utils.normalize_angle(540))
        self.assertEqual(180, usd_utils.normalize_angle(-540))
        self.assertEqual(170, usd_utils.normalize_angle(-550))
        self.assertEqual(-170, usd_utils.normalize_angle(-530))
        
    def test_almost_equal(self):
        self.assertEqual(False, usd_utils.float_almost_equal(0.0,1.0))
        self.assertEqual(False, usd_utils.float_almost_equal(0.0,0.01))
        self.assertEqual(False, usd_utils.float_almost_equal(0.0,0.00001))
        self.assertEqual(False, usd_utils.float_almost_equal(0.0,0.000001))
        self.assertEqual(False, usd_utils.float_almost_equal(1.0,-1.0))
        
        self.assertEqual(True, usd_utils.float_almost_equal(1.0,1.0))
        self.assertEqual(True, usd_utils.float_almost_equal(0.0,0.0000009))
        self.assertEqual(True, usd_utils.float_almost_equal(100.0,100.0000009))
        self.assertEqual(True, usd_utils.float_almost_equal(-100.0,-100.000000999))
        self.assertEqual(True, usd_utils.float_almost_equal(-100.0,-100.000000999))
        self.assertEqual(True, usd_utils.float_almost_equal(5.000000000001, 5.000000000002))

    def test_maxUsd_python_module_version_special_attribute(self):
        self.assertEqual(True, re.match(r"\d+.\d+.\d+", maxUsd.__version__) != None)
 

def run_tests():
    return unittest.TextTestRunner(stream=sys.stdout, verbosity=2).run(unittest.TestLoader().loadTestsFromTestCase(TestUtils))

if __name__ == "__main__":
    unittest.main()
