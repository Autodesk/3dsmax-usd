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

from pxr import Usd
from pymxs import runtime as mxs

import os, sys
from enum import Enum
import traceback

# The UserDataImportChaserSample class
# This Import Chaser sample reads customData from the imported prims and adds
# user defined properties or custom attributes to the corresponding node
# depending on the arguments. The sample chaser acts with default data or using
# the arguments that may have been passed to the chaser.

# the supported data types by the UserDataImportChaserSample
class PropertyType(Enum):
    USER_PROP = 1
    CUSTOM_DATA = 2

class userDataImportChaserSample(maxUsd.ImportChaser):
    default_property = { PropertyType.USER_PROP : ["myUserProperty"], PropertyType.CUSTOM_DATA : [] }

	# constructor - can be customized for the Import Chaser requirements
	# the minimal arguments for a functional chaser, and the chaser arguments, which are
	# not mandatory but can be use to parametrize the chaser.
    def __init__(self, factoryContext, *args, **kwargs):
        super(userDataImportChaserSample, self).__init__(factoryContext, *args, **kwargs)
        self.properties = userDataImportChaserSample.default_property

        # retrieve the specified arguments to be used with the import chaser
        # the chaser's arguments can be specified for different chasers
        # the returned arguments are in the form of a dictionary
        if 'UserData' in factoryContext.GetJobArgs().GetAllChaserArgs():
            chaser_args = factoryContext.GetJobArgs().GetAllChaserArgs()['UserData']
            for type, names in chaser_args.items():
                if type =="user":
                    self.properties[PropertyType.USER_PROP] = []
                    for name in names.split(','):
                        self.properties[PropertyType.USER_PROP].append(name)
                elif type == "custom":
                    self.properties[PropertyType.CUSTOM_DATA] = []
                    for name in names.split(','):
                        self.properties[PropertyType.CUSTOM_DATA].append(name)

        # retrieve the import mapping dictionary for the stage
        # the dictionary maps the resulting USD prim paths to their MAXScript NodeHandles origin
        self.primsToNodeHandles = factoryContext.GetPrimsToNodeHandles()

        # retrieve the USD stage being written to
        self.stage = factoryContext.GetStage()
        
	# processing that needs to run after the main 3ds Max USD import loop.
    def PostImport(self):
        try:
            for prim_path, node_handle in self.primsToNodeHandles.items():
                node = mxs.maxOps.getNodeByHandle(node_handle)
                prim = self.stage.GetPrimAtPath(prim_path)
                addCustAttrib = False
                params = ""
                paramsUi = ""

                if not prim:
                    print(f"Prim not found at path: {prim_path}")
                    return False
                
                for type, names in self.properties.items():
                    customData = prim.GetCustomData()
                    for name in names:
                        val = customData.get(name)
                        
                        if val != None:
                            if type == PropertyType.USER_PROP:
                                mxs.setUserProp(node, name, val)

                            elif type == PropertyType.CUSTOM_DATA:
                                if(isinstance(val, str)):
                                    params += f'{name} type:#string ui:{name} default:"{val}" animateable:True\n'
                                    paramsUi += f'edittext {name} "{name}" type:#string\n'
                                elif(isinstance(val, float)):
                                    params += f'{name} type:#float ui:{name} default:{val} animateable:True\n'
                                    paramsUi += f'spinner {name} "{name}" type:#float\n'
                                elif(isinstance(val, bool)):
                                    params += f'{name} type:#boolean ui:{name} default:{val} animateable:True\n'
                                    paramsUi += f'checkbox {name} "{name}" type:#boolean\n'
                                elif(isinstance(val, int)):
                                    params += f'{name} type:#integer ui:{name} default:{val} animateable:True\n'
                                    paramsUi += f'slider {name} "{name}" type:#integer\n'
                                addCustAttrib = True
                
                if(addCustAttrib):
                    baseCA2=f'''attributes "USD"
                    (
                        parameters main rollout:params
                        (
                            {params}
                        )

                        rollout params "USD"
                        (
                            {paramsUi}
                        )
                    )'''
                    attr = mxs.execute(baseCA2)
                    mxs.custAttributes.add(node.baseObject, attr)

        except Exception as e:
            # Quite useful to debug errors in a Python callback
            print('Write() - Error: %s' % str(e))
            print(traceback.format_exc())
            return False
        return True

# Job Contexts allow to customize several options during a maxUsd import
#   - 'chaserNames'
#   - 'chaserArgs'
#
# This sample will add a "Import User Data Python DEMO" option in the "PlugIn configuration" drop down in maxUsd imports
def CustomImportJobContextSample():
    # build a dictionary of the options to set using the context
    extraArgs = {}
    
    # The UserData chaser needs to be enabled in this job context
    extraArgs['chaserNames']  = ['UserDataImport']

    # The following arguments are based on the variables contained in the 
    # custom data of the prims found in: ./SceneFiles/UserDataChaserSample.usda
    extraArgs['chaserArgs'] = [['UserData', 'user', 'myUserFloatProperty,myUserProperty'], ['UserData', 'custom', 'inGame,strVal']]
    return extraArgs

maxUsd.ImportChaser.Register(userDataImportChaserSample, "UserDataImport", "Import User Data Python DEMO", "Chaser that adds custom data from prims to their corresponding nodes")
maxUsd.JobContextRegistry.RegisterImportJobContext("CustomImportContext", "Import User Data context Python DEMO", "Custom plug-in configuration", CustomImportJobContextSample)
