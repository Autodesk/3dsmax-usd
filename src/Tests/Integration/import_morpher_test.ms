--
-- Copyright 2024 Autodesk
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
clearlistener()

testDataDir = getFilenamePath (getThisScriptFilename())
importMorpherUsdPath = (testDataDir + @"data\morpher_box.usd")
importFemaleWalkUsdPath = (testDataDir + @"data\usdskel_sample.usdc")
output_prefix = pathConfig.appendPath (getDir #temp) "/IMPORT_MORPHER_TEST_"
deleteTestFiles(output_prefix + "*.*")

struct import_morpher_test
(
    private 
    importOptions = USDImporter.CreateOptions(),
    initialUnits = units.SystemType,
    initialScale = units.SystemScale,
    
    function resetMaxFileAndSetupUnits = 
    (
        resetmaxfile #noprompt
        units.SystemType = #Centimeters
        units.SystemScale = 1
    ),

    public
    function setup =
    (
        resetMaxFileAndSetupUnits()
    ),

    function test_import_morpher = (
        resetMaxFileAndSetupUnits()
        USDImporter.ImportFile importMorpherUsdPath importOptions:importOptions

        local node = $b1
        local modi = node.modifiers[1]
        assert_true (iskindof modi Modifier) message: "Top modifier should have been a morpher modifier"
        assert_equal (WM3_MC_GetName modi 1) "Box0002" message: "Morpher channel was not named correctly"
        assert_equal (WM3_NumberOfProgressiveMorphs modi 1) 3 message: "Expected 3 progressive morpher in channel"

        local numVertices = meshOp.getNumVerts node.mesh
        local expectedAt0 = #([-50,-50,0], [50,-50,0], [-50,50,0], [50,50,0], [0,0,100], [0,0,100], [0,0,100], [0,0,100])
        local expectedAt3 = #()
        maxver = maxversion()
        if maxver[1] > 25000 then (  -- 3ds Max 2024 and up
            expectedAt3 = #([-50,-50,0], [50,-50,0], [-50,50,0], [50,50,0], [-42.1593,-42.1593,100], [42.1593,-42.1593,100], [-42.1593,42.1593,100], [42.1593,42.1593,100])
        ) else (
            expectedAt3 = #([-50,-50,0], [50,-50,0], [-50,50,0], [50,50,0], [-49.214,-49.214,100], [49.214,-49.214,100], [-49.214,49.214,100], [49.214,49.214,100])
        )
        local expectedAt5 = #([-50,-50,0], [50,-50,0], [-50,50,0], [50,50,0], [-50,-50,100], [50,-50,100], [-50,50,100], [50,50,100])
        local expectedAt7 = #([-50,-50,0], [50,-50,0], [-50,50,0], [50,50,0], [-57.8125,-57.8125,100], [57.8125,-57.8125,100], [-57.8125,57.8125,100], [57.8125,57.8125,100])
        local expectedAt10 = #([-50,-50,0], [50,-50,0], [-50,50,0], [50,50,0], [-100,-100,100], [100,-100,100], [-100,100,100], [100,100,100])
        
        local errorMessage = "Expected different value at position: "
        for vert = 1 to numVertices do
        (
            assert_point3_equal (meshOp.getVert node.mesh vert) (expectedAt0[vert]) message: (errorMessage + vert as string)
        )

        slidertime = 3
        for vert = 1 to numVertices do
        (
            assert_point3_equal (meshOp.getVert node.mesh vert) (expectedAt3[vert]) message: (errorMessage + vert as string)
        )

        slidertime = 5
        for vert = 1 to numVertices do
        (
            assert_point3_equal (meshOp.getVert node.mesh vert) (expectedAt5[vert]) message: (errorMessage + vert as string)
        )

        slidertime = 7
        for vert = 1 to numVertices do
        (
            assert_point3_equal (meshOp.getVert node.mesh vert) (expectedAt7[vert]) message: (errorMessage + vert as string)
        )

        slidertime = 10
        for vert = 1 to numVertices do
        (
            assert_point3_equal (meshOp.getVert node.mesh vert) (expectedAt10[vert]) message: (errorMessage + vert as string)
        )
    ),

    function test_several_channels = (
        resetMaxFileAndSetupUnits()
        USDImporter.ImportFile importFemaleWalkUsdPath importOptions:importOptions

        local n = $Body_sbdv
        local modi = n.modifiers[2]

        maxver = maxversion()
        -- A bug that would prevent progressive morphers to be added on channel above 100 was fix on max 2025.1
        local skipInbetweensOver100 = (maxver[1] < 27000) or ((maxver[1] <= 27000) and (maxver[5] < 1))

        local totalUsedChannels = 342
        local totalChannels = 400

        assert_equal totalChannels (WM3_NumberOfChannels modi) message: "Didn't create the proper number of channels"

        local expectedNumProgressiveMorpher = #()
        if skipInbetweensOver100 then
        (
            -- on channels above 100, there will only be 1 progressive morpher on max versions before 2025.1
            expectedNumProgressiveMorpher = #(3, 1, 3, 3, 3, 3, 1, 3, 1, 1, 1, 1, 1, 3, 3, 1, 1, 1, 3, 1, 3, 1, 3, 3, 3, 3, 3, 3, 1, 1, 3, 3, 1, 1, 1, 1, 3, 3, 1, 1, 1, 3, 3, 1, 3, 1, 1, 3, 3, 3, 3, 3, 3, 3, 3, 3, 1, 3, 3, 3, 1, 3, 3, 3, 1, 1, 1, 3, 3, 3, 3, 1, 1, 3, 3, 1, 3, 3, 1, 3, 3, 1, 3, 3, 3, 1, 3, 1, 3, 1, 3, 3, 3, 1, 3, 3, 3, 3, 3, 3, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1)
        )
        else
        (
            expectedNumProgressiveMorpher = #(3 ,1 ,3 ,3 ,3 ,3 ,1 ,3 ,1 ,1 ,1 ,1 ,1 ,3 ,3 ,1 ,1 ,1 ,3 ,1 ,3 ,1 ,3 ,3 ,3 ,3 ,3 ,3 ,1 ,1 ,3 ,3 ,1 ,1 ,1 ,1 ,3 ,3 ,1 ,1 ,1 ,3 ,3 ,1 ,3 ,1 ,1 ,3 ,3 ,3 ,3 ,3 ,3 ,3 ,3 ,3 ,1 ,3 ,3 ,3 ,1 ,3 ,3 ,3 ,1 ,1 ,1 ,3 ,3 ,3 ,3 ,1 ,1 ,3 ,3 ,1 ,3 ,3 ,1 ,3 ,3 ,1 ,3 ,3 ,3 ,1 ,3 ,1 ,3 ,1 ,3 ,3 ,3 ,1 ,3 ,3 ,3 ,3 ,3 ,3 ,3 ,3 ,3 ,3 ,1 ,3 ,3 ,1 ,3 ,3 ,3 ,3 ,3 ,1 ,1 ,3 ,3 ,3 ,1 ,3 ,1 ,3 ,1 ,3 ,3 ,3 ,1 ,3 ,1 ,3 ,1 ,3 ,1 ,1 ,3 ,3 ,3 ,3 ,3 ,1 ,3 ,3 ,3 ,1 ,3 ,3 ,1 ,3 ,3 ,1 ,3 ,1 ,1 ,3 ,1 ,3 ,1 ,1 ,1 ,1 ,1 ,3 ,3 ,1 ,1 ,3 ,1 ,3 ,1 ,1 ,1 ,1 ,3 ,3 ,1 ,3 ,1 ,1 ,3 ,1 ,1 ,1 ,3 ,3 ,3 ,1 ,1 ,1 ,3 ,1 ,3 ,3 ,3 ,1 ,3 ,1 ,3 ,1 ,1 ,3 ,3 ,1 ,3 ,1 ,3 ,3 ,3 ,3 ,3 ,3 ,1 ,3 ,1 ,1 ,3 ,3 ,1 ,3 ,1 ,1 ,1 ,3 ,1 ,3 ,3 ,3 ,3 ,3 ,3 ,1 ,3 ,1 ,3 ,3 ,3 ,3 ,3 ,3 ,3 ,3 ,1 ,1 ,1 ,1 ,1 ,1 ,3 ,1 ,1 ,3 ,3 ,3 ,3 ,1 ,3 ,1 ,3 ,3 ,3 ,3 ,3 ,3 ,3 ,3 ,1 ,3 ,3 ,1 ,3 ,1 ,1 ,3 ,3 ,1 ,3 ,1 ,1 ,1 ,3 ,1 ,3 ,1 ,3 ,3 ,3 ,3 ,3 ,1 ,3 ,1 ,3 ,3 ,1 ,3 ,1 ,3 ,3 ,1 ,1 ,3 ,1 ,1 ,3 ,3 ,3 ,3 ,1 ,1 ,3 ,3 ,3 ,3 ,3 ,3 ,1 ,3 ,3 ,3 ,1 ,1 ,3 ,3 ,3 ,3 ,1 ,3 ,1 ,1 ,3 ,3 ,1 ,3 ,3 ,3 ,3 ,1 ,3 ,1 ,1 ,3 ,3 ,3)
        )
        
        for c = 1 to totalUsedChannels do
        (
            assert_equal (WM3_NumberOfProgressiveMorphs modi c) expectedNumProgressiveMorpher[c] message: ("Wrong number of progressive morpher for channel " + (c as string))
        )
    ),

    function teardown = (
        units.SystemType = initialUnits
        units.SystemScale = initialScale
    ),

    tests = #(test_import_morpher,
        test_several_channels
    )
)

runUsdTestFixture import_morpher_test (getThisScriptFilename())
