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
clearlistener()

pyUsd = python.import("pxr.Usd")
pyUsdGeom = python.import("pxr.UsdGeom")
pyGf = python.import("pxr.Gf")
pyKind = python.import("pxr.Kind")

input_prefix = pathConfig.appendPath (getDir #temp) "/IMPORT_METADATA_TEST_"
deleteTestFiles(input_prefix + "*.*")

struct import_metadata_test
(
    private 
    importOptions = USDImporter.CreateOptions(),

    function resetMaxFileAndSetupUnits = 
    (
        resetmaxfile #noprompt
    ),
    
    public
    function setup =
    (
        resetMaxFileAndSetupUnits()
    ),

    function populateUsdStageWithAllMetaDataCombinations usdStage = (
        local prim = pyUsdGeom.Mesh.Define usdStage "/all"
        local purposeAttr = (pyUsdGeom.Imageable(prim)).CreatePurposeAttr()
        local model = (pyUsd.ModelAPI(prim))
        local primObj = (prim.GetPrim())
        purposeAttr.Set("render")
        model.SetKind("component")
        primObj.SetHidden(True)

        prim = pyUsdGeom.Mesh.Define usdStage "/hiddenTrue_purposeProxy"
        purposeAttr = (pyUsdGeom.Imageable(prim)).CreatePurposeAttr()
        model = (pyUsd.ModelAPI(prim))
        primObj = (prim.GetPrim())
        purposeAttr.Set("proxy")
        primObj.SetHidden(True)

        prim = pyUsdGeom.Mesh.Define usdStage "/hiddenTrue"
        purposeAttr = (pyUsdGeom.Imageable(prim)).CreatePurposeAttr()
        model = (pyUsd.ModelAPI(prim))
        primObj = (prim.GetPrim())
        primObj.SetHidden(True)

        prim = pyUsdGeom.Mesh.Define usdStage "/kindComponent_hiddenFalse"
        purposeAttr = (pyUsdGeom.Imageable(prim)).CreatePurposeAttr()
        model = (pyUsd.ModelAPI(prim))
        primObj = (prim.GetPrim())
        model.SetKind("component")
        primObj.SetHidden(False)

        prim = pyUsdGeom.Mesh.Define usdStage "/kindGroup"
        purposeAttr = (pyUsdGeom.Imageable(prim)).CreatePurposeAttr()
        model = (pyUsd.ModelAPI(prim))
        primObj = (prim.GetPrim())
        model.SetKind("group")

        prim = pyUsdGeom.Mesh.Define usdStage "/none"

        prim = pyUsdGeom.Mesh.Define usdStage "/purposeDefault"
        purposeAttr = (pyUsdGeom.Imageable(prim)).CreatePurposeAttr()
        model = (pyUsd.ModelAPI(prim))
        primObj = (prim.GetPrim())
        model.SetKind("component")

        prim = pyUsdGeom.Mesh.Define usdStage "/purposeProxy"
        purposeAttr = (pyUsdGeom.Imageable(prim)).CreatePurposeAttr()
        model = (pyUsd.ModelAPI(prim))
        primObj = (prim.GetPrim())
        purposeAttr.Set("proxy")
    ),

    function test_metadata = (
        -- given
        testUsdFile = input_prefix + "test_metadata.usda"
        local stage = pyUsd.Stage.CreateNew(testUsdFile)
        local prim001 = pyUsdGeom.Mesh.Define stage "/Prim001"
        local purposeAttr = (pyUsdGeom.Imageable(prim001)).CreatePurposeAttr()
        purposeAttr.Set(pyUsdGeom.Tokens.proxy)
        (pyUsd.ModelAPI(prim001)).SetKind(pyKind.Tokens.component)
        (prim001.GetPrim()).SetHidden(True)
        (stage.GetRootLayer()).Save()

        -- when
        USDImporter.ImportFile testUsdFile

        -- then
        local importedPrim = $Prim001
        assert_true importedPrim.usd_hidden message:"usd_hidden expected to be True"
        assert_equal "component" importedPrim.usd_kind message:"usd_kind expected to be 'component'"
        assert_equal "proxy" importedPrim.usd_purpose message:"usd_purpose expected to be 'default'"
    ),

    function test_empty_values = (
        -- given
        testUsdFile = input_prefix + "test_empty_values.usda"
        local stage = pyUsd.Stage.CreateNew(testUsdFile)
        local prim001 = pyUsdGeom.Mesh.Define stage "/Prim001"
        local purposeAttr = (pyUsdGeom.Imageable(prim001)).CreatePurposeAttr()
        purposeAttr.Set("")
        (pyUsd.ModelAPI(prim001)).SetKind("")
        (stage.GetRootLayer()).Save()

        -- when
        USDImporter.ImportFile testUsdFile

        -- then
        local importedPrim = $Prim001
        assert_false (isProperty importedPrim #usd_kind) message:"usd_kind should not be defined"
        assert_false (isProperty importedPrim #usd_purpose) message:"usd_purpose should not be defined"
    ),

    function test_default_metadata = (
        -- given
        testUsdFile = input_prefix + "test_default_metadata.usda"
        local stage = pyUsd.Stage.CreateNew(testUsdFile)
        local prim001 = pyUsdGeom.Mesh.Define stage "/Prim001"
        (stage.GetRootLayer()).Save()

        -- when
        USDImporter.ImportFile testUsdFile

        -- then
        local importedPrim = $Prim001
        assert_false (isProperty importedPrim #usd_hidden) message:"usd_hidden should not be defined"
        assert_false (isProperty importedPrim #usd_kind) message:"usd_kind should not be defined"
        assert_false (isProperty importedPrim #usd_purpose) message:"usd_purpose should not be defined"
    ),

    function test_hidden_false = (
        -- given
        testUsdFile = input_prefix + "test_hidden_false.usda"
        local stage = pyUsd.Stage.CreateNew(testUsdFile)
        local prim001 = pyUsdGeom.Mesh.Define stage "/Prim001"
        local prim001Obj = prim001.GetPrim()
        prim001Obj.SetHidden(False)
        (stage.GetRootLayer()).Save()

        -- when
        USDImporter.ImportFile testUsdFile

        -- then
        importedPrim = $Prim001
        assert_false (isProperty importedPrim #usd_hidden) message:"usd_hidden should not be defined"
    ),

    function test_non_default_values = (
        -- given
        testUsdFile = input_prefix + "test_non_default_values.usda"
        purposeStr = "blablaP"
        kindStr = "blebleK"
        local stage = pyUsd.Stage.CreateNew(testUsdFile)
        local prim001 = pyUsdGeom.Mesh.Define stage "/Prim001"
        local purposeAttr = (pyUsdGeom.Imageable(prim001)).CreatePurposeAttr()
        local model = (pyUsd.ModelAPI(prim001))
        local primObj = (prim001.GetPrim())
        local rootLayer = (stage.GetRootLayer())
        purposeAttr.Set(purposeStr)
        model.SetKind(kindStr)
        rootLayer.Save()

        -- when
        USDImporter.ImportFile testUsdFile

        -- then
        local importedPrim = $Prim001
        assert_equal kindStr importedPrim.usd_kind
        assert_equal purposeStr importedPrim.usd_purpose
    ),

    function test_special_chars = (
        -- given
        testUsdFile = input_prefix + "test_special_chars.usda"
        purposeStr = "~!@#$%^&*()_+{}[]:;'<>?,./'|\\\""
        kindStr = "~!@#$%^&*()_+{}[]:;'<>?,./'|\\\""
        local stage = pyUsd.Stage.CreateNew(testUsdFile)
        local prim001 = pyUsdGeom.Mesh.Define stage "/Prim001"
        local purposeAttr = (pyUsdGeom.Imageable(prim001)).CreatePurposeAttr()
        local model = (pyUsd.ModelAPI(prim001))
        local primObj = (prim001.GetPrim())
        local rootLayer = (stage.GetRootLayer())
        purposeAttr.Set(purposeStr)
        model.SetKind(kindStr)
        rootLayer.Save()

        -- when
        USDImporter.ImportFile testUsdFile

        -- then
        local importedPrim = $Prim001
        assert_equal kindStr importedPrim.usd_kind
        assert_equal purposeStr importedPrim.usd_purpose
    ),

    function test_metadata_options = (
        -- given
        testUsdFile = input_prefix + "test_metadata_options.usda"
        local stage = pyUsd.Stage.CreateNew(testUsdFile)
        local rootLayer = (stage.GetRootLayer())
        populateUsdStageWithAllMetaDataCombinations stage
        rootLayer.Save()

        -- when
        USDImporter.ImportFile testUsdFile

        -- then
        local importedPrim = $all
        assert_equal "component" importedPrim.usd_kind
        assert_equal "render" importedPrim.usd_purpose
        assert_true importedPrim.usd_hidden

        -- when
        resetmaxfile #noprompt
        importOptions.MetaData = #(#kind)
        USDImporter.ImportFile testUsdFile importOptions:importOptions

        -- then
        local importedPrim = $all
        assert_equal "component" importedPrim.usd_kind
        local groupPrim = $kindGroup
        assert_equal "group" groupPrim.usd_kind
        assert_false (isProperty importedPrim #usd_hidden) message:"usd_hidden should not be defined"
        assert_false (isProperty importedPrim #usd_purpose) message:"usd_purpose should not be defined"

        -- when
        resetmaxfile #noprompt
        importOptions.MetaData = #(#kind, #purpose, #kind, #kind, #purpose)
        USDImporter.ImportFile testUsdFile importOptions:importOptions

        -- then
        assert_equal 2 importOptions.MetaData.count message:"duplicate values should be ignored"
        local importedPrim = $all
        assert_equal "component" importedPrim.usd_kind
        assert_equal "render" importedPrim.usd_purpose
        assert_false (isProperty importedPrim #usd_hidden) message:"usd_hidden should not be defined"
        local groupPrim = $kindGroup
        assert_equal "group" groupPrim.usd_kind
        assert_false (isProperty groupPrim #usd_hidden) message:"usd_hidden should not be defined"
        assert_false (isProperty groupPrim #usd_purpose) message:"usd_purpose should not be defined"

        -- when
        resetmaxfile #noprompt
        importOptions.MetaData = #(#hidden)
        USDImporter.ImportFile testUsdFile importOptions:importOptions

        -- then
        local importedPrim = $all
        assert_true importedPrim.usd_hidden
        assert_false (isProperty importedPrim #usd_kind) message:"usd_kind should not be defined"
        assert_false (isProperty importedPrim #usd_purpose) message:"usd_purpose should not be defined"
        local hiddenPrim = $kindComponent_hiddenFalse
        assert_false (isProperty hiddenPrim #usd_hidden) message:"usd_kind should not be defined"
        assert_false (isProperty hiddenPrim #usd_kind) message:"usd_kind should not be defined"

        -- when
        resetmaxfile #noprompt
        importOptions.MetaData = #()
        USDImporter.ImportFile testUsdFile importOptions:importOptions

        -- then
        local importedPrim = $all
        assert_false (isProperty importedPrim #usd_kind) message:"usd_kind should not be defined"
        assert_false (isProperty importedPrim #usd_hidden) message:"usd_hidden should not be defined"
        assert_false (isProperty importedPrim #usd_purpose) message:"usd_purpose should not be defined"
    ),

    function teardown = (

    ),

    tests = #(test_metadata,
        test_non_default_values,
        test_special_chars,
        test_default_metadata,
        test_hidden_false,
        test_empty_values,
        test_metadata_options
    )
)

runUsdTestFixture import_metadata_test (getThisScriptFilename())