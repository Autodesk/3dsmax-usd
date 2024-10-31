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
pyGf = python.import("pxr.Gf")
pyKind = python.import("pxr.Kind")
pyUsdTf = python.import("pxr.Tf")
pyShade = python.import("pxr.UsdShade")
pysdf = python.import("pxr.Sdf")
pyUsdUI = python.import("pxr.UsdUI")
    
output_prefix = pathConfig.appendPath (getDir #temp) "/UNICODE_TEST_"
deleteTestFiles(output_prefix + "*.*")

struct unicode_test
(
    private
    exportOptions = USDExporter.CreateOptions(),
    usdAllCA,
    originalTempDir = getDir #temp,

    public
    function setup =
    (
        resetmaxfile #noprompt
        exportOptions.FileFormat = #ascii
        exportOptions.RootPrimPath = "/"
        exportOptions.Materials = true

        
        usdAllCA = attributes USDData
        (
            parameters USD
            (
                usd_purpose type:#string
                usd_hidden type:#boolean
                usd_kind type:#string
            )
        )
    ),

    function resetTempDir = (
        setDir #temp originalTempDir
    ),

    function createEmptyUsdaFile filename = (
        outFile = createfile filename
        format "#usda 1.0\\n(\\n    metersPerUnit = 0.0254\\n    upAxis = \"Z\"\\n)\\n" to:outFile
        close outFile
    ),

    function test_unicode_usdstage_filename = (
        if (USDStageObject == undefined) do (
            print("Skipping unicode_test.test_unicode_usdstage_filename() as MaxUsdObjects.dlo plugin not available.")
            return -1
        )

        --given
        local unicodeFilename = "いろはにほへ.usda"
        testUsdFile = output_prefix + unicodeFilename
        createEmptyUsdaFile testUsdFile

        --when
        local maxUsdStage = USDStageObject()
        try (
            maxUsdStage.SetRootLayer testUsdFile
        )
        catch 
        (
            exceptionThrown = true
        )
        assert_true exceptionThrown message:"Attempting to set a unicode rootlayer name should throw an exception."
    ),

    function test_unicode_usdz_tempdir = (
        --given
        local tempDir = getDir #temp
        tempDir = pathConfig.appendPath tempDir "/いろはにほへ"
        setDir #temp tempDir

        exportPath = output_prefix + "file_format_archive.usdz"

        --when
        status = USDExporter.ExportFile exportPath exportOptions:exportOptions

        --then
        assert_false status message:"usdz export should fail due to unicode characters in `getDir #temp` filepath"
        assert_false (doesFileExist exportPath) message:"usdz file should not have been exported due to unicode characters in `getDir #temp` filepath"

        resetTempDir()
    ),

    function test_unicode_filename = (
        -- given
        local unicodeFilename = "いろはにほへ.usda"
        mybox1 = Box pos:[0,0,0] width:1 length:1 height:1 name:"Box001"
        -- when
        testUsdFile = output_prefix + unicodeFilename
        status = exportFile testUsdFile #noprompt
        -- then
        assert_false status message:("Exporting to filename with unicode characters should be rejected")

        -- when
        testUsdFile = output_prefix + unicodeFilename
        createEmptyUsdaFile testUsdFile
        status = importFile testUsdFile #noprompt
        -- then
        assert_false status message:("Importing filename with unicode characters should be rejected")
    ),

    function test_unicode_metadata = (
        -- given
        unicodeTestString1 = "unicode Καλημέρα κόσμε, コンニチハ"
        unicodeTestString12 = "unicode £©µÀÆÖÞßéöÿ–—‘“”„†•…‰™œŠŸž€ ΑΒΓΔΩαβγδω АБВГДабвгд"
        mybox1 = Box pos:[0,0,0] width:0.001 length:0.001 height:0.001 name:"Box001"
        custAttributes.add mybox1.baseObject usdAllCA #Unique BaseObject:true
        mybox1.usd_hidden = true
        mybox1.usd_kind = unicodeTestString1
        mybox1.usd_purpose = unicodeTestString12

        -- when
        testUsdFile = output_prefix + "test_unicode_metadata.usda"
        status = USDExporter.ExportFile testUsdFile exportOptions:exportOptions

        -- then
        assert_true status message:"export to usd failed"
        local stage = pyUsd.Stage.Open(testUsdFile)
        local prim = stage.GetPrimAtPath("/Box001")
        local imageablePrim = pyUsdGeom.Imageable(prim)
        local purposeAttr = imageablePrim.GetPurposeAttr()
        local model = pyUsd.ModelAPI(prim)
        assert_equal unicodeTestString12 (purposeAttr.Get())
        assert_equal unicodeTestString1 (model.GetKind())

        -- when import unicode data encoded as ascii (multibyte) in usd file
        resetmaxfile #noprompt
        status = USDImporter.ImportFile testUsdFile

        -- then
        assert_true status message:"import usd file with unicode metadata failed"
        importedBox = $Box001
        assert_equal unicodeTestString1 importedBox.usd_kind message:("failed expectation on unicode data imported into custom attribute")
        assert_equal unicodeTestString12 importedBox.usd_purpose message:("failed expectation on unicode data imported into custom attribute")
    ),

    function test_unicode_nodenames = (
        --given
        boxAName = "איך הקליטה Ξεσκεπάζω τὴν ψυχοφθόρα βδελυγμία Zwölf Boxkämpfer Sævör grét því úlpan var ónýt" --91
        boxBName = "more unicode いろはにほへ イロハニホヘト พูดจาให้จ๊ะๆ จ๋าๆ น่าฟังเอย" --32
        boxA = box()
        boxA.name = boxAName
        boxB = box()
        boxB.name = boxBName

        --when
        local exportPath = (output_prefix + "unicode_export.usda")

        --then
        boxAIdentifierName = "_____________________________________________________________________________________Zw__lf_Boxk__mpfer_S__v__r_gr__t___v_____lpan_var___n__t"
        boxBIdentifierName = "more_unicode_______________________________________________________________________________________________________________________"
        status = USDExporter.ExportFile exportPath exportOptions:exportOptions
        assert_equal 1 status
        assert_true (doesFileExist exportPath)
        stage = pyUsd.Stage.Open(exportPath)
        local usdBoxA = stage.GetPrimAtPath("/" + boxAIdentifierName)
        local usdBoxB = stage.GetPrimAtPath("/" + boxBIdentifierName)
        assert_true (pyUsd.Prim.IsValid(usdBoxA)) message:("prim for boxA was not found")
        assert_true (pyUsd.Prim.IsValid(usdBoxB)) message:("prim for boxB was not found")
    ),

    function test_export_material_textures_unicode = (
        local box001 = Box pos:[0,0,0] name:"box001"
        local physicalMtl = PhysicalMaterial name:"usdPhysMapTestMtl"
        box001.material = physicalMtl
        
        local baseColorBitmapPath = @"C:\folderXY\uniaいろはcode_にほへ.png"
        
        physicalMtl.base_color_map = Bitmaptexture fileName:baseColorBitmapPath
        
        local exportPath = output_prefix + "export_material_bitmap_unicode_filename.usda"
        USDExporter.ExportFile exportPath exportOptions:exportOptions
        
        local stage = pyUsd.Stage.Open(exportPath)
        local shaderPrim = pyShade.Shader.Get stage "/mtl/usdPhysMapTestMtl/usdPhysMapTestMtl"
        assert_defined shaderPrim message:("missing USD material")
        assert_true ((shaderPrim.GetInput("diffuseColor")).HasConnectedSource())
        local connectedSourceTuple = ((shaderPrim.GetInput("diffuseColor")).GetConnectedSource())
        local inputFile = ((connectedSourceTuple[1]).GetInput("file")).Get()
        assert_undefined inputFile message:("'asset inputs:file' should not be set for filepath containing unicode characters on export")
    ),

    function test_export_material_name_unicode = (
        local box001 = Box pos:[0,0,0] name:"box001"
        local physicalMtl = PhysicalMaterial name:"uniaいろはcode_にほへ"
        box001.material = physicalMtl
        
        local exportPath = output_prefix + "export_material_name_unicode.usda"
        USDExporter.ExportFile exportPath exportOptions:exportOptions
        
        local stage = pyUsd.Stage.Open(exportPath)
        local shaderPrim = pyShade.Shader.Get stage "/mtl/unia_________code__________/unia_________code__________"
        assert_defined shaderPrim
    ),

    function teardown = (
        custAttributes.deleteDef usdAllCA
        resetTempDir()
    ),

    Tests = #(test_unicode_nodenames, test_unicode_metadata, test_unicode_filename, test_export_material_name_unicode,
        test_export_material_textures_unicode, test_unicode_usdz_tempdir, test_unicode_usdstage_filename)
)

runUsdTestFixture unicode_test (getThisScriptFilename())