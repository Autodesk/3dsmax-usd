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
import traceback
import jsonShaderWriterGlobals
        
class ShaderWriterFromJson(maxUsd.ShaderWriter):
    def Write(self):
        jsonShaderWriterGlobals.jsonShaderWriter_write_called = True
        return True
        
    @classmethod
    def CanExport(cls, exportArgs):
        return maxUsd.ShaderWriter.ContextSupport.Supported    
       
maxUsd.ShaderWriter.Register(ShaderWriterFromJson, "Matte/Shadow")