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

# maxUsd - the Python module for the 3ds Max USD component
import maxUsd

from pymxs import runtime as rt
from pxr import UsdShade

import traceback

# the Python methods for material translation to UsdPreviewSurface
import usd_material_writer

class DefaultShaderWriter(maxUsd.ShaderWriter):
    """
    Default Shader Writer class handling the material translation for
    the 3ds Max default materials
    inherits from the base class ShaderWriter from the 3ds Max USD component
    """
    
    # We keep a map of all written usd texture prims, to their associated 3dsMax maps.
    # We should clear this map between USD exports, but there is no great way to know this 
    # currently. We will clear the map when the stage changes, which necessarily implies
    # we are in a different export.
    _stage = None
    _usd_shade_to_max_map = {}

    @classmethod
    def CanExport(cls, exportArgs):
        """
        Static class method required to determine if the class is handling the
        translation context required by the export job. The current class only
        handles converting materials to 'UsdPreviewSurface' elements.
        """
        if exportArgs.GetConvertMaterialsTo() == "UsdPreviewSurface":
            return maxUsd.ShaderWriter.ContextSupport.Supported
        return maxUsd.ShaderWriter.ContextSupport.Unsupported

    def Write(self):
        """Main export function that runs when the applicable material gets hit"""
        try:
            # If the stage changes, it is a different export, clear _usd_shade_to_max_map.
            if self.GetUsdStage() != DefaultShaderWriter._stage:
                DefaultShaderWriter._stage = self.GetUsdStage()
                DefaultShaderWriter._usd_shade_to_max_map = {}
            
            # the 'GetMaterial()' method returns the anim handle on the material
            # need to fetch the anim on the handle to get back the pymxs.Material back
            material = rt.GetAnimByHandle(self.GetMaterial())

            # create the Shade prim
            nodeShader = UsdShade.Shader.Define(self.GetUsdStage(), self.GetUsdPath())
            nodeShader.CreateIdAttr("UsdPreviewSurface")
            # assign the created Shade prim as the USD prim for the ShaderWriter
            self.SetUsdPrim(nodeShader.GetPrim())
            
            usd_material_writer.export_material(material, self.GetUsdStage(), nodeShader, self.GetUsdPath(), self.GetFilename(), self.GetExportArgs(), DefaultShaderWriter._usd_shade_to_max_map, self.IsUSDZFile())

        except Exception as e:
            # Quite useful to debug errors in a Python callback
            print('Write() - Error: %s' % str(e))
            print(traceback.format_exc())


# register the ShaderWriter to use for the supported materials
maxUsd.ShaderWriter.Register(DefaultShaderWriter, rt.PhysicalMaterial.nonLocalizedName) # or use the actual non-localized string "Physical Material"
maxUsd.ShaderWriter.Register(DefaultShaderWriter, "PBR Material (Metal/Rough)")
maxUsd.ShaderWriter.Register(DefaultShaderWriter, "PBR Material (Spec/Gloss)")
maxUsd.ShaderWriter.Register(DefaultShaderWriter, "USD Preview Surface")
