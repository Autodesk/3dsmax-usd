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

class MeshConverterDummyWriter(maxUsd.PrimWriter):
    
    def Write(self, prim, applyOffset, time):
        try: 
            nodeHandle = self.GetNodeHandle()
            opts = self.GetExportArgs()
            stage = prim.GetStage()
            path = "/foo/mesh"
            
            # Write the box somewhere with applyOffset = false
            usdMesh = maxUsd.MeshConverter.ConvertToUSDMesh(nodeHandle, stage, path, opts, False, time)
            
            meshPrim = usdMesh.GetPrim()
            rt.assert_true(meshPrim.IsValid())
            rt.assert_true(meshPrim.IsA(UsdGeom.Mesh))
            rt.assert_equal("/foo/mesh", str(meshPrim.GetPath()))
            
            rt.assert_equal(8, len(usdMesh.GetPointsAttr().Get()))
            rt.assert_equal(6, len(usdMesh.GetFaceVertexCountsAttr().Get()))
            rt.assert_equal(24, len(usdMesh.GetFaceVertexIndicesAttr().Get()))

            # Called with apply offset = false, expect no transform on the mesh
            xformable = UsdGeom.Xformable(meshPrim)
            ops = xformable.GetOrderedXformOps()
            rt.assert_equal(0, len(ops))

            # Write the box somewhere else, with applyOffset = true
            path = "/bar/mesh"
            usdMesh = maxUsd.MeshConverter.ConvertToUSDMesh(nodeHandle, stage, path, opts, True,  time)
            meshPrim = usdMesh.GetPrim()
            xformable = UsdGeom.Xformable(prim)
            ops = xformable.GetOrderedXformOps()
            # This also tests that options are wired correctly, as without BakeObjectOffset = false, there
            # would still be 0 xformOps.
            rt.assert_equal(1, len(ops))
            return True
                       
            
        except Exception as e:
            rt.assert_true(False, message="\nFailed to test MeshConverter.ConvertToUSDMesh()!")
            print('\nWrite() - Error: %s' % str(e))

    @classmethod
    def CanExport(cls, nodeHandle, exportArgs):
        node = rt.maxOps.getNodeByHandle(nodeHandle)
        if rt.classOf(node) == rt.Box:
            return maxUsd.PrimWriter.ContextSupport.Supported
        return maxUsd.PrimWriter.ContextSupport.Unsupported


class TestPythonMeshConverter(unittest.TestCase):

    def setUp(self):
        rt.resetMaxFile(rt.Name("noprompt"))
        self.output_prefix = usd_test_helpers.standard_output_prefix("PYHTON_MESH_CONVERTER_TEST_")

    def tearDown(self):
        pass

    def test_inode_to_usdmesh(self):
        maxUsd.PrimWriter.Unregister(MeshConverterDummyWriter, "MeshConverterDummyWriter")
        maxUsd.PrimWriter.Register(MeshConverterDummyWriter, "MeshConverterDummyWriter")
        
        box = rt.Box()
        
        box.objectOffsetPos = rt.Point3(0, 0, 10)
        
        export_options = rt.UsdExporter.CreateOptions()
        export_options.FileFormat = rt.Name("ascii")
        test_usd_file_path = self.output_prefix + "test_convert_to_usd_mesh.usda"
        print(test_usd_file_path)
        ret = rt.USDExporter.ExportFile(test_usd_file_path, exportOptions=export_options)
        self.assertTrue(ret)
        maxUsd.PrimWriter.Unregister(MeshConverterDummyWriter, "MeshConverterDummyWriter")
        

rt.clearListener()
def run_tests():
    return unittest.TextTestRunner(stream=sys.stdout, verbosity=2).run(unittest.TestLoader().loadTestsFromTestCase(TestPythonMeshConverter))

if __name__ == '__main__':
    run_tests()
