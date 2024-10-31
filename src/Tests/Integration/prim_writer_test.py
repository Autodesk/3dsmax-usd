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

from pxr import Usd, UsdGeom, Sdf, Plug

# A prim writer used to test that all overriden methods are called into and 
# their results used. Each implemented methods sets a static variable used 
# to validate that the method is indeed called into by the system. A similar approach is 
# used to validate the received method parameters - for example
# TestWriter.SphereNodeHandle is set from the test method to the exported node's 
# handle value.
class TestWriter(maxUsd.PrimWriter):
    
    def GetPrimType(self):
        nodeHandle = self.GetNodeHandle()
        rt.assert_equal(TestWriter.SphereNodeHandle, nodeHandle)
        TestWriter.GetPrimType_Called = True
        # Writer is writing to a sphere.
        return "Sphere"
        
    def GetPrimName(self, suggestedName):
        nodeHandle = self.GetNodeHandle()
        rt.assert_equal(TestWriter.SphereNodeHandle, nodeHandle)
        # Because we specify "Always" as xform split requirement, expect the object suffix.
        rt.assert_equal("Sphere001_TestSuffix", suggestedName)
        TestWriter.GetPrimName_Called = True           
        # Customize the name by just appening somethng to what the system suggests.
        return suggestedName + "_foo"
        
    def GetObjectPrimSuffix(self):
        nodeHandle = self.GetNodeHandle()
        rt.assert_equal(TestWriter.SphereNodeHandle, nodeHandle)
        TestWriter.GetObjectPrimSuffix_Called = True        
        return "TestSuffix"
        
    def HandlesObjectOffsetTransform(self):
        TestWriter.HandleObjectOffsetTransform_Called = True
        # This means we should expect no xformOp on the prim when we receive it in the write.
        # Even if the node's object has an offset...
        return True
        
    def RequiresXformPrim(self):
        nodeHandle = self.GetNodeHandle()
        rt.assert_equal(TestWriter.SphereNodeHandle, nodeHandle)
        TestWriter.RequireXformPrim_Called = True        
        return maxUsd.PrimWriter.XformSplitRequirement.Always
    
    def RequiresMaterialAssignment(self):
        nodeHandle = self.GetNodeHandle()
        rt.assert_equal(TestWriter.SphereNodeHandle, nodeHandle)
        TestWriter.RequiresMaterialAssignment_Called = True                
        return maxUsd.PrimWriter.MaterialAssignRequirement.Default
        
    def RequiresInstancing(self):
        TestWriter.RequiresInstancing_Called = True                
        return maxUsd.PrimWriter.InstancingRequirement.Default
    
    def Write(self, prim, applyObjectOffset, time):
        nodeHandle = self.GetNodeHandle()
        rt.assert_true(prim.IsA(UsdGeom.Xformable))
        
        nodesToPrims = self.GetNodesToPrims()
        keys = list(nodesToPrims.keys())
        rt.assert_equal(True, nodeHandle in keys)
        # This writer splits the node into an xform + object prim.
        # The prim in the map is the root prim for the node, but the writer receives
        # the object prim. So expect the parent path of this prim in the map.
        rt.assert_equal(str(prim.GetPath().GetParentPath()), str(nodesToPrims[nodeHandle]))

        xformable = UsdGeom.Xformable(prim)
        ops = xformable.GetOrderedXformOps()
        rt.assert_equal(0, len(ops))
        rt.assert_true(applyObjectOffset)
        # Received prim should be a sphere (GetPrimType() -> "Sphere")
        rt.assert_true(prim.IsA(UsdGeom.Sphere))
        # We requested an xform prim, specified a suffix, and appended foo to the suggested name.
        rt.assert_equal(str(prim.GetPath()), '/EXPORT_PRIM_WRITER_TEST_test_dummy_sphere_writer/Sphere001/Sphere001_TestSuffix_foo')
        rt.assert_equal(TestWriter.SphereNodeHandle, nodeHandle)
        TestWriter.Write_Called = True
        return True
    
    def PostExport(self, prim):
        rt.assert_equal(str(prim.GetPath()), '/EXPORT_PRIM_WRITER_TEST_test_dummy_sphere_writer/Sphere001/Sphere001_TestSuffix_foo')
        TestWriter.PostExport_Called = True
        return True
    
    def GetValidityInterval(self, time):
        val = maxUsd.PrimWriter.GetValidityInterval(self, time)
        TestWriter.GetValidityInterval_Called = True
        return val

    @classmethod
    def CanExport(cls, nodeHandle, exportArgs):
        TestWriter.CanExport_Called = True
        node = rt.maxOps.getNodeByHandle(nodeHandle)
        if rt.classOf(node) == rt.Sphere:
            return maxUsd.PrimWriter.ContextSupport.Supported
        return maxUsd.PrimWriter.ContextSupport.Unsupported
        

# Simplest possible writer.
# As it doesn't support anything, it just validating that
# the execution of the base method of Write() can be called
# correctly.
class TestMinimalWriter(maxUsd.PrimWriter):
    @classmethod
    def CanExport(cls, nodeHandle, exportArgs):
        return maxUsd.PrimWriter.ContextSupport.Unsupported

# Dummy writer to test the other default implementations. This 
# writer tells the system it can convert everything - this way 
# we ensure all the other base class methods are called into (as
# the are not implemented here).
class TestMinimalSupportedWriter(maxUsd.PrimWriter):
    @classmethod
    def CanExport(cls, nodeHandle, exportArgs):
        TestMinimalSupportedWriter.CanExport_Called = True
        return maxUsd.PrimWriter.ContextSupport.Supported

# Dummy to test export options exposure to the writers.
class DummyExportArgsWriter(maxUsd.PrimWriter):        
    @classmethod
    def CanExport(cls, nodeHandle, exportArgs):
        try: 
            # Test export options access.
            options = exportArgs
            rt.assert_equal(True, options.GetBakeObjectOffsetTransform())
            rt.assert_equal(False, options.GetChannelPrimvarAutoExpandType(0))
            rt.assert_equal("vertexColor", options.GetChannelPrimvarName(0))
            rt.assert_equal(maxUsd.PrimvarType.Color3fArray, options.GetChannelPrimvarType(0))
            rt.assert_equal("", options.GetConvertMaterialsTo())
            rt.assert_equal(0, options.GetEndFrame())
            rt.assert_equal(0, options.GetStartFrame())
            rt.assert_equal(1, options.GetSamplesPerFrame())
            rt.assert_equal(maxUsd.TimeMode.CurrentFrame, options.GetTimeMode())
            rt.assert_equal(maxUsd.LogLevel.Off, options.GetLogLevel())
            rt.assert_equal(maxUsd.MeshFormat.FromScene, options.GetMeshFormat())
            rt.assert_equal(maxUsd.FileFormat.ASCII , options.GetFileFormat())
            rt.assert_equal(maxUsd.NormalsMode.AsPrimvar, options.GetNormalsMode())
            rt.assert_equal(False, options.GetOpenInUsdview())
            rt.assert_equal(False, options.GetPreserveEdgeOrientation())
            rt.assert_equal("/EXPORT_PRIM_WRITER_TEST_test_export_args_exposure_in_writer", str(options.GetRootPrimPath()))           
            rt.assert_equal("useRegistry", options.GetShadingMode())
            maxver = rt.maxversion()
            if maxver[0] >= 26000:  # 3ds Max 2024 and up
                rt.assert_equal(maxUsd.MtlSwitcherExportStyle.AsVariantSets, options.GetMtlSwitcherExportStyle())
            rt.assert_equal(True, options.GetTranslateCameras())
            rt.assert_equal(True, options.GetTranslateHidden())
            rt.assert_equal(True, options.GetTranslateLights())
            rt.assert_equal(True, options.GetTranslateMaterials())
            rt.assert_equal(True, options.GetTranslateMeshes())
            rt.assert_equal(True, options.GetTranslateShapes())
            rt.assert_equal(maxUsd.UpAxis.Z, options.GetUpAxis())
            rt.assert_equal(True, options.GetUsdStagesAsReferences())
            rt.assert_equal(False, options.GetUseUSDVisibility())
            rt.assert_equal(str({'UsdPreviewSurface'}), str(options.GetAllMaterialConversions()))
            #The token in the default option value should be resolved at this point.
            rt.assert_equal("EXPORT_PRIM_WRITER_TEST_test_export_args_exposure_in_writer_mtl.usda", options.GetMaterialLayerPath())
        except Exception as e:
            # Quite useful to debug errors in a Python callback
            print('Write() - Error: %s' % str(e))
            print(traceback.format_exc())
            rt.assert_true(False, message="Python exposure of Export options not working as expected")
            
        return maxUsd.PrimWriter.ContextSupport.Supported
        
    def Write(self, prim, applyObjectOffset, time):
            # Test the exported Filename and stage
            rt.assert_equal(DummyExportArgsWriter.ExpectedPath, self.GetFilename())
            rt.assert_not_equal(None, self.GetUsdStage())

# Test class.
class TestPrimWriter(unittest.TestCase):

    def setUp(self):
        rt.resetMaxFile(rt.Name("noprompt"))
        self.output_prefix = usd_test_helpers.standard_output_prefix("EXPORT_PRIM_WRITER_TEST_")

    def tearDown(self):
        pass

    def test_dummy_sphere_writer(self):
        maxUsd.PrimWriter.Unregister(TestWriter, "dummySphereWriter")
        maxUsd.PrimWriter.Register(TestWriter, "dummySphereWriter")
        # Create a sphere to export.
        sphereNode = rt.Sphere()
        sphereNode.objectOffsetPos = rt.Point3(0, 0, 10)
        
        TestWriter.SphereNodeHandle = sphereNode.handle
        
        test_usd_file_path = self.output_prefix + "test_dummy_sphere_writer.usda"
        
        
        TestWriter.GetPrimType_Called = False
        TestWriter.GetPrimName_Called = False
        TestWriter.GetObjectPrimSuffix_Called = False
        TestWriter.HandleObjectOffsetTransform_Called = False
        TestWriter.Write_Called = False
        TestWriter.PostExport_Called = False
        TestWriter.GetValidityInterval_Called = False
        TestWriter.CanExport_Called = False
        TestWriter.RequireXformPrim_Called = False
        TestWriter.RequiresMaterialAssignment_Called = False
        
        export_options = rt.UsdExporter.CreateOptions()
        export_options.FileFormat = rt.Name("ascii")
        export_options.RootPrimPath = ""
        ret = rt.USDExporter.ExportFile(test_usd_file_path, exportOptions=export_options)
        
        self.assertTrue(ret)
        
        self.assertTrue(TestWriter.GetPrimType_Called)
        self.assertTrue(TestWriter.GetPrimName_Called)
        self.assertTrue(TestWriter.GetObjectPrimSuffix_Called)
        self.assertTrue(TestWriter.HandleObjectOffsetTransform_Called)
        self.assertTrue(TestWriter.GetValidityInterval_Called)
        self.assertTrue(TestWriter.Write_Called)
        self.assertTrue(TestWriter.PostExport_Called)
        self.assertTrue(TestWriter.CanExport_Called)
        self.assertTrue(TestWriter.RequireXformPrim_Called)      
        self.assertTrue(TestWriter.RequiresMaterialAssignment_Called)
        
        maxUsd.PrimWriter.Unregister(TestWriter, "dummySphereWriter")
        
    def test_dummy_sphere_writer_not_used(self):
        maxUsd.PrimWriter.Unregister(TestWriter, "dummySphereWriter")
        maxUsd.PrimWriter.Register(TestWriter, "dummySphereWriter")
        
        # Create a sphere to export.
        boxNode = rt.Box()
                
        test_usd_file_path = self.output_prefix + "test_dummy_sphere_writer_not_used.usda"
        
        TestWriter.GetPrimType_Called = False
        TestWriter.GetPrimName_Called = False
        TestWriter.GetObjectPrimSuffix_Called = False
        TestWriter.GetValidityInterval_Called = False
        TestWriter.HandleObjectOffsetTransform_Called = False
        TestWriter.Write_Called = False
        TestWriter.PostExport_Called = False
        TestWriter.CanExport_Called = False
        TestWriter.RequireXformPrim_Called = False
        TestWriter.RequiresMaterialAssignment_Called = False
        
        export_options = rt.UsdExporter.CreateOptions()
        export_options.FileFormat = rt.Name("ascii")
        export_options.RootPrimPath = ""
        ret = rt.USDExporter.ExportFile(test_usd_file_path, exportOptions=export_options)
                
        self.assertTrue(ret)
        # Can convert is called (Can the writer convert the box?)
        self.assertTrue(TestWriter.CanExport_Called)
        # All other methods should not be called.
        self.assertFalse(TestWriter.GetPrimType_Called)
        self.assertFalse(TestWriter.GetPrimName_Called)
        self.assertFalse(TestWriter.GetObjectPrimSuffix_Called)
        self.assertFalse(TestWriter.GetValidityInterval_Called)
        self.assertFalse(TestWriter.HandleObjectOffsetTransform_Called)
        self.assertFalse(TestWriter.Write_Called)
        self.assertFalse(TestWriter.PostExport_Called)
        self.assertFalse(TestWriter.RequireXformPrim_Called)
        self.assertFalse(TestWriter.RequiresMaterialAssignment_Called)
        
        maxUsd.PrimWriter.Unregister(TestWriter, "dummySphereWriter")

    def test_minimal_writer(self):  
        maxUsd.PrimWriter.Unregister(TestMinimalWriter, "dummyMinimalWriter")
        maxUsd.PrimWriter.Register(TestMinimalWriter, "dummyMinimalWriter")

        # Create a sphere to export.
        boxNode = rt.Box()
                
        test_usd_file_path = self.output_prefix + "test_minimal_writer.usda"
        export_options = rt.UsdExporter.CreateOptions()
        export_options.FileFormat = rt.Name("ascii")
        export_options.RootPrimPath = ""
        ret = rt.USDExporter.ExportFile(test_usd_file_path, exportOptions=export_options)
                
        self.assertTrue(ret)
        maxUsd.PrimWriter.Unregister(TestMinimalWriter, "dummyMinimalWriter")
        
    def test_minimal_supported_writer(self):
        maxUsd.PrimWriter.Unregister(TestMinimalSupportedWriter, "dummyMinimalSupportedlWriter")
        maxUsd.PrimWriter.Register(TestMinimalSupportedWriter, "dummyMinimalSupportedlWriter")
        
        boxNode = rt.Box()
        test_usd_file_path = self.output_prefix + "test_minimal_supported_writer.usda"        
        
        TestMinimalSupportedWriter.CanExport_Called = False
        
        export_options = rt.UsdExporter.CreateOptions()
        export_options.FileFormat = rt.Name("ascii")
        export_options.RootPrimPath = ""
        ret = rt.USDExporter.ExportFile(test_usd_file_path, exportOptions=export_options)
        
        self.assertTrue(ret)
        # Can convert is called (Can the writer conver the box?)
        self.assertTrue(TestMinimalSupportedWriter.CanExport_Called)
        
        exportedStage = Usd.Stage.Open(test_usd_file_path)
        boxPrim = exportedStage.GetPrimAtPath("/EXPORT_PRIM_WRITER_TEST_test_minimal_supported_writer/Box001")
        self.assertTrue(boxPrim.IsValid())
        self.assertTrue(boxPrim.IsA(UsdGeom.Xform))
        maxUsd.PrimWriter.Unregister(TestMinimalSupportedWriter, "dummyMinimalSupportedlWriter")
        
    def test_export_args_exposure_in_writer(self):
        maxUsd.PrimWriter.Unregister(DummyExportArgsWriter, "dummyExportArgsWriter")
        maxUsd.PrimWriter.Register(DummyExportArgsWriter, "dummyExportArgsWriter")
        # Create a sphere to export.
        sphereNode = rt.Sphere()    
        test_usd_file_path = self.output_prefix + "test_export_args_exposure_in_writer.usda"
        DummyExportArgsWriter.ExpectedPath = test_usd_file_path
        export_options = rt.UsdExporter.CreateOptions()
        export_options.FileFormat = rt.Name("ascii")
        export_options.RootPrimPath = ""
        ret = rt.USDExporter.ExportFile(test_usd_file_path, exportOptions=export_options)
        self.assertTrue(ret)
    
        maxUsd.PrimWriter.Unregister(DummyExportArgsWriter, "dummyExportArgsWriter")
    
    def test_prim_writer_from_json(self):
        pluginRegistry = Plug.Registry()
        
        scriptDir = os.path.dirname(os.path.realpath(__file__))
        plugsFound = pluginRegistry.RegisterPlugins(scriptDir + "/pythonPrimWriter/plugInfo.json")
        sys.path.insert(0, scriptDir + "/pythonPrimWriter/")

        plugin = pluginRegistry.GetPluginWithName('primWriterFromJson')
        self.assertNotEqual(None, plugin)
        # Limitation of the USD plugin system - no way to unload a plugin. 
        # This test assumes an "unloaded" state, so it will only run once per max session.
        if not plugin.isLoaded:
            plugin.Load()            
            # Create a Cone to export.
            sphereNode = rt.Cone()    
            test_usd_file_path = self.output_prefix + "test_export_args_exposure_in_writer.usda"
            export_options = rt.UsdExporter.CreateOptions()
            export_options.FileFormat = rt.Name("ascii")
            export_options.RootPrimPath = ""
            
            # Shared global used to make sure our writer was called.
            import jsonWriterGlobals 
            jsonWriterGlobals.initialize()
                        
            ret = rt.USDExporter.ExportFile(test_usd_file_path, exportOptions=export_options)
            self.assertTrue(ret)
            self.assertTrue(jsonWriterGlobals.jsonPrimWriter_write_called)
        else:
            print("\nWarning test_prim_writer_from_json() skipped, plugin is already loaded.")
        sys.path.remove(scriptDir + "/pythonPrimWriter/")
    
    def test_enum_python_exposure(self):
        self.assertTrue(hasattr(maxUsd.PrimWriter.ContextSupport, 'Unsupported'))
        self.assertTrue(hasattr(maxUsd.PrimWriter.ContextSupport, 'Supported'))
        self.assertTrue(hasattr(maxUsd.PrimWriter.ContextSupport, 'Fallback'))
        
        self.assertTrue(hasattr(maxUsd.PrimWriter.XformSplitRequirement, 'Always'))
        self.assertTrue(hasattr(maxUsd.PrimWriter.XformSplitRequirement, 'ForOffsetObjects'))
        self.assertTrue(hasattr(maxUsd.PrimWriter.XformSplitRequirement, 'Never'))
        
        self.assertTrue(hasattr(maxUsd.PrimWriter.MaterialAssignRequirement, 'Default'))
        self.assertTrue(hasattr(maxUsd.PrimWriter.MaterialAssignRequirement, 'NoAssignment'))
        
        self.assertTrue(hasattr(maxUsd.PrimWriter.InstancingRequirement, 'Default'))
        self.assertTrue(hasattr(maxUsd.PrimWriter.InstancingRequirement, 'NoInstancing'))
        
rt.clearListener()
def run_tests():
    return unittest.TextTestRunner(stream=sys.stdout, verbosity=2).run(unittest.TestLoader().loadTestsFromTestCase(TestPrimWriter))

if __name__ == '__main__':
    run_tests()