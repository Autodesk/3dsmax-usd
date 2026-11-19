#
# Copyright 2026 Autodesk
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

from pxr import Usd, UsdRender
import usd_test_helpers

from pymxs import runtime as mxs

import sys
import unittest

# Registered by USDMenuToolsRenderSetupActionItem in UsdMenu.h / UsdMenu.cpp.
kRenderSetupActionId = 42262
kUsdMenuActionTableId = 0x29760F36


def _findUsdMenuActionTable():
    """Return the USD menu ActionTable, or None if it is not registered."""
    actionMan = mxs.actionMan
    for tableIndex in range(1, actionMan.numActionTables + 1):
        actionTable = actionMan.getActionTable(tableIndex)
        if actionTable is not None and actionTable.id == kUsdMenuActionTableId:
            return actionTable
    return None


def _findRenderSetupActionItem(actionTable):
    """Return the Render Setup ActionItem from a table, or None if missing."""
    for itemIndex in range(1, actionTable.numActionItems + 1):
        actionItem = actionTable.getActionItem(itemIndex)
        if actionItem is not None and actionItem.id == kRenderSetupActionId:
            return actionItem
    return None


def _executeRenderSetupAction():
    """Execute the USD Render Setup action via actionMan."""
    return mxs.actionMan.executeAction(
        kUsdMenuActionTableId, str(kRenderSetupActionId))


class testRenderSetup(unittest.TestCase):
    def setUp(self):
        mxs.resetMaxFile(mxs.Name('NOPROMPT'))
        usd_test_helpers.load_usd_plugins()

    def testRenderSetupMenuActionExists(self):
        """The USD Render Setup action item must be registered in the USD action table."""
        action_table = _findUsdMenuActionTable()
        self.assertIsNotNone(action_table, "USD Menu Action Table was not found")

        action_item = _findRenderSetupActionItem(action_table)
        self.assertIsNotNone(
            action_item,
            "Render Setup action item (id=42262) was not found in the USD action table")
        self.assertTrue(
            action_item.isEnabled,
            "Render Setup action item should be enabled")

    def testRenderSetupWithLoadedStage(self):
        """Opening the Render Setup dialog with a loaded stage must not raise."""
        # Create a simple in-memory stage with a UsdRenderSettings prim so that
        # the library has something to display.
        stage = Usd.Stage.CreateInMemory()
        UsdRender.Settings.Define(stage, "/Render/PrimaryRenderSettings")
        stage.SetMetadata("renderSettingsPrimPath", "/Render/PrimaryRenderSettings")

        # Create a USDStageObject backed by a temp file so it shows in the stage map.
        import tempfile, os
        tmp = tempfile.NamedTemporaryFile(suffix=".usda", delete=False)
        tmp_path = tmp.name
        tmp.close()
        try:
            stage.GetRootLayer().Export(tmp_path)
            stageNode = mxs.USDStageObject()
            stageNode.RootLayer = tmp_path

            action_table = _findUsdMenuActionTable()
            self.assertIsNotNone(action_table, "USD Menu Action Table was not found")
            action_item = _findRenderSetupActionItem(action_table)
            self.assertIsNotNone(
                action_item,
                "Render Setup action item (id=42262) was not found in the USD action table")
            self.assertTrue(action_item.isEnabled, "Render Setup action item should be enabled")
            self.assertTrue(
                _executeRenderSetupAction(),
                "Render Setup action failed to execute")
        finally:
            try:
                os.unlink(tmp_path)
            except OSError:
                pass


def run_tests():
    return unittest.TextTestRunner(stream=sys.stdout, verbosity=2).run(
        unittest.TestLoader().loadTestsFromTestCase(testRenderSetup)
    )


if __name__ == "__main__":
    from importlib import reload
    import usd_utils
    reload(usd_utils)
    usd_utils.get_config_data(update=True)
    mxs.clearListener()
    run_tests()
