#
# Copyright 2024 Autodesk
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
import pymxs
import usd_test_helpers
mxs = pymxs.runtime

from layer_editor_test import UsdLayerEditorTest

from pxr import Sdf, Usd, UsdUtils, UsdGeom

def createStage(rootFile):
    maxUsdObj = mxs.USDStageObject()
    maxUsdObj.SetRootLayer(rootFile, stageMask='/')
    stageCache = UsdUtils.StageCache.Get()
    stage = stageCache.Find(Usd.StageCache.Id.FromLongInt(maxUsdObj.CacheId))
    stage.Reload()
    return stage
    
def resetScene():
    mxs.resetMaxFile(mxs.Name("noprompt"))

def undo():
    pymxs.run_undo()
def redo():
    pymxs.run_redo()

def setup():
    UsdLayerEditorTest._createStage = staticmethod(createStage)
    UsdLayerEditorTest._resetScene = staticmethod(resetScene)
    UsdLayerEditorTest._undo = staticmethod(undo)
    UsdLayerEditorTest._redo = staticmethod(redo)
