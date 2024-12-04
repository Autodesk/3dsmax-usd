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
pyShade = python.import("pxr.UsdShade")
pyVt = python.import("pxr.Vt")

input_prefix = pathConfig.appendPath (getDir #temp) "/IMPORT_MATERIAL_ID_TEST_"
deleteTestFiles(input_prefix + "*.*")

struct import_material_id_test
(
    private 
    pyramidPrimPath = "/Prim001",
    
    public
    function setup =
    (
        resetmaxfile #noprompt
    ),

    function add_pyramid_to_stage &stage addMaterialIdData displayColor = (
        pyUsdGeom.SetStageUpAxis stage pyUsdGeom.Tokens.z
        local prim001 = pyUsdGeom.Mesh.Define stage pyramidPrimPath
        local points = #((pyGf.Vec3f -10 0 -10), (pyGf.Vec3f 10 0 -10), (pyGf.Vec3f -10 0 10), (pyGf.Vec3f 10 0 10), (pyGf.Vec3f 0 20 0))
        local vertexCount = #(3,3,3,3,4)
        local vertexIndices = #(0, 1, 4, 0, 2, 4, 3, 1, 4, 3, 2, 4, 0, 1, 3, 2)
        prim001.CreatePointsAttr(points)
        prim001.CreateFaceVertexCountsAttr(vertexCount)
        prim001.CreateFaceVertexIndicesAttr(vertexIndices)
        if addMaterialIdData == true do
        (
            local bindingAPI = pyShade.MaterialBindingAPI(prim001)
            for i = 0 to 4 do (
                indices = pyVt.IntArray(#(i))
                subsetName = "_" + ((i+1) as string) + "_"
                bindingAPI.CreateMaterialBindSubset subsetName indices
            )
        )
        
        if (displayColor != undefined) then (
            local colorAttr = prim001.GetDisplayColorAttr()
            colorAttr.Set((pyVt.Vec3fArray ((pyGf.Vec3f (displayColor.r/255) (displayColor.g/255) (displayColor.b/255)))))
        )
    ),
    
    function create_usd_pyramid_file filePath addMaterialIdData color:undefined = (
        local stage = pyUsd.Stage.CreateNew(filePath)
        add_pyramid_to_stage &stage addMaterialIdData color
        (stage.GetRootLayer()).Save()
    ),
    
    function test_import_material_id = (
        -- Populate usd scene file
        local importPath = input_prefix + "import_material_id_test.usda"
        create_usd_pyramid_file importPath true
            
        -- Import file
        USDImporter.ImportFile importPath

        local prim001 = $prim001
        
        for i = 1 to 5 do (
            mtlId = polyop.getFaceMatID prim001 i
            assert_equal mtlId i
        )
        
        -- If different material Ids are used, make sure the importer generated a Multimaterial
        -- ready to accept the different materials. The slot names should correspond to 
        -- the material names.
        
        assert_equal (classOf prim001.material) Multimaterial message:"Found unexpected material type."
        local slotNames = prim001.material.names
        local materials = prim001.material.materialList
        for i = 1 to 5 do (
            local subsetName = "_" + (i as string) + "_"
            assert_equal subsetName slotNames[i] message:"Unexpected material slot name."
            -- Make sure the same fallback material is connected to all slots.
            if (i > 2) then (
                assert_equal materials[i-1] materials[i] message:"All of the mutlimaterial's slots are not connected to the same material."
            )    
        )
    ),
    
    function test_import_file_with_no_material_id = (
        local importPath = input_prefix + "import_file_with_no_material_id_test.usda"
        -- Populate usd scene file
        create_usd_pyramid_file importPath false
        -- Import file
        USDImporter.ImportFile importPath

        local prim001 = $prim001
        for i = 1 to 5 do (
            mtlId = polyop.getFaceMatID prim001 i
            assert_equal mtlId 1
        )
        
        -- If the same material ID is used accross the mesh, the importer should
        -- not have created a multi-material.
        assert_undefined prim001.material message:"A multimaterial should not have been created if a single material Id is used."
    ),
    
    function test_subset_material_default_color = (
        local importPath = input_prefix + "subset_material_default_color_test.usda"
        -- displayColor attribute is not authored, fallback material color should be gray : Color(127.5 127.5 127.5)
        create_usd_pyramid_file importPath true
        USDImporter.ImportFile importPath
        local prim001 = $prim001
        local mat = prim001.material.materialList[1]
        assert_point3_equal (Point3 127.5 127.5 127.5) (mat.Base_Color as Point3) message:"Incorrect color found on the default material."
        
        resetMaxFile #noprompt
        -- displayColor attribute is now authored, fallback material color be that same color.
        local displayColorValue = (Color 255 0 0)
        local importPath = input_prefix + "subset_material_default_color_test.usda"
        create_usd_pyramid_file importPath true color:displayColorValue
        USDImporter.ImportFile importPath
        prim001 = $prim001
        mat = prim001.material.materialList[1]
        assert_point3_equal (mat.Base_Color as Point3) displayColorValue message:"Incorrect color found on the default material."
    ),

    function test_import_material_id_from_custom_data = (
        local importPath = input_prefix + "import_material_id_from_custom_data_test.usda"
        -- displayColor attribute is not authored, fallback material color should be gray : Color(127.5 127.5 127.5)
        local stage = pyUsd.Stage.CreateNew(importPath)
        
        add_pyramid_to_stage &stage false undefined
        
        local pyramidPrim = stage.GetPrimAtPath(pyramidPrimPath)
        local bindingAPI = pyShade.MaterialBindingAPI(pyramidPrim)
        local subsets = #()
        for i = 0 to 4 do (
            indices = pyVt.IntArray(#(i))
            subsetName = "_" + ((i+1) as string) + "_"
            subset = bindingAPI.CreateMaterialBindSubset subsetName indices
            append subsets subset
        )
        
        -- Set subset 2 matId to a invalid value (string)
        local subset1 = subsets[1]
        subset1Prim = subset1.GetPrim()
        subset1Prim.SetCustomDataByKey "3dsmax:matId" "SUPRISE!"
        
        -- Set subset 2 matId to 12
        local subset2 = subsets[2]
        subset2Prim = subset2.GetPrim()
        subset2Prim.SetCustomDataByKey "3dsmax:matId" 12
        
        -- Set subset 3 matId to an invalid integer value
        local subset3 = subsets[3]
        subset3Prim = subset3.GetPrim()
        subset3Prim.SetCustomDataByKey "3dsmax:matId" -1
        
        -- Leave subset 4 with no custom data
        
        (stage.GetRootLayer()).Save()
        
        -- Validate the expected behavior
        -- Invalid and missing material id in usd should have been assigned an id from 1 to X on import
        USDImporter.ImportFile importPath
        local prim001 = $prim001
 
        local matId1 = polyop.getFaceMatID prim001 1
        assert_equal matId1 1
            
        local matId2 = polyop.getFaceMatID prim001 2
        assert_equal matId2 12

        local matId3 = polyop.getFaceMatID prim001 3
        assert_equal matId3 2   
            
        local matId4 = polyop.getFaceMatID prim001 4
        assert_equal matId4 3
    ),
    
    function test_subset_material_invalid_face_indice = (
        local importPath = input_prefix + "subset_material_invalid_face_indice_test.usda"
        local stage = pyUsd.Stage.CreateNew(importPath)
        
        add_pyramid_to_stage &stage false undefined
        
        local pyramidPrim = stage.GetPrimAtPath(pyramidPrimPath)
        local bindingAPI = pyShade.MaterialBindingAPI(pyramidPrim)
        local subsets = #()
        for i = 0 to 4 do (
            -- Add invalid value in the indices array
            indices = pyVt.IntArray(#(i, 23383283, -23383283))
            subsetName = "_" + ((i+1) as string) + "_"
            subset = bindingAPI.CreateMaterialBindSubset subsetName indices
            append subsets subset
        )
        (stage.GetRootLayer()).Save()
        
        USDImporter.ImportFile importPath   
        
        local prim001 = $prim001
        
        for i = 1 to 5 do (
            mtlId = polyop.getFaceMatID prim001 i
            assert_equal mtlId i
        )
    ),
    
    function test_import_file_with_only_one_mat_id = (
        local importPath = input_prefix + "test_import_file_with_only_one_mat_id_test.usda"
        -- Populate usd scene file
        local stage = pyUsd.Stage.CreateNew(importPath)
        
        add_pyramid_to_stage &stage false undefined
        
        local pyramidPrim = stage.GetPrimAtPath(pyramidPrimPath)
        pyramidPrim.SetCustomDataByKey "3dsmax:matId" 12
        
        -- Import file
        USDImporter.ImportFile importPath

        local prim001 = $prim001
        for i = 1 to 5 do (
            mtlId = polyop.getFaceMatID prim001 i
            assert_equal mtlId 12
        )
        
        -- If the same material ID is used accross the mesh, the importer should
        -- not have created a multi-material.
        assert_undefined prim001.material message:"A multimaterial should not have been created if a single material Id is used."
    ),
    
    function teardown = (
    ),

    tests = #(test_import_material_id, test_import_file_with_no_material_id, test_subset_material_default_color, test_import_material_id_from_custom_data, test_subset_material_invalid_face_indice, test_import_file_with_only_one_mat_id)
)

runUsdTestFixture import_material_id_test (getThisScriptFilename())