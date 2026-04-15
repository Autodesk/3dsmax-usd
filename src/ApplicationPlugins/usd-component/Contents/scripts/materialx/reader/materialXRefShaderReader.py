#
# Copyright 2025 Autodesk
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
from pxr import Usd

import traceback

class materialXRefShaderReader(maxUsd.ShaderReader):

    @classmethod
    def CanImport(cls, importArgs):
        """
        Static class method required to determine if the class is handling the
        translation context required by the import job. The current class only
        handles converting materials from 'ND_standard_surface_surfaceshader' 
        or 'ND_open_pbr_surface_surfaceshader' elements.
        """
        max_version = rt.maxversion()
        is_2024_or_higher = max_version[0] >= 26000  # 3ds Max 2024 version number
        if not is_2024_or_higher:
            return maxUsd.ShaderReader.ContextSupport.Unsupported
        
        if importArgs.GetPreferredMaterial() == "none" or \
           importArgs.GetPreferredMaterial() == "MaterialX":
            return maxUsd.ShaderReader.ContextSupport.Fallback
        return maxUsd.ShaderReader.ContextSupport.Unsupported

    def Read(self):
        """Main import function that runs when the applicable material gets hit"""
        try:
            
            shader = UsdShade.Shader(self.GetUsdPrim())
            parentPrim = self.GetUsdPrim().GetParent()
            refMatPrim = parentPrim
            matName = parentPrim.GetName()
            # Assumed structure that comes out of UsdMtlx module
            while parentPrim.GetName() != "Materials":
                parentPrim = parentPrim.GetParent()
                if not parentPrim.IsValid():
                    prim_path = self.GetUsdPrim().GetPath()
                    raise Exception(f"Unexpected prim structure for prim: {prim_path}")
            parentPrim = parentPrim.GetParent()
            arcs = Usd.PrimCompositionQuery.GetDirectReferences(parentPrim).GetCompositionArcs()
            mtlFilePath = None
            for arc in arcs:
                if arc.GetTargetLayer().identifier.endswith(".mtlx"):
                    mtlFilePath = arc.GetTargetLayer().resolvedPath.GetPathString()
                    break
            if mtlFilePath is None:
                prim_path = self.GetUsdPrim().GetPath()
                raise Exception(f"No MaterialX file found for prim: {prim_path}")

            mat = rt.MaterialXMat()
            mat.importMaterial(mtlFilePath, matName=matName)
            if mat:
                self.RegisterCreatedMaterial(shader.GetPath(), rt.GetHandleByAnim(mat))
                
        except Exception as e:
            # Quite useful to debug errors in a Python callback
            print('MaterialXRefShaderReader Read() - Warning: %s' % str(e))

        return True


# register the ShaderReader to use for the supported materials
maxUsd.ShaderReader.Register(materialXRefShaderReader, "ND_standard_surface_surfaceshader")
maxUsd.ShaderReader.Register(materialXRefShaderReader, "ND_open_pbr_surface_surfaceshader")

maxUsd.ShadingModeRegistry.RegisterImportConversion( \
        "MaterialXReference", \
        "mtlx", \
        "MaterialX Reference", \
        "Resolves a MaterialX Reference to a 3ds Max material" )
