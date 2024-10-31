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
import re
import unittest

adskRegex = r"""
.* Copyright \d\d\d\d Autodesk
.*
.* Licensed under the Apache License, Version 2\.0 \(the \"License\"\);
.* you may not use this file except in compliance with the License\.
.* You may obtain a copy of the License at
.*
.*     http://www\.apache\.org/licenses/LICENSE-2\.0
.*
.* Unless required by applicable law or agreed to in writing, software
.* distributed under the License is distributed on an \"AS IS\" BASIS,
.* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied\.
.* See the License for the specific language governing permissions and
.* limitations under the License\.
"""

pixarRegex = r"""
.* Copyright \d\d\d\d Pixar
.*
.* Licensed under the Apache License, Version 2\.0 \(the \"License\"\);
.* you may not use this file except in compliance with the License\.
.* You may obtain a copy of the License at
.*
.*     http://www\.apache\.org/licenses/LICENSE-2\.0
.*
.* Unless required by applicable law or agreed to in writing, software
.* distributed under the License is distributed on an \"AS IS\" BASIS,
.* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied\.
.* See the License for the specific language governing permissions and
.* limitations under the License\.
.*
.* © \d\d\d\d Autodesk, Inc. All rights reserved\.
"""

excludedExtensions = [
    "ui", "png", "jpg", "mtlx", "rc", "vcxproj", "vcxproj.filters", "vcxproj.user", 
    "filters", "db", "def", "ts", "pyc", "json", "usd", "usda", "abc", "xml", 
    "ipch", "props", "md", "VC", "common", "qrc", "cmd", "max", "suo", "sln",
    ".mat", "material_conversion", "mat_def", "mcr", "ms.res", "mcr.res",
    "html", "css", "min.css", "dlo.mui", "dli.mui", "test.pdb", "test.ilk",
    "test.gup", "dll.mui", "db-wal", "VC.db-shm", "VC.opendb", "VC.db-wal",
    "VC.db-shm", ".component.localization.targets", "dll", "dle.mui", "aps",
    "usdc", "usdz", "mnx", "user", "component.localization.targets", "r",
    "clang-format", "gitattributes", "gitignore", "txt", "c07", "vbs"
]
excludedFiles = ["qrc_resource.cpp"]
excludedDirs = ["build","docs",".git",".github",".jenkins","artifacts"]
root = "..\\..\\"

class headerCheckTest(unittest.TestCase):
    def setUp(self):
        mxs.resetMaxFile(mxs.Name('NOPROMPT'))
        usd_test_helpers.load_usd_plugins()

    def testAllFilesHeader(self):
        print("\n")
        errorFound = False
        totalFiles = 0
        totalMissing = 0

        extStr = ""
        for i, ext in enumerate(excludedExtensions):
            if i != len(excludedExtensions)-1:
                extStr += ext + "|"
            else:
                extStr += ext
        regex = f".*\.({extStr})$"

        for path, subdirs, files in os.walk(root):
            firstFolder = ""
            folders = path.split("\\")

            for folder in folders:
                if folder != "..":
                    firstFolder = folder
                    break
            
            if(firstFolder not in excludedDirs):
                for name in files:
                    if name not in excludedFiles and not re.search(regex, name):
                        try:
                            totalFiles +=1
                            filePath = os.path.join(path, name)
                            file = open(filePath, 'r', encoding='UTF8')
                            content = file.read()
                            file.close()

                            if not re.search(adskRegex, content) and not re.search(pixarRegex, content):
                                print("[ERROR] Header missing for: " + filePath)
                                totalMissing += 1
                                errorFound = True
                        except:
                            print("Can't read: " + filePath + ", please check the encoding.")

        if not errorFound:
            print(f"Verified {totalFiles} and there's no missing headers.")
        else:
            print(f"Header missing for {totalMissing}/{totalFiles} files")
        self.assertFalse(errorFound)

def run_tests():
    return unittest.TextTestRunner(stream=sys.stdout, verbosity=2).run(
        unittest.TestLoader().loadTestsFromTestCase(headerCheckTest)
    )

if __name__ == "__main__":
    from importlib import reload
    import usd_utils
    reload(usd_utils)
    usd_utils.get_config_data(update=True)
    mxs.clearListener()
    run_tests()