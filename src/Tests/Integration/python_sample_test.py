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
import os
from pymxs import runtime as mxs
import usd_test_helpers

class ImportTestCase(unittest.TestCase):
	def setUp(self):
		mxs.resetMaxFile(mxs.Name("noprompt"))
		usd_test_helpers.load_usd_plugins()		
		self.tempDir = mxs.getDir(mxs.Name("temp"))

	def tearDown(self):
		print("end")

	def test_import(self):
		#given
		from pxr import Kind, Sdf, Usd, UsdGeom, UsdShade
		usdFilePath = os.path.join(self.tempDir, "dummy_import.usd")
		stage = Usd.Stage.CreateInMemory()
		UsdGeom.SetStageUpAxis(stage, UsdGeom.Tokens.y)
		root = UsdGeom.Xform.Define(stage, "/TexModel")
		Usd.ModelAPI(root).SetKind(Kind.Tokens.component)
		usdFile = open(usdFilePath, "w")
		usdFile.write(stage.GetRootLayer().ExportToString())
		usdFile.close()

		#when
		status = mxs.importFile(usdFilePath, mxs.Name("noprompt"))

		#then
		assert (status == 1), "import usd failed"

	def test_export(self):
		#given
		from pxr import Kind, Sdf, Usd, UsdGeom, UsdShade
		usdFilePath = os.path.join(self.tempDir, "dummy_export.usda")
		mxs.teapot()

		#when
		status = mxs.exportFile(usdFilePath, mxs.Name("noprompt"))

		#then
		stage = Usd.Stage.Open(usdFilePath)
		meshTeapot = stage.GetPrimAtPath("/Teapot001")
		assert isinstance(meshTeapot, Usd.Prim), "could not find expected mesh 'Teapot001'"

		
if __name__ == '__main__':
	unittest.TextTestRunner(stream=sys.stdout, verbosity=2).run(unittest.TestLoader().loadTestsFromTestCase(ImportTestCase))