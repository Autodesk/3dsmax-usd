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

from pxr import Usd, Sdf, Plug, UsdShade

# A shader writer used to test that all overriden methods are called into and 
# their results used. Each implemented methods sets a static variable used 
# to validate that the method is indeed called into by the system. A similar approach is 
# used to validate the received method parameters - for example
# TestShaderWriter.SphereNodeHandle is set from the test method to the exported node's 
# handle value.
class BasicShaderWriter(maxUsd.ShaderWriter):
        
    def Write(self):
        BasicShaderWriter.Write_Called = True
        return True

    def HasMaterialDependencies(self):
        BasicShaderWriter.HasMaterialDependencies_Called = True
        return True

    def GetSubMtlDependencies(self):
        BasicShaderWriter.GetSubMtlDependencies_Called = True
        return []

    def PostWrite(self):
        BasicShaderWriter.PostWrite_Called = True
    
    @classmethod
    def CanExport(cls, exportArgs):
        BasicShaderWriter.CanExport_Called = True

        try: 
            # Test export options access.
            options = exportArgs
            rt.assert_equal(True, options.GetBakeObjectOffsetTransform())
            rt.assert_equal(False, options.GetChannelPrimvarAutoExpandType(0))
            rt.assert_equal("vertexColor", options.GetChannelPrimvarName(0))
            rt.assert_equal(maxUsd.PrimvarType.Color3fArray, options.GetChannelPrimvarType(0))
            rt.assert_equal("UsdPreviewSurface", options.GetConvertMaterialsTo())
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
            rt.assert_equal("/root", str(options.GetRootPrimPath()))           
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
        except Exception as e:
            # Quite useful to debug errors in a Python callback
            print('Write() - Error: %s' % str(e))
            print(traceback.format_exc())
            rt.assert_true(False, message="Python exposure of Export options not working as expected")
        return maxUsd.ShaderWriter.ContextSupport.Supported        

# A shader writer used to test the registration of the Writer and the IsMaterialTargetAgnostic method in python.
class AgnosticShaderWriter(maxUsd.ShaderWriter):
    # Not really what we should do with a Shell material, but just for the purpose of testing :)
    def Write(self):
        # create the Shader prim
        nodeShader = UsdShade.Shader.Define(self.GetUsdStage(), self.GetUsdPath())
        nodeShader.CreateIdAttr("UsdPreviewSurface")
        # assign the created Shader prim as the USD prim for the ShaderWriter
        self.SetUsdPrim(nodeShader.GetPrim())
        col = (1.0, 0.0, 0.0)
        inp = nodeShader.CreateInput("diffuseColor",Sdf.ValueTypeNames.Color3f)
        inp.Set(col)
    
    def HasMaterialDependencies(self):
        return False
        
    def PostWrite(self):
        # verify the map of exported materials is functional
        rt.assert_equal(str("/root/mtl/Shell"), str(self.GetMaterialsToPrimsMap()[self.GetMaterial()]))
    
    @classmethod
    def CanExport(cls, exportArgs):
        return maxUsd.ShaderWriter.ContextSupport.Supported
    
    @classmethod
    def IsMaterialTargetAgnostic(cls):
        return True

# Test class.
class TestShaderWriter(unittest.TestCase):

    def setUp(self):
        rt.resetMaxFile(rt.Name("noprompt"))
        self.output_prefix = usd_test_helpers.standard_output_prefix("EXPORT_SHADER_WRITER_TEST_")

    def tearDown(self):
        pass

    def test_shader_writer(self):
        maxUsd.ShaderWriter.Register(BasicShaderWriter, "Shell Material")
        # Create a Box to export.
        boxNode = rt.Box()
        boxNode.mat = rt.Shell_Material()
        
        test_usd_file_path = self.output_prefix + "test_shader_writer.usda"
        
        
        BasicShaderWriter.CanExport_Called = False
        BasicShaderWriter.Write_Called = False
        BasicShaderWriter.HasMaterialDependencies_Called = False
        BasicShaderWriter.GetSubMtlDependencies_Called = False
        BasicShaderWriter.PostWrite_Called = False
        
        export_options = rt.UsdExporter.CreateOptions()
        export_options.FileFormat = rt.Name("ascii")
        export_options.RootPrimPath = "/root"
        ret = rt.USDExporter.ExportFile(test_usd_file_path, exportOptions=export_options)
        
        self.assertTrue(ret)
        
        self.assertTrue(BasicShaderWriter.CanExport_Called)
        self.assertTrue(BasicShaderWriter.Write_Called)
        self.assertTrue(BasicShaderWriter.HasMaterialDependencies_Called)
        self.assertTrue(BasicShaderWriter.GetSubMtlDependencies_Called)
        self.assertTrue(BasicShaderWriter.PostWrite_Called)
        
        maxUsd.ShaderWriter.Unregister(BasicShaderWriter, "Shell Material")
    
    def test_shader_writer_from_json(self):
        pluginRegistry = Plug.Registry()
        
        scriptDir = os.path.dirname(os.path.realpath(__file__))
        plugsFound = pluginRegistry.RegisterPlugins(scriptDir + "/pythonShaderWriter/plugInfo.json")
        sys.path.insert(0, scriptDir + "/pythonShaderWriter/")

        plugin = pluginRegistry.GetPluginWithName('shaderWriterFromJson')
        self.assertNotEqual(None, plugin)
        # Limitation of the USD plugin system - no way to unload a plugin. 
        # This test assumes an "unloaded" state, so it will only run once per max session.
        if not plugin.isLoaded:
            plugin.Load()
            # Create a Box to export.
            boxNode = rt.Box()
            boxNode.mat = rt.matteShadow()
            test_usd_file_path = self.output_prefix + "test_json_shader_writer.usda"
            export_options = rt.UsdExporter.CreateOptions()
            export_options.FileFormat = rt.Name("ascii")
            export_options.RootPrimPath = ""
            
            # Shared global used to make sure our writer was called.
            import jsonShaderWriterGlobals 
            jsonShaderWriterGlobals.initialize()
                        
            ret = rt.USDExporter.ExportFile(test_usd_file_path, exportOptions=export_options)
            self.assertTrue(ret)
            self.assertTrue(jsonShaderWriterGlobals.jsonShaderWriter_write_called)
        else:
            print("\nWarning test_shader_writer_from_json() skipped, plugin is already loaded.")
        sys.path.remove(scriptDir + "/pythonShaderWriter/")
    
    def test_enum_python_exposure(self):
        self.assertTrue(hasattr(maxUsd.ShaderWriter.ContextSupport, 'Unsupported'))
        self.assertTrue(hasattr(maxUsd.ShaderWriter.ContextSupport, 'Supported'))
        self.assertTrue(hasattr(maxUsd.ShaderWriter.ContextSupport, 'Fallback'))

    # Test that when exporting a material that has been register as Target Agnostic, it does indeed not export multiple
    # targets and just the Material + its shader.
    def test_agnostic_material(self):
        maxUsd.ShaderWriter.Register(AgnosticShaderWriter, "Shell Material")
        # Create a Box to export.
        boxNode = rt.Box()
        mat = rt.Shell_Material()
        mat.name = "Shell"
        boxNode.mat = mat

        test_usd_file_path = self.output_prefix + "test_agnostic_shader_writer.usda"
        export_options = rt.UsdExporter.CreateOptions()
        export_options.FileFormat = rt.Name("ascii")
        export_options.RootPrimPath = "/root"
        materialTargets = rt.Array("UsdPreviewSurface", "MaterialX")
        export_options.AllMaterialTargets = materialTargets
        ret = rt.USDExporter.ExportFile(test_usd_file_path, exportOptions=export_options)
        self.assertTrue(ret)

        stage = Usd.Stage.Open(test_usd_file_path)
        self.assertIsInstance(stage, Usd.Stage)
        shade = UsdShade.Shader.Get(stage, "/root/mtl/Shell/Shell")
        colorInput = shade.GetInput("diffuseColor")
        color = colorInput.Get()
        self.assertEqual(color, (1, 0, 0))

        maxUsd.ShaderWriter.Unregister(AgnosticShaderWriter, "Shell Material")
        
rt.clearListener()
def run_tests():
    return unittest.TextTestRunner(stream=sys.stdout, verbosity=2).run(unittest.TestLoader().loadTestsFromTestCase(TestShaderWriter))

if __name__ == '__main__':
    run_tests()