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

FileIn "helpers/usd_test_utils.mxs"

-- Make sure the USD plugin is properly loaded.
setupUsdPlugins()
outputPrefix = pathConfig.appendPath (getDir #temp) "/IO_CALLBACK_LOGGING_TEST_"
deleteTestFiles(outputPrefix + "*.*")

struct io_callback_logging_test
(  
    private 
    exportOptions = USDExporter.CreateOptions(),
    infoMessage = "This is an info message.",
    warningMessage = "This is a warning message.",
    errorMessage = "This is an error message.",
    
    public 
    function setup =
    (
        -- Basic setup
        resetmaxfile #noprompt
        
        -- Populate basic 3ds Max scene with a simple sphere
        Sphere()
        
        chaser_script_registration =
"import maxUsd\n" +
"from pymxs import runtime as mxs\n" +
"class exportChaserLogTest(maxUsd.ExportChaser):\n" +
"    def __init__(self, factoryContext, *args, **kwargs):\n" +
"        super(exportChaserLogTest, self).__init__(factoryContext, *args, **kwargs)\n" +
"\n" +
"    def PostExport(self):\n" +
"        mxs.USDExporter.Log(mxs.Name(\"info\"), \"" + infoMessage + "\")\n" +
"        mxs.USDExporter.Log(mxs.Name(\"warn\"), \"" + warningMessage + "\")\n" +
"        mxs.USDExporter.Log(mxs.Name(\"error\"), \"" + errorMessage + "\")\n" +
"        return True\n" +
"\n" +
"maxUsd.ExportChaser.Register(exportChaserLogTest, \"logTest\")"
        python.execute(chaser_script_registration)
    ),

    function validateLogs logPath logLevel = (
        if logLevel != #off do
        (
            -- Make sure the file can be read
            local fileStream = openFile logPath
            assert_defined fileStream message:("Could not open file '" + logPath + "'!")
            
            -- Validate file content depending on the log level
            if logLevel == #info do
            (
                assert_defined (skipToString fileStream infoMessage) message:("Could not read line '" + infoMessage + "'!")
            )

            if logLevel == #info or logLevel == #warn do
            (
                assert_defined (skipToString fileStream warningMessage) message:("Could not read line '" + warningMessage + "'!")
            )

            assert_defined (skipToString fileStream errorMessage) message:("Could not read line '" + errorMessage + "'!")

            -- Close the file
            close fileStream
        )
    ),
    
    function export_callback_logging_test = (
        local usdFile = outputPrefix + "SimpleSphere.usd"
        
        -- Register a chaser on export (aka export completed callback)
        exportOptions.ChaserNames = #("logTest")
        
        -- First, test with log level equals to #info, we should find all 3 messages in the MaxUsdExport.log
        exportOptions.LogLevel = #info
        USDExporter.ExportFile usdFile exportOptions:exportOptions
        validateLogs exportOptions.LogPath exportOptions.LogLevel
        deleteFile exportOptions.LogPath

        -- Second, test with log level equals to #warn, we should find only warning and error messages in the MaxUsdExport.log
        exportOptions.LogLevel = #warn
        USDExporter.ExportFile usdFile exportOptions:exportOptions
        validateLogs exportOptions.LogPath exportOptions.LogLevel
        deleteFile exportOptions.LogPath

        -- Third, test with log level equals to #error, we should find only error message in the MaxUsdExport.log
        exportOptions.LogLevel = #error
        USDExporter.ExportFile usdFile exportOptions:exportOptions
        validateLogs exportOptions.LogPath exportOptions.LogLevel
        deleteFile exportOptions.LogPath

        -- Finally, test with log level equals to #off, shouldn't find any messages in the MaxUsdExport.log
        exportOptions.LogLevel = #off
        USDExporter.ExportFile usdFile exportOptions:exportOptions
        validateLogs exportOptions.LogPath exportOptions.LogLevel
    ),

    function teardown = (
    ),

    tests = #(export_callback_logging_test)
)

runUsdTestFixture io_callback_logging_test (getThisScriptFilename())