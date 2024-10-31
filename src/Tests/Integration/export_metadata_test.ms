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
    
output_prefix = pathConfig.appendPath (getDir #temp) "/EXPORT_METADATA_TEST_"
deleteTestFiles(output_prefix + "*.*")

struct export_metadata_test
(
    private
    importOptions = USDImporter.CreateOptions(),
    exportOptions = USDExporter.CreateOptions(),

    function resetMaxFileAndSetupUnits =
    (
        resetmaxfile #noprompt
    ),

    public
    function setup =
    (
        exportOptions.FileFormat = #ascii
        exportOptions.RootPrimPath = "/"
        resetMaxFileAndSetupUnits()
        -- This makes all the custom attribute identifiers upper case for the session, ensuring case insensitivity.
        upper_case_test = attributes USDData
        (
            parameters USD
            (
                USD_PURPOSE type:#string
                USD_HIDDEN type:#boolean
                USD_KIND type:#string
            )
        )        
    ),

    function createTestSceneWithUsdCustomAttributes =
    (
        testFile = output_prefix + "test_file.usda"
        local stage = pyUsd.Stage.CreateNew(testFile)
        local prim001 = pyUsdGeom.Mesh.Define stage "/Box001"
        local purposeAttr = (pyUsdGeom.Imageable(prim001)).CreatePurposeAttr()
        purposeAttr.Set(pyUsdGeom.Tokens.proxy)
        (pyUsd.ModelAPI(prim001)).SetKind(pyKind.Tokens.component)
        (prim001.GetPrim()).SetHidden(True)
        (stage.GetRootLayer()).Save()

        USDImporter.ImportFile testFile importOptions:importOptions
    ),

    function createTestSceneWithDefaultValues =
    (
        testFile = output_prefix + "test_default_file.usda"
        local stage = pyUsd.Stage.CreateNew(testFile)
        local prim001 = pyUsdGeom.Mesh.Define stage "/Box001"
        (stage.GetRootLayer()).Save()

        USDImporter.ImportFile testFile importOptions:importOptions
    ),

    function test_metadata = (
        -- given
        createTestSceneWithUsdCustomAttributes()
        testBox = $Box001
        testBox.baseObject.usd_hidden = true
        testBox.baseObject.usd_purpose = "render"
        testBox.baseObject.usd_kind = "component"

        -- when
        testUsdFile = output_prefix + "test_metadata.usda"
        status = USDExporter.ExportFile testUsdFile exportOptions:exportOptions

        -- then
        assert_true status message:"export to usd failed"
        local stage = pyUsd.Stage.Open(testUsdFile)
        local prim = stage.GetPrimAtPath("/Box001")
        assert_true ((prim.GetPrim()).IsHidden()) message:"prim.IsHidden() should be true"
        assert_equal "render" (((pyUsdGeom.Imageable(prim)).GetPurposeAttr()).Get())
        assert_equal "component" ((pyUsd.ModelAPI(prim)).GetKind())
    ),

    function test_ca_on_node = (
        -- given
        createTestSceneWithUsdCustomAttributes()
        testBox = $Box001
        -- modifying custom attribute from node (but CA is attached to baseObject)
        testBox.usd_hidden = true
        testBox.usd_purpose = "render"
        testBox.usd_kind = "component"

        -- when
        testUsdFile = output_prefix + "test_ca_on_node.usda"
        status = USDExporter.ExportFile testUsdFile exportOptions:exportOptions

        -- then
        assert_true status message:"export to usd failed"
        local stage = pyUsd.Stage.Open(testUsdFile)
        local prim = stage.GetPrimAtPath("/Box001")
        -- modifying CA from node should still reflect changes on baseObject
        assert_true ((prim.GetPrim()).IsHidden()) message:"prim.IsHidden() should be true"
        assert_equal "render" (((pyUsdGeom.Imageable(prim)).GetPurposeAttr()).Get())
        assert_equal "component" ((pyUsd.ModelAPI(prim)).GetKind())
    ),

    function test_ca_on_node_with_modifier = (
        -- given
        createTestSceneWithUsdCustomAttributes()
        testBox = $Box001
        testBox.baseObject.usd_hidden = true
        testBox.baseObject.usd_purpose = "proxy"
        testBox.baseObject.usd_kind = "component"

        -- when
        -- add Edit_Mesh modifier to change type of node to Editable_mesh
        editMeshModifier = Edit_Mesh()
        addModifier testBox editMeshModifier
        testUsdFile = output_prefix + "test_ca_on_node_with_modifier.usda"
        status = USDExporter.ExportFile testUsdFile exportOptions:exportOptions

        -- then
        assert_equal "component" testBox.baseObject.usd_kind
        assert_equal "proxy" testBox.baseObject.usd_purpose
        assert_equal true testBox.baseObject.usd_hidden
        assert_equal "component" testBox.usd_kind
        assert_equal "proxy" testBox.usd_purpose
        assert_equal true testBox.usd_hidden

        -- custom attributes should be exported as they were assigned to the BaseObject
        assert_true status message:"export to usd failed"
        local stage = pyUsd.Stage.Open(testUsdFile)
        local prim = stage.GetPrimAtPath("/Box001")
        assert_true ((prim.GetPrim()).IsHidden()) message:"prim.IsHidden() should be true"
        assert_equal "proxy" (((pyUsdGeom.Imageable(prim)).GetPurposeAttr()).Get())
        assert_equal "component" ((pyUsd.ModelAPI(prim)).GetKind())
    ),
    function test_empty_values = (
        -- given
        usdAllCA = attributes USDData
        (
            parameters USD rollout:USD_Rollout
            (
                usd_purpose type:#string ui:usd_purpose
                usd_kind type:#string ui:usd_kind
            )
            rollout USD_Rollout "USD Metadata"
            (
                edittext usd_purpose "Purpose: "
                edittext usd_kind "Kind: "
            )
        )
        testBox = Box name:"empty_values"
        custAttributes.add testBox usdAllCA #Unique
        testBox.usd_purpose = ""
        assert_true (isProperty testBox "usd_purpose")
        assert_true (isProperty testBox "usd_kind")
        assert_equal "" testBox.usd_purpose
        assert_undefined testBox.usd_kind

        -- when
        testUsdFile = output_prefix + "test_empty_values.usda"
        status = USDExporter.ExportFile testUsdFile exportOptions:exportOptions

        -- then
        assert_true status message:"export to usd failed"
        local stage = pyUsd.Stage.Open(testUsdFile)
        local prim = stage.GetPrimAtPath("/empty_values")
        local primObj = prim.GetPrim()
        local imageablePrim = pyUsdGeom.Imageable(prim)
        local purposeAttr = imageablePrim.GetPurposeAttr()
        local model = pyUsd.ModelAPI(prim)
        assert_false (purposeAttr.HasAuthoredValue())
        assert_false (prim.HasMetadata("kind"))
    ),

    function test_default_values = (
        -- given
        createTestSceneWithDefaultValues()
        testBox = $Box001

        -- when
        testUsdFile = output_prefix + "test_default_values.usda"
        status = USDExporter.ExportFile testUsdFile exportOptions:exportOptions

        -- then
        assert_true status message:"export to usd failed"
        local stage = pyUsd.Stage.Open(testUsdFile)
        local prim = stage.GetPrimAtPath("/Box001")
        local primObj = prim.GetPrim()
        local imageablePrim = pyUsdGeom.Imageable(prim)
        local purposeAttr = imageablePrim.GetPurposeAttr()
        local model = pyUsd.ModelAPI(prim)
        assert_false (purposeAttr.HasAuthoredValue())
        assert_false (prim.HasMetadata("kind"))
        assert_false (prim.HasMetadata("hidden"))
    ),

    function test_hidden_false = (
        -- given
        createTestSceneWithUsdCustomAttributes()
        testBox = $Box001
        testBox.baseObject.usd_hidden = false

        -- when
        testUsdFile = output_prefix + "test_default_values.usda"
        status = USDExporter.ExportFile testUsdFile exportOptions:exportOptions

        -- then
        assert_true status message:"export to usd failed"
        local stage = pyUsd.Stage.Open(testUsdFile)
        local prim = stage.GetPrimAtPath("/Box001")
        local primObj = prim.GetPrim()
        local imageablePrim = pyUsdGeom.Imageable(prim)
        local purposeAttr = imageablePrim.GetPurposeAttr()
        local model = pyUsd.ModelAPI(prim)
        assert_true (primObj.HasMetadata("hidden"))
        assert_false (primObj.IsHidden())
    ),

    function test_non_default_values = (
        -- given
        createTestSceneWithUsdCustomAttributes()
        testBox = $Box001
        testBox.baseObject.usd_purpose = "_custom_purpose_*()"
        testBox.baseObject.usd_kind = "dummyKind$123"

        -- when
        testUsdFile = output_prefix + "test_non_default_values.usda"
        status = USDExporter.ExportFile testUsdFile exportOptions:exportOptions

        -- then
        assert_true status message:"export to usd failed"
        local stage = pyUsd.Stage.Open(testUsdFile)
        local prim = stage.GetPrimAtPath("/Box001")
        local imageablePrim = pyUsdGeom.Imageable(prim)
        local purposeAttr = imageablePrim.GetPurposeAttr()
        local model = pyUsd.ModelAPI(prim)
        assert_equal testBox.usd_purpose (purposeAttr.Get())
        assert_equal testBox.usd_kind (model.GetKind())
    ),

    function test_manually_added_ca = (
        -- given
        usdAllCA = attributes USDData
        (
            parameters USD
            (
                usd_purpose type:#string
                usd_hidden type:#boolean
                usd_kind type:#string
            )
        )

        usdHiddenCA = attributes USDData
        (
            parameters USD
            (
                usd_hidden type:#boolean
            )
        )

        usdKindCA = attributes USDData
        (
            parameters USD
            (
                usd_kind type:#string
            )
        )

        usdPurposeCA = attributes USDData
        (
            parameters USD
            (
                usd_purpose type:#string
            )
        )

        mybox1 = Box name:"box_all_default"
        custAttributes.add mybox1.baseObject usdAllCA #Unique BaseObject:true
        mybox1.usd_hidden = true
        mybox1.usd_kind = "component"
        mybox1.usd_purpose = "render"

        mybox2 = Box name:"box_hidden_false"
        custAttributes.add mybox2.baseObject usdHiddenCA #Unique BaseObject:true
        mybox2.usd_hidden = false

        mybox3 = Box name:"box_kind_empty"
        custAttributes.add mybox3.baseObject usdKindCA #Unique BaseObject:true
        mybox3.usd_kind = ""

        mybox4 = Box name:"box_purpose_default"
        custAttributes.add mybox4.baseObject usdPurposeCA #Unique BaseObject:true
        mybox4.usd_purpose = "default"

        mybox5 = Box name:"box_all_non_default"
        custAttributes.add mybox5.baseObject usdAllCA #Unique BaseObject:true
        mybox5.usd_hidden = true
        mybox5.usd_kind = "blablaK"
        mybox5.usd_purpose = "blebleK"

        -- when
        testUsdFile = output_prefix + "test_manually_added_ca.usda"
        status = USDExporter.ExportFile testUsdFile exportOptions:exportOptions

        -- then
        assert_true status message:"export to usd failed"
        local stage = pyUsd.Stage.Open(testUsdFile)

        local prim1 = stage.GetPrimAtPath("/box_all_default")
        local primObj1 = prim1.GetPrim()
        local imageablePrim1 = pyUsdGeom.Imageable(prim1)
        local purposeAttr1 = imageablePrim1.GetPurposeAttr()
        local model1 = pyUsd.ModelAPI(prim1)
        assert_equal mybox1.usd_purpose (purposeAttr1.Get())
        assert_equal mybox1.usd_kind (model1.GetKind())
        assert_equal mybox1.usd_hidden (primObj1.IsHidden())

        local prim2 = stage.GetPrimAtPath("/box_hidden_false")
        local primObj2 = prim2.GetPrim()
        assert_true (primObj2.HasMetadata("hidden"))

        local prim3 = stage.GetPrimAtPath("/box_kind_empty")
        local primObj3 = prim3.GetPrim()
        assert_false (primObj3.HasMetadata("kind"))

        local prim4 = stage.GetPrimAtPath("/box_purpose_default")
        local imageablePrim4 = pyUsdGeom.Imageable(prim4)
        local purposeAttr4 = imageablePrim4.GetPurposeAttr()
        assert_false (purposeAttr4.HasAuthoredValue())

        local prim5 = stage.GetPrimAtPath("/box_all_non_default")
        local primObj5 = prim5.GetPrim()
        local imageablePrim5 = pyUsdGeom.Imageable(prim5)
        local purposeAttr5 = imageablePrim5.GetPurposeAttr()
        local model5 = pyUsd.ModelAPI(prim5)
        assert_equal mybox5.usd_purpose (purposeAttr5.Get())
        assert_equal mybox5.usd_kind (model5.GetKind())
        assert_equal mybox5.usd_hidden (primObj5.IsHidden())
    ),
    function test_manually_added_ca_wrong_datatypes = (
        -- given
        usdAllCA = attributes USDData
        (
            parameters USD
            (
                usd_purpose type:#float
                usd_hidden type:#string
                usd_kind type:#integer
            )
        )
        mybox1 = Box name:"box_bad_datatypes"
        custAttributes.add mybox1.baseObject usdAllCA #Unique BaseObject:true
        mybox1.usd_hidden = "1"
        mybox1.usd_kind = 1
        mybox1.usd_purpose = 1.0f

        -- when
        testUsdFile = output_prefix + "test_manually_added_ca_wrong_datatypes.usda"
        status = USDExporter.ExportFile testUsdFile exportOptions:exportOptions

        -- then
        assert_true status message:"export to usd failed"
        local stage = pyUsd.Stage.Open(testUsdFile)

        local prim1 = stage.GetPrimAtPath("/box_bad_datatypes")
        local primObj1 = prim1.GetPrim()
        local imageablePrim1 = pyUsdGeom.Imageable(prim1)
        local purposeAttr1 = imageablePrim1.GetPurposeAttr()
        local model1 = pyUsd.ModelAPI(prim1)
        assert_false (purposeAttr1.HasAuthoredValue())
        assert_false (primObj1.HasMetadata("kind"))
        assert_false (primObj1.HasMetadata("hidden"))
    ),

    function test_ca_on_modifier = (
        -- given
        testBox = Box name:"box_no_metadata_on_baseobject"
        usdAllCA = attributes USDData
        (
            parameters USD
            (
                usd_purpose type:#string
                usd_hidden type:#boolean
                usd_kind type:#string
            )
        )
        editMeshModifier = Edit_Mesh()
        addModifier testBox editMeshModifier
        custAttributes.add editMeshModifier usdAllCA

        editMeshModifier.usd_purpose = "proxy"
        editMeshModifier.usd_kind = "component"
        editMeshModifier.usd_hidden = true

        -- when
        testUsdFile = output_prefix + "test_ca_on_modifier.usda"
        status = USDExporter.ExportFile testUsdFile exportOptions:exportOptions

        -- then
        -- manually added CA on modifier should affect export
        assert_true status message:"export to usd failed"
        local stage = pyUsd.Stage.Open(testUsdFile)
        local prim = stage.GetPrimAtPath("/box_no_metadata_on_baseobject")
        local primObj = prim.GetPrim()
        local imageablePrim = pyUsdGeom.Imageable(prim)
        local purposeAttr = imageablePrim.GetPurposeAttr()
        local model = pyUsd.ModelAPI(prim)
        assert_true (purposeAttr.HasAuthoredValue())
        assert_true (primObj.HasMetadata("kind"))
        assert_true (primObj.HasMetadata("hidden"))
    ),

    function test_ca_on_modifier_and_baseobject = (
        -- given
        createTestSceneWithUsdCustomAttributes()
        testBox = $Box001
        testBox.baseObject.usd_hidden = true
        testBox.baseObject.usd_purpose = "proxy"
        testBox.baseObject.usd_kind = "component"
        usdAllCA = attributes USDData
        (
            parameters USD
            (
                usd_purpose type:#string
                usd_hidden type:#boolean
                usd_kind type:#string
            )
        )
        editMeshModifier = Edit_Mesh()
        addModifier testBox editMeshModifier
        custAttributes.add editMeshModifier usdAllCA
        editMeshModifier.usd_purpose = "blebleK"
        editMeshModifier.usd_kind = "blablaK"
        editMeshModifier.usd_hidden = false

        -- when

        testUsdFile = output_prefix + "test_ca_on_modifier_and_baseobject.usda"
        status = USDExporter.ExportFile testUsdFile exportOptions:exportOptions

        -- then
        assert_equal "component" testBox.baseObject.usd_kind
        assert_equal "proxy" testBox.baseObject.usd_purpose
        assert_equal true testBox.baseObject.usd_hidden
        assert_equal "component" testBox.usd_kind
        assert_equal "proxy" testBox.usd_purpose
        assert_equal true testBox.usd_hidden

        -- manually added CA on modifier should affect the export
        assert_true status message:"export to usd failed"
        local stage = pyUsd.Stage.Open(testUsdFile)
        local prim = stage.GetPrimAtPath("/Box001")
        assert_false ((prim.GetPrim()).IsHidden()) message:"prim.IsHidden() should be false"
        assert_equal "blebleK" (((pyUsdGeom.Imageable(prim)).GetPurposeAttr()).Get())
        assert_equal "blablaK" ((pyUsd.ModelAPI(prim)).GetKind())
    ),

    function test_scene_metadata = (
        myBox = Box name:"Box001"

        testUsdFile = output_prefix + "test_scene_metadata.usda"
        status = USDExporter.ExportFile testUsdFile exportOptions:exportOptions
        assert_true status message:"Export to usd failed"

        local stage = pyUsd.Stage.Open(testUsdFile)
        local rootLayer = (stage.GetRootLayer())
        assert_true (rootLayer.HasCustomLayerData()) message:"Root layer should have custom layer data."

        local customData = rootLayer.customLayerData
        assert_defined (customData["creator"]) message:"Creator should be defined."
    ),

    function test_group_kind_metadata = (
        -- given
        myBox1 = Box name:"Box001"
        myBox2 = Box name:"Box002"
        myBox3 = Box name:"Box003"
		
        myGroup = group #(myBox1, myBox2, myBox3) name:"myGroup001"
		
        -- when
        testUsdFile = output_prefix + "test_kind_metadata.usda"
        status = USDExporter.ExportFile testUsdFile exportOptions:exportOptions
		
        -- then
        local stage = pyUsd.Stage.Open(testUsdFile)
        local prim = stage.GetPrimAtPath("/myGroup001")
        assert_equal "group" ((pyUsd.ModelAPI(prim)).GetKind())      
    ),
	
    function test_user_defined_kind_metadata_prioritized = (
		
        usdKindCA = attributes USDData
        (
            parameters USD
            (
                usd_kind type:#string
            )
        )
		
        -- given
        myBox1 = Box name:"Box001"
        myBox2 = Box name:"Box002"
        myBox3 = Box name:"Box003"
		      
		
        myGroup = group #(myBox1, myBox2, myBox3) name:"myGroup001"
		
        custAttributes.add myGroup usdKindCA #Unique BaseObject:true
        myGroup.usd_kind = "userDefined"
		
        -- when
        testUsdFile = output_prefix + "test_kind_metadata.usda"
        status = USDExporter.ExportFile testUsdFile exportOptions:exportOptions
		
        -- then
        local stage = pyUsd.Stage.Open(testUsdFile)
        local prim = stage.GetPrimAtPath("/myGroup001")
        assert_equal "userDefined" ((pyUsd.ModelAPI(prim)).GetKind()) 	
    ),


    function teardown = (
        resetMaxFile #noPrompt
        -- clean up manually scripted custom attributes
        scriptedCADefs = custAttributes.getSceneDefs()
        toDeleteDefs = #()
        for def in scriptedCADefs do (
            if def.Name == #USDData then (
                append toDeleteDefs def
            )
        )
        for def in toDeleteDefs do (
            custAttributes.deleteDef def
        )
    ),

    tests =     #(test_metadata,
                test_ca_on_node,
                test_non_default_values,
                test_default_values,
                test_ca_on_node_with_modifier,
                test_hidden_false,
                test_manually_added_ca,
                test_manually_added_ca_wrong_datatypes,
                test_ca_on_modifier_and_baseobject,
                test_ca_on_modifier,
                test_empty_values,
                test_scene_metadata,
                test_group_kind_metadata,
                test_user_defined_kind_metadata_prioritized
                )
)

runUsdTestFixture export_metadata_test (getThisScriptFilename())