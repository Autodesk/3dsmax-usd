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

class importChaserFirstTest(maxUsd.ImportChaser):
    PostImportCalled = False
    stage = None
    ChaserNames = set()
    ChaserArgs = {}
    context = {}
    filename = ""

    def __init__(self, factoryContext, *args, **kwargs):
        super(importChaserFirstTest, self).__init__(factoryContext, *args, **kwargs)
        importChaserFirstTest.context = factoryContext.GetContext()
        importChaserFirstTest.stage = factoryContext.GetStage()
        importChaserFirstTest.ChaserNames = factoryContext.GetJobArgs().GetChaserNames()
        importChaserFirstTest.ChaserArgs = factoryContext.GetJobArgs().GetAllChaserArgs()['first']
        importChaserFirstTest.filename = factoryContext.GetFilename()

    def PostImport(self):
        importChaserFirstTest.PostImportCalled = True
        return True

class importChaserSecondTest(maxUsd.ImportChaser):
    PostImportCalled = False
    stage = None
    ChaserNames = set()
    ChaserArgs = {}
    context = {}
    filename = ""

    def __init__(self, factoryContext, *args, **kwargs):
        super(importChaserSecondTest, self).__init__(factoryContext, *args, **kwargs)
        importChaserSecondTest.context = factoryContext.GetContext()
        importChaserSecondTest.stage = factoryContext.GetStage()
        importChaserSecondTest.ChaserNames = factoryContext.GetJobArgs().GetChaserNames()
        importChaserSecondTest.ChaserArgs = factoryContext.GetJobArgs().GetAllChaserArgs()['second']
        importChaserSecondTest.filename = factoryContext.GetFilename()

    def PostImport(self):
        importChaserSecondTest.PostImportCalled = True
        return True

class testImportChaser(unittest.TestCase):
    def setUp(self):
        self.output_prefix = usd_test_helpers.standard_output_prefix("TEST_IMPORT_CHASER_")
        
        mxs.resetMaxFile(mxs.Name('NOPROMPT'))
        usd_test_helpers.load_usd_plugins()

    def testSimpleImportChaser(self):
        import_file_path = os.path.dirname(__file__) + "\\data\\box_sample.usda"
        maxUsd.ImportChaser.Register(importChaserFirstTest, "first", "Chaser First Test", "This is the first chaser test description")
        maxUsd.ImportChaser.Register(importChaserSecondTest, "second")

        # Import usd file with args from a dictionary
        import_options = mxs.USDImporter.CreateOptions()
        import_options.ChaserNames = mxs.Array("first")
        import_options.AllChaserArgs = mxs.Dictionary(mxs.Name('string'), mxs.Array('first', (mxs.Dictionary(mxs.Name('string'), mxs.Array("bar", "ometer"), mxs.Array("foo", "tball")))))
        ret = mxs.USDImporter.ImportFile(import_file_path, importOptions=import_options)

        self.assertTrue(ret)
        self.assertTrue(importChaserFirstTest.PostImportCalled)
        self.assertFalse(importChaserSecondTest.PostImportCalled)
        self.assertIsNotNone(importChaserFirstTest.stage)
        self.assertIsNone(importChaserSecondTest.stage)
        self.assertEqual(len(importChaserFirstTest.ChaserNames), 1)
        self.assertTrue('first' in importChaserFirstTest.ChaserNames)
        self.assertEqual(importChaserFirstTest.ChaserArgs,{'bar': 'ometer', 'foo': 'tball'})
        self.assertEqual(importChaserFirstTest.filename, import_file_path)
        self.assertEqual(importChaserSecondTest.filename, "")

        # Import using the first chaser with array args
        importChaserFirstTest.PostImportCalled = False
        importChaserFirstTest.stage = None
        importChaserFirstTest.filename = ""
        import_options.AllChaserArgs = mxs.Array("first", "bar", "baric", "first", "foo", "tlocker")
        ret = mxs.USDImporter.ImportFile(import_file_path, importOptions=import_options)
        
        self.assertTrue(ret)
        self.assertTrue(importChaserFirstTest.PostImportCalled)
        self.assertFalse(importChaserSecondTest.PostImportCalled)
        self.assertIsNotNone(importChaserFirstTest.stage)
        self.assertIsNone(importChaserSecondTest.stage)
        self.assertEqual(len(importChaserFirstTest.ChaserNames), 1)
        self.assertTrue('first' in importChaserFirstTest.ChaserNames)
        self.assertEqual(importChaserFirstTest.ChaserArgs,{'bar': 'baric', 'foo': 'tlocker'})
        self.assertEqual(importChaserFirstTest.filename, import_file_path)
        self.assertEqual(importChaserSecondTest.filename, "")

        #Import using both chasers with their own args
        importChaserFirstTest.PostImportCalled = False
        importChaserFirstTest.stage = None
        importChaserFirstTest.filename = ""
        import_options.AllChaserArgs = mxs.Array("first", "bar", "baric", "second", "bar", "ometer", "first", "foo", "tlocker", "second", "foo", "tball")
        import_options.ChaserNames = mxs.Array("first", "second")
        ret = mxs.USDImporter.ImportFile(import_file_path, importOptions=import_options)

        self.assertTrue(ret)
        self.assertTrue(importChaserFirstTest.PostImportCalled)
        self.assertTrue(importChaserSecondTest.PostImportCalled)
        self.assertIsNotNone(importChaserFirstTest.stage)
        self.assertIsNotNone(importChaserSecondTest.stage)
        self.assertEqual(len(importChaserFirstTest.ChaserNames), 2)
        self.assertEqual(len(importChaserSecondTest.ChaserNames), 2)
        self.assertTrue('first' in importChaserFirstTest.ChaserNames)
        self.assertTrue('second' in importChaserSecondTest.ChaserNames)
        self.assertEqual(importChaserFirstTest.ChaserArgs,{'bar': 'baric', 'foo': 'tlocker'})
        self.assertEqual(importChaserSecondTest.ChaserArgs,{'bar': 'ometer', 'foo': 'tball'})
        self.assertEqual(importChaserFirstTest.filename, import_file_path)
        self.assertEqual(importChaserSecondTest.filename, import_file_path)

def run_tests():
    return unittest.TextTestRunner(stream=sys.stdout, verbosity=2).run(
        unittest.TestLoader().loadTestsFromTestCase(testImportChaser)
    )

if __name__ == "__main__":
    from importlib import reload
    import usd_utils
    reload(usd_utils)
    usd_utils.get_config_data(update=True)
    mxs.clearListener()
    run_tests()