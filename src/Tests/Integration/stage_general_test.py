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
import pymxs

import unittest
import sys
import os

from pxr import Sdf, Usd, UsdUtils, UsdGeom

import maxUsd
import ufe
import usdUfe
import UsdLayerEditor

import usd_test_helpers

mxs = pymxs.runtime

mxs.pluginManager.loadClass(mxs.USDStageObject)

class TestStageGeneral(unittest.TestCase):

    def setUp(self):
        self.output_prefix = usd_test_helpers.standard_output_prefix("STAGE_GENERAL_")
        usd_test_helpers.load_usd_plugins()
        mxs.resetMaxFile(mxs.Name("noprompt"))

        box = mxs.Box(name="box")
        self.test_usd_file_path = self.output_prefix + "box.usda"
        export_options = mxs.USDExporter.CreateOptions()
        export_options.FileFormat = mxs.Name("ascii")
        export_options.RootPrimPath = "/"
        ret = mxs.USDExporter.ExportFile(
            self.test_usd_file_path,
            exportOptions=export_options
        )

    def tearDown(self) -> None:
        return None

    def test_stage_classname(self):
        maxUsdObj = mxs.USDStageObject()
        self.assertEqual(mxs.GetClassName(maxUsdObj), "USD Stage")

    def test_default_payload_rules(self):
        maxUsdObj = mxs.USDStageObject()
        maxUsdObj.SetRootLayer(self.test_usd_file_path, stageMask='/')

        stageCache = UsdUtils.StageCache.Get()
        stage = stageCache.Find(Usd.StageCache.Id.FromLongInt(maxUsdObj.CacheId))
        loadRules = stage.GetLoadRules().GetRules()
        self.assertEqual(len(loadRules), 0)

    def test_reload_all_layers(self):
        maxUsdObj = mxs.USDStageObject()
        maxUsdObj.SetRootLayer(self.test_usd_file_path, stageMask='/')
        
        # Make sure we start in a clean state - layer in registry matches disk.
        layer = Sdf.Layer.FindOrOpen(self.test_usd_file_path)
        layer.Reload()
        
        init_units_per_meter = maxUsdObj.SourceMetersPerUnit
        
        stageCache = UsdUtils.StageCache.Get()
        stage = stageCache.Find(Usd.StageCache.Id.FromLongInt(maxUsdObj.CacheId))
        
        # Dirty the root layer (session layer is not affected by the reload)
        stage.SetEditTarget(stage.GetRootLayer())                
        new_units_per_meter = 0.4
        UsdGeom.SetStageMetersPerUnit(stage, new_units_per_meter)
        
        self.assertAlmostEqual(new_units_per_meter, maxUsdObj.SourceMetersPerUnit, places=6)

        maxUsdObj.Reload(quiet=True)
        self.assertAlmostEqual(init_units_per_meter, maxUsdObj.SourceMetersPerUnit, places=6)

    def test_deactivation_crash_fix(self):
        # Test crash fix, see https://jira.autodesk.com/browse/MAXX-71391
        stageObject = mxs.USDStageObject()
        testDataDir = os.path.dirname(__file__)
        sampleFile = (testDataDir + "\\data\\instance_deactivate_crash.usda")
        stageObject.SetRootLayer(sampleFile, stageMask='/')
        stageCache = UsdUtils.StageCache.Get()
        stage = stageCache.Find(Usd.StageCache.Id.FromLongInt(stageObject.CacheId))
        # Deactivation/deletion was not properly handled, leading to bad render data being used, leading to crashes.
        prim = stage.GetPrimAtPath("/root/teapot_ref004")
        prim.SetActive(False)
        stageObject.DisplayProxy = False
        stageObject.DisplayRender = True
        
    def test_make_invisible_instanced_child_crash(self):        
        # Test crash fix see https://jira.autodesk.com/browse/MAXX-74798
        stageObject = mxs.USDStageObject()
        testDataDir = os.path.dirname(__file__)
        sampleFile = (testDataDir + "\\data\\instance_vis_crash.usda")
        stageObject.SetRootLayer(sampleFile, stageMask='/')
        stageObject.Reload(quiet=True)
        stageCache = UsdUtils.StageCache.Get()
        stage = stageCache.Find(Usd.StageCache.Id.FromLongInt(stageObject.CacheId))
        # Use of make invisible on a instanced prim, if the parent was already hidden 
        # was causing a crash.
        parentPrim = stage.GetPrimAtPath("/root/Box003")
        parentImg = UsdGeom.Imageable(parentPrim)
        parentImg.MakeInvisible()
        # Force redraw to make sure the invis. is completed independantly.
        mxs.forceCompleteRedraw()
        # Hide child, used to crash at the render.
        childPrim = stage.GetPrimAtPath("/root/Box003/Box002")
        childImg = UsdGeom.Imageable(childPrim)
        childImg.MakeInvisible()
        mxs.forceCompleteRedraw()
        
    # Testing the clear session layer mxs function. This function is basically what runs when
    # the button is hit from the UI.
    def test_clear_session_layer(self):
        maxUsdObj = mxs.USDStageObject()
        maxUsdObj.SetRootLayer(self.test_usd_file_path, stageMask='/')
        stageCache = UsdUtils.StageCache.Get()
        stage = stageCache.Find(Usd.StageCache.Id.FromLongInt(maxUsdObj.CacheId))
        
        boxPrim = stage.GetPrimAtPath("/box")
        stage.SetEditTarget(stage.GetSessionLayer())
        
        visAttr = UsdGeom.Imageable(boxPrim).GetVisibilityAttr()
        visAttr.Set(UsdGeom.Tokens.invisible)
        
        self.assertEqual(visAttr.Get(),UsdGeom.Tokens.invisible)
        maxUsdObj.ClearSessionLayer()
        self.assertEqual(visAttr.Get(),UsdGeom.Tokens.inherited)        
    
    # Checks if the given prim's visibility matches the passed token.
    def check_prim_vis(self, stageId, primPath, token):
        stageCache = UsdUtils.StageCache.Get()
        stage = stageCache.Find(Usd.StageCache.Id.FromLongInt(stageId))
        prim = stage.GetPrimAtPath(primPath)
        visAttr = UsdGeom.Imageable(prim).GetVisibilityAttr()
        self.assertEqual(visAttr.Get(), token)    
    
    # Test the save & load of the session layer to the .max scene.
    def test_save_load_session_layer(self):
        # Create a simple stage.
        stageName = "stage"
        maxUsdObj = mxs.USDStageObject(name=stageName)
        maxUsdObj.SetRootLayer(self.test_usd_file_path, stageMask='/')
        stageCache = UsdUtils.StageCache.Get()
        stage = stageCache.Find(Usd.StageCache.Id.FromLongInt(maxUsdObj.CacheId))
        
        # Hide a prim on the session layer, and save the max scene to disk.
        primPath = "/box"
        boxPrim = stage.GetPrimAtPath(primPath)
        stage.SetEditTarget(stage.GetSessionLayer())
        visAttr = UsdGeom.Imageable(boxPrim).GetVisibilityAttr()
        visAttr.Set(UsdGeom.Tokens.invisible)
        maxSceneSavePath = self.output_prefix + "test_save_load_of_primvar_mapping.max"
        mxs.saveMaxFile(maxSceneSavePath, quiet=True)
                    
        # Load the scene from disk, make sure the prim is hidden, 
        # meaning the session layer was properly loaded & applied).
        mxs.loadMaxFile(maxSceneSavePath)
        loadedStageObject = mxs.getNodeByName(stageName)
        self.check_prim_vis(loadedStageObject.CacheId, primPath, UsdGeom.Tokens.invisible)
        
        # Clear the session layer, and reload the file (without saving).
        loadedStageObject.ClearSessionLayer()
        mxs.loadMaxFile(maxSceneSavePath)
        # Prim should still be hidden, as we did not save the .max scene, and so the session
        # layer was not serialized.
        loadedStageObject = mxs.getNodeByName(stageName)
        self.check_prim_vis(loadedStageObject.CacheId, primPath, UsdGeom.Tokens.invisible)

        # Clear the session layer again, but this time, save.
        loadedStageObject.ClearSessionLayer()
        mxs.saveMaxFile(maxSceneSavePath, quiet=True)
        
        # Reload the .max file. The prim should be visible - the prim was hidden from the session
        # layer, which was cleared (and in its empty state).
        mxs.loadMaxFile(maxSceneSavePath)
        loadedStageObject = mxs.getNodeByName(stageName)
        self.check_prim_vis(loadedStageObject.CacheId, primPath, UsdGeom.Tokens.inherited)

    def test_get_ufe_prim_path(self):
        
        # Create a simple stage object.
        maxUsdObj = mxs.USDStageObject()
        maxUsdObj.SetRootLayer(self.test_usd_file_path, stageMask='/')
                        
        # Test absolute root path
        pathStr = maxUsd.GetUsdPrimUfePath(maxUsdObj.handle, "/")
        expectedStr = "/" + maxUsdObj.Guid
        self.assertEqual(expectedStr, pathStr)
        # Conversion to a ufe path, will throw on failure. Failing the test.
        ufe.PathString.path(pathStr)
        
        # Test valid prim path
        pathStr = maxUsd.GetUsdPrimUfePath(maxUsdObj.handle, "/box")
        expectedStr = "/" + maxUsdObj.Guid + ",/box"
        self.assertEqual(expectedStr, pathStr)
        # Conversion to a ufe path, will throw on failure. Failing the test.
        ufe.PathString.path(pathStr)
        
        # Test invalid prim path
        pathStr = maxUsd.GetUsdPrimUfePath(maxUsdObj.handle, "/nothing")
        self.assertEqual("", pathStr)
        
        # Test invalid object handle
        pathStr = maxUsd.GetUsdPrimUfePath(999, "/")
        self.assertEqual("", pathStr)
        
        # Test object handle of non-usd object.
        pathStr = maxUsd.GetUsdPrimUfePath(mxs.box().handle, "/")
        self.assertEqual("", pathStr)
                
        # Test stage object with no loaded usd stage.
        emptyStageObj = mxs.USDStageObject()
        pathStr = maxUsd.GetUsdPrimUfePath(emptyStageObj.handle, "/")
        self.assertEqual("", pathStr)

    def test_payload_none_legacy(self):
        testDataDir = os.path.dirname(__file__)
        sampleLegacyFile = (testDataDir + "\\data\\stage_payload_none_legacy.max")
        mxs.loadMaxFile(sampleLegacyFile)
        stageCache = UsdUtils.StageCache.Get()
        stage = stageCache.Find(Usd.StageCache.Id.FromLongInt(mxs.objects[0].CacheId))
        loadRules = stage.GetLoadRules().GetRules()
        self.assertEqual(len([item for item in loadRules if item[0] == Sdf.Path("/") and item[1] == Usd.StageLoadRules.NoneRule]), 1)

    def test_save_payload_rules(self):
        # remove the setup object that was created before executing the rest of this test
        mxs.delete(mxs.objects[0])

        stageName = "stage"
        stageObject = mxs.USDStageObject(name=stageName)
        testDataDir = os.path.dirname(__file__)
        sampleFile = (testDataDir + "\\data\\stage_payload.usda")
        stageObject.SetRootLayer(sampleFile, stageMask='/')

        # the stage should not have any specific payload rules applied on it
        stageCache = UsdUtils.StageCache.Get()
        stage = stageCache.Find(Usd.StageCache.Id.FromLongInt(stageObject.CacheId))
        loadRules = stage.GetLoadRules().GetRules()
        self.assertEqual(len(loadRules), 0)

        # unload a section of the stage
        pathStr = maxUsd.GetUsdPrimUfePath(stageObject.handle, "/StagePayloadTest/NativeGeoms")
        unloadCommand = usdUfe.UnloadPayloadCommand(usdUfe.ufePathToPrim(pathStr))
        unloadCommand.execute()

        # the stage should now have payload rule applied on it
        loadRules = stage.GetLoadRules().GetRules()
        self.assertEqual(len(loadRules), 1)
        self.assertEqual(len([item for item in loadRules if item[0] == Sdf.Path("/StagePayloadTest/NativeGeoms") and item[1] == Usd.StageLoadRules.NoneRule]), 1)

        # save the scene with an unloaded stage section
        maxSceneSavePath = self.output_prefix + "test_save_load_payload_rules.max"
        mxs.saveMaxFile(maxSceneSavePath, quiet=True)
                    
        # Load the scene from disk, make sure the payload rules were applied
        mxs.loadMaxFile(maxSceneSavePath)
        loadedStageObject = mxs.getNodeByName(stageName)
        # the stage should have the same payload rule as the file was saved with
        stageObject = mxs.objects[0]
        stageCache = UsdUtils.StageCache.Get()
        stage = stageCache.Find(Usd.StageCache.Id.FromLongInt(stageObject.CacheId))
        loadRules = stage.GetLoadRules().GetRules()
        self.assertEqual(len(loadRules), 1)
        self.assertEqual(len([item for item in loadRules if item[0] == Sdf.Path("/StagePayloadTest/NativeGeoms") and item[1] == Usd.StageLoadRules.NoneRule]), 1)

        # reload payloads for a section of the stage
        # and be more specific about a given payload we want to unload
        pathStr = maxUsd.GetUsdPrimUfePath(stageObject.handle, "/StagePayloadTest/NativeGeoms")
        loadCommand = usdUfe.LoadPayloadCommand(usdUfe.ufePathToPrim(pathStr), Usd.LoadWithDescendants)
        loadCommand.execute()
        pathStr = maxUsd.GetUsdPrimUfePath(stageObject.handle, "/StagePayloadTest/NativeGeoms/Cone")
        unloadCommand = usdUfe.UnloadPayloadCommand(usdUfe.ufePathToPrim(pathStr))
        unloadCommand.execute()
        # the stage should have a payload rule to include all payload from the specified path and all descendants
        loadRules = stage.GetLoadRules().GetRules()
        self.assertEqual(len(loadRules), 2)
        self.assertEqual(len([item for item in loadRules if item[0] == Sdf.Path("/StagePayloadTest/NativeGeoms") and item[1] == Usd.StageLoadRules.AllRule]), 1)
        self.assertEqual(len([item for item in loadRules if item[0] == Sdf.Path("/StagePayloadTest/NativeGeoms/Cone") and item[1] == Usd.StageLoadRules.NoneRule]), 1)

        # save the scene with an loaded stage section
        maxSceneSavePath = self.output_prefix + "test_resave_load_payload_rules.max"
        mxs.saveMaxFile(maxSceneSavePath, quiet=True)
                    
        # Load the scene from disk, make sure the payload rules were applied
        mxs.loadMaxFile(maxSceneSavePath)
        loadedStageObject = mxs.getNodeByName(stageName)
        # the stage should have the same payload rule as the file was saved with
        stageObject = mxs.objects[0]
        stageCache = UsdUtils.StageCache.Get()
        stage = stageCache.Find(Usd.StageCache.Id.FromLongInt(stageObject.CacheId))
        loadRules = stage.GetLoadRules().GetRules()
        self.assertEqual(len(loadRules), 2)
        self.assertEqual(len([item for item in loadRules if item[0] == Sdf.Path("/StagePayloadTest/NativeGeoms") and item[1] == Usd.StageLoadRules.AllRule]), 1)
        self.assertEqual(len([item for item in loadRules if item[0] == Sdf.Path("/StagePayloadTest/NativeGeoms/Cone") and item[1] == Usd.StageLoadRules.NoneRule]), 1)
        
    def test_stage_geometry_stats(self):
        # Making sure that the stage object reports geometry stats correctly.
        stageObject = mxs.USDStageObject()
        testDataDir = os.path.dirname(__file__)
        # Use this test scene as it has a mixture of instanced and non-instanced data : 
        # - 3 instanced boxes (18 faces, 24 verts)
        # - 3 planes of 4 faces each (12 faces, 27 verts)
        sampleFile = (testDataDir + "\\data\\consolidation_instance_subset_mixed.usda")
        stageObject.SetRootLayer(sampleFile, stageMask='/')
        mxs.forceCompleteRedraw()
        stats = mxs.getPolygonCount(stageObject)
        self.assertEqual(stats[0], 30) #numFaces
        self.assertEqual(stats[1], 51) #numVerts
        
    def test_default_edit_target_to_root_layer(self):
        # Making sure that upon initialization, the edit target is initialized to the root layer.
        stageObject = mxs.USDStageObject()
        testDataDir = os.path.dirname(__file__)
        sampleFile = (testDataDir + "\\data\\box_sample.usda")
        stageObject.SetRootLayer(sampleFile, stageMask='/')
        stageCache = UsdUtils.StageCache.Get()
        stage = stageCache.Find(Usd.StageCache.Id.FromLongInt(stageObject.CacheId))
        self.assertEqual(stage.GetEditTarget(), stage.GetRootLayer())

    def test_stage_without_loading_payloads(self):
        # remove the setup object that was created before executing the rest of this test
        mxs.delete(mxs.objects[0])

        stageName = "stage"
        stageObject = mxs.USDStageObject(name=stageName)
        testDataDir = os.path.dirname(__file__)
        sampleFile = (testDataDir + "\\data\\stage_payload.usda")
        stageObject.SetRootLayer(sampleFile, stageMask='/', payloadsLoaded=False)

        # the stage should have a rules set to load nothing directly at root
        stageCache = UsdUtils.StageCache.Get()
        stage = stageCache.Find(Usd.StageCache.Id.FromLongInt(stageObject.CacheId))
        loadRules = stage.GetLoadRules().GetRules()
        self.assertEqual(len(loadRules), 1)
        self.assertEqual(len([item for item in loadRules if item[0] == Sdf.Path("/") and item[1] == Usd.StageLoadRules.NoneRule]), 1)

    def test_save_layer_lock_state(self):
        stageName = "stage"
        maxUsdObj = mxs.USDStageObject(name=stageName)
        
        testDataDir = os.path.dirname(__file__)
        sampleFile = (testDataDir + "\\data\\sublayers\\root.usda")
        
        maxUsdObj.SetRootLayer(sampleFile, stageMask='/')
        stageCache = UsdUtils.StageCache.Get()
        stage = stageCache.Find(Usd.StageCache.Id.FromLongInt(maxUsdObj.CacheId))
        
        rootLayer = stage.GetRootLayer()
        subLayer1 = Sdf.Layer.FindRelativeToLayer(rootLayer, rootLayer.subLayerPaths[0])
        subLayer2 = Sdf.Layer.FindRelativeToLayer(rootLayer, rootLayer.subLayerPaths[1])
        subLayer3 = Sdf.Layer.FindRelativeToLayer(rootLayer, rootLayer.subLayerPaths[2])
        
        cmd = UsdLayerEditor.LockLayerCommand(stage, subLayer1,  UsdLayerEditor.LayerLock_Locked, False, False)
        cmd.execute();
        cmd = UsdLayerEditor.LockLayerCommand(stage, subLayer3,  UsdLayerEditor.LayerLock_Locked, False, False)
        cmd.execute();
        
        self.assertFalse(subLayer1.permissionToEdit)
        self.assertTrue(subLayer2.permissionToEdit) 
        self.assertFalse(subLayer3.permissionToEdit)
        
        # Save the scene, the locks will save with the stage object
        maxSceneSavePath = self.output_prefix + "layer_lock_save.max"
        mxs.saveMaxFile(maxSceneSavePath, quiet=True)
    
        # Clear the locks - and do not save.
        cmd = UsdLayerEditor.LockLayerCommand(stage, rootLayer,  UsdLayerEditor.LayerLock_Unlocked, True, False)
        cmd.execute()
        
        self.assertTrue(subLayer1.permissionToEdit)
        self.assertTrue(subLayer2.permissionToEdit)  
        self.assertTrue(subLayer3.permissionToEdit)
        
        # Load the scene from disk, make sure the locks are applied.
        mxs.loadMaxFile(maxSceneSavePath)
        loadedStageObject = mxs.getNodeByName(stageName)
        stage = stageCache.Find(Usd.StageCache.Id.FromLongInt(loadedStageObject.CacheId))
        
        rootLayer = stage.GetRootLayer()
        subLayer1 = Sdf.Layer.FindRelativeToLayer(rootLayer, rootLayer.subLayerPaths[0])
        subLayer2 = Sdf.Layer.FindRelativeToLayer(rootLayer, rootLayer.subLayerPaths[1])
        subLayer3 = Sdf.Layer.FindRelativeToLayer(rootLayer, rootLayer.subLayerPaths[2])
        
        self.assertFalse(subLayer1.permissionToEdit)
        self.assertTrue(subLayer2.permissionToEdit)
        self.assertFalse(subLayer3.permissionToEdit)
                
    def test_save_layer_mute_state(self):
        stageName = "stage"
        maxUsdObj = mxs.USDStageObject(name=stageName)
        
        testDataDir = os.path.dirname(__file__)
        sampleFile = (testDataDir + "\\data\\sublayers\\root.usda")
        
        maxUsdObj.SetRootLayer(sampleFile, stageMask='/')
        stageCache = UsdUtils.StageCache.Get()
        stage = stageCache.Find(Usd.StageCache.Id.FromLongInt(maxUsdObj.CacheId))
        
        rootLayer = stage.GetRootLayer()
        rootLayer.Reload() # Force a reload as other tests use this layer.
        subLayer1 = Sdf.Layer.FindRelativeToLayer(rootLayer, rootLayer.subLayerPaths[0])
        subLayer2 = Sdf.Layer.FindRelativeToLayer(rootLayer, rootLayer.subLayerPaths[1])
        subLayer3 = Sdf.Layer.FindRelativeToLayer(rootLayer, rootLayer.subLayerPaths[2])
        
        cmd = UsdLayerEditor.MuteLayerCommand(stage, subLayer1,  True)
        cmd.execute();
        cmd = UsdLayerEditor.MuteLayerCommand(stage, subLayer3,  True)
        cmd.execute();
        
        # USD can let go muted layers. Make sure we have valid objects.
        subLayer1 = Sdf.Layer.FindRelativeToLayer(rootLayer, rootLayer.subLayerPaths[0])
        subLayer3 = Sdf.Layer.FindRelativeToLayer(rootLayer, rootLayer.subLayerPaths[2])
        
        self.assertTrue(stage.IsLayerMuted(subLayer1.identifier))
        self.assertFalse(stage.IsLayerMuted(subLayer2.identifier)) 
        self.assertTrue(stage.IsLayerMuted(subLayer3.identifier))
        
        # Save the scene, the locks will save with the stage object
        maxSceneSavePath = self.output_prefix + "layer_mute_save.max"
        mxs.saveMaxFile(maxSceneSavePath, quiet=True)
    
        # Clear the mutes - and do not save that.
        cmd = UsdLayerEditor.MuteLayerCommand(stage, subLayer1,  False)
        cmd.execute();
        cmd = UsdLayerEditor.MuteLayerCommand(stage, subLayer3,  False)
        cmd.execute();
        
        self.assertFalse(stage.IsLayerMuted(subLayer1.identifier))
        self.assertFalse(stage.IsLayerMuted(subLayer2.identifier)) 
        self.assertFalse(stage.IsLayerMuted(subLayer3.identifier))
            
        # Load the scene from disk, make sure the mutes are applied
        mxs.loadMaxFile(maxSceneSavePath)
        loadedStageObject = mxs.getNodeByName(stageName)
        stage = stageCache.Find(Usd.StageCache.Id.FromLongInt(loadedStageObject.CacheId))
        
        rootLayer = stage.GetRootLayer()
        subLayer1 = Sdf.Layer.FindRelativeToLayer(rootLayer, rootLayer.subLayerPaths[0])
        subLayer2 = Sdf.Layer.FindRelativeToLayer(rootLayer, rootLayer.subLayerPaths[1])
        subLayer3 = Sdf.Layer.FindRelativeToLayer(rootLayer, rootLayer.subLayerPaths[2])
        
        self.assertTrue(stage.IsLayerMuted(subLayer1.identifier))
        self.assertFalse(stage.IsLayerMuted(subLayer2.identifier)) 
        self.assertTrue(stage.IsLayerMuted(subLayer3.identifier))

    def test_save_and_restore_edit_target(self):
        
        stageName = "foo"
        stageObject = mxs.USDStageObject(name=stageName)
        testDataDir = os.path.dirname(__file__)
        sampleFile = (testDataDir + "\\data\\sublayers\\root.usda")
        stageObject.SetRootLayer(sampleFile, stageMask='/')
        stageCache = UsdUtils.StageCache.Get()
        stage = stageCache.Find(Usd.StageCache.Id.FromLongInt(stageObject.CacheId))
        rootLayer = stage.GetRootLayer()
        self.assertEqual(stage.GetEditTarget(), rootLayer)
        
        # Change edit target to a sublayer and save.
        sub0 = Sdf.Layer.FindRelativeToLayer(rootLayer, rootLayer.subLayerPaths[0])
        stage.SetEditTarget(sub0)
        maxSceneSavePath = self.output_prefix + "save_sublayer_target.max"
        mxs.saveMaxFile(maxSceneSavePath, quiet=True)
        
        mxs.resetMaxFile(mxs.Name("noprompt"))
        
        # Load and check the edit target was properly restored.
        mxs.loadMaxFile(maxSceneSavePath, quiet=True)
        loadedStageObject = mxs.getNodeByName(stageName)
        stage = stageCache.Find(Usd.StageCache.Id.FromLongInt(loadedStageObject.CacheId))
        rootLayer = stage.GetRootLayer()
        sub0 = Sdf.Layer.FindRelativeToLayer(rootLayer, rootLayer.subLayerPaths[0])
        self.assertEqual(stage.GetEditTarget(), sub0) 
        
        # Change edit target to the session layer and save.
        stage.SetEditTarget(stage.GetSessionLayer())
        maxSceneSavePath = self.output_prefix + "save_session_target.max"
        mxs.saveMaxFile(maxSceneSavePath, quiet=True)
        
        # Load and check the edit target was properly restored.
        mxs.loadMaxFile(maxSceneSavePath, quiet=True)
        loadedStageObject = mxs.getNodeByName(stageName)
        stage = stageCache.Find(Usd.StageCache.Id.FromLongInt(loadedStageObject.CacheId))
        self.assertEqual(stage.GetEditTarget(), stage.GetSessionLayer()) 
        
        # Test changing the root layer - the target layer should go to the default, the root.
        loadedStageObject.SetRootLayer((testDataDir + "\\data\\box_sample.usda"), stageMask='/')
        stage = stageCache.Find(Usd.StageCache.Id.FromLongInt(loadedStageObject.CacheId))
        self.assertEqual(stage.GetEditTarget(), stage.GetRootLayer()) 
   
        # Create an anon sublayer, and save. This edit target will be lost on save.
        anon = Sdf.Layer.CreateAnonymous();
        rootLayer = stage.GetRootLayer()
        rootLayer.subLayerPaths.append(anon.identifier)
        stage.SetEditTarget(anon)
        maxSceneSavePath = self.output_prefix + "save_anon_target.max"
        mxs.saveMaxFile(maxSceneSavePath, quiet=True)
        
        # Reload, we should get back to the root layer.
        mxs.loadMaxFile(maxSceneSavePath, quiet=True)
        loadedStageObject = mxs.getNodeByName(stageName)
        stage = stageCache.Find(Usd.StageCache.Id.FromLongInt(loadedStageObject.CacheId))
        self.assertEqual(stage.GetEditTarget(), stage.GetRootLayer())

def run_tests():
    return unittest.TextTestRunner(stream=sys.stdout, verbosity=2).run(unittest.TestLoader().loadTestsFromTestCase(TestStageGeneral))

if __name__ == '__main__':
    run_tests()
