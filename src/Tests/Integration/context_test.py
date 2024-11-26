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
    ChaserNames = set()
    ChaserArgs = {}
    AllMaterialTargets = {}
    primsToNodeHandles = {}
    filename = ""

    def __init__(self, factoryContext, *args, **kwargs):
        super(exportChaserFirstTest, self).__init__(factoryContext, *args, **kwargs)
        exportChaserFirstTest.primsToNodeHandles = factoryContext.GetPrimsToNodeHandles()
        exportChaserFirstTest.ChaserNames = factoryContext.GetJobArgs().GetChaserNames()
        exportChaserFirstTest.ChaserArgs = factoryContext.GetJobArgs().GetAllChaserArgs()['firstExport']
        exportChaserFirstTest.AllMaterialTargets = factoryContext.GetJobArgs().GetAllMaterialConversions()
        exportChaserFirstTest.ContextNames = factoryContext.GetJobArgs().GetContextNames()
        exportChaserFirstTest.filename = factoryContext.GetFilename()

    def PostExport(self):
        exportChaserFirstTest.PostExportCalled = True
        return True

class exportChaserSecondTest(maxUsd.ExportChaser):
    PostExportCalled = False
    ChaserNames = set()
    ChaserArgs = {}
    AllMaterialTargets = {}
    primsToNodeHandles = {}
    filename = ""

    def __init__(self, factoryContext, *args, **kwargs):
        super(exportChaserSecondTest, self).__init__(factoryContext, *args, **kwargs)
        exportChaserSecondTest.primsToNodeHandles = factoryContext.GetPrimsToNodeHandles()
        exportChaserSecondTest.ChaserNames = factoryContext.GetJobArgs().GetChaserNames()
        if 'secondExport' in factoryContext.GetJobArgs().GetAllChaserArgs():
            exportChaserSecondTest.ChaserArgs = factoryContext.GetJobArgs().GetAllChaserArgs()['secondExport']
        exportChaserSecondTest.AllMaterialTargets = factoryContext.GetJobArgs().GetAllMaterialConversions()
        exportChaserSecondTest.ContextNames = factoryContext.GetJobArgs().GetContextNames()
        exportChaserSecondTest.filename = factoryContext.GetFilename()

    def PostExport(self):
        exportChaserSecondTest.PostExportCalled = True
        return True
    

class importChaserFirstTest(maxUsd.ImportChaser):
    PostImportCalled = False
    ChaserNames = set()
    ChaserArgs = {}
    context = {}
    filename = ""

    def __init__(self, factoryContext, *args, **kwargs):
        super(importChaserFirstTest, self).__init__(factoryContext, *args, **kwargs)
        importChaserFirstTest.context = factoryContext.GetContext()
        importChaserFirstTest.ChaserNames = factoryContext.GetJobArgs().GetChaserNames()
        importChaserFirstTest.ChaserArgs = factoryContext.GetJobArgs().GetAllChaserArgs()['firstImport']
        importChaserFirstTest.filename = factoryContext.GetFilename()
        importChaserFirstTest.ContextNames = factoryContext.GetJobArgs().GetContextNames()

    def PostImport(self):
        importChaserFirstTest.PostImportCalled = True
        return True

class importChaserSecondTest(maxUsd.ImportChaser):
    PostImportCalled = False
    ChaserNames = set()
    ChaserArgs = {}
    context = {}
    filename = ""

    def __init__(self, factoryContext, *args, **kwargs):
        super(importChaserSecondTest, self).__init__(factoryContext, *args, **kwargs)
        importChaserSecondTest.context = factoryContext.GetContext()
        importChaserSecondTest.ChaserNames = factoryContext.GetJobArgs().GetChaserNames()
        if 'secondImport' in factoryContext.GetJobArgs().GetAllChaserArgs():
            importChaserSecondTest.ChaserArgs = factoryContext.GetJobArgs().GetAllChaserArgs()['secondImport']
        importChaserSecondTest.filename = factoryContext.GetFilename()
        importChaserSecondTest.ContextNames = factoryContext.GetJobArgs().GetContextNames()

    def PostImport(self):
        importChaserSecondTest.PostImportCalled = True
        return True

def firstExportContext():
    extraArgs = {}
    
    # The UserData chaser needs to be enabled in this job context
    extraArgs['chaserNames']  = ['firstExport']
    extraArgs['chaserArgs'] = [('firstExport', 'bar', 'ometer'), ('firstExport', 'foo', 'tball')]
    extraArgs['convertMaterialsTo'] = ['FunkyMaterial']
    return extraArgs

def secondExportContext():
    extraArgs = {}
    
    # The UserData chaser needs to be enabled in this job context
    extraArgs['chaserNames']  = ['secondExport']
    extraArgs['chaserArgs'] = [('secondExport', 'bar', 'baric'), ('secondExport', 'foo', 'tlocker')]
    return extraArgs

def firstImportContext():
    extraArgs = {}
    
    extraArgs['chaserNames']  = ['firstImport']
    extraArgs['chaserArgs'] = [('firstImport', 'bar', 'ometer'), ('firstImport', 'foo', 'tball')]
    return extraArgs

def secondImportContext():
    extraArgs = {}
    
    extraArgs['chaserNames']  = ['secondImport']
    extraArgs['chaserArgs'] = [('secondImport', 'bar', 'baric'), ('secondImport', 'foo', 'tlocker')]
    return extraArgs

class testContext(unittest.TestCase):
    def setUp(self):
        self.output_prefix = usd_test_helpers.standard_output_prefix("TEST_CONTEXT_SIMPLE_")
        
        mxs.resetMaxFile(mxs.Name('NOPROMPT'))
        usd_test_helpers.load_usd_plugins()

        # register chasers and contexts to use for tests
        maxUsd.ExportChaser.Register(exportChaserFirstTest, "firstExport", "Chaser Export First Test", "This is the first export chaser test description")
        maxUsd.ExportChaser.Register(exportChaserSecondTest, "secondExport")

        maxUsd.ImportChaser.Register(importChaserFirstTest, "firstImport", "Chaser Import First Test", "This is the first import chaser test description")
        maxUsd.ImportChaser.Register(importChaserSecondTest, "secondImport")
        
        # script re-run - prevent context from being registered twice
        registeredContexts = maxUsd.JobContextRegistry.ListJobContexts()
        if 'FirstExportContext' not in registeredContexts:
            maxUsd.JobContextRegistry.RegisterExportJobContext("FirstExportContext", "First export context test", "First export context test setting up the 'firstExport' chaser", firstExportContext)
        if 'SecondExportContext' not in registeredContexts:
            maxUsd.JobContextRegistry.RegisterExportJobContext("SecondExportContext", "Second export context test", "Second export context test setting up the 'secondExport' chaser", secondExportContext)
        if 'FirstImportContext' not in registeredContexts:
            maxUsd.JobContextRegistry.RegisterImportJobContext("FirstImportContext", "First import context test", "First import context test setting up the 'firstImport' chaser", firstImportContext)
        if 'SecondImportContext' not in registeredContexts:
            maxUsd.JobContextRegistry.RegisterImportJobContext("SecondImportContext", "Second import context test", "Second import context test setting up the 'secondExport' chaser", secondImportContext)

    def testSimpleContext(self):
        test_sphere = mxs.sphere(r = 3.5, name='apple')
        
        # all contexts for the test are registered
        self.assertTrue('FirstExportContext' in maxUsd.JobContextRegistry.ListJobContexts())
        self.assertTrue('SecondExportContext' in maxUsd.JobContextRegistry.ListJobContexts())
        self.assertTrue('FirstImportContext' in maxUsd.JobContextRegistry.ListJobContexts())
        self.assertTrue('SecondImportContext' in maxUsd.JobContextRegistry.ListJobContexts())

        # export using a dictionary as the chaser args
        # the chaser options will merge with the user options
        # the only option merging would the material conversion list
        test_usd_file_path = self.output_prefix + "simple_chaser.usda"
        export_options = mxs.USDExporter.CreateOptions()
        export_options.FileFormat = mxs.Name("ascii")
        export_options.RootPrimPath = "/"
        export_options.ContextNames = mxs.Array("FirstExportContext")
        ret = mxs.USDExporter.ExportFile(
            test_usd_file_path,
            exportOptions=export_options,
            contentSource=mxs.Name("nodeList"),
            nodeList=[test_sphere],
        )
        self.assertTrue(ret)

        self.assertTrue(exportChaserFirstTest.PostExportCalled)
        self.assertFalse(exportChaserSecondTest.PostExportCalled)
        self.assertEqual(list(exportChaserFirstTest.primsToNodeHandles.keys())[0], '/apple')
        self.assertEqual(len(exportChaserFirstTest.ChaserNames), 1)
        self.assertTrue('firstExport' in exportChaserFirstTest.ChaserNames)
        self.assertEqual(exportChaserFirstTest.ChaserArgs,{'bar': 'ometer', 'foo': 'tball'})
        self.assertEqual(exportChaserFirstTest.ContextNames,{'FirstExportContext'})
        self.assertEqual(exportChaserFirstTest.filename, test_usd_file_path)
        self.assertEqual(exportChaserFirstTest.AllMaterialTargets, {'UsdPreviewSurface','FunkyMaterial'})
        self.assertEqual(exportChaserSecondTest.filename, "")
        self.assertEqual(exportChaserSecondTest.AllMaterialTargets, {})

        # import the exported file
        import_options = mxs.USDImporter.CreateOptions()
        import_options.ContextNames = mxs.Array("FirstImportContext")
        ret = mxs.USDImporter.ImportFile(test_usd_file_path, importOptions=import_options)
        
        self.assertTrue(ret)
        self.assertTrue(importChaserFirstTest.PostImportCalled)
        self.assertFalse(importChaserSecondTest.PostImportCalled)
        self.assertEqual(len(importChaserFirstTest.ChaserNames), 1)
        self.assertTrue('firstImport' in importChaserFirstTest.ChaserNames)
        self.assertEqual(importChaserFirstTest.filename, test_usd_file_path)
        self.assertEqual(importChaserSecondTest.filename, "")

        # more complex user options are merged with the export context
        # setting a chaser + conflicting arguments on chaser being used by context set from the user options
        # the user conflicting chaser arguments should be lost and the context ones kept
        exportChaserFirstTest.PostExportCalled = False
        exportChaserFirstTest.ChaserNames = {}
        exportChaserFirstTest.ChaserArgs = {}
        exportChaserFirstTest.ContextNames = {}
        exportChaserFirstTest.filename = ""

        test_usd_file_path = self.output_prefix + "simple_chaser_override.usda"
        export_options.AllMaterialTargets = mxs.Array() # remove default 'UsdPreviewSurface' material target
        export_options.ChaserNames = mxs.Array('secondExport')
        export_options.AllChaserArgs = mxs.Array("firstExport", "bar", "baric", "firstExport", "foo", "tlocker")
        export_options.LogLevel = mxs.Name('info')
        logFileExportDefault = export_options.LogPath # keep filename before changing it
        logFileTest = self.output_prefix + "conflicting_export.log"
        export_options.LogPath = logFileTest
        ret = mxs.USDExporter.ExportFile(
            test_usd_file_path,
            exportOptions=export_options,
            contentSource=mxs.Name("nodeList"),
            nodeList=[test_sphere],
        )
        self.assertTrue(ret)

        self.assertTrue(exportChaserFirstTest.PostExportCalled)
        self.assertTrue(exportChaserSecondTest.PostExportCalled)
        self.assertEqual(list(exportChaserFirstTest.primsToNodeHandles.keys())[0], '/apple')
        self.assertEqual(len(exportChaserFirstTest.ChaserNames), 2)
        self.assertTrue('firstExport' in exportChaserFirstTest.ChaserNames)
        self.assertTrue('secondExport' in exportChaserFirstTest.ChaserNames)
        self.assertEqual(exportChaserFirstTest.AllMaterialTargets, {'FunkyMaterial'})
        self.assertEqual(exportChaserFirstTest.ChaserArgs,{'bar': 'ometer', 'foo': 'tball'})
        self.assertEqual(exportChaserSecondTest.ChaserArgs,{})
        self.assertEqual(exportChaserFirstTest.ContextNames,{'FirstExportContext'})
        self.assertEqual(exportChaserFirstTest.filename, test_usd_file_path)
        self.assertEqual(exportChaserSecondTest.filename, test_usd_file_path)
        with open(logFileTest, 'r') as file:
            content = file.read()
            self.assertTrue("Multiple argument value for 'foo' associated to chaser 'firstExport'. Keeping the argument value set to 'tball' from Context." in content)
        # log file cleanup before going to the next test
        os.remove(logFileTest)

        # more complex user options are merged with the import context
        # setting a chaser + conflicting arguments on chaser being used by context set from the user options
        # the user conflicting chaser arguments should be lost and the context ones kept
        importChaserFirstTest.PostImportCalled = False
        importChaserFirstTest.ChaserNames = {}
        importChaserFirstTest.ChaserArgs = {}
        importChaserFirstTest.ContextNames = {}
        importChaserFirstTest.filename = ""

        import_options.ChaserNames = mxs.Array('secondImport')
        import_options.AllChaserArgs = mxs.Array("firstImport", "bar", "baric", "firstImport", "foo", "tlocker")
        import_options.LogLevel = mxs.Name('info')
        import_options.ContextNames = mxs.Array("FirstImportContext")
        logFileImportDefault = import_options.LogPath # keep filename before changing it
        logFileTest = self.output_prefix + "conflicting_import.log"
        import_options.LogPath = logFileTest
        ret = mxs.USDImporter.ImportFile(test_usd_file_path, importOptions=import_options)
        
        self.assertTrue(ret)
        self.assertTrue(importChaserFirstTest.PostImportCalled)
        self.assertTrue(importChaserSecondTest.PostImportCalled)
        self.assertEqual(len(importChaserFirstTest.ChaserNames), 2)
        self.assertTrue('firstImport' in importChaserFirstTest.ChaserNames)
        self.assertTrue('secondImport' in importChaserFirstTest.ChaserNames)
        self.assertEqual(importChaserFirstTest.ChaserArgs,{'bar': 'ometer', 'foo': 'tball'})
        self.assertEqual(importChaserSecondTest.ChaserArgs,{})
        self.assertEqual(importChaserFirstTest.filename, test_usd_file_path)
        self.assertEqual(importChaserSecondTest.filename, test_usd_file_path)
        self.assertEqual(importChaserFirstTest.ContextNames,{'FirstImportContext'})
        with open(logFileTest, 'r') as file:
            content = file.read()
            self.assertTrue("Multiple argument value for 'foo' associated to chaser 'firstImport'. Keeping the argument value set to 'tball' from Context." in content)
        # log file cleanup before going to the next test
        os.remove(logFileTest)

        # now merge two contexts which export using 2 chasers with their own arguments
        # keeping a chaser in the user option (with no arguments)
        # however the second context makes use of the same user specified chaser but with arguments
        # the chaser setup from the context should be the one remaining
        exportChaserFirstTest.PostExportCalled = False
        exportChaserSecondTest.PostExportCalled = False
        exportChaserFirstTest.filename = ""
        exportChaserFirstTest.ChaserNames = {}
        exportChaserFirstTest.ChaserArgs = {}
        exportChaserFirstTest.ContextNames = {}
        test_usd_file_path = self.output_prefix + "simple_chaser_merge.usda"
        export_options.ContextNames = mxs.Array("FirstExportContext", "SecondExportContext")
        export_options.AllMaterialTargets = mxs.Array('UsdPreviewSurface') # putting back default 'UsdPreviewSurface' material target
        export_options.LogLevel = mxs.Name('off')
        export_options.LogPath = logFileExportDefault
        ret = mxs.USDExporter.ExportFile(
            test_usd_file_path,
            exportOptions=export_options,
            contentSource=mxs.Name("nodeList"),
            nodeList=[test_sphere],
        )
        self.assertTrue(ret)

        self.assertTrue(exportChaserFirstTest.PostExportCalled)
        self.assertTrue(exportChaserSecondTest.PostExportCalled)
        self.assertEqual(list(exportChaserFirstTest.primsToNodeHandles.keys())[0], '/apple')
        self.assertEqual(list(exportChaserSecondTest.primsToNodeHandles.keys())[0], '/apple')
        self.assertEqual(exportChaserFirstTest.AllMaterialTargets, {'UsdPreviewSurface', 'FunkyMaterial'})
        self.assertEqual(len(exportChaserFirstTest.ChaserNames), 2)
        self.assertEqual(len(exportChaserSecondTest.ChaserNames), 2)
        self.assertTrue('firstExport' in exportChaserFirstTest.ChaserNames)
        self.assertTrue('secondExport' in exportChaserSecondTest.ChaserNames)
        self.assertEqual(exportChaserFirstTest.ChaserArgs,{'bar': 'ometer', 'foo': 'tball'})
        self.assertEqual(exportChaserSecondTest.ChaserArgs,{'bar': 'baric', 'foo': 'tlocker'})
        self.assertTrue('FirstExportContext' in exportChaserFirstTest.ContextNames)
        self.assertTrue('SecondExportContext' in exportChaserSecondTest.ContextNames)
        self.assertEqual(exportChaserFirstTest.filename, test_usd_file_path)
        self.assertEqual(exportChaserSecondTest.filename, test_usd_file_path)

        # now merge two contexts which import using 2 chasers with their own arguments
        # keeping a chaser in the user option (with no arguments)
        # however the second context makes use of the same user specified chaser but with arguments
        # the chaser setup from the context should be the one remaining
        importChaserFirstTest.PostImportCalled = False
        importChaserSecondTest.PostImportCalled = False
        importChaserFirstTest.filename = ""
        importChaserFirstTest.ChaserNames = {}
        importChaserFirstTest.ChaserArgs = {}
        importChaserFirstTest.ContextNames = {}

        import_options.ContextNames = mxs.Array("FirstImportContext", "SecondImportContext")
        import_options.LogLevel = mxs.Name('off')
        import_options.LogPath = logFileImportDefault
        ret = mxs.USDImporter.ImportFile(test_usd_file_path, importOptions=import_options)

        self.assertTrue(importChaserFirstTest.PostImportCalled)
        self.assertTrue(importChaserSecondTest.PostImportCalled)
        self.assertEqual(len(importChaserFirstTest.ChaserNames), 2)
        self.assertEqual(len(importChaserSecondTest.ChaserNames), 2)
        self.assertTrue('firstImport' in importChaserFirstTest.ChaserNames)
        self.assertTrue('secondImport' in importChaserSecondTest.ChaserNames)
        self.assertEqual(importChaserFirstTest.ChaserArgs,{'bar': 'ometer', 'foo': 'tball'})
        self.assertEqual(importChaserSecondTest.ChaserArgs,{'bar': 'baric', 'foo': 'tlocker'})
        self.assertTrue('FirstImportContext' in importChaserFirstTest.ContextNames)
        self.assertTrue('SecondImportContext' in importChaserSecondTest.ContextNames)
        self.assertEqual(importChaserFirstTest.filename, test_usd_file_path)
        self.assertEqual(importChaserSecondTest.filename, test_usd_file_path)

def run_tests():
    return unittest.TextTestRunner(stream=sys.stdout, verbosity=2).run(
        unittest.TestLoader().loadTestsFromTestCase(testContext)
    )

if __name__ == "__main__":
    from importlib import reload
    import usd_utils
    reload(usd_utils)
    usd_utils.get_config_data(update=True)
    mxs.clearListener()
    run_tests()
