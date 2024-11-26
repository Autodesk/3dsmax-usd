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

# Export Chaser sample that adds Custom Data to the exported prims if the specified user
# property or custom attribute are found in the corresponding Node. The sampled chaser
# acts with default data or using the arguments that may have been passed to the chaser.

# the supported data types by the UserDataExportChaserSample
class PropertyType(Enum):
    USER_PROP = 1
    CUSTOM_DATA = 2


class userDataExportChaserSample(maxUsd.ExportChaser):
    
    default_property = { PropertyType.USER_PROP : ["myUserProperty"], PropertyType.CUSTOM_DATA : [] }

	# constructor - can be customized for the Export Chaser requirements
	# in the provided sample, it receives the stage and Prims to Node map, which are 
	# the minimal arguments for a functional chaser, and the chaser arguments, which are
	# not mandatory but can be use to parametrize the chaser.
    def __init__(self, factoryContext, *args, **kwargs):
        super(userDataExportChaserSample, self).__init__(factoryContext, *args, **kwargs)

        self.properties = userDataExportChaserSample.default_property

        # retrieve the specified arguments to be used with the export chaser
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

        # retrieve the export mapping dictionary for the stage
        # the dictionary maps the resulting USD prim paths to their MAXScript NodeHandles origin
        self.primsToNodeHandles = factoryContext.GetPrimsToNodeHandles()

        # retrieve the USD stage being written to
        self.stage = factoryContext.GetStage()

        # retrieve the jobContextOptions the user may have modified through the UI
        self.contextOptions = factoryContext.GetJobArgs().GetJobContextOptions("CustomDemoContext")

	# processing that needs to run after the main 3ds Max USD export loop.
    def PostExport(self):
        
        # the jobContextOptions the user may have modified through the UI 
        if self.contextOptions.get('do_something_special', False):
            # do something Special
            pass

        try:
            for prim_path, node_handle in self.primsToNodeHandles.items():
                node = mxs.maxOps.getNodeByHandle(node_handle)
                prim = self.stage.GetPrimAtPath(prim_path)
                
                for type, names in self.properties.items():
                    for name in names:
                        if type == PropertyType.USER_PROP:
                            prop = mxs.getUserProp(node, name)
                            # verify the property exists
                            if prop == None:
                                continue
                            if mxs.classOf(prop) == mxs.String:
                                # strip the quoting marks from the string
                                prop = prop[1:len(prop)-1]
                            prim.SetCustomDataByKey(name, prop)
                
                        elif type == PropertyType.CUSTOM_DATA:
                            if mxs.isProperty(node, name):
                                prop = mxs.getProperty(node, name)
                                prim.SetCustomDataByKey(name, prop)
                
        except Exception as e:
            # Quite useful to debug errors in a Python callback
            print('Write() - Error: %s' % str(e))
            print(traceback.format_exc())
            return False
        
        return True


# Job Contexts allow to customize several options during a maxUsd export
#   - 'chaserNames'
#   - 'chaserArgs'
#   - 'convertMaterialsTo'
#
# This sample will add a "Custom Context Demo" option in the "PlugIn configuration" drop down in maxUsd exports
def CustomExportJobContextSample():
    # build a dictionary of the options to set using the context
    extraArgs = {}
    
    # The UserData chaser needs to be enabled in this job context
    extraArgs['chaserNames']  = ['UserData']
    extraArgs['chaserArgs'] = [['UserData', 'user', 'myUserFloatProperty,myUserProperty'], ['UserData', 'custom', 'inGame']]
    return extraArgs



maxUsd.ExportChaser.Register(userDataExportChaserSample, "UserData", "User Data Python DEMO", "Chaser to export user data along the exported USD prims")

maxUsd.JobContextRegistry.RegisterExportJobContext("CustomDemoContext", "Custom Context Python DEMO", "Custom plug-in configuration", CustomExportJobContextSample)

# Uncomment code below to export a scene using the export chaser sample
# The code prepares the exporter options to
#  - export to a USD file in ASCII
#  - sets the ChaserNames to only use 'UserData'; the name of export chaser sample
#  - sets the arguments for the 'UserData' export chaser
#    - the list of user property ('user') to export as USD Custom Data; comma separated property names
#    - the list of custom attribute ('custom') to export as USD Custom Data; comma separated property names

#opt = mxs.USDExporter.CreateOptions()
#opt.FileFormat = mxs.Name("ascii")
#opt.RootPrimPath = "/"
#opt.ChaserNames = mxs.Array("UserData")
#opt.AllChaserArgs = mxs.Array("UserData", "user", "myUserFloatProperty,myUserProperty", "UserData", "custom", "inGame")
#mxs.USDExporter.ExportFile( "c:\\temp\\myscene_with_custom_data.usd",
#                            exportOptions=opt)

def showJobExportOptions(jobName, parentUI, contextOptions):
    # 3dsMax 2025 and later is using PySide6, while 3dsMax 2024 and earlier is
    # using PySide2 - so we need to check for both
    try: 
        import PySide6.QtWidgets as QtWidgets
    except ImportError:
        try: 
            import PySide2.QtWidgets as QtWidgets
        except ImportError as err:
            print(f"Could not import QtWidgets from neither PySide6 nor PySide2 {err=}.")
            raise err
    
    try: 
        dlg = QtWidgets.QDialog(parentUI)
        dlg.setWindowTitle(f"Export Options for {jobName}")
        dlg.setModal(True)
        dlg.setLayout(QtWidgets.QVBoxLayout())

        checkBoxSpecial = QtWidgets.QCheckBox("Do something Special")
        checkBoxSpecial.setChecked(contextOptions.get('do_something_special', False))
        dlg.layout().addWidget(checkBoxSpecial)

        # ...

        buttonBox = QtWidgets.QDialogButtonBox(QtWidgets.QDialogButtonBox.Ok | QtWidgets.QDialogButtonBox.Cancel)
        buttonBox.accepted.connect(dlg.accept)
        buttonBox.rejected.connect(dlg.reject)
        dlg.layout().addWidget(buttonBox, 0)

        if dlg.exec() == QtWidgets.QDialog.Accepted:
            contextOptions['do_something_special'] = checkBoxSpecial.isChecked()
            #...

    except Exception as err:
        print(f"Unexpected {err=}, {type(err)=}")

    return contextOptions

maxUsd.JobContextRegistry.SetExportOptionsUI("CustomDemoContext", showJobExportOptions)
