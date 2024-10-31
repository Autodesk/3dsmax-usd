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
    
output_prefix = pathConfig.appendPath (getDir #temp) "/IO_COLOR_N_VISIBILITY_TEST_"
deleteTestFiles(output_prefix)

struct io_color_n_visibility_test
(
    private 
    exportOptions = USDExporter.CreateOptions(),

    public
    function setup =
    (
        exportOptions.FileFormat = #ascii
        exportOptions.RootPrimPath = "/"
        resetmaxfile #noprompt
    ),
    
    function test_io_display_color = (
        testFilePath = output_prefix + "test_io_display_color.usd"
        -- Basic case, wire color is outputted to the displayColor primvar...
        local b = box name:"box"
        local redColor = Color 255 0 0
        b.wireColor = redColor
        USDExporter.ExportFile testFilePath exportOptions:exportOptions

        stage = pyUsd.Stage.Open(testFilePath)
        prim = pyUsdGeom.Gprim.Get stage ("/" + b.name)
        local mesh = pyUsdGeom.Mesh(prim)
        local displayColorAttr = mesh.GetDisplayColorAttr()
        local dispColor = (displayColorAttr.Get())[0]
        assert_equal 1 dispColor[1] message:"Unexpected displayColor, should be from the wire color."
        assert_equal 0 dispColor[2] message:"Unexpected displayColor, should be from the wire color."
        assert_equal 0 dispColor[3] message:"Unexpected displayColor, should be from the wire color."
        
        resetMaxFile #noprompt
        -- Reimport the file, now we should get back the right wire color.
        USDImporter.ImportFile testFilePath
        b = $box    
        assert_equal redColor b.wireColor message:"Unexpected wireColor on import. Should be from primvars:displayColor."
    ),
    
    function test_display_color_override = (
        testFilePath = output_prefix + "test_display_color_override.usd"
        local b = box name:"box"
        convertToPoly b
        b.wireColor = Color 255 0 0 -- red
        local greenColor = Color 0 255 0
        
        -- Set green as vertex color.
        polyop.setVertColor b 0 #(1,2,3,4,5,6,7,8) greenColor

        -- Explicitely map the vertex color channel to primvar:displayColor.
        exportOptions.SetChannelPrimvarMapping 0 "displayColor"
        USDExporter.ExportFile testFilePath exportOptions:exportOptions

        -- Now the display color should respect our mapping, and not use the wire color.
        stage = pyUsd.Stage.Open(testFilePath)
        prim = pyUsdGeom.Gprim.Get stage ("/" + b.name)
        local mesh = pyUsdGeom.Mesh(prim)
        local displayColorAttr = mesh.GetDisplayColorAttr()
        local dispColor = (displayColorAttr.Get())[0]
        assert_equal 0 dispColor[1] message:"Unexpected displayColor, should be from the vertex color."
        assert_equal 1 dispColor[2] message:"Unexpected displayColor, should be from the vertex color."
        assert_equal 0 dispColor[3] message:"Unexpected displayColor, should be from the vertex color."
            
        resetMaxFile #noprompt
        -- Reimport the file, now we should get back green as the wire color...
        USDImporter.ImportFile testFilePath
        b = $box    
        assert_equal greenColor b.wireColor message:"Unexpected wireColor on import. Should be from primvars:displayColor."
    ),    

    function test_visibility = (
        testFilePath = output_prefix + "test_visibility.usd"
        
        local b = box name:"box"
        convertToPoly b
            
        local hiddenBox = box name:"hiddenBox"
        convertToPoly hiddenBox
        hiddenBox.isNodeHidden = true
        hiddenBox.pos = [20, 0, 0]
        
        local opts = USDExporter.CreateOptions()
        opts.RootPrimPath = "/"
        opts.HiddenObjects = true
        opts.UseUSDVisibility = true
        USDExporter.ExportFile testFilePath exportOptions:opts

        stage = pyUsd.Stage.Open(testFilePath)
        
        local prim = pyUsdGeom.Gprim.Get stage ("/" + b.name)
        local mesh = pyUsdGeom.Mesh(prim)
        local visibilityAttr = mesh.GetVisibilityAttr()
        local visibility = (visibilityAttr.Get())
        assert_equal "inherited" visibility message:"Unexpected box.visibility, should be inherited."
        
        prim = pyUsdGeom.Gprim.Get stage ("/" + hiddenBox.name)
        mesh = pyUsdGeom.Mesh(prim)
        visibilityAttr = mesh.GetVisibilityAttr()
        visibility = (visibilityAttr.Get())
        assert_equal "invisible" visibility message:"Unexpected hiddenBox.visibility, should be invisible."
            
        resetMaxFile #noprompt

        USDImporter.ImportFile testFilePath
        b = $box   
        assert_false b.isNodeHidden message:"Unexpected box.isNodeHidden on import. Node should not be hidden."
        hiddenBox = $hiddenBox   
        assert_true hiddenBox.isNodeHidden message:"Unexpected hiddenBox.isNodeHidden on import. Node should be hidden."
    ),
    
    function test_import_inherited_visibility = (
        testFilePath = output_prefix + "test_inherited_visibility.usda"
        
        local stage = pyUsd.Stage.CreateNew(testFilePath)
        local rootPrim = stage.DefinePrim "/rootPrim"
        local branch1Prim = pyUsdGeom.Xform.Define stage "/rootPrim/invisiblePrim"
        local branch1ChildPrim = stage.DefinePrim "/rootPrim/invisiblePrim/child"
        local branch1GrandchildPrim = stage.DefinePrim "/rootPrim/invisiblePrim/child/grandchild"
        local branch2Prim = stage.DefinePrim "/rootPrim/visiblePrim"
        local branch2ChildPrim = stage.DefinePrim "/rootPrim/visiblePrim/child"
        local branch2GrandchildPrim = pyUsdGeom.Xform.Define stage "/rootPrim/visiblePrim/child/invisibleGrandchild"
        
        -- Setting the branch 1 root prim invisible should make the entire branch invisible
        invisibleImageablePrim = pyUsdGeom.Imageable(branch1Prim)
        visibilityAttr = invisibleImageablePrim.GetVisibilityAttr()
        visibilityAttr.Set(pyUsdGeom.Tokens.invisible)
        
        -- Setting the branch 2 grand child prim invisible should make only this prim invisible
        invisibleImageablePrim2 = pyUsdGeom.Imageable(branch2GrandchildPrim)
        visibilityAttr = invisibleImageablePrim2.GetVisibilityAttr()
        visibilityAttr.Set(pyUsdGeom.Tokens.invisible)
        
        (stage.GetRootLayer()).Save()
        USDImporter.ImportFile testFilePath
        
        assert_false $rootPrim.isNodeHidden message:"The root prim should not be hidden."
        
        -- validate branch 1
        assert_true $rootPrim/invisiblePrim.isNodeHidden message:"The invisible prim should  be hidden."
        assert_true $rootPrim/invisiblePrim/child.isNodeHidden message:"The invisible prim child should be hidden."
        assert_true $rootPrim/invisiblePrim/child/grandchild.isNodeHidden message:"The invisible prim grandchild should be hidden."
        
        -- validate branch 2
        assert_false $rootPrim/visiblePrim.isNodeHidden message:"The visible prim should not be hidden."
        assert_false $rootPrim/visiblePrim/child.isNodeHidden message:"The visible prim child should not be hidden."
        assert_true $rootPrim/visiblePrim/child/invisibleGrandchild.isNodeHidden message:"The invisible grand child prim should be hidden."
    ),
    
    
    function count_file_lines filePath = (
        local file = openFile filePath
        local count = 0
        while not eof file do (
            readLine file
            count += 1
        )
        close file
        return count
    ),
    
    -- In USD, visibility is inherited, in Max it is not. This is a fundamental difference for which we 
    -- do not have a solution at this time. When we find problematic cases, we log a warning...
    function test_export_visibility_warnings = (
        
        testFilePath = output_prefix + "test_visibility_warnings.usd"
        
        local b1 = box name:"box1"
        local b2 = box name:"box2"
        b2.parent = b1
        local b3 = box name:"box3"
        b3.parent = b2
        local b4 = box name:"box4"
        b4.parent = b3
        
        local expOptions = USDExporter.CreateOptions()
        expOptions.RootPrimPath = "/"
        expOptions.LogLevel = #warn
        expOptions.HiddenObjects = true
        expOptions.UseUSDVisibility = true
        
        -- At this point there is no issue with visibility, expect no warnings. 
        USDExporter.ExportFile testFilePath exportOptions:expOptions
        assert_equal 0 (count_file_lines(expOptions.LogPath))
        deleteFile expOptions.LogPath
            
        -- Hide box3 and box4, still no issue, visibility cant still properly be represented
        -- on the USD side.
        b3.isNodeHidden = true
        b4.isNodeHidden = true
        
        USDExporter.ExportFile testFilePath exportOptions:expOptions
        assert_equal 0 (count_file_lines(expOptions.LogPath))
        deleteFile expOptions.LogPath
        
        -- Now hide box1. This will be an issue as box2 is still visible. Expect one warning.
        b1.isNodeHidden = true
        USDExporter.ExportFile testFilePath exportOptions:expOptions
        assert_equal 1 (count_file_lines(expOptions.LogPath))
        deleteFile expOptions.LogPath
            
        -- Now show again box4. Still expect a single warning, we only warn once on the node highest in 
        -- the problematic hierarchy.
        b4.isNodeHidden = false
        b1.isNodeHidden = true
        USDExporter.ExportFile testFilePath exportOptions:expOptions
        assert_equal 1 (count_file_lines(expOptions.LogPath))
        deleteFile expOptions.LogPath
        
        -- Create a second problematic hierarchy, to make sure that we indeed get one warning per problematic 
        -- hierarchy.
        local s1 = sphere name:"shpere1"
        local s2 = sphere name:"shpere2"
        s2.parent = s1
        s1.isNodeHidden = true
        USDExporter.ExportFile testFilePath exportOptions:expOptions
        assert_equal 2 (count_file_lines(expOptions.LogPath))
        deleteFile expOptions.LogPath
    ),
    
    function teardown = (
    ),

    tests = #(test_io_display_color, 
        test_display_color_override, 
        test_visibility, 
        test_import_inherited_visibility,
        test_export_visibility_warnings)
)

runUsdTestFixture io_color_n_visibility_test (getThisScriptFilename())