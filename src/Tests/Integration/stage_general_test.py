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

    def test_reload_with_anonymous_root(self):
        """Test that reload only affects file-backed layers, not anonymous layers."""
        import tempfile
        import uuid
        
        # Create temporary file-backed layers with random names
        temp_dir = tempfile.gettempdir()
        file_backed_layer_path = os.path.join(temp_dir, f"test_layer_{uuid.uuid4().hex[:8]}.usda")
        file_backed_layer_2_path = os.path.join(temp_dir, f"test_layer_2_{uuid.uuid4().hex[:8]}.usda")
        
        # Create the layers and save them
        file_backed_layer = Sdf.Layer.CreateNew(file_backed_layer_path)
        file_backed_layer.Save()
        
        file_backed_layer_2 = Sdf.Layer.CreateNew(file_backed_layer_2_path)
        file_backed_layer_2.Save()
        
        # Track files for cleanup
        temp_files = [file_backed_layer_path, file_backed_layer_2_path]
        
        try:
            # Create a UsdStageObject with default anonymous root layer
            maxUsdObj = mxs.USDStageObject()
            stageCache = UsdUtils.StageCache.Get()
            stage = stageCache.Find(Usd.StageCache.Id.FromLongInt(maxUsdObj.CacheId))
            
            # Get references to the layers
            anon_root_layer = stage.GetRootLayer()
            session_layer = stage.GetSessionLayer()
            
            # Verify the root layer is anonymous
            self.assertTrue(anon_root_layer.anonymous)
            self.assertTrue(session_layer.anonymous)
            
            # Use UsdLayerEditor commands to add sublayers
            import ufe
            mgr = ufe.UndoableCommandMgr.instance()
            
            # Add the file-backed layer as a sublayer to the anonymous root using InsertSubPathCommand
            insert_file_cmd = UsdLayerEditor.InsertSubPathCommand(stage, anon_root_layer, file_backed_layer_path, 0)
            mgr.executeCmd(insert_file_cmd)
            
            # Add an anonymous sublayer directly to the anonymous root
            add_anon_to_root_cmd = UsdLayerEditor.AddAnonSubLayerCommand(stage, anon_root_layer)
            mgr.executeCmd(add_anon_to_root_cmd)
            anon_sublayer_of_root_id = add_anon_to_root_cmd.addedLayer()
            anon_sublayer_of_root = Sdf.Layer.Find(anon_sublayer_of_root_id)
            
            # Add another anonymous sublayer to the file-backed layer
            add_anon_to_file_cmd = UsdLayerEditor.AddAnonSubLayerCommand(stage, file_backed_layer)
            mgr.executeCmd(add_anon_to_file_cmd)
            anon_sublayer_of_file_id = add_anon_to_file_cmd.addedLayer()
            anon_sublayer_of_file = Sdf.Layer.Find(anon_sublayer_of_file_id)
            
            # Add the same sublayer structure under the session layer
            # Add the file-backed layer as a sublayer to the session layer
            insert_file_to_session_cmd = UsdLayerEditor.InsertSubPathCommand(stage, session_layer, file_backed_layer_2_path, 0)
            mgr.executeCmd(insert_file_to_session_cmd)
            
            # Add an anonymous sublayer to the session layer
            add_anon_to_session_cmd = UsdLayerEditor.AddAnonSubLayerCommand(stage, file_backed_layer_2)
            mgr.executeCmd(add_anon_to_session_cmd)
            anon_sublayer_of_session_file_backed_layer_id = add_anon_to_session_cmd.addedLayer()
            anon_sublayer_of_session_file_backed_layer = Sdf.Layer.Find(anon_sublayer_of_session_file_backed_layer_id)
            
            # Get the file-backed layer identifier for proper checking
            file_backed_layer_id = file_backed_layer.identifier
            file_backed_layer_2_id = file_backed_layer_2.identifier
            
            # Verify initial state - all sublayers are present
            self.assertEqual(len(anon_root_layer.subLayerPaths), 2)
            self.assertIn(anon_sublayer_of_root_id, anon_root_layer.subLayerPaths)
            self.assertIn(file_backed_layer_id, anon_root_layer.subLayerPaths)
            self.assertEqual(len(file_backed_layer.subLayerPaths), 1)
            self.assertIn(anon_sublayer_of_file_id, file_backed_layer.subLayerPaths)
     
            self.assertEqual(len(session_layer.subLayerPaths), 1)
            self.assertIn(file_backed_layer_2_id, session_layer.subLayerPaths)
            self.assertEqual(len(file_backed_layer_2.subLayerPaths), 1)
            self.assertIn(anon_sublayer_of_session_file_backed_layer_id, file_backed_layer_2.subLayerPaths)
            
            # Perform reload
            maxUsdObj.Reload(quiet=True)
            
            # After reload, verify behavior:
            # 1. Anonymous root should still have the file-backed sublayer
            self.assertIn(file_backed_layer_id, anon_root_layer.subLayerPaths)
            
            # 2. Anonymous root should still have its direct anonymous sublayer
            self.assertIn(anon_sublayer_of_root_id, anon_root_layer.subLayerPaths)
            
            # 3. File-backed layer should be reloaded from disk and no longer have anonymous sublayer
            self.assertEqual(len(file_backed_layer.subLayerPaths), 0)
            
            # 4. Session layer should remain unchanged (anonymous but not affected by reload)
            # Session layer should still have both its sublayers (file-backed and anonymous)
            self.assertEqual(len(session_layer.subLayerPaths), 1)
            self.assertIn(file_backed_layer_2_id, session_layer.subLayerPaths)
            self.assertEqual(len(file_backed_layer_2.subLayerPaths), 1)
            self.assertIn(anon_sublayer_of_session_file_backed_layer_id, file_backed_layer_2.subLayerPaths)
            
            self.assertTrue(session_layer.anonymous)
        
        finally:
            # Clean up temporary files
            for temp_file in temp_files:
                try:
                    if os.path.exists(temp_file):
                        os.remove(temp_file)
                except:
                    pass  # Ignore cleanup errors

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
        maxSceneSavePath = self.output_prefix + "test_save_load_of_session_layer.max"
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

    # Test the save & load of dirty layers to the .max scene.
    def test_save_load_dirty_layer(self):
        # Create a simple stage.
        stageName = "stage"
        maxUsdObj = mxs.USDStageObject(name=stageName)
        test_file_path = os.path.join(os.path.join(os.path.dirname(__file__), "data"), "dirty_layers.usda")
        maxUsdObj.SetRootLayer(test_file_path, stageMask='/')
        stageCache = UsdUtils.StageCache.Get()
        stage = stageCache.Find(Usd.StageCache.Id.FromLongInt(maxUsdObj.CacheId))
        
        # Hide a prim on the root layer, and save the max scene to disk.
        primPath = "/box_sample/Box001"
        boxPrim = stage.GetPrimAtPath(primPath)
        stage.SetEditTarget(stage.GetRootLayer())
        visAttr = UsdGeom.Imageable(boxPrim).GetVisibilityAttr()
        visAttr.Set(UsdGeom.Tokens.invisible)

        # 1
        # Set to save dirty layers to max scene
        mxs.USDStageObject.SetDefaultSaveMode("saveAllEditsMax")
        maxSceneSavePath = self.output_prefix + "test_save_load_of_dirty_layer.max"

        # Save the data
        mxs.saveMaxFile(maxSceneSavePath, quiet=True)            
        mxs.resetMaxFile(mxs.Name("noprompt"))
        # Make sure no layers with same id/path exists in memeory for whatever reason
        self.assertEqual(Sdf.Layer.Find(maxSceneSavePath), None)

        # Load the scene from disk, make sure the prim is hidden, 
        # meaning the dirty layer was properly loaded & applied.
        mxs.loadMaxFile(maxSceneSavePath)
        loadedStageObject = mxs.getNodeByName(stageName)
        stage = stageCache.Find(Usd.StageCache.Id.FromLongInt(loadedStageObject.CacheId))

        # Check that the prim is invisible
        self.check_prim_vis(loadedStageObject.CacheId, primPath, UsdGeom.Tokens.invisible)

        # Check if the root layer is dirty
        self.assertEqual(stage.GetRootLayer().dirty, True)

        # Save the root layer identifier for the next test
        saved_root_layer_id = stage.GetRootLayer().identifier

        mxs.resetMaxFile(mxs.Name("noprompt"))
        # 2 
        # Test loading max file with dirty layer data, while layer with same identifier exists
        dummy_layer_with_same_id = Sdf.Layer.CreateAnonymous()
        dummy_layer_with_same_id.identifier = saved_root_layer_id

        mxs.loadMaxFile(maxSceneSavePath)
        loadedStageObject = mxs.getNodeByName(stageName)
        stage = stageCache.Find(Usd.StageCache.Id.FromLongInt(loadedStageObject.CacheId))

        # Check that the prim is invisible
        self.check_prim_vis(loadedStageObject.CacheId, primPath, UsdGeom.Tokens.invisible)

        # Check if the root layer is dirty
        self.assertEqual(stage.GetRootLayer().dirty, True)

        mxs.resetMaxFile(mxs.Name("noprompt"))
        # 3
        # Test loading max file with dirty layer data, while layer with same identifier exists
        # but define the anonymous layer with ".usda" format
        dummy_layer_with_same_id = Sdf.Layer.CreateAnonymous(".usda")
        dummy_layer_with_same_id.identifier = saved_root_layer_id

        # Load the scene from disk, make sure the prim is hidden, 
        # meaning the dirty layer was properly loaded & applied).
        mxs.loadMaxFile(maxSceneSavePath)
        loadedStageObject = mxs.getNodeByName(stageName)
        stage = stageCache.Find(Usd.StageCache.Id.FromLongInt(loadedStageObject.CacheId))

        # Check that the prim is invisible
        self.check_prim_vis(loadedStageObject.CacheId, primPath, UsdGeom.Tokens.invisible)

        # Check if the root layer is dirty
        self.assertEqual(stage.GetRootLayer().dirty, True)

        mxs.resetMaxFile(mxs.Name("noprompt"))
        # 4
        # Test loading max file with dirty layer data, while layer with same identifier
        # but define the anonymous layer with ".usdc" format
        dummy_layer_with_same_id = Sdf.Layer.CreateAnonymous(".usdc")
        dummy_layer_with_same_id.identifier = saved_root_layer_id

        mxs.loadMaxFile(maxSceneSavePath)
        loadedStageObject = mxs.getNodeByName(stageName)
        stage = stageCache.Find(Usd.StageCache.Id.FromLongInt(loadedStageObject.CacheId))

        # Check that the prim is invisible
        self.check_prim_vis(loadedStageObject.CacheId, primPath, UsdGeom.Tokens.invisible)

        # Check if the root layer is dirty
        self.assertEqual(stage.GetRootLayer().dirty, True)

        # 5 Now add a sublayer and edit it as well as a test
        new_layer_name = os.path.join(os.path.join(os.path.dirname(__file__), "data"), "box_no_uvs.usda")
        new_sublayer = Sdf.Layer.OpenAsAnonymous(new_layer_name)
        new_sublayer.identifier = new_layer_name

        # sublayer the new layer to the root layer
        stage.GetRootLayer().subLayerPaths.append(new_layer_name)
        boxPrimInSublayer = stage.GetPrimAtPath("/box_no_uvs/Box001")
        stage.SetEditTarget(new_sublayer)

        # edit the visibility in the sublayer
        visAttrBoxSubLayer = UsdGeom.Imageable(boxPrimInSublayer).GetVisibilityAttr()
        visAttrBoxSubLayer.Set(UsdGeom.Tokens.invisible)
        # Save the data
        mxs.saveMaxFile(maxSceneSavePath, quiet=True)            
        mxs.resetMaxFile(mxs.Name("noprompt"))
        # Get rid of the handle to the anonymous stage with the same id
        # otherwise it gets picked up when the stage is reloaded again
        dummy_layer_with_same_id = None
        # also the new sublayer
        new_sublayer = None

        mxs.loadMaxFile(maxSceneSavePath)
        loadedStageObject = mxs.getNodeByName(stageName)
        stage = stageCache.Find(Usd.StageCache.Id.FromLongInt(loadedStageObject.CacheId))

        # Check that the prim is in the "invisible" state
        self.check_prim_vis(loadedStageObject.CacheId, primPath, UsdGeom.Tokens.invisible)

        # Check if the root layer is dirty
        self.assertEqual(stage.GetRootLayer().dirty, True)   

        # make sure the sublayer is actually present
        saved_sublayer = Sdf.Layer.Find(new_layer_name)
        self.assertNotEqual(saved_sublayer, None)

        containsSublayer = False
        for sublayerPath in stage.GetRootLayer().subLayerPaths:
            if os.path.normcase(os.path.normpath(sublayerPath)) == os.path.normcase(os.path.normpath(new_layer_name)):
                containsSublayer = True
                break
        # Check if the sublayer is part of the stage's layers
        self.assertEqual(containsSublayer, True)   
        # Check that the prim of the sublayer is in the "invisible" state
        self.check_prim_vis(loadedStageObject.CacheId, primPath, UsdGeom.Tokens.invisible)

        # Check if the sublayer is dirty
        self.assertEqual(saved_sublayer.dirty, True)   

        # 6
        # Now save with save3dsMaxOnly option
        mxs.USDStageObject.SetDefaultSaveMode("save3dsMaxOnly")
        mxs.saveMaxFile(maxSceneSavePath, quiet=True)            
        mxs.resetMaxFile(mxs.Name("noprompt"))

        mxs.loadMaxFile(maxSceneSavePath)
        loadedStageObject = mxs.getNodeByName(stageName)
        stage = stageCache.Find(Usd.StageCache.Id.FromLongInt(loadedStageObject.CacheId))

        # Check that the prim is in the default "inherited" state
        self.check_prim_vis(loadedStageObject.CacheId, primPath, UsdGeom.Tokens.inherited)

        # Check if the root layer is dirty -- it shouldn't be
        self.assertEqual(stage.GetRootLayer().dirty, False)   

        # 7 
        # Test UsdStageObject with anonymous root layer by default
        # Create a UsdStageObject without setting a root layer - should have anonymous root layer
        anonStageName = "anonStage"
        anonStageObj = mxs.USDStageObject(name=anonStageName)
        anonStageCache = UsdUtils.StageCache.Get()
        anonStage = anonStageCache.Find(Usd.StageCache.Id.FromLongInt(anonStageObj.CacheId))
        
        # Verify the root layer is anonymous
        self.assertTrue(anonStage.GetRootLayer().anonymous)
        
        # Create some content in the anonymous root layer
        anonStage.SetEditTarget(anonStage.GetRootLayer())
        anonRootPrim = anonStage.DefinePrim("/anonRoot", "Xform")
        anonCubePrim = anonStage.DefinePrim("/anonRoot/cube", "Cube")
        
        # Make the cube invisible
        anonCubeImg = UsdGeom.Imageable(anonCubePrim)
        anonCubeImg.MakeInvisible()
        
        # Save the scene with the anonymous root layer
        mxs.USDStageObject.SetDefaultSaveMode("saveAllEditsMax")
        anonSceneSavePath = self.output_prefix + "test_save_load_anonymous_root.max"
        mxs.saveMaxFile(anonSceneSavePath, quiet=True)
        
        anonCubeNotSavedPrim = anonStage.DefinePrim("/anonRoot/cubeNotSaved", "Cube")
        
        mxs.resetMaxFile(mxs.Name("noprompt"))
        
        # Load the scene and verify the anonymous root layer content is preserved
        mxs.loadMaxFile(anonSceneSavePath)
        loadedAnonStageObject = mxs.getNodeByName(anonStageName)
        loadedAnonStage = anonStageCache.Find(Usd.StageCache.Id.FromLongInt(loadedAnonStageObject.CacheId))
        
        # Verify the root layer is still anonymous
        self.assertTrue(loadedAnonStage.GetRootLayer().anonymous)
        
        # Verify the content is preserved
        loadedAnonRootPrim = loadedAnonStage.GetPrimAtPath("/anonRoot")
        self.assertTrue(loadedAnonRootPrim.IsValid())
        loadedAnonCubePrim = loadedAnonStage.GetPrimAtPath("/anonRoot/cube")
        self.assertTrue(loadedAnonCubePrim.IsValid())

        # Verify the content added after save is not there after load
        loadedAnonCubeNotSavedPrim = loadedAnonStage.GetPrimAtPath("/anonRoot/cubeNotSaved")
        self.assertFalse(loadedAnonCubeNotSavedPrim.IsValid())
        
        # Check that the cube is invisible
        loadedAnonCubeImg = UsdGeom.Imageable(loadedAnonCubePrim)
        self.assertEqual(loadedAnonCubeImg.GetVisibilityAttr().Get(), UsdGeom.Tokens.invisible)
        
        # Check if the anonymous root layer is dirty
        self.assertEqual(loadedAnonStage.GetRootLayer().dirty, True)
        
        mxs.resetMaxFile(mxs.Name("noprompt"))
        # 8
        # Test anonymous sublayers with file-based root layer
        # Create stage with file-based root layer and add anonymous sublayer
        anonSublayerStageName = "anonSublayerStage"
        anonSublayerStageObj = mxs.USDStageObject(name=anonSublayerStageName)
        anonSublayerStageObj.SetRootLayer(test_file_path, stageMask='/')
        anonSublayerStageCache = UsdUtils.StageCache.Get()
        anonSublayerStage = anonSublayerStageCache.Find(Usd.StageCache.Id.FromLongInt(anonSublayerStageObj.CacheId))
        
        # Use the proper command to add an anonymous sublayer
        addAnonSublayerCmd = UsdLayerEditor.AddAnonSubLayerCommand(anonSublayerStage, anonSublayerStage.GetRootLayer())
        addAnonSublayerCmd.execute()
        
        # Get the anonymous sublayer that was added
        anonSublayerPath = addAnonSublayerCmd.addedLayer()
        anonSublayer = Sdf.Layer.Find(anonSublayerPath)
        self.assertIsNotNone(anonSublayer)
        self.assertTrue(anonSublayer.anonymous)
        
        # Add content to the anonymous sublayer
        anonSublayerStage.SetEditTarget(anonSublayer)
        anonSublayerRootPrim = anonSublayerStage.DefinePrim("/anonSublayerRoot", "Xform")
        anonSublayerCylinderPrim = anonSublayerStage.DefinePrim("/anonSublayerRoot/cylinder", "Cylinder")
        
        # Make the cylinder invisible in the anonymous sublayer
        anonSublayerCylinderImg = UsdGeom.Imageable(anonSublayerCylinderPrim)
        anonSublayerCylinderImg.MakeInvisible()
        
        # Save the scene with anonymous sublayer
        anonSublayerSceneSavePath = self.output_prefix + "test_anonymous_sublayer.max"
        mxs.USDStageObject.SetDefaultSaveMode("saveAllEditsMax")
        mxs.saveMaxFile(anonSublayerSceneSavePath, quiet=True)
        saved_anon_sublayer_id = anonSublayer.identifier
        
        mxs.resetMaxFile(mxs.Name("noprompt"))
        
        # Load the scene and verify the anonymous sublayer content is preserved
        mxs.loadMaxFile(anonSublayerSceneSavePath)
        loadedAnonSublayerStageObject = mxs.getNodeByName(anonSublayerStageName)
        loadedAnonSublayerStage = anonSublayerStageCache.Find(Usd.StageCache.Id.FromLongInt(loadedAnonSublayerStageObject.CacheId))
        
        # Verify the root layer is file-based
        self.assertFalse(loadedAnonSublayerStage.GetRootLayer().anonymous)
        
        # Check that the anonymous sublayer is present
        containsAnonSublayer = False
        for sublayerPath in loadedAnonSublayerStage.GetRootLayer().subLayerPaths:
            if Sdf.Layer.IsAnonymousLayerIdentifier(sublayerPath):
                containsAnonSublayer = True
                break
        self.assertTrue(containsAnonSublayer)
        
        # Verify the content from the anonymous sublayer is preserved
        loadedAnonSublayerRootPrim = loadedAnonSublayerStage.GetPrimAtPath("/anonSublayerRoot")
        self.assertTrue(loadedAnonSublayerRootPrim.IsValid())
        loadedAnonSublayerCylinderPrim = loadedAnonSublayerStage.GetPrimAtPath("/anonSublayerRoot/cylinder")
        self.assertTrue(loadedAnonSublayerCylinderPrim.IsValid())
        
        # Check that the cylinder is invisible
        loadedAnonSublayerCylinderImg = UsdGeom.Imageable(loadedAnonSublayerCylinderPrim)
        self.assertEqual(loadedAnonSublayerCylinderImg.GetVisibilityAttr().Get(), UsdGeom.Tokens.invisible)
        
        # Find the anonymous sublayer and check if it's dirty
        anonSublayerFound = None
        for sublayerPath in loadedAnonSublayerStage.GetRootLayer().subLayerPaths:
            if Sdf.Layer.IsAnonymousLayerIdentifier(sublayerPath):
                anonSublayerFound = Sdf.Layer.Find(sublayerPath)
                break
                self.assertIsNotNone(anonSublayerFound)
        self.assertEqual(anonSublayerFound.dirty, True)
        
        mxs.resetMaxFile(mxs.Name("noprompt"))
        # 9 
        # Test anonymous root layer with anonymous sublayer, both with edits
        # Create a UsdStageObject without setting root layer - gets anonymous root by default
        anonRootWithSubStageName = "anonRootWithSubStage"
        anonRootWithSubStageObj = mxs.USDStageObject(name=anonRootWithSubStageName)
        anonRootWithSubStageCache = UsdUtils.StageCache.Get()
        anonRootWithSubStage = anonRootWithSubStageCache.Find(Usd.StageCache.Id.FromLongInt(anonRootWithSubStageObj.CacheId))
        
        # Verify the root layer is anonymous
        self.assertTrue(anonRootWithSubStage.GetRootLayer().anonymous)
        
        # Add content to the anonymous root layer and make edits
        anonRootWithSubStage.SetEditTarget(anonRootWithSubStage.GetRootLayer())
        rootContentPrim = anonRootWithSubStage.DefinePrim("/rootContent", "Xform")
        rootSpherePrim = anonRootWithSubStage.DefinePrim("/rootContent/sphere", "Sphere")
        
        # Make the sphere in root layer invisible
        rootSphereImg = UsdGeom.Imageable(rootSpherePrim)
        rootSphereImg.MakeInvisible()
        
        # Add an anonymous sublayer using the proper command
        addAnonSubCmd = UsdLayerEditor.AddAnonSubLayerCommand(anonRootWithSubStage, anonRootWithSubStage.GetRootLayer())
        addAnonSubCmd.execute()
        
        # Get the anonymous sublayer
        anonSubLayerPath = addAnonSubCmd.addedLayer()
        anonSubLayer = Sdf.Layer.Find(anonSubLayerPath)
        self.assertIsNotNone(anonSubLayer)
        self.assertTrue(anonSubLayer.anonymous)
        
        # Switch edit target to the anonymous sublayer and add content
        anonRootWithSubStage.SetEditTarget(anonSubLayer)
        subContentPrim = anonRootWithSubStage.DefinePrim("/subContent", "Xform")
        subCubePrim = anonRootWithSubStage.DefinePrim("/subContent/cube", "Cube")
        
        # Make the cube in sublayer invisible
        subCubeImg = UsdGeom.Imageable(subCubePrim)
        subCubeImg.MakeInvisible()
        
        # Verify both layers are dirty
        self.assertTrue(anonRootWithSubStage.GetRootLayer().dirty)
        self.assertTrue(anonSubLayer.dirty)
        
        # Save the scene with both anonymous layers having edits
        anonRootWithSubSceneSavePath = self.output_prefix + "test_anon_root_with_anon_sub.max"
        mxs.USDStageObject.SetDefaultSaveMode("saveAllEditsMax")
        mxs.saveMaxFile(anonRootWithSubSceneSavePath, quiet=True)
        saved_anon_root_id = anonRootWithSubStage.GetRootLayer().identifier
        saved_anon_sub_id = anonSubLayer.identifier
        
        mxs.resetMaxFile(mxs.Name("noprompt"))
        
        # Load the scene and verify both anonymous layers and their content are preserved
        mxs.loadMaxFile(anonRootWithSubSceneSavePath)
        loadedAnonRootWithSubStageObject = mxs.getNodeByName(anonRootWithSubStageName)
        loadedAnonRootWithSubStage = anonRootWithSubStageCache.Find(Usd.StageCache.Id.FromLongInt(loadedAnonRootWithSubStageObject.CacheId))
        
        # Verify the root layer is still anonymous
        self.assertTrue(loadedAnonRootWithSubStage.GetRootLayer().anonymous)
        
        # Verify the anonymous sublayer is present
        containsAnonSub = False
        loadedAnonSubLayerFound = None
        for sublayerPath in loadedAnonRootWithSubStage.GetRootLayer().subLayerPaths:
            if Sdf.Layer.IsAnonymousLayerIdentifier(sublayerPath):
                containsAnonSub = True
                loadedAnonSubLayerFound = Sdf.Layer.Find(sublayerPath)
                break
        self.assertTrue(containsAnonSub)
        self.assertIsNotNone(loadedAnonSubLayerFound)
        self.assertTrue(loadedAnonSubLayerFound.anonymous)
        
        # Verify content from the anonymous root layer is preserved
        loadedRootContentPrim = loadedAnonRootWithSubStage.GetPrimAtPath("/rootContent")
        self.assertTrue(loadedRootContentPrim.IsValid())
        loadedRootSpherePrim = loadedAnonRootWithSubStage.GetPrimAtPath("/rootContent/sphere")
        self.assertTrue(loadedRootSpherePrim.IsValid())
        
        # Check that the sphere in root layer is invisible
        loadedRootSphereImg = UsdGeom.Imageable(loadedRootSpherePrim)
        self.assertEqual(loadedRootSphereImg.GetVisibilityAttr().Get(), UsdGeom.Tokens.invisible)
        
        # Verify content from the anonymous sublayer is preserved
        loadedSubContentPrim = loadedAnonRootWithSubStage.GetPrimAtPath("/subContent")
        self.assertTrue(loadedSubContentPrim.IsValid())
        loadedSubCubePrim = loadedAnonRootWithSubStage.GetPrimAtPath("/subContent/cube")
        self.assertTrue(loadedSubCubePrim.IsValid())
        
        # Check that the cube in sublayer is invisible
        loadedSubCubeImg = UsdGeom.Imageable(loadedSubCubePrim)
        self.assertEqual(loadedSubCubeImg.GetVisibilityAttr().Get(), UsdGeom.Tokens.invisible)
        
        # Verify both layers are dirty after load (indicating edits were preserved)
        self.assertTrue(loadedAnonRootWithSubStage.GetRootLayer().dirty)
        self.assertTrue(loadedAnonSubLayerFound.dirty)
        
        # Reset the default save mode
        mxs.USDStageObject.SetDefaultSaveMode("saveAll")

    def test_get_ufe_prim_path(self):
        
        # Create a simple stage object.
        maxUsdObj = mxs.USDStageObject()
        maxUsdObj.SetRootLayer(self.test_usd_file_path, stageMask='/')
        maxUsdObjDefaultCreatedObjStr = maxUsd.GetUsdPrimUfePath(maxUsdObj.handle, "/")

        # Test stage object with no loaded usd stage.
        self.assertNotEqual("", maxUsdObjDefaultCreatedObjStr)
                        
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

        mxs.resetMaxFile(mxs.Name("noprompt"))

        # Now test anon sublayer cases
        stageName = "anon"
        stageObject = mxs.USDStageObject(name=stageName)

        stageCache = UsdUtils.StageCache.Get()
        stage = stageCache.Find(Usd.StageCache.Id.FromLongInt(stageObject.CacheId))
        rootLayer = stage.GetRootLayer()

        addedAnonSublayerCmd = UsdLayerEditor.AddAnonSubLayerCommand(stage, rootLayer)
        addedAnonSublayerCmd.execute();
        anonSublayerId = addedAnonSublayerCmd.addedLayer()
        anonSublayer = Sdf.Layer.Find(anonSublayerId)

        cmd = UsdLayerEditor.LockLayerCommand(stage, anonSublayer,  UsdLayerEditor.LayerLock_Locked, False, False)
        cmd.execute();

        # Add an anon sublayer to the session layer
        addedAnonSublayerCmd = UsdLayerEditor.AddAnonSubLayerCommand(stage, stage.GetSessionLayer())
        addedAnonSublayerCmd.execute();
        anonSublayerFromSessionId = addedAnonSublayerCmd.addedLayer()
        anonSublayerFromSession = Sdf.Layer.Find(anonSublayerFromSessionId)

        cmd = UsdLayerEditor.LockLayerCommand(stage, anonSublayerFromSession,  UsdLayerEditor.LayerLock_Locked, False, False)
        cmd.execute();

        self.assertFalse(anonSublayer.permissionToEdit)
        self.assertTrue(rootLayer.permissionToEdit)
        self.assertFalse(anonSublayerFromSession.permissionToEdit)
        self.assertTrue(stage.GetSessionLayer().permissionToEdit)

        maxSceneSavePath = self.output_prefix + "layer_lock_save_anon.max"
        mxs.USDStageObject.SetDefaultSaveMode("saveAllEditsMax")
        mxs.saveMaxFile(maxSceneSavePath, quiet=True)

        cmd = UsdLayerEditor.LockLayerCommand(stage, anonSublayer,  UsdLayerEditor.LayerLock_Unlocked, True, False)
        cmd.execute();
        cmd = UsdLayerEditor.LockLayerCommand(stage, anonSublayerFromSession,  UsdLayerEditor.LayerLock_Unlocked, True, False)
        cmd.execute();

        self.assertTrue(anonSublayer.permissionToEdit)
        self.assertTrue(rootLayer.permissionToEdit)
        self.assertTrue(anonSublayerFromSession.permissionToEdit)
        self.assertTrue(stage.GetSessionLayer().permissionToEdit)

        mxs.loadMaxFile(maxSceneSavePath)
        loadedStageObject = mxs.getNodeByName(stageName)
        stage = stageCache.Find(Usd.StageCache.Id.FromLongInt(loadedStageObject.CacheId))

        self.assertFalse(anonSublayer.permissionToEdit)
        self.assertTrue(rootLayer.permissionToEdit)
        self.assertFalse(anonSublayerFromSession.permissionToEdit)
        self.assertTrue(stage.GetSessionLayer().permissionToEdit)

        # Reset the default save mode
        mxs.USDStageObject.SetDefaultSaveMode("saveAll")

                
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

        mxs.resetMaxFile(mxs.Name("noprompt"))

        # Now test anon sublayer cases
        stageName = "anon"
        stageObject = mxs.USDStageObject(name=stageName)

        stageCache = UsdUtils.StageCache.Get()
        stage = stageCache.Find(Usd.StageCache.Id.FromLongInt(stageObject.CacheId))
        rootLayer = stage.GetRootLayer()

        # Add an anon sublayer to root layer
        addedAnonSublayerCmd = UsdLayerEditor.AddAnonSubLayerCommand(stage, rootLayer)
        addedAnonSublayerCmd.execute();
        anonSublayerFromRootId = addedAnonSublayerCmd.addedLayer()
        anonSublayerFromRoot = Sdf.Layer.Find(anonSublayerFromRootId)


        # Add an anon sublayer to the session layer
        addedAnonSublayerCmd = UsdLayerEditor.AddAnonSubLayerCommand(stage, stage.GetSessionLayer())
        addedAnonSublayerCmd.execute();
        anonSublayerFromSessionId = addedAnonSublayerCmd.addedLayer()
        anonSublayerFromSession = Sdf.Layer.Find(anonSublayerFromSessionId)

        cmd = UsdLayerEditor.MuteLayerCommand(stage, anonSublayerFromRoot,  True)
        cmd.execute();
        cmd = UsdLayerEditor.MuteLayerCommand(stage, anonSublayerFromSession,  True)
        cmd.execute();

        self.assertTrue(stage.IsLayerMuted(anonSublayerFromRoot.identifier))
        self.assertFalse(stage.IsLayerMuted(rootLayer.identifier))
        self.assertTrue(stage.IsLayerMuted(anonSublayerFromSession.identifier))
        self.assertFalse(stage.IsLayerMuted(stage.GetSessionLayer().identifier))
        
        maxSceneSavePath = self.output_prefix + "layer_mute_save_anon.max"
        mxs.USDStageObject.SetDefaultSaveMode("saveAllEditsMax")
        mxs.saveMaxFile(maxSceneSavePath, quiet=True)

        cmd = UsdLayerEditor.MuteLayerCommand(stage, anonSublayerFromRoot,  False)
        cmd.execute();
        cmd = UsdLayerEditor.MuteLayerCommand(stage, anonSublayerFromSession,  False)
        cmd.execute();

        self.assertFalse(stage.IsLayerMuted(anonSublayerFromRoot.identifier))
        self.assertFalse(stage.IsLayerMuted(rootLayer.identifier))
        self.assertFalse(stage.IsLayerMuted(anonSublayerFromSession.identifier))
        self.assertFalse(stage.IsLayerMuted(stage.GetSessionLayer().identifier))

        mxs.loadMaxFile(maxSceneSavePath)
        loadedStageObject = mxs.getNodeByName(stageName)
        stage = stageCache.Find(Usd.StageCache.Id.FromLongInt(loadedStageObject.CacheId))
        self.assertTrue(stage.IsLayerMuted(anonSublayerFromRoot.identifier))
        self.assertFalse(stage.IsLayerMuted(rootLayer.identifier))
        self.assertTrue(stage.IsLayerMuted(anonSublayerFromSession.identifier))
        self.assertFalse(stage.IsLayerMuted(stage.GetSessionLayer().identifier))

        # Reset the default save mode
        mxs.USDStageObject.SetDefaultSaveMode("saveAll")


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

        mxs.resetMaxFile(mxs.Name("noprompt"))

        # Now test anon sublayer cases using command system
        stageObject = mxs.USDStageObject(name=stageName)
        stageCache = UsdUtils.StageCache.Get()
        stage = stageCache.Find(Usd.StageCache.Id.FromLongInt(stageObject.CacheId))
        rootLayer = stage.GetRootLayer()

        # Add an anon sublayer to root layer
        addedAnonSublayerCmd = UsdLayerEditor.AddAnonSubLayerCommand(stage, rootLayer)
        addedAnonSublayerCmd.execute();
        anonSublayerFromRootId = addedAnonSublayerCmd.addedLayer()
        anonSublayerFromRoot = Sdf.Layer.Find(anonSublayerFromRootId)
        
        stage.SetEditTarget(anonSublayerFromRoot)
        maxSceneSavePath = self.output_prefix + "save_sublayer_target.max"
        mxs.USDStageObject.SetDefaultSaveMode("saveAllEditsMax")
        mxs.saveMaxFile(maxSceneSavePath, quiet=True)
        
        mxs.resetMaxFile(mxs.Name("noprompt"))

        # Load the scene from disk, make sure the edit target was properly restored.
        mxs.loadMaxFile(maxSceneSavePath, quiet=True)
        loadedStageObject = mxs.getNodeByName(stageName)
        stage = stageCache.Find(Usd.StageCache.Id.FromLongInt(loadedStageObject.CacheId))
        rootLayer = stage.GetRootLayer()
        anonSublayerFromRoot = Sdf.Layer.FindRelativeToLayer(rootLayer, rootLayer.subLayerPaths[0])
        self.assertEqual(stage.GetEditTarget(), anonSublayerFromRoot) 

        # Reset the default save mode
        mxs.USDStageObject.SetDefaultSaveMode("saveAll")


    def test_undo_redo_stage_root_layer_preservation(self):
        """Test that undo/redo operations preserve stage objects when switching between anonymous and file-backed root layers"""
        # Create a USDStageObject which by default comes with an anonymous root layer
        stageName = "testStage"
        stageObject = mxs.USDStageObject(name=stageName)
        
        # Verify the stage starts with an anonymous root layer
        stageCache = UsdUtils.StageCache.Get()
        initialStage = stageCache.Find(Usd.StageCache.Id.FromLongInt(stageObject.CacheId))
        self.assertTrue(initialStage.GetRootLayer().anonymous)
        
        # Add some content to the anonymous root layer to make it more interesting
        initialStage.SetEditTarget(initialStage.GetRootLayer())
        rootPrim = initialStage.DefinePrim("/testRoot", "Xform")
        spherePrim = initialStage.DefinePrim("/testRoot/sphere", "Sphere")
        
        # set the root layer to a file-backed layer
        stageObject.SetRootLayer(self.test_usd_file_path, stageMask='/')
        
        # Verify the root layer changed to file-backed
        fileBackedStage = stageCache.Find(Usd.StageCache.Id.FromLongInt(stageObject.CacheId))
        self.assertFalse(fileBackedStage.GetRootLayer().anonymous)
        
        # Verify the stage content changed (should have the box from the file now)
        boxPrim = fileBackedStage.GetPrimAtPath("/box")
        self.assertTrue(boxPrim.IsValid())
        
        # Undo the operation
        pymxs.run_undo()
        
        # Verify we're back to the anonymous root layer
        undoStage = stageCache.Find(Usd.StageCache.Id.FromLongInt(stageObject.CacheId))
        self.assertTrue(undoStage.GetRootLayer().anonymous)
        
        # Verify the stage object is preserved after undo
        self.assertEqual(initialStage, undoStage)
        
        # Verify the original anonymous content is back
        undoRootPrim = undoStage.GetPrimAtPath("/testRoot")
        self.assertTrue(undoRootPrim.IsValid())
        undoSpherePrim = undoStage.GetPrimAtPath("/testRoot/sphere")
        self.assertTrue(undoSpherePrim.IsValid())
        
        # Redo the operation
        pymxs.run_redo()
        
        # Verify we're back to the file-backed root layer
        redoStage = stageCache.Find(Usd.StageCache.Id.FromLongInt(stageObject.CacheId))
        self.assertFalse(redoStage.GetRootLayer().anonymous)
        
        # Verify the stage object is preserved after redo
        self.assertEqual(fileBackedStage, redoStage)
        
        # Verify the file-backed content is back
        redoBoxPrim = redoStage.GetPrimAtPath("/box")
        self.assertTrue(redoBoxPrim.IsValid())


    def test_undo_redo_guarded_ufe_commands(self):
        """Test that multiple commands executed inside a UFE UndoableCommandGuard
        are registered on the 3ds Max undo stack as a single composite operation
        via MaxUfeUndoableCommandMgr::registerCmd, so that one undo reverts both."""
        maxUsdObj = mxs.USDStageObject()
        maxUsdObj.SetRootLayer(self.test_usd_file_path, stageMask='/')
        stageCache = UsdUtils.StageCache.Get()
        stage = stageCache.Find(Usd.StageCache.Id.FromLongInt(maxUsdObj.CacheId))
        rootLayer = stage.GetRootLayer()
        self.assertEqual(len(rootLayer.subLayerPaths), 0)

        with ufe.UndoableCommandGuard("TestGuardedOp") as guard:
            mgr = ufe.UndoableCommandMgr.instance()
            cmd1 = UsdLayerEditor.AddAnonSubLayerCommand(stage, rootLayer)
            mgr.executeCmd(cmd1)
            addedLayerId1 = cmd1.addedLayer()
            cmd2 = UsdLayerEditor.AddAnonSubLayerCommand(stage, rootLayer)
            mgr.executeCmd(cmd2)
            addedLayerId2 = cmd2.addedLayer()
            guard.setSuccess()

        self.assertEqual(len(rootLayer.subLayerPaths), 2)
        self.assertIn(addedLayerId1, rootLayer.subLayerPaths)
        self.assertIn(addedLayerId2, rootLayer.subLayerPaths)

        # Single undo must revert both additions (composite registered via registerCmd).
        pymxs.run_undo()
        self.assertEqual(len(rootLayer.subLayerPaths), 0)

        # Single redo must re-apply both.
        pymxs.run_redo()
        self.assertEqual(len(rootLayer.subLayerPaths), 2)
        self.assertIn(addedLayerId1, rootLayer.subLayerPaths)
        self.assertIn(addedLayerId2, rootLayer.subLayerPaths)

    def test_default_anonymous_stage_settings(self):
        """Test that USDStageObject with default anonymous root has 3dsMax default TPS, FPS, Units, and Z up-axis"""
        # Create a USDStageObject with default anonymous root layer
        stageObject = mxs.USDStageObject()
        
        # Get the stage from cache
        stageCache = UsdUtils.StageCache.Get()
        stage = stageCache.Find(Usd.StageCache.Id.FromLongInt(stageObject.CacheId))
        
        # Verify the root layer is anonymous
        self.assertTrue(stage.GetRootLayer().anonymous)
        
        # Test default time codes per second (TPS)
        expectedTPS = 4800.0 / mxs.ticksPerFrame
        actualTPS = stage.GetTimeCodesPerSecond()
        self.assertEqual(actualTPS, expectedTPS, 
                        f"Expected TPS {expectedTPS}, got {actualTPS}")
        
        # Test default frames per second (FPS)
        expectedFPS = expectedTPS
        actualFPS = stage.GetFramesPerSecond()
        self.assertEqual(actualFPS, expectedFPS,
                        f"Expected FPS {expectedFPS}, got {actualFPS}")
        
        # Test default units - default 3ds Max units are in inches
        expectedMetersPerUnit = 0.0254
        actualMetersPerUnit = UsdGeom.GetStageMetersPerUnit(stage)
        self.assertAlmostEqual(actualMetersPerUnit, expectedMetersPerUnit, places=6,
                              msg=f"Expected meters per unit {expectedMetersPerUnit}, got {actualMetersPerUnit} (in inches)")
        
        # Test default up-axis - should be Z
        expectedUpAxis = UsdGeom.Tokens.z
        actualUpAxis = UsdGeom.GetStageUpAxis(stage)
        self.assertEqual(actualUpAxis, expectedUpAxis,
                        f"Expected up-axis {expectedUpAxis}, got {actualUpAxis}")

    def test_set_stage_from_cache(self):
        """Test SetStageFromCache functionality with undo/redo and SetRootLayer combinations"""
        # Create anonymous stages from scratch using pxr APIs
        stageCache = UsdUtils.StageCache.Get()
        
        # Create first anonymous stage with a sphere
        stage1 = Usd.Stage.CreateInMemory()  # Anonymous stage
        spherePrim = UsdGeom.Sphere.Define(stage1, "/sphere")
        spherePrim.GetRadiusAttr().Set(5.0)
        cubePrim = UsdGeom.Cube.Define(stage1, "/cube")
        cubePrim.GetSizeAttr().Set(2.0)
        
        # Insert into cache and get cache ID
        stageCache.Insert(stage1)
        stage1Id = stageCache.GetId(stage1).ToLongInt()
        
        # Create second anonymous stage with different content
        stage2 = Usd.Stage.CreateInMemory()  # Anonymous stage
        conePrim = UsdGeom.Cone.Define(stage2, "/cone")
        conePrim.GetHeightAttr().Set(10.0)
        conePrim.GetRadiusAttr().Set(3.0)
        cylinderPrim = UsdGeom.Cylinder.Define(stage2, "/cylinder")
        cylinderPrim.GetHeightAttr().Set(8.0)
        
        # Insert into cache and get cache ID
        stageCache.Insert(stage2)
        stage2Id = stageCache.GetId(stage2).ToLongInt()
        
        # Create a USDStageObject
        stageName = "testStageFromCache"
        stageObject = mxs.USDStageObject(name=stageName)
        
        # Add some prims to the initial stage in the USDStageObject
        initialStage = stageCache.Find(Usd.StageCache.Id.FromLongInt(stageObject.CacheId))
        initialStage.SetEditTarget(initialStage.GetRootLayer())
        initialRootPrim = initialStage.DefinePrim("/initialRoot", "Xform")
        initialPlanePrim = initialStage.DefinePrim("/initialRoot/plane", "Mesh")
        
        # Test setting stage from cache with first stage
        stageObject.SetStageFromCache(stage1Id)
        
        # Verify the stage object now uses the cached stage
        currentStage = stageCache.Find(Usd.StageCache.Id.FromLongInt(stageObject.CacheId))
        self.assertEqual(currentStage, stage1)
        
        # Verify the prims from stage1 are accessible
        spherePrimFromStage = currentStage.GetPrimAtPath("/sphere")
        self.assertTrue(spherePrimFromStage.IsValid())
        cubePrimFromStage = currentStage.GetPrimAtPath("/cube")
        self.assertTrue(cubePrimFromStage.IsValid())
        
        # Verify sphere properties
        sphereFromStage = UsdGeom.Sphere(spherePrimFromStage)
        self.assertEqual(sphereFromStage.GetRadiusAttr().Get(), 5.0)
        
        # Test undo functionality
        pymxs.run_undo()
        
        # After undo, should be back to the original anonymous stage
        undoStage = stageCache.Find(Usd.StageCache.Id.FromLongInt(stageObject.CacheId))
        self.assertTrue(undoStage.GetRootLayer().anonymous)
        
        # The sphere and cube should not be present in the original stage
        self.assertFalse(undoStage.GetPrimAtPath("/sphere").IsValid())
        self.assertFalse(undoStage.GetPrimAtPath("/cube").IsValid())
        
        # Check that the initial prims we added are still there after undo
        undoInitialRootPrim = undoStage.GetPrimAtPath("/initialRoot")
        self.assertTrue(undoInitialRootPrim.IsValid())
        undoInitialPlanePrim = undoStage.GetPrimAtPath("/initialRoot/plane")
        self.assertTrue(undoInitialPlanePrim.IsValid())

        # Test redo functionality
        pymxs.run_redo()
        
        # After redo, should be back to stage1
        redoStage = stageCache.Find(Usd.StageCache.Id.FromLongInt(stageObject.CacheId))
        self.assertEqual(redoStage, stage1)
        self.assertTrue(redoStage.GetPrimAtPath("/sphere").IsValid())
        self.assertTrue(redoStage.GetPrimAtPath("/cube").IsValid())
        
        # Test switching to second stage from cache
        stageObject.SetStageFromCache(stage2Id)
        
        # Verify the stage object now uses the second cached stage
        currentStage2 = stageCache.Find(Usd.StageCache.Id.FromLongInt(stageObject.CacheId))
        self.assertEqual(currentStage2, stage2)
        
        # Verify the prims from stage2 are accessible
        conePrimFromStage = currentStage2.GetPrimAtPath("/cone")
        self.assertTrue(conePrimFromStage.IsValid())
        cylinderPrimFromStage = currentStage2.GetPrimAtPath("/cylinder")
        self.assertTrue(cylinderPrimFromStage.IsValid())
        
        # Verify cone properties
        coneFromStage = UsdGeom.Cone(conePrimFromStage)
        self.assertEqual(coneFromStage.GetHeightAttr().Get(), 10.0)
        self.assertEqual(coneFromStage.GetRadiusAttr().Get(), 3.0)
    

def run_tests():
    return unittest.TextTestRunner(stream=sys.stdout, verbosity=2).run(unittest.TestLoader().loadTestsFromTestCase(TestStageGeneral))

if __name__ == '__main__':
    run_tests()
