--
-- Copyright 2023 Autodesk
--
-- Licensed under the Apache License, Version 2.0 (the "License");
-- you may not use this file except in compliance with the License.
-- You may obtain a copy of the License at
--
--     http://www.apache.org/licenses/LICENSE-2.0
--
-- Unless required by applicable law or agreed to in writing, software
-- distributed under the License is distributed on an "AS IS" BASIS,
-- WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
-- See the License for the specific language governing permissions and
-- limitations under the License.
--

fileIn "helpers/usd_test_utils.mxs"

struct plugin_test
(
    function setup =
    (
    ),

    function testImportPluginDetected = (
        assert_not_equal -1 (getUsdImporterPluginIndex()) message:"USDImport.dli not found"
    ),

    function testExportPluginDetected = (
        assert_not_equal -1 (getUsdExporterPluginIndex()) message:"USDExport.dle not found"
    ),

    function testUsdUtilsExists = (
        usdBinPath = getUsdBinPath()
        usdviewPath = pathConfig.appendPath usdBinPath "usdview.cmd"
        
        local usdcatPath
        -- In more recent versions of USD (starting in 23.08), usdcat was ported to c++.
        maxver = maxVersion()
        if maxver[1] <= 26000 then (
            -- 2024 RC and before
            usdcatPath = pathConfig.appendPath usdBinPath "usdcat.cmd"
        )
        else (
            -- 2025 builds is when we introduce 23.08
            usdcatPath = pathConfig.appendPath usdBinPath "usdcat.exe"
        )
        
        pysys = python.import("sys")
        if pysys.version_info.major == 3 do (
            --usdview is only available with python3 due to issues with getting pyside2uic.exe in max's python2
            assert_not_equal 0 ((getFiles usdviewPath).count) message:("usdview.cmd not found at " + usdviewPath)
        )

        assert_not_equal 0 ((getFiles usdcatPath).count) message:("usdcat.cmd not found at " + usdcatPath)
    ),

    function teardown = (
    ),

    tests = #(testImportPluginDetected, testExportPluginDetected, testUsdUtilsExists)
)

runUsdTestFixture plugin_test (getThisScriptFilename())