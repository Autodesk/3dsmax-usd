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
import sys, os

import usd_test_helpers
import maxUsd
import pymxs
from pymxs import runtime as rt

from pxr import Usd, UsdGeom, UsdUtils, Sdf, Plug

# A prim reader used to test that all overriden methods are called into and 
# their results used. Each implemented methods sets a static variable used 
# to validate that the method is indeed called into by the system. A similar approach is 
# used to validate the received method parameters - for example
# TestReader.SphereNodeHandle is set from the test method to the exported node's 
# handle value.
class TestReader(maxUsd.PrimReader):
    InstancesCreated = []
    
    def Read(self):
        TestReader.Read_Called = True

        usdPrim = self.GetUsdPrim()
        box = UsdGeom.Cube(usdPrim)
        length = box.GetSizeAttr().Get()

        # create box node and attach newly created node to its parents if any
        parentHandle = self.GetJobContext().GetNodeHandle(usdPrim.GetPath().GetParentPath(), False)
        if (parentHandle):
            node = rt.box(length=length, width=length, height=length, name=usdPrim.GetName(), parent=rt.GetAnimByHandle(parentHandle))
        else:
            node = rt.box(length=length, width=length, height=length, name=usdPrim.GetName())

        self.GetJobContext().RegisterCreatedNode(usdPrim.GetPath(), rt.GetHandleByAnim(node))
        
        # forcing a 30 degrees Z rotation on import
        # need to feed the correction as a list of point3
        self.ReadXformable([[0.866025,0.5,0], [-0.5,0.866025,0], [0,0,1], [0,0,0]])

        return True
    
    def HasPostReadSubtree(self):
        return True
        
    def PostReadSubtree(self):
        TestReader.PostRead_Called = True
        
    def InstanceCreated(self, prim, nodeHandle):
        TestReader.InstancesCreated.append(prim)
        TestReader.InstanceCreated_Called = True
    
    @classmethod
    def CanImport(cls, args, prim):
        TestReader.CanImport_Called = True

        try: 
            # Test import options access.
            options = args
            rt.assert_equal(True, options.GetTranslateMaterials())
            rt.assert_equal(Usd.Stage.LoadAll, options.GetStageInitialLoadSet())
            rt.assert_equal(str(0.0), str(options.GetStartTimeCode()))
            rt.assert_equal(1, len(options.GetShadingModes()))
            rt.assert_equal("useRegistry", options.GetShadingModes()[0]['mode'])
            rt.assert_equal("UsdPreviewSurface", options.GetShadingModes()[0]['materialConversion'])
            rt.assert_equal("none", options.GetPreferredMaterial())
            rt.assert_equal(1, len(options.GetStageMaskPaths()))
            rt.assert_equal("/", str(options.GetStageMaskPaths()[0]))
            rt.assert_equal(str({0,1,2}), str(options.GetMetaData()))
            rt.assert_equal((rt.getDir(rt.name('temp')) + '\\MaxUsdImport.log'), options.GetLogPath())
            rt.assert_equal(maxUsd.LogLevel.Off, options.GetLogLevel())
            rt.assert_equal(True, options.GetImportUnmappedPrimvars())
            
            rt.assert_equal(500, len(options.GetMappedPrimvars()))
            rt.assert_equal("displayOpacity", options.GetMappedPrimvars()[0])
            rt.assert_equal(True, options.IsMappedPrimvar('displayOpacity'))
            rt.assert_equal(-2, options.GetPrimvarChannel('displayOpacity'))

        except Exception as e:
            # Quite useful to debug errors in a Python callback
            print('Read() - Error: %s' % str(e))
            print(traceback.format_exc())
            rt.assert_true(False, message="Python exposure of Import options not working as expected")

        if prim.IsA(UsdGeom.Cube):
            return maxUsd.PrimReader.ContextSupport.Supported
        return maxUsd.PrimReader.ContextSupport.Unsupported

class TestGeneralUsdGeomXformableReader(maxUsd.PrimReader):
    def Read(self):
        TestGeneralUsdGeomXformableReader.Read_Called = True

        usdPrim = self.GetUsdPrim()
        sphere = UsdGeom.Sphere(usdPrim)

        # create sphere node and attach newly created node to its parents if any
        parentHandle = self.GetJobContext().GetNodeHandle(usdPrim.GetPath().GetParentPath(), False)
        if (parentHandle):
            node = rt.sphere(name=usdPrim.GetName(), parent=rt.GetAnimByHandle(parentHandle))
        else:
            node = rt.sphere(name=usdPrim.GetName())

        def RadiusSetter(value, usdTimeCode, maxFrame) -> bool:
            node.radius = value
            return True
        maxUsd.TranslationUtils.ReadUsdAttribute(sphere.GetRadiusAttr(),
                                                 maxUsd.AnimatedAttributeHelper(RadiusSetter)(),
                                                 self.GetJobContext())

        self.GetJobContext().RegisterCreatedNode(usdPrim.GetPath(), rt.GetHandleByAnim(node))
        self.ReadXformable()
        
        return True
    
    @classmethod
    def CanImport(cls, args, prim):
        if prim.IsA(UsdGeom.Sphere):
            TestGeneralUsdGeomXformableReader.CanImportSphere_Called = True
            return maxUsd.PrimReader.ContextSupport.Supported
        return maxUsd.PrimReader.ContextSupport.Unsupported

# UsdGeomMesh Reader overriding the default 3ds Max USD one written in C++
# It also verifies the material assignment from Pyton
class TestMeshReader(maxUsd.PrimReader):
    @classmethod
    def CanImport(cls, args, prim):
        return maxUsd.PrimReader.ContextSupport.Supported

    def Read(self):
        try: 
            usdPrim = self.GetUsdPrim()
            usdMesh = UsdGeom.Mesh(usdPrim)

            trimesh = rt.USDImporter.ConvertUsdMesh(UsdUtils.StageCache.Get().GetId(usdPrim.GetStage()).ToLongInt(), usdPrim.GetPath())
            meshNode = rt.mesh(mesh=trimesh.mesh)
            meshNode.name = usdPrim.GetName() + "_usd" # overriding the name for test purpose
            # create editable mesh node and attach newly created node to its parents if any
            parentHandle = self.GetJobContext().GetNodeHandle(usdPrim.GetPath().GetParentPath(), False)
            if (parentHandle):
                meshNode.parent = rt.GetAnimByHandle(parentHandle)
            self.GetJobContext().RegisterCreatedNode(usdPrim.GetPath(), rt.GetHandleByAnim(meshNode))

            # properly position the node
            self.ReadXformable()

            # assign material
            maxUsd.TranslatorMaterial.AssignMaterial(self.GetArgs(), usdMesh, rt.GetHandleByAnim(meshNode), self.GetJobContext())
            
            return True

        except Exception as e:
            # Quite useful to debug errors in a Python callback
            print('Read() - Error: %s' % str(e))
            print(traceback.format_exc())
            return False

# Test class.
class TestPrimReader(unittest.TestCase):

    def setUp(self):
        rt.resetMaxFile(rt.Name("noprompt"))
        self.output_prefix = usd_test_helpers.standard_output_prefix("IMPORT_PRIM_READER_TEST_")

    def tearDown(self):
        pass

    # test a reader registered for a GeomCube prim type
    # the reader changes the default import orientation of the imported box
    def test_cube_reader(self):
        maxUsd.PrimReader.Unregister(TestReader, "UsdGeomCube")
        maxUsd.PrimReader.Register(TestReader, "UsdGeomCube")

        test_data_dir = os.path.dirname(__file__)
        cube_file_path = (test_data_dir + "\\data\\cube_native.usda")

        TestReader.Read_Called = False
        TestReader.PostRead_Called = False
        TestReader.CanImport_Called = False
        TestReader.InstanceCreated_Called = False
        TestReader.InstancesCreated = []

        rt.units.SystemType = rt.Name("Millimeters")
        rt.units.SystemScale = 1
        
        import_options = rt.UsdImporter.CreateOptions()
        ret = rt.USDImporter.ImportFile(cube_file_path, importOptions=import_options)
        
        self.assertTrue(ret)
        
        self.assertTrue(TestReader.Read_Called)
        self.assertTrue(TestReader.PostRead_Called)
        self.assertTrue(TestReader.CanImport_Called)
        self.assertFalse(TestReader.InstanceCreated_Called)
        self.assertEqual(len(TestReader.InstancesCreated), 0)
        self.assertEqual(rt.objects[0].name, 'box')
        self.assertEqual(rt.objects[0].length, 4.0)
        self.assertEqual(rt.objects[0].width, 4.0)
        self.assertEqual(rt.objects[0].height, 4.0)
        self.assertEqual(rt.objects[0].rotation, rt.Quat(rt.RotateZMatrix(-30)))

        maxUsd.PrimReader.Unregister(TestReader, "UsdGeomCube")

    # test a reader registered for a general xformable prim type but only import usd native spheres
    def test_general_usd_geom_xformable_reader(self):
        maxUsd.PrimReader.Unregister(TestGeneralUsdGeomXformableReader, "UsdGeomXformable")
        maxUsd.PrimReader.Register(TestGeneralUsdGeomXformableReader, "UsdGeomXformable")

        test_data_dir = os.path.dirname(__file__)
        sphere_file_path = (test_data_dir + "\\data\\sphere_native.usda")

        TestGeneralUsdGeomXformableReader.Read_Called = False
        TestGeneralUsdGeomXformableReader.CanImportSphere_Called = False

        rt.units.SystemType = rt.Name("Millimeters")
        rt.units.SystemScale = 1

        import_options = rt.UsdImporter.CreateOptions()
        ret = rt.USDImporter.ImportFile(sphere_file_path, importOptions=import_options)
        
        self.assertTrue(ret)
        
        self.assertTrue(TestGeneralUsdGeomXformableReader.Read_Called)
        self.assertTrue(TestGeneralUsdGeomXformableReader.CanImportSphere_Called)
        self.assertEqual(rt.objects[0].name, 'sphere')
        self.assertEqual(rt.objects[0].radius, 4.0)

        #maxUsd.PrimReader.Unregister(TestGeneralUsdGeomXformableReader, "UsdGeomXformable")

    # test a reader with animated attributes on usd native spheres
    #def test_general_usd_geom_xformable_reader_with_animated_attributes(self):
        #maxUsd.PrimReader.Unregister(TestGeneralUsdGeomXformableReader, "UsdGeomXformable")
        #maxUsd.PrimReader.Register(TestGeneralUsdGeomXformableReader, "UsdGeomXformable")
        rt.resetMaxFile(rt.Name("noprompt"))

        test_data_dir = os.path.dirname(__file__)
        # file defines a UsdGeomSphere with its radius changing thru time
        # at start at 0s = 8mm, 20s = 4mm and 40s = 16mm
        sphere_file_path = (test_data_dir + "\\data\\sphere_native_animated.usda")

        TestGeneralUsdGeomXformableReader.Read_Called = False
        TestGeneralUsdGeomXformableReader.CanImportSphere_Called = False

        rt.units.SystemType = rt.Name("Millimeters")
        rt.units.SystemScale = 1

        import_options = rt.UsdImporter.CreateOptions()
        import_options.TimeMode = maxUsd.ImportTimeMode.CustomRange
        import_options.StartTimeCode = 0
        import_options.EndTimeCode = 60
        ret = rt.USDImporter.ImportFile(sphere_file_path, importOptions=import_options)
        
        self.assertTrue(ret)
        
        self.assertTrue(TestGeneralUsdGeomXformableReader.Read_Called)
        self.assertTrue(TestGeneralUsdGeomXformableReader.CanImportSphere_Called)
        self.assertEqual(rt.objects[0].name, 'sphere')
        sphere_radius_controller = rt.getSubAnim(rt.getSubAnim(rt.objects[0], 4), 1)
        self.assertEqual(sphere_radius_controller.keys.count, 3)
        self.assertEqual(str(sphere_radius_controller.keys), '#keys(0f, 20f, 40f)')
        with pymxs.attime(0):
            self.assertEqual(rt.objects[0].radius, 8.0)
        with pymxs.attime(20):
            self.assertEqual(rt.objects[0].radius, 4.0)
        with pymxs.attime(40):
            self.assertEqual(rt.objects[0].radius, 16.0)
        with pymxs.attime(60):
            self.assertEqual(rt.objects[0].radius, 16.0)

        rt.resetMaxFile(rt.Name("noprompt"))
        rt.units.SystemType = rt.Name("Millimeters")
        rt.units.SystemScale = 1

        TestGeneralUsdGeomXformableReader.Read_Called = False
        TestGeneralUsdGeomXformableReader.CanImportSphere_Called = False

        # verify import of animation as a subset of the full time range of original
        import_options.TimeMode = rt.Name("customRange")
        import_options.StartTimeCode = 10
        import_options.EndTimeCode = 60
        ret = rt.USDImporter.ImportFile(sphere_file_path, importOptions=import_options)
        
        self.assertTrue(ret)

        self.assertTrue(TestGeneralUsdGeomXformableReader.Read_Called)
        self.assertTrue(TestGeneralUsdGeomXformableReader.CanImportSphere_Called)
        self.assertEqual(rt.objects[0].name, 'sphere')
        sphere_radius_controller = rt.getSubAnim(rt.getSubAnim(rt.objects[0], 4), 1)
        self.assertEqual(sphere_radius_controller.keys.count, 3)
        self.assertEqual(str(sphere_radius_controller.keys), '#keys(10f, 20f, 40f)')
        with pymxs.attime(0):
            self.assertEqual(rt.objects[0].radius, 6.0)
        with pymxs.attime(10):
            self.assertEqual(rt.objects[0].radius, 6.0)
        with pymxs.attime(20):
            self.assertEqual(rt.objects[0].radius, 4.0)
        with pymxs.attime(40):
            self.assertEqual(rt.objects[0].radius, 16.0)
        with pymxs.attime(60):
            self.assertEqual(rt.objects[0].radius, 16.0)

        maxUsd.PrimReader.Unregister(TestGeneralUsdGeomXformableReader, "UsdGeomXformable")

    # test a reader with animated attributes on usd native spheres
    def test_general_usd_geom_xformable_reader_with_animated_attributes(self):
        maxUsd.PrimReader.Unregister(TestGeneralUsdGeomXformableReader, "UsdGeomXformable")
        maxUsd.PrimReader.Register(TestGeneralUsdGeomXformableReader, "UsdGeomXformable")

        test_data_dir = os.path.dirname(__file__)
        # file defines a UsdGeomSphere with its radius changing thru time
        # at start at 0s = 8mm, 20s = 4mm and 40s = 16mm
        sphere_file_path = (test_data_dir + "\\data\\sphere_native_animated.usda")

        TestGeneralUsdGeomXformableReader.Read_Called = False
        TestGeneralUsdGeomXformableReader.CanImportSphere_Called = False

        rt.units.SystemType = rt.Name("Millimeters")
        rt.units.SystemScale = 1

        import_options = rt.UsdImporter.CreateOptions()
        import_options.TimeMode = rt.Name("customRange")
        import_options.StartTimeCode = 0
        import_options.EndTimeCode = 60
        ret = rt.USDImporter.ImportFile(sphere_file_path, importOptions=import_options)
        
        self.assertTrue(ret)
        
        self.assertTrue(TestGeneralUsdGeomXformableReader.Read_Called)
        self.assertTrue(TestGeneralUsdGeomXformableReader.CanImportSphere_Called)
        self.assertEqual(rt.objects[0].name, 'sphere')
        sphere_radius_controller = rt.getSubAnim(rt.getSubAnim(rt.objects[0], 4), 1)
        self.assertEqual(sphere_radius_controller.keys.count, 3)
        self.assertEqual(str(sphere_radius_controller.keys), '#keys(0f, 20f, 40f)')
        with pymxs.attime(0):
            self.assertEqual(rt.objects[0].radius, 8.0)
        with pymxs.attime(20):
            self.assertEqual(rt.objects[0].radius, 4.0)
        with pymxs.attime(40):
            self.assertEqual(rt.objects[0].radius, 16.0)
        with pymxs.attime(60):
            self.assertEqual(rt.objects[0].radius, 16.0)

        # verify import of animation as a subset of the full time range of original
        # starting inside animated range and ending outside of range
        rt.resetMaxFile(rt.Name("noprompt"))
        rt.units.SystemType = rt.Name("Millimeters")
        rt.units.SystemScale = 1

        TestGeneralUsdGeomXformableReader.Read_Called = False
        TestGeneralUsdGeomXformableReader.CanImportSphere_Called = False

        import_options.TimeMode = rt.Name("customRange")
        import_options.StartTimeCode = 10
        import_options.EndTimeCode = 60
        ret = rt.USDImporter.ImportFile(sphere_file_path, importOptions=import_options)
        
        self.assertTrue(ret)

        self.assertTrue(TestGeneralUsdGeomXformableReader.Read_Called)
        self.assertTrue(TestGeneralUsdGeomXformableReader.CanImportSphere_Called)
        self.assertEqual(rt.objects[0].name, 'sphere')
        sphere_radius_controller = rt.getSubAnim(rt.getSubAnim(rt.objects[0], 4), 1)
        self.assertEqual(sphere_radius_controller.keys.count, 3)
        self.assertEqual(str(sphere_radius_controller.keys), '#keys(10f, 20f, 40f)')
        with pymxs.attime(0):
            self.assertEqual(rt.objects[0].radius, 6.0)
        with pymxs.attime(10):
            self.assertEqual(rt.objects[0].radius, 6.0)
        with pymxs.attime(20):
            self.assertEqual(rt.objects[0].radius, 4.0)
        with pymxs.attime(40):
            self.assertEqual(rt.objects[0].radius, 16.0)
        with pymxs.attime(60):
            self.assertEqual(rt.objects[0].radius, 16.0)

        # verify import of animation as a subset of the full time range of original
        # inside two samples
        rt.resetMaxFile(rt.Name("noprompt"))
        rt.units.SystemType = rt.Name("Millimeters")
        rt.units.SystemScale = 1

        TestGeneralUsdGeomXformableReader.Read_Called = False
        TestGeneralUsdGeomXformableReader.CanImportSphere_Called = False

        import_options.TimeMode = rt.Name("customRange")
        import_options.StartTimeCode = 5
        import_options.EndTimeCode = 10
        ret = rt.USDImporter.ImportFile(sphere_file_path, importOptions=import_options)
        
        self.assertTrue(ret)

        self.assertTrue(TestGeneralUsdGeomXformableReader.Read_Called)
        self.assertTrue(TestGeneralUsdGeomXformableReader.CanImportSphere_Called)
        self.assertEqual(rt.objects[0].name, 'sphere')
        sphere_radius_controller = rt.getSubAnim(rt.getSubAnim(rt.objects[0], 4), 1)
        self.assertEqual(sphere_radius_controller.keys.count, 2)
        self.assertEqual(str(sphere_radius_controller.keys), '#keys(5f, 10f)')
        with pymxs.attime(0):
            self.assertEqual(rt.objects[0].radius, 7.0)
        with pymxs.attime(5):
            self.assertEqual(rt.objects[0].radius, 7.0)
        with pymxs.attime(10):
            self.assertEqual(rt.objects[0].radius, 6.0)
        with pymxs.attime(20):
            self.assertEqual(rt.objects[0].radius, 6.0)
        with pymxs.attime(40):
            self.assertEqual(rt.objects[0].radius, 6.0)

        # verify import of animation after the end of the animation range
        rt.resetMaxFile(rt.Name("noprompt"))
        rt.units.SystemType = rt.Name("Millimeters")
        rt.units.SystemScale = 1

        TestGeneralUsdGeomXformableReader.Read_Called = False
        TestGeneralUsdGeomXformableReader.CanImportSphere_Called = False

        import_options.TimeMode = rt.Name("customRange")
        import_options.StartTimeCode = 50
        import_options.EndTimeCode = 60
        ret = rt.USDImporter.ImportFile(sphere_file_path, importOptions=import_options)
        
        self.assertTrue(ret)

        self.assertTrue(TestGeneralUsdGeomXformableReader.Read_Called)
        self.assertTrue(TestGeneralUsdGeomXformableReader.CanImportSphere_Called)
        self.assertEqual(rt.objects[0].name, 'sphere')
        sphere_radius_controller = rt.getSubAnim(rt.getSubAnim(rt.objects[0], 4), 1)
        self.assertEqual(sphere_radius_controller.keys, None)
        with pymxs.attime(0):
            self.assertEqual(rt.objects[0].radius, 16.0)

        maxUsd.PrimReader.Unregister(TestGeneralUsdGeomXformableReader, "UsdGeomXformable")

    # test loading a reader plugin
    # the plugin loads to support a general 'UsdGeomGprim'; ancestor type to UsdGeomCone
    def test_prim_reader_from_json(self):
        pluginRegistry = Plug.Registry()
        
        scriptDir = os.path.dirname(os.path.realpath(__file__))
        plugsFound = pluginRegistry.RegisterPlugins(scriptDir + "/pythonReader/plugInfo.json")
        sys.path.insert(0, scriptDir + "/pythonReader/")

        plugin = pluginRegistry.GetPluginWithName('primReaderFromJson')
        self.assertNotEqual(None, plugin)
        # Limitation of the USD plugin system - no way to unload a plugin. 
        # This test assumes an "unloaded" state, so it will only run once per max session.
        if plugin.isLoaded:
            print("\nWarning test_prim_reader_from_json(), plugin is already loaded.")
            
        # Shared global used to make sure our Reader was called.
        import jsonReaderGlobals 
        jsonReaderGlobals.initialize()
                     
        rt.units.SystemType = rt.Name("Millimeters")
        rt.units.SystemScale = 1

        test_data_dir = os.path.dirname(__file__)
        cone_file_path = (test_data_dir + "\\data\\cone_native.usda")
        ret = rt.USDImporter.ImportFile(cone_file_path)
        self.assertTrue(ret)
        self.assertTrue(plugin.isLoaded)
        self.assertTrue(jsonReaderGlobals.jsonPrimReader_read_called)
        self.assertFalse(jsonReaderGlobals.jsonPrimReader_postread_called)
        sys.path.remove(scriptDir + "/pythonReader/")

    def test_prim_reader_instance(self):
        maxUsd.PrimReader.Unregister(TestReader, "UsdGeomCube")
        maxUsd.PrimReader.Register(TestReader, "UsdGeomCube")

        test_usd_file_path = self.output_prefix + "test_prim_reader_instance.usda"
        stage = Usd.Stage.CreateNew(test_usd_file_path)
        classCube = stage.CreateClassPrim("/classCube")
        refcube = UsdGeom.Cube.Define(stage, "/classCube/referenceCubeCube")
        instance1 = UsdGeom.Xform.Define(stage, "/CubePrimInstance1")
        instance2 = UsdGeom.Xform.Define(stage, "/CubePrimInstance2")

        instance1.GetPrim().GetInherits().AddInherit("/classCube")
        instance2.GetPrim().GetInherits().AddInherit("/classCube")

        instance1.GetPrim().SetInstanceable(True)
        instance2.GetPrim().SetInstanceable(True)

        stage.GetRootLayer().Save()

        TestReader.Read_Called = False
        TestReader.PostRead_Called = False
        TestReader.CanImport_Called = False
        TestReader.InstanceCreated_Called = False
        TestReader.InstancesCreated = []

        import_options = rt.UsdImporter.CreateOptions()
        ret = rt.USDImporter.ImportFile(test_usd_file_path, importOptions=import_options)
        self.assertTrue(ret)
        
        self.assertTrue(TestReader.Read_Called)
        self.assertTrue(TestReader.PostRead_Called)
        self.assertTrue(TestReader.CanImport_Called)
        self.assertTrue(TestReader.InstanceCreated_Called)
        self.assertEqual(len(TestReader.InstancesCreated), 2)

        maxUsd.PrimReader.Unregister(TestReader, "UsdGeomCube")

    def test_material_assignment(self):
        maxUsd.PrimReader.Unregister(TestMeshReader, "UsdGeomMesh")
        maxUsd.PrimReader.Register(TestMeshReader, "UsdGeomMesh")

        test_usd_file_path = self.output_prefix + "cube_wt_material.usda"
        
        # create meshes
        test_trans = rt.Box(name="phys_trans_cube")
        # create material for mesh
        mat_name = "physical_trans_mat"
        test_trans.material = rt.PhysicalMaterial(name=mat_name)
        # set material settings
        transparency_value = 0.9
        test_trans.material.transparency = transparency_value
        
        # export
        export_options = rt.USDExporter.CreateOptions()
        export_options.FileFormat = rt.Name("ascii")
        export_options.RootPrimPath = "/"
        ret = rt.USDExporter.ExportFile(
            test_usd_file_path,
            exportOptions=export_options,
            contentSource=rt.Name("nodeList"),
            nodeList=[test_trans],
        )
        self.assertTrue(ret)

        # reset 3ds Max scene state
        rt.resetMaxFile(rt.Name("noprompt"))

        # import what was just exported
        import_options = rt.UsdImporter.CreateOptions()
        import_options.PreferredMaterial = "maxUsdPreviewSurface"
        ret = rt.USDImporter.ImportFile(test_usd_file_path, importOptions=import_options)
        self.assertTrue(ret)

        # the special mesh reader appends '_usd' to the original prim name
        # this is done to make sure the reader override works
        # and that the imported material was done through the test mesh reader
        self.assertEqual(len(rt.objects), 1)
        max_node = rt.objects[0]
        self.assertEqual(rt.ClassOf(max_node), rt.Editable_Mesh)
        self.assertEqual(max_node.name, "phys_trans_cube_usd")
        
        self.assertEqual(len(rt.sceneMaterials), 1)
        max_material = rt.sceneMaterials[0]
        self.assertEqual(rt.ClassOf(max_material), rt.MaxUsdPreviewSurface)
        self.assertEqual(max_material.name, "physical_trans_mat")
        
        maxUsd.PrimReader.Unregister(TestMeshReader, "UsdGeomMesh")

    def test_enum_python_exposure(self):
        self.assertTrue(hasattr(maxUsd.PrimReader.ContextSupport, 'Unsupported'))
        self.assertTrue(hasattr(maxUsd.PrimReader.ContextSupport, 'Supported'))
        self.assertTrue(hasattr(maxUsd.PrimReader.ContextSupport, 'Fallback'))

rt.clearListener()
def run_tests():
    return unittest.TextTestRunner(stream=sys.stdout, verbosity=2).run(unittest.TestLoader().loadTestsFromTestCase(TestPrimReader))

if __name__ == '__main__':
    run_tests()