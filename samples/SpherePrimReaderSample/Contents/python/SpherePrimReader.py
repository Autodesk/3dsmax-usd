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
from pxr import UsdGeom
from pxr import Gf as pyGf
import pymxs
import traceback

class SphereReader(maxUsd.PrimReader):
    '''
    Prim reader for importing a USD native sphere into 3ds Max
    '''

    @classmethod
    def CanImport(cls, args, prim):
        '''
        This method is responsible of telling the import process if it can import the current prim.
        In the case of this sample, the PrimReaderRegistry already filters readers based on the
        prim type they registered themselves with. Here, we want to make sure the default fallback
        reader is superceeded by this one by returning 'Supported'.
        '''
        return maxUsd.PrimReader.ContextSupport.Supported

    def Read(self):
        try: 
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

        except Exception as e:
            # Quite useful to debug errors in a Python callback
            print('Read() - Error: %s' % str(e))
            print(traceback.format_exc())
            return False
   
# Register the reader.
# First argument is the reader class, second argument is the USD type name handled by the reader
maxUsd.PrimReader.Register(SphereReader, "UsdGeomSphere")