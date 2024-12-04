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
pyVt = python.import("pxr.Vt")
pySdf = python.import("pxr.Sdf")
pyShade = python.import("pxr.UsdShade")

input_prefix = pathConfig.appendPath (getDir #temp) "/IMPORT_INSTANCE_TEST_"
deleteTestFiles(input_prefix + "*.*")

struct import_instance_test
(
    public 
    function setup =
    (
        resetmaxfile #noprompt
    ),
    
    function import_ref_instance_test = (
        local filepath = input_prefix + "import_instance_test.usda"
        local stage = pyUsd.Stage.CreateNew(filepath)
        
        -- Create cube ref file
        local refPrim = stage.DefinePrim "/referencePrim"
        local cubePrim = pyUsdGeom.Mesh.Define stage "/referencePrim/CubePrim"
        local points = #((pyGf.Vec3f -2.5 -2.5 0), (pyGf.Vec3f 2.5 -2.5 0), (pyGf.Vec3f -2.5 2.5 0), (pyGf.Vec3f 2.5 2.5 0), (pyGf.Vec3f -2.5 -2.5 5), (pyGf.Vec3f 2.5 -2.5 5), (pyGf.Vec3f -2.5 2.5 5), (pyGf.Vec3f 2.5 2.5 5))
        local vertexCount = #(4, 4, 4, 4, 4, 4)
        local vertexIndices = #(0, 2, 3, 1, 4, 5, 7, 6, 0, 1, 5, 4, 1, 3, 7, 5, 3, 2, 6, 7, 2, 0, 4, 6)
        cubePrim.CreatePointsAttr(points)
        cubePrim.CreateFaceVertexCountsAttr(vertexCount)
        cubePrim.CreateFaceVertexIndicesAttr(vertexIndices)

        -- Reference cube file in prims
        local instance1 = pyUsdGeom.Xform.Define stage "/CubePrimInstance1"
        local instance2 = pyUsdGeom.Xform.Define stage "/CubePrimInstance2"
        local instance3 = pyUsdGeom.Xform.Define stage "/CubePrimInstance3"
        local instance3CubeOverwrite = stage.OverridePrim "/CubePrimInstance3/CubePrim"
        local meshoverride = pyUsdGeom.Gprim(instance3CubeOverwrite)

        ((instance1.GetPrim()).GetInherits()).AddInherit("/referencePrim")
        ((instance2.GetPrim()).GetInherits()).AddInherit("/referencePrim")
        ((instance3.GetPrim()).GetInherits()).AddInherit("/referencePrim")

            
        (instance1.GetPrim()).SetInstanceable(true)
        (instance2.GetPrim()).SetInstanceable(true)
            
        meshoverride.CreateDisplayColorAttr (pyVt.Vec3fArray (pyGf.Vec3f 1 0 0))
        (stage.GetRootLayer()).Save()
    
        importFile filepath #noPrompt
            
        local cubeNode1 = $CubePrimInstance1/CubePrim
        local cubeNode2 = $CubePrimInstance2/CubePrim
        local cubeNode3 = $CubePrimInstance3/CubePrim
        
        local numberOfInstanceNode1 = instanceMgr.GetInstances cubeNode1 &listOfNode
        local numberOfInstanceNode2 = instanceMgr.GetInstances cubeNode2 &listOfNode
        local numberOfInstanceNode3 = instanceMgr.GetInstances cubeNode3 &listOfNode
        assert_equal 2 numberOfInstanceNode1 message:"Node 1 should be an instance of Node 2"
        assert_equal 2 numberOfInstanceNode2 message:"Node 1 should be an instance of Node 2"
        assert_equal 1 numberOfInstanceNode3 message:"Node 3 should not be an instance of any node"
    ),
    
    function test_import_invisible_instance = (
        local filepath = input_prefix + "import_invisible_instance_test.usda"
        local stage = pyUsd.Stage.CreateNew(filepath)

        -- Create cube ref file
        local refPrim = stage.DefinePrim "/referencePrim"
        local childPrim = pyUsdGeom.Mesh.Define stage "/referencePrim/childPrim"

        -- Reference cube file in prims
        local instance1 = pyUsdGeom.Xform.Define stage "/PrimInstance1"
        local instance2 = pyUsdGeom.Xform.Define stage "/PrimInstance2"
        local instance1ImageablePrim = pyUsdGeom.Imageable(instance1)
        local visibilityAttr = instance1ImageablePrim.CreateVisibilityAttr()
        visibilityAttr.Set(pyUsdGeom.Tokens.invisible)

        ((instance1.GetPrim()).GetInherits()).AddInherit("/referencePrim")
        ((instance2.GetPrim()).GetInherits()).AddInherit("/referencePrim")

        (instance1.GetPrim()).SetInstanceable(true)
        (instance2.GetPrim()).SetInstanceable(true)

        (stage.GetRootLayer()).Save()

        importFile filepath #noPrompt
        local listOfNode = #()

        local node1 = $PrimInstance1/childPrim
        local node2 = $PrimInstance2/childPrim

        assert_true node1.isHidden message:"Node 1 should be hidden"
        assert_false node2.isHidden message:"Node 2 should not be hidden"
    ),
    
    function test_import_positioned_instances = (
        local filepath = input_prefix + "import_positioned_instances_test.usda"
        local stage = pyUsd.Stage.CreateNew(filepath)
        pyUsdGeom.SetStageUpAxis stage pyUsdGeom.Tokens.z
        pyUsdGeom.SetStageMetersPerUnit stage pyUsdGeom.LinearUnits.inches
        
        -- Create cube ref file
        local refPrim = stage.CreateClassPrim "/referencePrim"
        local cubePrim = pyUsdGeom.Mesh.Define stage "/referencePrim/CubePrim"
        local points = #((pyGf.Vec3f -2.5 -2.5 0), (pyGf.Vec3f 2.5 -2.5 0), (pyGf.Vec3f -2.5 2.5 0), (pyGf.Vec3f 2.5 2.5 0), (pyGf.Vec3f -2.5 -2.5 5), (pyGf.Vec3f 2.5 -2.5 5), (pyGf.Vec3f -2.5 2.5 5), (pyGf.Vec3f 2.5 2.5 5))
        local vertexCount = #(4, 4, 4, 4, 4, 4)
        local vertexIndices = #(0, 2, 3, 1, 4, 5, 7, 6, 0, 1, 5, 4, 1, 3, 7, 5, 3, 2, 6, 7, 2, 0, 4, 6)
        cubePrim.CreatePointsAttr(points)
        cubePrim.CreateFaceVertexCountsAttr(vertexCount)
        cubePrim.CreateFaceVertexIndicesAttr(vertexIndices)
        
        -- Reference cube file in prims
        local instance1 = pyUsdGeom.Xform.Define stage "/CubePrimInstance1"
        local instance1TranslateOp = instance1.AddTranslateOp()
        instance1TranslateOp.Set (pyGf.Vec3f 10. 5. 0.)
        local instance1RotateXYZOp = instance1.AddRotateXYZOp()
        instance1RotateXYZOp.Set (pyGf.Vec3f 0. 50. 0.)
        local instance2 = pyUsdGeom.Xform.Define stage "/CubePrimInstance2"
        local instance2TranslateOp = instance2.AddTranslateOp()
        instance2TranslateOp.Set (pyGf.Vec3f -10. 5. 0.)
        local instance2RotateXYZOp = instance2.AddRotateXYZOp()
        instance2RotateXYZOp.Set (pyGf.Vec3f 0. 0. 0.)
        local instance3 = pyUsdGeom.Xform.Define stage "/CubePrimInstance3"
        local instance3TranslateOp = instance3.AddTranslateOp()
        instance3TranslateOp.Set (pyGf.Vec3f 10. -5. 0.)
        local instance3RotateXYZOp = instance3.AddRotateXYZOp()
        instance3RotateXYZOp.Set (pyGf.Vec3f 0. 20. 0.)
        local instance3CubeOverwrite = stage.OverridePrim "/CubePrimInstance3/CubePrim"
        local meshoverride = pyUsdGeom.Gprim(instance3CubeOverwrite)

        ((instance1.GetPrim()).GetInherits()).AddInherit("/referencePrim")
        ((instance2.GetPrim()).GetInherits()).AddInherit("/referencePrim")
        ((instance3.GetPrim()).GetInherits()).AddInherit("/referencePrim")
            
        (instance1.GetPrim()).SetInstanceable(true)
        (instance2.GetPrim()).SetInstanceable(true)
            
        meshoverride.CreateDisplayColorAttr (pyVt.Vec3fArray (pyGf.Vec3f 1 0 0))
        (stage.GetRootLayer()).Save()
    
        importFile filepath #noPrompt
        
        -- the prototype itself is not kept at the end of the import process
        -- the instances and their point helper (xform) should be imported
        assert_equal 6 objects.count message:"The number of elements in the imported stage should be 6."

        -- all instances are properly positioned
        local cubeNode1Helper = $CubePrimInstance1
        assert_equal ([10,5,0]) cubeNode1Helper.pos
        assert_equal ((EulerAngles 0 -50 0) as quat) cubeNode1Helper.rotation
        local cubeNode1 = $CubePrimInstance1/CubePrim
        assert_equal ([10,5,0]) cubeNode1.pos
        assert_equal ((EulerAngles 0 -50 0) as quat) cubeNode1.rotation
        local cubeNode2Helper = $CubePrimInstance2
        assert_equal ([-10,5,0]) cubeNode2Helper.pos
        assert_equal ((EulerAngles 0 0 0) as quat) cubeNode2Helper.rotation
        local cubeNode2 = $CubePrimInstance2/CubePrim
        assert_equal ([-10,5,0]) cubeNode2.pos
        assert_equal ((EulerAngles 0 0 0) as quat) cubeNode2.rotation
        local cubeNode3Helper = $CubePrimInstance3
        assert_equal ([10,-5,0]) cubeNode3Helper.pos
        assert_equal ((EulerAngles 0 -20 0) as quat) cubeNode3Helper.rotation
        local cubeNode3 = $CubePrimInstance3/CubePrim
        assert_equal ([10,-5,0]) cubeNode3.pos
        assert_equal ((EulerAngles 0 -20 0) as quat) cubeNode3.rotation
    ),

    -- test the positioning of instances in Y up axis stage
    -- the test is also constructed for the stage to have multiple level of prototype references
    -- to add multiple level of transformation to apply on the resulting node
    --  - a first cube is created (centered with no transformation)
    --  - a set of cubes are referenced and, are positioned and oriented to form a pattern formed of 4 instanced cubes
    --  - the final stage uses the pattern to create a figure by positioning and orienting 4 instances of the pattern
    -- the number of instances is not relevant to the test but it was done just for dev artistic reasons
    -- we are interested by the final position and orientation of a cube in the last stage
    -- to validate the total transform which is the result from the combined multiple xforms
    function test_import_positioned_instances_multi_level = (
        
        -- Create cube ref file
        local filepathCubeRef = input_prefix + "import_positioned_instances_multi_levelA_test.usda"
        local cubeStage = pyUsd.Stage.CreateNew(filepathCubeRef)
        local defaultPrim = pyUsdGeom.Xform.Define cubeStage "/Asset"
        local cubePrim = pyUsdGeom.Mesh.Define cubeStage "/Asset/CubePrim"
        local points = #((pyGf.Vec3f -2.5 -2.5 -2.5), (pyGf.Vec3f 2.5 -2.5 -2.5), (pyGf.Vec3f -2.5 2.5 -2.5), (pyGf.Vec3f 2.5 2.5 -2.5), (pyGf.Vec3f -2.5 -2.5 2.5), (pyGf.Vec3f 2.5 -2.5 2.5), (pyGf.Vec3f -2.5 2.5 2.5), (pyGf.Vec3f 2.5 2.5 2.5))
        local vertexCount = #(4, 4, 4, 4, 4, 4)
        local vertexIndices = #(0, 2, 3, 1, 4, 5, 7, 6, 0, 1, 5, 4, 1, 3, 7, 5, 3, 2, 6, 7, 2, 0, 4, 6)
        cubePrim.CreatePointsAttr(points)
        cubePrim.CreateFaceVertexCountsAttr(vertexCount)
        cubePrim.CreateFaceVertexIndicesAttr(vertexIndices)
        cubePrim.CreateDisplayColorAttr (pyVt.Vec3fArray (pyGf.Vec3f 1 0 0))
        cubeStage.SetDefaultPrim (defaultPrim.GetPrim())
        (cubeStage.GetRootLayer()).Save()
            
        local filepathFirstInference = input_prefix + "import_positioned_instances_multi_levelB_test.usda"
        local firstInferenceStage = pyUsd.Stage.CreateNew(filepathFirstInference)
        local cubesXform = pyUsdGeom.Xform.Define firstInferenceStage "/Cubes"
        firstInferenceStage.SetDefaultPrim (cubesXform.GetPrim())
            
        local instance1 = pyUsdGeom.Xform.Define firstInferenceStage "/Cubes/Cube1"
        ((instance1.GetPrim()).GetReferences()).AddReference("./IMPORT_INSTANCE_TEST_import_positioned_instances_multi_levelA_test.usda")
        local instance1RotateXYZOp = instance1.AddRotateXYZOp()
        instance1RotateXYZOp.Set (pyGf.Vec3f 0. 45. 0.)
        (instance1.GetPrim()).SetInstanceable(true)

        local instance2 = pyUsdGeom.Xform.Define firstInferenceStage "/Cubes/Cube2"
        ((instance2.GetPrim()).GetReferences()).AddReference("./IMPORT_INSTANCE_TEST_import_positioned_instances_multi_levelA_test.usda")
        local instance2TranslateOp = instance2.AddTranslateOp()
        instance2TranslateOp.Set (pyGf.Vec3f 0. 0. -10)
        local instance2RotateXYZOp = instance2.AddRotateXYZOp()
        instance2RotateXYZOp.Set (pyGf.Vec3f 0. 45. 0.)
        (instance2.GetPrim()).SetInstanceable(true)

        local instance3 = pyUsdGeom.Xform.Define firstInferenceStage "/Cubes/Cube3"
        ((instance3.GetPrim()).GetReferences()).AddReference("./IMPORT_INSTANCE_TEST_import_positioned_instances_multi_levelA_test.usda")
        local instance3TranslateOp = instance3.AddTranslateOp()
        instance3TranslateOp.Set (pyGf.Vec3f 0. 10. 0.)
        local instance3RotateXYZOp = instance3.AddRotateXYZOp()
        instance3RotateXYZOp.Set (pyGf.Vec3f 0. 45. 0.)
        (instance3.GetPrim()).SetInstanceable(true)

        local instance4 = pyUsdGeom.Xform.Define firstInferenceStage "/Cubes/Cube4"
        ((instance4.GetPrim()).GetReferences()).AddReference("./IMPORT_INSTANCE_TEST_import_positioned_instances_multi_levelA_test.usda")
        local instance4TranslateOp = instance4.AddTranslateOp()
        instance4TranslateOp.Set (pyGf.Vec3f 0. -10. 0.)
        local instance4RotateXYZOp = instance4.AddRotateXYZOp()
        instance4RotateXYZOp.Set (pyGf.Vec3f 0. 45. 0.)
        (instance4.GetPrim()).SetInstanceable(true)

        (firstInferenceStage.GetRootLayer()).Save()

        local filepathSecondInference = input_prefix + "import_positioned_instances_multi_levelC_test.usda"
        local secondInferenceStage = pyUsd.Stage.CreateNew(filepathSecondInference)
        pyUsdGeom.SetStageMetersPerUnit secondInferenceStage pyUsdGeom.LinearUnits.inches
        -- keeping the default Y up axis
        local manyMoreCubes = pyUsdGeom.Xform.Define secondInferenceStage "/ManyMoreCubes"

        local manyCubes1 = pyUsdGeom.Xform.Define secondInferenceStage "/ManyMoreCubes/ManyCubes1"
        ((manyCubes1.GetPrim()).GetReferences()).AddReference("./IMPORT_INSTANCE_TEST_import_positioned_instances_multi_levelB_test.usda")
        local manyCubesRotateXYZOp = manyCubes1.AddRotateXYZOp()
        manyCubesRotateXYZOp.Set (pyGf.Vec3f 45. 0. 0.)
        local manyCubesTranslateOp = manyCubes1.AddTranslateOp()
        manyCubesTranslateOp.Set (pyGf.Vec3f 0. 0. 30)
        (manyCubes1.GetPrim()).SetInstanceable(true)

        local manyCubes2 = pyUsdGeom.Xform.Define secondInferenceStage "/ManyMoreCubes/ManyCubes2"
        ((manyCubes2.GetPrim()).GetReferences()).AddReference("./IMPORT_INSTANCE_TEST_import_positioned_instances_multi_levelB_test.usda")
        local manyCubes2RotateXYZOp = manyCubes2.AddRotateXYZOp()
        manyCubes2RotateXYZOp.Set (pyGf.Vec3f 135. 0. 0.)
        local manyCubes2TranslateOp = manyCubes2.AddTranslateOp()
        manyCubes2TranslateOp.Set (pyGf.Vec3f 0. 0. 30)
        (manyCubes2.GetPrim()).SetInstanceable(true)
            
        local manyCubes3 = pyUsdGeom.Xform.Define secondInferenceStage "/ManyMoreCubes/ManyCubes3"
        ((manyCubes3.GetPrim()).GetReferences()).AddReference("./IMPORT_INSTANCE_TEST_import_positioned_instances_multi_levelB_test.usda")
        local manyCubes3RotateXYZOp = manyCubes3.AddRotateXYZOp()
        manyCubes3RotateXYZOp.Set (pyGf.Vec3f 225. 0. 0.)
        local manyCubes3TranslateOp = manyCubes3.AddTranslateOp()
        manyCubes3TranslateOp.Set (pyGf.Vec3f 0. 0. 30)
        (manyCubes3.GetPrim()).SetInstanceable(true)

        local manyCubes4 = pyUsdGeom.Xform.Define secondInferenceStage "/ManyMoreCubes/ManyCubes4"
        ((manyCubes4.GetPrim()).GetReferences()).AddReference("./IMPORT_INSTANCE_TEST_import_positioned_instances_multi_levelB_test.usda")
        local manyCubes4RotateXYZOp = manyCubes4.AddRotateXYZOp()
        manyCubes4RotateXYZOp.Set (pyGf.Vec3f 315. 0. 0.)
        local manyCubes4TranslateOp = manyCubes4.AddTranslateOp()
        manyCubes4TranslateOp.Set (pyGf.Vec3f 0. 0. 30)
        (manyCubes4.GetPrim()).SetInstanceable(true)

        (secondInferenceStage.GetRootLayer()).Save()

        importFile filepathSecondInference #noPrompt

        -- the prototype itself is not kept at the end of the import process
        -- the instances and their point helper (xform) should be imported
        assert_equal 37 objects.count message:"The number of elements in the imported stage should be 37."

         -- instances are properly positioned
        local cubeNode = $ManyMoreCubes/ManyCubes1/Cube1/CubePrim
        assert_point3_equal ([0, -21.2132, -21.2132]) cubeNode.pos
        assert_equal ((EulerAngles -135 -45 0) as quat) cubeNode.rotation
    ),
    
    function test_import_one_material_for_all_instances = (
        local filepath = input_prefix + "import_one_material_for_all_instances_test.usda"
        local stage = pyUsd.Stage.CreateNew(filepath)
        pyUsdGeom.SetStageUpAxis stage pyUsdGeom.Tokens.z
        pyUsdGeom.SetStageMetersPerUnit stage pyUsdGeom.LinearUnits.inches
        
        -- Create cube ref file
        local refPrim = stage.CreateClassPrim "/referencePrim"
        local cubePrim = pyUsdGeom.Mesh.Define stage "/referencePrim/CubePrim"
        local points = #((pyGf.Vec3f -2.5 -2.5 0), (pyGf.Vec3f 2.5 -2.5 0), (pyGf.Vec3f -2.5 2.5 0), (pyGf.Vec3f 2.5 2.5 0), (pyGf.Vec3f -2.5 -2.5 5), (pyGf.Vec3f 2.5 -2.5 5), (pyGf.Vec3f -2.5 2.5 5), (pyGf.Vec3f 2.5 2.5 5))
        local vertexCount = #(4, 4, 4, 4, 4, 4)
        local vertexIndices = #(0, 2, 3, 1, 4, 5, 7, 6, 0, 1, 5, 4, 1, 3, 7, 5, 3, 2, 6, 7, 2, 0, 4, 6)
        cubePrim.CreatePointsAttr(points)
        cubePrim.CreateFaceVertexCountsAttr(vertexCount)
        cubePrim.CreateFaceVertexIndicesAttr(vertexIndices)

        local scopeMtl = pyUsdGeom.Scope.Define stage "/mtl"
        local bbmaterial = pyShade.Material.Define stage "/mtl/MaterialName"
        local pbrShader = pyShade.Shader.Define stage "/mtl/MaterialName/PBRShader"

        roughness_value = 0.4
        metallic_value = 0.12
        diffuseColor_value = (pyGf.Vec3f 111.0 222.0 111.0)
        opacity_value = 0.77

        pbrShader.CreateIdAttr "UsdPreviewSurface"
        local roughnessInput = pbrShader.CreateInput "roughness" pySdf.ValueTypeNames.Float
        roughnessInput.Set roughness_value
        local metallicInput = pbrShader.CreateInput "metallic" pySdf.ValueTypeNames.Float
        metallicInput.Set(metallic_value)
        local diffuseInput = pbrShader.CreateInput "diffuseColor"  pySdf.ValueTypeNames.Color3f
        diffuseInput.Set (pyGf.Vec3f (diffuseColor_value[1]/255.0) (diffuseColor_value[2]/255.0) (diffuseColor_value[3]/255.0))
        local opacityInput = pbrShader.CreateInput "opacity" pySdf.ValueTypeNames.Float
        opacityInput.Set opacity_value

        local surfaceOutput = bbmaterial.CreateSurfaceOutput()
        surfaceOutput.ConnectToSource (pbrShader.ConnectableAPI()) "surface"

        local cubeMaterialBinding = pyShade.MaterialBindingAPI cubePrim
        cubeMaterialBinding.Bind bbmaterial
        
        local instance1 = pyUsdGeom.Xform.Define stage "/CubePrimInstance1"
        local instance2 = pyUsdGeom.Xform.Define stage "/CubePrimInstance2"
        local instance3 = pyUsdGeom.Xform.Define stage "/CubePrimInstance3"
        local instance3CubeOverwrite = stage.OverridePrim "/CubePrimInstance3/CubePrim"
        local meshoverride = pyUsdGeom.Gprim(instance3CubeOverwrite)

        ((instance1.GetPrim()).GetInherits()).AddInherit("/referencePrim")
        ((instance2.GetPrim()).GetInherits()).AddInherit("/referencePrim")
        ((instance3.GetPrim()).GetInherits()).AddInherit("/referencePrim")
            
        (instance1.GetPrim()).SetInstanceable(true)
        (instance2.GetPrim()).SetInstanceable(true)
            
        meshoverride.CreateDisplayColorAttr (pyVt.Vec3fArray (pyGf.Vec3f 1 0 0))
        (stage.GetRootLayer()).Save()
    
        importFile filepath #noPrompt
        
        -- the prototype itself is not kept at the end of the import process
        -- the instances and their point helper (xform) should be imported
        assert_equal 6 objects.count message:"The number of elements in the imported stage should be 6."
        local cubeNode1 = $CubePrimInstance1/CubePrim
        local cubeNode2 = $CubePrimInstance2/CubePrim
        local cubeNode3 = $CubePrimInstance3/CubePrim
        
        -- only one material is shared between are instances
        assert_equal 1 sceneMaterials.count
        local cubeNode1Material = cubeNode1.mat
        assert_equal (ClassOf cubeNode1Material) MaxUsdPreviewSurface
        assert_equal cubeNode1Material.name "MaterialName"

        assert_equal cubeNode1Material.roughness roughness_value
        assert_equal cubeNode1Material.metallic metallic_value
        assert_equal cubeNode1Material.diffusecolor (color diffuseColor_value[1] diffuseColor_value[2] diffuseColor_value[3])
        assert_equal cubeNode1Material.opacity opacity_value
            
        assert_equal cubeNode1Material cubeNode2.mat
        assert_equal cubeNode1Material cubeNode3.mat
    ),

    function test_import_material_override_on_instance = (
        local filepath = input_prefix + "import_material_override_on_instance_test.usda"
        local stage = pyUsd.Stage.CreateNew(filepath)
        pyUsdGeom.SetStageUpAxis stage pyUsdGeom.Tokens.z
        
        -- Create cube ref file
        local refPrim = stage.CreateClassPrim "/referencePrim"
        local cubePrim = pyUsdGeom.Mesh.Define stage "/referencePrim/CubePrim"
        local points = #((pyGf.Vec3f -2.5 -2.5 0), (pyGf.Vec3f 2.5 -2.5 0), (pyGf.Vec3f -2.5 2.5 0), (pyGf.Vec3f 2.5 2.5 0), (pyGf.Vec3f -2.5 -2.5 5), (pyGf.Vec3f 2.5 -2.5 5), (pyGf.Vec3f -2.5 2.5 5), (pyGf.Vec3f 2.5 2.5 5))
        local vertexCount = #(4, 4, 4, 4, 4, 4)
        local vertexIndices = #(0, 2, 3, 1, 4, 5, 7, 6, 0, 1, 5, 4, 1, 3, 7, 5, 3, 2, 6, 7, 2, 0, 4, 6)
        cubePrim.CreatePointsAttr(points)
        cubePrim.CreateFaceVertexCountsAttr(vertexCount)
        cubePrim.CreateFaceVertexIndicesAttr(vertexIndices)

        local instance1 = pyUsdGeom.Xform.Define stage "/CubePrimInstance1"
        ((instance1.GetPrim()).GetInherits()).AddInherit("/referencePrim")
        (instance1.GetPrim()).SetInstanceable(true)

        local instance2 = pyUsdGeom.Xform.Define stage "/CubePrimInstance2"
        local instance3 = pyUsdGeom.Xform.Define stage "/CubePrimInstance3"

        -- create a material to bind to the reference
        local scopeMtl = pyUsdGeom.Scope.Define stage "/mtl"
        local material1 = pyShade.Material.Define stage "/mtl/MaterialName"
        local pbrShader = pyShade.Shader.Define stage "/mtl/MaterialName/PBRShader"

        roughness_value = 0.4
        metallic_value = 0.12
        diffuseColor_value = (pyGf.Vec3f 111.0 222.0 111.0)
        opacity_value = 0.77

        pbrShader.CreateIdAttr "UsdPreviewSurface"
        local roughnessInput = pbrShader.CreateInput "roughness" pySdf.ValueTypeNames.Float
        roughnessInput.Set roughness_value
        local metallicInput = pbrShader.CreateInput "metallic" pySdf.ValueTypeNames.Float
        metallicInput.Set(metallic_value)
        local diffuseInput = pbrShader.CreateInput "diffuseColor"  pySdf.ValueTypeNames.Color3f
        diffuseInput.Set (pyGf.Vec3f (diffuseColor_value[1]/255.0) (diffuseColor_value[2]/255.0) (diffuseColor_value[3]/255.0))
        local opacityInput = pbrShader.CreateInput "opacity" pySdf.ValueTypeNames.Float
        opacityInput.Set opacity_value

        local surfaceOutput = material1.CreateSurfaceOutput()
        surfaceOutput.ConnectToSource (pbrShader.ConnectableAPI()) "surface"

        local cubeMaterialBinding = pyShade.MaterialBindingAPI cubePrim
        cubeMaterialBinding.Bind material1
        
        -- create an override material to bind to the second instance
        local material2= pyShade.Material.Define stage "/mtl/MaterialOverrideName"
        local pbrShader2 = pyShade.Shader.Define stage "/mtl/MaterialOverrideName/PBRShader"

        diffuseOverrideColor_value = (pyGf.Vec3f 222.0 200.0 200.0)

        pbrShader2.CreateIdAttr "UsdPreviewSurface"
        local roughnessInput2 = pbrShader2.CreateInput "roughness" pySdf.ValueTypeNames.Float
        roughnessInput2.Set roughness_value
        local metallicInput2 = pbrShader2.CreateInput "metallic" pySdf.ValueTypeNames.Float
        metallicInput2.Set(metallic_value)
        local diffuseInput2 = pbrShader2.CreateInput "diffuseColor"  pySdf.ValueTypeNames.Color3f
        diffuseInput2.Set (pyGf.Vec3f (diffuseOverrideColor_value[1]/255.0) (diffuseOverrideColor_value[2]/255.0) (diffuseOverrideColor_value[3]/255.0))
        local opacityInput2 = pbrShader2.CreateInput "opacity" pySdf.ValueTypeNames.Float
        opacityInput2.Set opacity_value

        local surfaceOutput2 = material2.CreateSurfaceOutput()
        surfaceOutput2.ConnectToSource (pbrShader2.ConnectableAPI()) "surface"

        local instance2CubeOverwrite = stage.OverridePrim "/CubePrimInstance2/CubePrim"
        local cubeMaterialBinding2 = pyShade.MaterialBindingAPI instance2CubeOverwrite
        cubeMaterialBinding2.Bind material2
        ((instance2.GetPrim()).GetInherits()).AddInherit("/referencePrim")

        ((instance3.GetPrim()).GetInherits()).AddInherit("/referencePrim")
        local instance3CubeOverwrite = stage.OverridePrim "/CubePrimInstance3/CubePrim"
        local meshoverride = pyUsdGeom.Gprim(instance3CubeOverwrite)
        meshoverride.CreateDisplayColorAttr (pyVt.Vec3fArray (pyGf.Vec3f 1 0 0))

        (stage.GetRootLayer()).Save()
    
        importFile filepath #noPrompt
        
        -- the prototype itself is not kept at the end of the import process
        -- the instances and their point helper (xform) should be imported
        assert_equal 6 objects.count message:"The number of elements in the imported stage should be 6."
        local cubeNode1 = $CubePrimInstance1/CubePrim
        local cubeNode2 = $CubePrimInstance2/CubePrim
        local cubeNode3 = $CubePrimInstance3/CubePrim

        -- instance1 and instance3 share the same material
        -- instance3 has its own material
        assert_equal 2 sceneMaterials.count

        local cubeNode1 = $CubePrimInstance1/CubePrim
        local cubeNode1Material = cubeNode1.mat
        assert_equal (ClassOf cubeNode1Material) MaxUsdPreviewSurface
        assert_equal "MaterialName" cubeNode1Material.name

        assert_equal roughness_value cubeNode1Material.roughness
        assert_equal metallic_value cubeNode1Material.metallic
        assert_equal (color diffuseColor_value[1] diffuseColor_value[2] diffuseColor_value[3]) cubeNode1Material.diffusecolor
        assert_equal opacity_value cubeNode1Material.opacity
            
        local cubeNode2 = $CubePrimInstance2/CubePrim
        local cubeNode2Material = cubeNode2.mat
        assert_equal (ClassOf cubeNode2Material) MaxUsdPreviewSurface
        assert_equal cubeNode2Material.name "MaterialOverrideName"

        assert_equal roughness_value cubeNode2Material.roughness
        assert_equal metallic_value cubeNode2Material.metallic
        assert_equal (color diffuseOverrideColor_value[1] diffuseOverrideColor_value[2] diffuseOverrideColor_value[3]) cubeNode2Material.diffusecolor 
        assert_equal cubeNode2Material.opacity opacity_value

        local cubeNode3 = $CubePrimInstance3/CubePrim
        -- cube3 has the same material has cube1
        assert_equal cubeNode1Material cubeNode3.mat
    ),
    
    function teardown = (
    ),

    Tests = #(
        import_ref_instance_test,
        test_import_invisible_instance,
        test_import_positioned_instances,
        test_import_positioned_instances_multi_level,
        test_import_one_material_for_all_instances,
        test_import_material_override_on_instance
    )
)

runUsdTestFixture import_instance_test (getThisScriptFilename())