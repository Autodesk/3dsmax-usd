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

EXPORT_AS_NATIVE_SPHERE = True

# Prim writer for exporting Sphere object to USD native sphere.
class SphereWriter(maxUsd.PrimWriter):

    # This Writer only deals with Sphere objects, so we return a "Sphere" token because we want to convert the node to a native USD Sphere.
    # For performance reasons, the export is done in two passes.
    # The first pass creates all the prims inside a single SdfChangeBlock, the second pass populate each prim's attributes.
    # It is not mandatory to implement this function, it is also possible to define the prim from the Write() method (which would override the prim's type created in the first pass),
    # but you would lose the performance benefit. If not implemented, the base implementation returns Xform.
    def GetPrimType(self):
        if EXPORT_AS_NATIVE_SPHERE:
            return "Sphere"
        return "Mesh"

    # For this sample, we will demonstrate how to export the Radius and DisplayColor attributes.
    # We'll also demonstrate one way to handle animation.    
    def Write(self, prim, applyOffset, time):
        try: 
            nodeHandle = self.GetNodeHandle()
            stage = prim.GetStage()
            opts = self.GetExportArgs()
            node = rt.maxOps.getNodeByHandle(nodeHandle)
            
            usdTime = time.GetUsdTime()
            maxTime = time.GetMaxTime()
            
            if EXPORT_AS_NATIVE_SPHERE:            
                # prim is already a Sphere, it was created for us from the type returned in GetPrimType()
                spherePrim = UsdGeom.Sphere(prim)
                radiusAttr = spherePrim.GetRadiusAttr()
                extentAttr = spherePrim.CreateExtentAttr()
                radius = None
                with pymxs.attime(maxTime):
                    radius = node.radius
                radiusAttr.Set(radius, usdTime)
                extent = pyGf.Vec3f(radius, radius, radius)
                extentAttr.Set([-extent, extent], usdTime)
                
            # Alternatively, we could export the Sphere as a mesh, using the MeshConverter utility :
            else:
                spherePrim = maxUsd.MeshConverter.ConvertToUSDMesh(nodeHandle, stage, prim.GetPath(), opts, applyOffset, time)
            
            if time.IsFirstFrame():
                if EXPORT_AS_NATIVE_SPHERE:
                    print("Write a Sphere as a USD Sphere prim!")
                else:
                    print("Write a Sphere as a USD Mesh prim!")
                
                # Setup the wireColor as displayColor.
                displayColorAttr = spherePrim.GetDisplayColorAttr()
                r = node.wireColor.r / 255
                g = node.wireColor.g / 255
                b = node.wireColor.b / 255
                displayColorAttr.Set([(r,g,b)])
                return True

        except Exception as e:
            # Quite useful to debug errors in a Python callback
            print('Write() - Error: %s' % str(e))
            print(traceback.format_exc())
            return False

    def GetValidityInterval(self, timeFrame):
        # The base implementation of GetValidityInterval() will return the object's validity interval.
        # So the write() method would only be called when the object changes. 
        # for demonstration purposes, lets force the exporter to call the object's write every frame, 
        # by telling it that what we export at each frame is only valid at that exact frame.
        return maxUsd.Interval(timeFrame,timeFrame)

    # This method is responsible of telling the export process if it can export the current node's object.
    # In the case of this sample, the only object type we want to handle is the Sphere.    
    @classmethod
    def CanExport(cls, nodeHandle, exportArgs):
        node = rt.maxOps.getNodeByHandle(nodeHandle)
        if rt.classOf(node) == rt.Sphere:
            return maxUsd.PrimWriter.ContextSupport.Supported
        return maxUsd.PrimWriter.ContextSupport.Unsupported
   
# Register the writer.
# First argument is the class, second argument is the Writer name, which will be used as an ID internaly.
maxUsd.PrimWriter.Register(SphereWriter, "SphereWriter")