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

# the Python methods for material translation from UsdPreviewSurface
import usd_material_reader

_material_import_options = {
    "texture_target_id": usd_material_reader._uberbitmapfile
}

class DefaultShaderReader(maxUsd.ShaderReader):
    """
    Default Shader Reader class handling the material translation to 3ds Max default materials
    inherits from the base class ShaderReader from the 3ds Max USD component
    """
    
    @classmethod
    def CanImport(cls, importArgs):
        """
        Static class method required to determine if the class is handling the
        translation context required by the import job. The current class only
        handles converting materials from 'UsdPreviewSurface' elements.
        """
        if importArgs.GetPreferredMaterial() == "none" or \
           importArgs.GetPreferredMaterial() == "maxUsdPreviewSurface" or \
           importArgs.GetPreferredMaterial() == "pbrMetalRough" or \
           importArgs.GetPreferredMaterial() == "physicalMaterial":
            return maxUsd.ShaderReader.ContextSupport.Fallback
        return maxUsd.ShaderReader.ContextSupport.Unsupported

    def Read(self):
        """Main import function that runs when the applicable material gets hit"""
        try:
            import_options = self.GetArgs()
            material_import_options = dict(_material_import_options)
            preferredMaterial = import_options.GetPreferredMaterial()
            if preferredMaterial == "none" or preferredMaterial == "maxUsdPreviewSurface":
                material_import_options['material_target_id'] = "MaxUsdPreviewSurface"
            elif preferredMaterial == "pbrMetalRough":
                material_import_options['material_target_id'] = "PBRMetalRough"
            elif preferredMaterial == "physicalMaterial":
                material_import_options['material_target_id'] = "PhysicalMaterial"
            else:
                material_import_options['material_target_id'] = preferredMaterial
            material_import_options['usd_import_options'] = import_options
            shader = UsdShade.Shader(self.GetUsdPrim())
            mat = usd_material_reader.output_max_material(shader, material_import_options, {})
            if mat:
                self.RegisterCreatedMaterial(shader.GetPath(), rt.GetHandleByAnim(mat))

        except Exception as e:
            # Quite useful to debug errors in a Python callback
            print('Read() - Error: %s' % str(e))
            print(traceback.format_exc())

        return True


# register the ShaderReader to use for the supported materials
maxUsd.ShaderReader.Register(DefaultShaderReader, "UsdPreviewSurface")

maxUsd.ShadingModeRegistry.RegisterImportConversion( \
        "UsdPreviewSurface", \
        UsdShade.Tokens.universalRenderContext, \
        "USD Preview Surface", \
        "Fetches back a UsdPreviewSurface material dumped as UsdShade" )

# preferred 3ds Max specific materials
#   - maxUsdPreviewSurface -> "MaxUsdPreviewSurface"
#   - pbrMetalRough -> "PBRMetalRough"
#   - physicalMaterial -> "PhysicalMaterial"
