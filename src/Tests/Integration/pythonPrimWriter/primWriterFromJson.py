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
from pxr import UsdShade, UsdGeom
import traceback
import jsonWriterGlobals
class WriterFromJson(maxUsd.PrimWriter):
    
    def GetPrimType(self):
        return "Cone"
        
    def Write(self, prim, applyOffset, time):
        # Do nothing in the write...except unregister the writer - this is so we only only
        # execute this writer once part of it's associated test.
        maxUsd.PrimWriter.Unregister(WriterFromJson, "writerFromJson")
        jsonWriterGlobals.jsonPrimWriter_write_called = True
        print(jsonPrimWriter_write_called)
        return True

    @classmethod
    def CanExport(cls, nodeHandle, exportArgs):
        node = rt.maxOps.getNodeByHandle(nodeHandle)
        if rt.classOf(node) == rt.Cone:
            return maxUsd.PrimWriter.ContextSupport.Supported
        return maxUsd.PrimWriter.ContextSupport.Unsupported
		
maxUsd.PrimWriter.Register(WriterFromJson, "writerFromJson")