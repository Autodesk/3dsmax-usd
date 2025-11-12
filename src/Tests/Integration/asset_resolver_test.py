#
# Copyright 2025 Autodesk
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
from pxr import Ar
import AdskAssetResolver

from pymxs import runtime as mxs

import os, sys, glob
import unittest

class testAdskAssetResolver(unittest.TestCase):
    def setUp(self):
        self.output_prefix = usd_test_helpers.standard_output_prefix("TEST_ADSK_ASSET_RESOLVER_")
        
        mxs.resetMaxFile(mxs.Name('NOPROMPT'))
        usd_test_helpers.load_usd_plugins()

    def testDefaultResolver(self):
        resolver = Ar.GetUnderlyingResolver()
        self.assertTrue(isinstance(resolver, AdskAssetResolver.AdskAssetResolver))

    def testResolveWith3dsmaxTokens(self):
        current_project_folder = mxs.pathConfig.getCurrentProjectFolder()
        temp_project_folder = mxs.getDir(mxs.Name("temp"))
        mxs.pathConfig.setCurrentProjectFolder(temp_project_folder)

        resolver = Ar.GetResolver()
        ctx = resolver.CreateDefaultContext()
        with Ar.ResolverContextBinder(ctx):
            self.assertEqual(temp_project_folder, resolver.Resolve("<project>").GetPathString())

        mxs.pathConfig.setCurrentProjectFolder(current_project_folder)        

def run_tests():
    return unittest.TextTestRunner(stream=sys.stdout, verbosity=2).run(
        unittest.TestLoader().loadTestsFromTestCase(testAdskAssetResolver)
    )


if __name__ == "__main__":
    from importlib import reload
    import usd_utils
    reload(usd_utils)
    usd_utils.get_config_data(update=True)
    mxs.clearListener()
    run_tests()
