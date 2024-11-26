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
import maxUsd
from pymxs import runtime as rt
from pxr import UsdShade, Sdf
import pymxs
import traceback

class glTFMaterialWriter(maxUsd.ShaderWriter):
    # This python sample doesn't demonstrate animation, but an example can be found in the C++ sample.
    def Write(self):
        try: 
            # the 'GetMaterial()' method returns the anim handle on the material
            # need to fetch the anim on the handle to get back the pymxs.Material 
            material = rt.GetAnimByHandle(self.GetMaterial())

            # create the Shader prim
            nodeShader = UsdShade.Shader.Define(self.GetUsdStage(), self.GetUsdPath())
            nodeShader.CreateIdAttr("UsdPreviewSurface")
            # assign the created Shader prim as the USD prim for the ShaderWriter
            self.SetUsdPrim(nodeShader.GetPrim())
            col = (material.baseColor.r/255, material.baseColor.g/255, material.baseColor.b/255)
            inp = nodeShader.CreateInput("diffuseColor",Sdf.ValueTypeNames.Color3f)
            inp.Set(col)

        except Exception as e:
            # Quite useful to debug errors in a Python callback
            print('Write() - Error: %s' % str(e))
            print(traceback.format_exc())
        
    @classmethod
    def CanExport(cls, exportArgs):
        """
        Static class method required to determine if the class is handling the
        translation context required by the export job. The current class only
        handles converting materials to 'UsdPreviewSurface' elements.
        When this is called, we already know we're dealing with a glTF material,
        we registered this writer specificaly for it.
        """
        if exportArgs.GetConvertMaterialsTo() == "UsdPreviewSurface":
            return maxUsd.ShaderWriter.ContextSupport.Supported
        elif "UsdPreviewSurface" not in exportArgs.GetAllMaterialConversions():
            return maxUsd.ShaderWriter.ContextSupport.Fallback
        return maxUsd.ShaderWriter.ContextSupport.Unsupported
   
# Register the writer.        
maxUsd.ShaderWriter.Register(glTFMaterialWriter, "glTF Material")