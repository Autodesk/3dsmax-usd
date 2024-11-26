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

-- Make sure the USD plugin is properly loaded.
setupUsdPlugins()

pyUsd = python.import("pxr.Usd")
pyUsdGeom = python.import("pxr.UsdGeom")
pyGf = python.import("pxr.Gf")

input_prefix_main = pathConfig.appendPath (getDir #temp) "/IMPORT_METERSPERUNIT_TEST_"
input_prefix_ref = pathConfig.appendPath (getDir #temp) "/IMPORT_METERSPERUNIT_TEST_REF_"
deleteTestFiles(input_prefix_main + "*.*")
deleteTestFiles(input_prefix_ref + "*.*")

struct import_metersPerUnit_test
(
    private
    initialUnits,
    initialScale,
    
    public
    function createTestData mainFilePath refFilePath = (
        local exportOptions = USDExporter.CreateOptions()
        exportOptions.FileFormat = #ascii
        exportOptions.RootPrimPath = "/"
        Box pos:[0,0,0] width:0.001 length:0.001 height:0.001 name:"Box001"
        USDExporter.ExportFile refFilePath exportOptions:exportOptions
        
        local refStage = pyUsd.Stage.Open refFilePath
        pyUsdGeom.SetStageMetersPerUnit refStage 0.001
        (refStage.GetRootLayer()).Save()
        
        resetmaxfile #noprompt
        
        Box pos:[0,0,0] width:2 length:3 height:1 name:"Box001"
        Box pos:[0.5,1,1] width:1 length:1 height:1 name:"Box002"
        USDExporter.ExportFile mainFilePath exportOptions:exportOptions
        
        local stage = pyUsd.Stage.Open(mainFilePath)
        pyUsdGeom.SetStageMetersPerUnit stage 1
        
        local meterCubePrim = stage.OverridePrim "/meterCube"
        
        local references = meterCubePrim.GetReferences()
        references.AddReference refFilePath "/Box001"
        local refXform = pyUsdGeom.Xformable(meterCubePrim)
        local translationOp = refXform.AddTranslateOp()
        translationOp.Set (pyGf.Vec3f -0.5 -1 1)
        local scaleOp = refXform.AddScaleOp()
        scaleOp.Set (pyGf.Vec3f 1000 1000 1000)
        (stage.GetRootLayer()).Save()
        resetmaxfile #noprompt
    ),
    
    function setup =
    (
        initialUnits = units.SystemType
        initialScale = units.SystemScale
        
        resetmaxfile #noprompt
    ),
    
    function test_millimeter_units  = (
        local mainFilePath = input_prefix_main + "millimeter_units_test.usda"
        local refFilePath = input_prefix_ref + "millimeter_units_ref_test.usda"
        createTestData mainFilePath refFilePath

        --given
        units.SystemType = #Millimeters
        units.SystemScale = 1

        --when
        USDImporter.ImportFile mainFilePath
            
        --then
        assert_point3_equal [0,0,0] $Box001.pos
        assert_point3_equal [-1000,-1500,0] $Box001.vertices[1].pos
        assert_point3_equal [1000,-1500,0] $Box001.vertices[2].pos
        assert_point3_equal [-1000,1500,0] $Box001.vertices[3].pos
        assert_point3_equal [1000,1500,0] $Box001.vertices[4].pos
        assert_point3_equal [-1000,-1500,1000] $Box001.vertices[5].pos
        assert_point3_equal [1000,-1500,1000] $Box001.vertices[6].pos
        assert_point3_equal [-1000,1500,1000] $Box001.vertices[7].pos
        assert_point3_equal [1000,1500,1000] $Box001.vertices[8].pos
        
        assert_point3_equal [500,1000,1000] $Box002.pos
        assert_point3_equal [0,500,1000] $Box002.vertices[1].pos
        assert_point3_equal [1000,500,1000] $Box002.vertices[2].pos
        assert_point3_equal [0,1500,1000] $Box002.vertices[3].pos
        assert_point3_equal [1000,1500,1000] $Box002.vertices[4].pos
        assert_point3_equal [0,500,2000] $Box002.vertices[5].pos
        assert_point3_equal [1000,500,2000] $Box002.vertices[6].pos
        assert_point3_equal [0,1500,2000] $Box002.vertices[7].pos
        assert_point3_equal [1000,1500,2000] $Box002.vertices[8].pos
            
        assert_point3_equal [-500,-1000,1000] $meterCube.pos
        assert_point3_equal [-1000,-1500,1000] $meterCube.vertices[1].pos
        assert_point3_equal [0,-1500,1000] $meterCube.vertices[2].pos
        assert_point3_equal [-1000,-500,1000] $meterCube.vertices[3].pos
        assert_point3_equal [0,-500,1000] $meterCube.vertices[4].pos
        assert_point3_equal [-1000,-1500,2000] $meterCube.vertices[5].pos
        assert_point3_equal [0,-1500,2000] $meterCube.vertices[6].pos
        assert_point3_equal [-1000,-500,2000] $meterCube.vertices[7].pos
        assert_point3_equal [0,-500,2000] $meterCube.vertices[8].pos
    ),

    function test_centimeter_units = (
        local mainFilePath = input_prefix_main + "centimeter_units_test.usda"
        local refFilePath = input_prefix_ref + "centimeter_units_ref_test.usda"
        createTestData mainFilePath refFilePath

        --given
        units.SystemType = #Centimeters
        units.SystemScale = 1

        --when
        USDImporter.ImportFile mainFilePath
            
        --then
        assert_point3_equal [0,0,0] $Box001.pos
        assert_point3_equal [-100,-150,0] $Box001.vertices[1].pos
        assert_point3_equal [100,-150,0] $Box001.vertices[2].pos
        assert_point3_equal [-100,150,0] $Box001.vertices[3].pos
        assert_point3_equal [100,150,0] $Box001.vertices[4].pos
        assert_point3_equal [-100,-150,100] $Box001.vertices[5].pos
        assert_point3_equal [100,-150,100] $Box001.vertices[6].pos
        assert_point3_equal [-100,150,100] $Box001.vertices[7].pos
        assert_point3_equal [100,150,100] $Box001.vertices[8].pos
        
        assert_point3_equal [50,100,100] $Box002.pos
        assert_point3_equal [0,50,100] $Box002.vertices[1].pos
        assert_point3_equal [100,50,100] $Box002.vertices[2].pos
        assert_point3_equal [0,150,100] $Box002.vertices[3].pos
        assert_point3_equal [100,150,100] $Box002.vertices[4].pos
        assert_point3_equal [0,50,200] $Box002.vertices[5].pos
        assert_point3_equal [100,50,200] $Box002.vertices[6].pos
        assert_point3_equal [0,150,200] $Box002.vertices[7].pos
        assert_point3_equal [100,150,200] $Box002.vertices[8].pos
            
        assert_point3_equal [-50,-100,100] $meterCube.pos
        assert_point3_equal [-100,-150,100] $meterCube.vertices[1].pos
        assert_point3_equal [0,-150,100] $meterCube.vertices[2].pos
        assert_point3_equal [-100,-50,100] $meterCube.vertices[3].pos
        assert_point3_equal [0,-50,100] $meterCube.vertices[4].pos
        assert_point3_equal [-100,-150,200] $meterCube.vertices[5].pos
        assert_point3_equal [0,-150,200] $meterCube.vertices[6].pos
        assert_point3_equal [-100,-50,200] $meterCube.vertices[7].pos
        assert_point3_equal [0,-50,200] $meterCube.vertices[8].pos
    ),
    
    function test_meter_units = (
        local mainFilePath = input_prefix_main + "meter_units_test.usda"
        local refFilePath = input_prefix_ref + "meter_units_ref_test.usda"
        createTestData mainFilePath refFilePath

        --given
        units.SystemType = #Meters
        units.SystemScale = 1

        --when
        USDImporter.ImportFile mainFilePath
            
        --then
        assert_point3_equal [0,0,0] $Box001.pos
        assert_point3_equal [-1,-1.5,0] $Box001.vertices[1].pos
        assert_point3_equal [1,-1.5,0] $Box001.vertices[2].pos
        assert_point3_equal [-1,1.5,0] $Box001.vertices[3].pos
        assert_point3_equal [1,1.5,0] $Box001.vertices[4].pos
        assert_point3_equal [-1,-1.5,1] $Box001.vertices[5].pos
        assert_point3_equal [1,-1.5,1] $Box001.vertices[6].pos
        assert_point3_equal [-1,1.5,1] $Box001.vertices[7].pos
        assert_point3_equal [1,1.5,1] $Box001.vertices[8].pos
        
        assert_point3_equal [0.5,1,1] $Box002.pos
        assert_point3_equal [0,0.5,1] $Box002.vertices[1].pos
        assert_point3_equal [1,0.5,1] $Box002.vertices[2].pos
        assert_point3_equal [0,1.5,1] $Box002.vertices[3].pos
        assert_point3_equal [1,1.5,1] $Box002.vertices[4].pos
        assert_point3_equal [0,0.5,2] $Box002.vertices[5].pos
        assert_point3_equal [1,0.5,2] $Box002.vertices[6].pos
        assert_point3_equal [0,1.5,2] $Box002.vertices[7].pos
        assert_point3_equal [1,1.5,2] $Box002.vertices[8].pos
            
        assert_point3_equal [-0.5,-1,1] $meterCube.pos
        assert_point3_equal [-1,-1.5,1] $meterCube.vertices[1].pos
        assert_point3_equal [0,-1.5,1] $meterCube.vertices[2].pos
        assert_point3_equal [-1,-0.5,1] $meterCube.vertices[3].pos
        assert_point3_equal [0,-0.5,1] $meterCube.vertices[4].pos
        assert_point3_equal [-1,-1.5,2] $meterCube.vertices[5].pos
        assert_point3_equal [0,-1.5,2] $meterCube.vertices[6].pos
        assert_point3_equal [-1,-0.5,2] $meterCube.vertices[7].pos
        assert_point3_equal [0,-0.5,2] $meterCube.vertices[8].pos
    ),
    
    function test_two_meters_units = (
        local mainFilePath = input_prefix_main + "two_meter_units_test.usda"
        local refFilePath = input_prefix_ref + "two_meter_units_ref_test.usda"
        createTestData mainFilePath refFilePath

        --given
        units.SystemType = #Meters
        units.SystemScale = 2

        --when
        USDImporter.ImportFile mainFilePath
            
        --then
        assert_point3_equal [0,0,0] $Box001.pos
        assert_point3_equal [-0.5,-0.75,0] $Box001.vertices[1].pos
        assert_point3_equal [0.5,-0.75,0] $Box001.vertices[2].pos
        assert_point3_equal [-0.5,0.75,0] $Box001.vertices[3].pos
        assert_point3_equal [0.5,0.75,0] $Box001.vertices[4].pos
        assert_point3_equal [-0.5,-0.75,0.5] $Box001.vertices[5].pos
        assert_point3_equal [0.5,-0.75,0.5] $Box001.vertices[6].pos
        assert_point3_equal [-0.5,0.75,0.5] $Box001.vertices[7].pos
        assert_point3_equal [0.5,0.75,0.5] $Box001.vertices[8].pos
        
        assert_point3_equal [0.25,0.5,0.5] $Box002.pos
        assert_point3_equal [0,0.25,0.5] $Box002.vertices[1].pos
        assert_point3_equal [0.5,0.25,0.5] $Box002.vertices[2].pos
        assert_point3_equal [0,0.75,0.5] $Box002.vertices[3].pos
        assert_point3_equal [0.5,0.75,0.5] $Box002.vertices[4].pos
        assert_point3_equal [0,0.25,1] $Box002.vertices[5].pos
        assert_point3_equal [0.5,0.25,1] $Box002.vertices[6].pos
        assert_point3_equal [0,0.75,1] $Box002.vertices[7].pos
        assert_point3_equal [0.5,0.75,1] $Box002.vertices[8].pos
            
        assert_point3_equal [-0.25,-0.5,0.5] $meterCube.pos
        assert_point3_equal [-0.5,-0.75,0.5] $meterCube.vertices[1].pos
        assert_point3_equal [0,-0.75,0.5] $meterCube.vertices[2].pos
        assert_point3_equal [-0.5,-0.25,0.5] $meterCube.vertices[3].pos
        assert_point3_equal [0,-0.25,0.5] $meterCube.vertices[4].pos
        assert_point3_equal [-0.5,-0.75,1] $meterCube.vertices[5].pos
        assert_point3_equal [0,-0.75,1] $meterCube.vertices[6].pos
        assert_point3_equal [-0.5,-0.25,1] $meterCube.vertices[7].pos
        assert_point3_equal [0,-0.25,1] $meterCube.vertices[8].pos
    ),
    
    function test_kilometer_units = (
        local mainFilePath = input_prefix_main + "kilometer_units_test.usda"
        local refFilePath = input_prefix_ref + "kilometer_units_ref_test.usda"
        createTestData mainFilePath refFilePath

        --given
        units.SystemType = #Kilometers
        units.SystemScale = 1

        --when
        USDImporter.ImportFile mainFilePath
            
        --then
        assert_point3_equal [0,0,0] $Box001.pos
        assert_point3_equal [-0.001,-0.0015,0] $Box001.vertices[1].pos
        assert_point3_equal [0.001,-0.0015,0] $Box001.vertices[2].pos
        assert_point3_equal [-0.001,0.0015,0] $Box001.vertices[3].pos
        assert_point3_equal [0.001,0.0015,0] $Box001.vertices[4].pos
        assert_point3_equal [-0.001,-0.0015,0.001] $Box001.vertices[5].pos
        assert_point3_equal [0.001,-0.0015,0.001] $Box001.vertices[6].pos
        assert_point3_equal [-0.001,0.0015,0.001] $Box001.vertices[7].pos
        assert_point3_equal [0.001,0.0015,0.001] $Box001.vertices[8].pos
        
        assert_point3_equal [0.0005,0.001,0.001] $Box002.pos
        assert_point3_equal [0,0.0005,0.001] $Box002.vertices[1].pos
        assert_point3_equal [0.001,0.0005,0.001] $Box002.vertices[2].pos
        assert_point3_equal [0,0.0015,0.001] $Box002.vertices[3].pos
        assert_point3_equal [0.001,0.0015,0.001] $Box002.vertices[4].pos
        assert_point3_equal [0,0.0005,0.002] $Box002.vertices[5].pos
        assert_point3_equal [0.001,0.0005,0.002] $Box002.vertices[6].pos
        assert_point3_equal [0,0.0015,0.002] $Box002.vertices[7].pos
        assert_point3_equal [0.001,0.0015,0.002] $Box002.vertices[8].pos
            
        assert_point3_equal [-0.0005,-0.001,0.001] $meterCube.pos
        assert_point3_equal [-0.001,-0.0015,0.001] $meterCube.vertices[1].pos
        assert_point3_equal [0,-0.0015,0.001] $meterCube.vertices[2].pos
        assert_point3_equal [-0.001,-0.0005,0.001] $meterCube.vertices[3].pos
        assert_point3_equal [0,-0.0005,0.001] $meterCube.vertices[4].pos
        assert_point3_equal [-0.001,-0.0015,0.002] $meterCube.vertices[5].pos
        assert_point3_equal [0,-0.0015,0.002] $meterCube.vertices[6].pos
        assert_point3_equal [-0.001,-0.0005,0.002] $meterCube.vertices[7].pos
        assert_point3_equal [0,-0.0005,0.002] $meterCube.vertices[8].pos
    ),
    
    function test_orginal_content_do_not_get_scaled = (
        local mainFilePath = input_prefix_main + "orginal_content_do_not_get_scaled_test.usda"
        local refFilePath = input_prefix_ref + "orginal_content_do_not_get_scaled_ref_test.usda"
        createTestData mainFilePath refFilePath

        --given
        units.SystemType = #Kilometers
        units.SystemScale = 1
        
        Sphere radius:10 pos:[1,2,3] isSelected:true name:"mySphere"
        
        --when
        USDImporter.ImportFile mainFilePath
            
        --then
        assert_point3_equal [1,2,3] $mySphere.pos
        assert_float_equal 10 $mySphere.radius
        assert_point3_equal [1,1,1] $mySphere.transform.scale
    ),

    function test_import_scaled_children_nodes = (
        local stageFileName = (input_prefix_main + "import_scaled_children_node.usda")

        local exportOptions = USDExporter.CreateOptions()
        exportOptions.FileFormat = #ascii
        exportOptions.RootPrimPath = "/"

        box3 = (Box pos:[1,1,1] width:1 length:1 height:1 name:"Box003")
        box3.rotation = (quat  0 0.707 0 0.707)
        box3.scale = [2, 2, 2]
        box4 = (Box pos:[2,2,2] width:1 length:1 height:1 name:"Box004")
        box4.rotation = (quat  0.3826834 0 0 0.9238795) -- 45 degrees x axis
        box4.parent = box3
        
        USDExporter.ExportFile stageFileName exportOptions:exportOptions

        local stage = pyUsd.Stage.Open(stageFileName)
        pyUsdGeom.SetStageMetersPerUnit stage 100
        (stage.GetRootLayer()).Save()

        resetmaxfile #noprompt
        units.SystemType = #Kilometers
        units.SystemScale = 1
        USDImporter.ImportFile stageFileName

        assert_point3_equal [0.2,0,0.282843] $Box004.pos
        assert_point3_equal [0.15,-0.0353553,0.247488] $Box004.vertices[1].pos
        assert_point3_equal [0.25,-0.0353553,0.247488] $Box004.vertices[2].pos
        assert_point3_equal [0.15,0.0353554,0.318198] $Box004.vertices[3].pos
        assert_point3_equal [0.25,0.0353554,0.318198] $Box004.vertices[4].pos
        assert_point3_equal [0.15,-0.106066,0.318198] $Box004.vertices[5].pos
        assert_point3_equal [0.25,-0.106066,0.318198] $Box004.vertices[6].pos
        assert_point3_equal [0.15,-0.0353553,0.388909] $Box004.vertices[7].pos
        assert_point3_equal [0.25,-0.0353553,0.388909] $Box004.vertices[8].pos

        resetmaxfile #noprompt
        units.SystemType = #Meters
        units.SystemScale = 2
        USDImporter.ImportFile stageFileName

        assert_point3_equal [100,1.14441e-05,141.421] $Box004.pos
        assert_point3_equal [75,-17.6777,123.744] $Box004.vertices[1].pos
        assert_point3_equal [125,-17.6777,123.744] $Box004.vertices[2].pos
        assert_point3_equal [75,17.6777,159.099] $Box004.vertices[3].pos
        assert_point3_equal [125,17.6777,159.099] $Box004.vertices[4].pos
        assert_point3_equal [75,-53.033,159.099] $Box004.vertices[5].pos
        assert_point3_equal [125,-53.033,159.099] $Box004.vertices[6].pos
        assert_point3_equal [75,-17.6777,194.454] $Box004.vertices[7].pos
        assert_point3_equal [125,-17.6777,194.454] $Box004.vertices[8].pos
    ),
    
    function test_import_scaled_instances = (
        local stageFileName = (input_prefix_main + "import_scaled_instances.usda")

        local exportOptions = USDExporter.CreateOptions()
        exportOptions.FileFormat = #ascii
        exportOptions.RootPrimPath = "/"

        -- Export the file in meters.
        units.SystemType = #Meters
        units.SystemScale = 1       
        
        local box1 = (Box pos:[1,0,0] width:1 length:1 height:1 name:"Box1")
        -- Create an instance
        local box2 = create_clone box1 #instance "Box2"
        box2.pos = [10,0,0]
        
        USDExporter.ExportFile stageFileName exportOptions:exportOptions

        -- Import in a scene in centimeters.
        resetmaxfile #noprompt
        units.SystemType = #Centimeters
        units.SystemScale = 1
        USDImporter.ImportFile stageFileName

        local box1Import = $Box1.children[1]
        local box2Import = $Box2.children[1]

        assert_point3_equal [100,0,0] box1Import.pos
        assert_point3_equal [50,-50,0] box1Import.vertices[1].pos
        assert_point3_equal [150,-50,0] box1Import.vertices[2].pos
        assert_point3_equal [50,50,0] box1Import.vertices[3].pos
        assert_point3_equal [150,50,0] box1Import.vertices[4].pos
        assert_point3_equal [50,-50,100] box1Import.vertices[5].pos
        assert_point3_equal [150,-50,100] box1Import.vertices[6].pos
        assert_point3_equal [50,50,100] box1Import.vertices[7].pos
        assert_point3_equal [150,50,100]  box1Import.vertices[8].pos

        assert_point3_equal [1000,0,0] box2Import.pos
        assert_point3_equal [950,-50,000] box2Import.vertices[1].pos
        assert_point3_equal [1050,-50,0] box2Import.vertices[2].pos
        assert_point3_equal [950,50,0] box2Import.vertices[3].pos
        assert_point3_equal [1050,50,0] box2Import.vertices[4].pos
        assert_point3_equal [950,-50,100] box2Import.vertices[5].pos
        assert_point3_equal [1050,-50,100] box2Import.vertices[6].pos
        assert_point3_equal [950,50,100] box2Import.vertices[7].pos
        assert_point3_equal [1050,50,100] box2Import.vertices[8].pos
        
        -- Import in a scene in meters.
        resetmaxfile #noprompt
        units.SystemType = #Meters
        units.SystemScale = 1
        USDImporter.ImportFile stageFileName
        
        local box1Xform = $box1
        local box2Xform = $box2
        local box1Geom = $Box1.children[1]
        local box2Geom = $Box2.children[1]

        assert_point3_equal [1,0,0] box1Xform.pos
        assert_point3_equal [1,0,0] box1Geom.pos
        assert_point3_equal [0.5,-0.5,0] box1Geom.vertices[1].pos
        assert_point3_equal [1.5,-0.5,0] box1Geom.vertices[2].pos
        assert_point3_equal [0.5,0.5,0] box1Geom.vertices[3].pos
        assert_point3_equal [1.5,0.5,0] box1Geom.vertices[4].pos
        assert_point3_equal [0.5,-0.5,1] box1Geom.vertices[5].pos
        assert_point3_equal [1.5,-0.5,1] box1Geom.vertices[6].pos
        assert_point3_equal [0.5,0.5,1] box1Geom.vertices[7].pos
        assert_point3_equal [1.5,0.5,1] box1Geom.vertices[8].pos

        assert_point3_equal [10,0,0] box2Xform.pos
        assert_point3_equal [10,0,0] box2Geom.pos
        assert_point3_equal [9.5,-0.5,0] box2Geom.vertices[1].pos
        assert_point3_equal [10.5,-0.5,0] box2Geom.vertices[2].pos
        assert_point3_equal [9.5,0.5,0] box2Geom.vertices[3].pos
        assert_point3_equal [10.5,0.5,0] box2Geom.vertices[4].pos
        assert_point3_equal [9.5,-0.5,1] box2Geom.vertices[5].pos
        assert_point3_equal [10.5,-0.5,1] box2Geom.vertices[6].pos
        assert_point3_equal [9.5,0.5,1] box2Geom.vertices[7].pos
        assert_point3_equal [10.5,0.5,1] box2Geom.vertices[8].pos
    ), 
    
    function teardown = (
        units.SystemType = initialUnits
        units.SystemScale = initialScale
    ),

    Tests = #(test_millimeter_units, 
        test_centimeter_units,
        test_meter_units,
        test_two_meters_units,
        test_kilometer_units,
        test_orginal_content_do_not_get_scaled,
        test_import_scaled_children_nodes,
        test_import_scaled_instances
    )
)

runUsdTestFixture import_metersPerUnit_test (getThisScriptFilename())