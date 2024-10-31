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
import usd_test_helpers

from pymxs import runtime as mxs

import os, sys, glob
import unittest

class exportChaserFirstTest(maxUsd.ExportChaser):
    PostExportCalled = False
    stage = None
    ChaserNames = set()
    ChaserArgs = {}
    primsToNodeHandles = {}
    filename = ""

    def __init__(self, factoryContext, *args, **kwargs):
        super(exportChaserFirstTest, self).__init__(factoryContext, *args, **kwargs)
        exportChaserFirstTest.stage = factoryContext.GetStage()
        exportChaserFirstTest.primsToNodeHandles = factoryContext.GetPrimsToNodeHandles()
        exportChaserFirstTest.ChaserNames = factoryContext.GetJobArgs().GetChaserNames()
        exportChaserFirstTest.ChaserArgs = factoryContext.GetJobArgs().GetAllChaserArgs()['first']
        exportChaserFirstTest.filename = factoryContext.GetFilename()

    def PostExport(self):
        exportChaserFirstTest.PostExportCalled = True
        return True

class exportChaserSecondTest(maxUsd.ExportChaser):
    PostExportCalled = False
    stage = None
    ChaserNames = set()
    ChaserArgs = {}
    primsToNodeHandles = {}
    filename = ""

    def __init__(self, factoryContext, *args, **kwargs):
        super(exportChaserSecondTest, self).__init__(factoryContext, *args, **kwargs)
        exportChaserSecondTest.stage = factoryContext.GetStage()
        exportChaserSecondTest.primsToNodeHandles = factoryContext.GetPrimsToNodeHandles()
        exportChaserSecondTest.ChaserNames = factoryContext.GetJobArgs().GetChaserNames()
        exportChaserSecondTest.ChaserArgs = factoryContext.GetJobArgs().GetAllChaserArgs()['second']
        exportChaserSecondTest.filename = factoryContext.GetFilename()

    def PostExport(self):
        exportChaserSecondTest.PostExportCalled = True
        return True

class testExportChaser(unittest.TestCase):
    def setUp(self):
        self.output_prefix = usd_test_helpers.standard_output_prefix("TEST_EXPORT_CHASER_")
        
        mxs.resetMaxFile(mxs.Name('NOPROMPT'))
        usd_test_helpers.load_usd_plugins()

    def testSimpleExportChaser(self):
        maxUsd.ExportChaser.Register(exportChaserFirstTest, "first", "Chaser First Test", "This is the first chaser test description")
        maxUsd.ExportChaser.Register(exportChaserSecondTest, "second")
        test_sphere = mxs.sphere(r = 3.5, name='apple')

        # export using a dictionary as the chaser args
        test_usd_file_path = self.output_prefix + "simple_chaser.usda"
        export_options = mxs.USDExporter.CreateOptions()
        export_options.FileFormat = mxs.Name("ascii")
        export_options.RootPrimPath = "/"
        export_options.ChaserNames = mxs.Array("first")
        export_options.AllChaserArgs = mxs.Dictionary(mxs.Name('string'), mxs.Array('first', (mxs.Dictionary(mxs.Name('string'), mxs.Array("bar", "ometer"), mxs.Array("foo", "tball")))))
        ret = mxs.USDExporter.ExportFile(
            test_usd_file_path,
            exportOptions=export_options,
            contentSource=mxs.Name("nodeList"),
            nodeList=[test_sphere],
        )
        self.assertTrue(ret)

        self.assertTrue(exportChaserFirstTest.PostExportCalled)
        self.assertFalse(exportChaserSecondTest.PostExportCalled)
        self.assertIsNotNone(exportChaserFirstTest.stage)
        self.assertIsNone(exportChaserSecondTest.stage)
        self.assertEqual(list(exportChaserFirstTest.primsToNodeHandles.keys())[0], '/apple')
        self.assertEqual(len(exportChaserFirstTest.ChaserNames), 1)
        self.assertTrue('first' in exportChaserFirstTest.ChaserNames)
        self.assertEqual(exportChaserFirstTest.ChaserArgs,{'bar': 'ometer', 'foo': 'tball'})
        self.assertEqual(exportChaserFirstTest.filename, test_usd_file_path)
        self.assertEqual(exportChaserSecondTest.filename, "")

        # now set the chaser args using an Array and export again
        test_usd_file_path = self.output_prefix + "new_args_array_chaser.usda"
        exportChaserFirstTest.stage = None
        exportChaserFirstTest.PostExportCalled = False
        exportChaserFirstTest.filename = ""
        export_options.AllChaserArgs = mxs.Array("first", "bar", "baric", "first", "foo", "tlocker")
        ret = mxs.USDExporter.ExportFile(
            test_usd_file_path,
            exportOptions=export_options,
            contentSource=mxs.Name("nodeList"),
            nodeList=[test_sphere],
        )
        self.assertTrue(ret)

        self.assertTrue(exportChaserFirstTest.PostExportCalled)
        self.assertFalse(exportChaserSecondTest.PostExportCalled)
        self.assertIsNotNone(exportChaserFirstTest.stage)
        self.assertIsNone(exportChaserSecondTest.stage)
        self.assertEqual(list(exportChaserFirstTest.primsToNodeHandles.keys())[0], '/apple')
        self.assertEqual(len(exportChaserFirstTest.ChaserNames), 1)
        self.assertTrue('first' in exportChaserFirstTest.ChaserNames)
        self.assertEqual(exportChaserFirstTest.ChaserArgs,{'bar': 'baric', 'foo': 'tlocker'})
        self.assertEqual(exportChaserFirstTest.filename, test_usd_file_path)
        self.assertEqual(exportChaserSecondTest.filename, "")

        # now call 2 export chasers with their own arguments
        test_usd_file_path = self.output_prefix + "two_export_chasers.usda"
        exportChaserFirstTest.stage = None
        exportChaserFirstTest.PostExportCalled = False
        exportChaserFirstTest.filename = ""
        export_options.AllChaserArgs = mxs.Array("first", "bar", "baric", "second", "bar", "ometer", "first", "foo", "tlocker", "second", "foo", "tball")
        export_options.ChaserNames = mxs.Array("first", "second")
        ret = mxs.USDExporter.ExportFile(
            test_usd_file_path,
            exportOptions=export_options,
            contentSource=mxs.Name("nodeList"),
            nodeList=[test_sphere],
        )
        self.assertTrue(ret)

        self.assertTrue(exportChaserFirstTest.PostExportCalled)
        self.assertTrue(exportChaserSecondTest.PostExportCalled)
        self.assertIsNotNone(exportChaserFirstTest.stage)
        self.assertIsNotNone(exportChaserSecondTest.stage)
        self.assertEqual(list(exportChaserFirstTest.primsToNodeHandles.keys())[0], '/apple')
        self.assertEqual(list(exportChaserSecondTest.primsToNodeHandles.keys())[0], '/apple')
        self.assertEqual(len(exportChaserFirstTest.ChaserNames), 2)
        self.assertEqual(len(exportChaserSecondTest.ChaserNames), 2)
        self.assertTrue('first' in exportChaserFirstTest.ChaserNames)
        self.assertTrue('second' in exportChaserSecondTest.ChaserNames)
        self.assertEqual(exportChaserFirstTest.ChaserArgs,{'bar': 'baric', 'foo': 'tlocker'})
        self.assertEqual(exportChaserSecondTest.ChaserArgs,{'bar': 'ometer', 'foo': 'tball'})
        self.assertEqual(exportChaserFirstTest.filename, test_usd_file_path)
        self.assertEqual(exportChaserSecondTest.filename, test_usd_file_path)


def run_tests():
    return unittest.TextTestRunner(stream=sys.stdout, verbosity=2).run(
        unittest.TestLoader().loadTestsFromTestCase(testExportChaser)
    )


if __name__ == "__main__":
    from importlib import reload
    import usd_utils
    reload(usd_utils)
    usd_utils.get_config_data(update=True)
    mxs.clearListener()
    run_tests()
