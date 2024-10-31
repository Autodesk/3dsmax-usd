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

-- Some useful python modules for USD validation.
pyUsd = python.import("pxr.Usd")
pyUsdGeom = python.import("pxr.UsdGeom")
    
output_prefix = pathConfig.appendPath (getDir #temp) "/EXPORT_METERSPERUNIT_TEST_"
deleteTestFiles(output_prefix + "*.*")

struct export_metersPerUnit_test
( 
    private
    initialUnits,
    initialScale,
    exportOptions = USDExporter.CreateOptions(),
    
    public
    function setup = (
        resetmaxfile #noprompt
        exportOptions.FileFormat = #ascii

        initialUnits = units.SystemType
        initialScale = units.SystemScale
    ),

    function testMillimeterUnits = (
        --given
        units.SystemType = #Millimeters
        units.SystemScale = 1
            
        --when
        exportPath = (output_prefix + "export_millimeters_test.usda")
        USDExporter.ExportFile exportPath exportOptions:exportOptions
        local stage = pyUsd.Stage.Open(exportPath)
        local metersPerUnits = pyUsdGeom.GetStageMetersPerUnit(stage)
        
        --then
        assert_float_equal 0.001 metersPerUnits
    ),

    function testCentimeterUnits = (
        --given
        units.SystemType = #Centimeters
        units.SystemScale = 1
            
        --when
        exportPath = (output_prefix + "export_centimeters_test.usda")
        USDExporter.ExportFile exportPath exportOptions:exportOptions
        local stage = pyUsd.Stage.Open(exportPath)
        local metersPerUnits = pyUsdGeom.GetStageMetersPerUnit(stage)
        
        --then
        assert_float_equal 0.01 metersPerUnits
    ),
    
    function testMeterUnits = (
        --given
        units.SystemType = #Meters
        units.SystemScale = 1
            
        --when
        exportPath = (output_prefix + "export_meters_test.usda")
        USDExporter.ExportFile exportPath exportOptions:exportOptions
        local stage = pyUsd.Stage.Open(exportPath)
        local metersPerUnits = pyUsdGeom.GetStageMetersPerUnit(stage)
        
        --then
        assert_float_equal 1 metersPerUnits
    ),
    
    function test2MetersUnits = (
        --given
        units.SystemType = #Meters
        units.SystemScale = 2
            
        --when
        exportPath = (output_prefix + "export_two_meters_test.usda")
        USDExporter.ExportFile exportPath exportOptions:exportOptions
        local stage = pyUsd.Stage.Open(exportPath)
        local metersPerUnits = pyUsdGeom.GetStageMetersPerUnit(stage)
        
        --then
        assert_float_equal 2 metersPerUnits
    ),
    
    function testKilometerUnits = (
        --given
        units.SystemType = #Kilometers
        units.SystemScale = 1
            
        --when
        exportPath = (output_prefix + "export_kilometers_test.usda")
        USDExporter.ExportFile exportPath exportOptions:exportOptions
        local stage = pyUsd.Stage.Open(exportPath)
        local metersPerUnits = pyUsdGeom.GetStageMetersPerUnit(stage)
        
        --then
        assert_float_equal 1000 metersPerUnits
    ),
    
    function testInchesUnits = (
        --given
        units.SystemType = #Inches
        units.SystemScale = 1
            
        --when
        exportPath = (output_prefix + "export_inchesmeters_test.usda")
        USDExporter.ExportFile exportPath exportOptions:exportOptions
        local stage = pyUsd.Stage.Open(exportPath)
        local metersPerUnits = pyUsdGeom.GetStageMetersPerUnit(stage)
        
        --then
        assert_float_equal 0.0254 metersPerUnits
    ),
    function teardown = (
        units.SystemType = initialUnits
        units.SystemScale = initialScale
    ),

    Tests = #(testMillimeterUnits, testCentimeterUnits, testMeterUnits, test2MetersUnits, testKilometerUnits, testInchesUnits)
)

runUsdTestFixture export_metersPerUnit_test (getThisScriptFilename())