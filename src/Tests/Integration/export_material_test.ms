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
pyUsdTf = python.import("pxr.Tf")
pyShade = python.import("pxr.UsdShade")
pysdf = python.import("pxr.Sdf")
pyGf = python.import("pxr.Gf")

-- Reload the material export module for convenience in development.
python.Execute "from importlib import reload"
python.Execute "import usd_material_writer"
python.Execute "reload(usd_material_writer)"
python.Execute "import shaderWriter"
python.Execute "reload(shaderWriter)"    
pyUsdMaterialWriter = python.import("usd_material_writer")

output_prefix = pathConfig.appendPath (getDir #temp) "/EXPORT_MATERIAL_TEST_"
deleteTestFiles(output_prefix + "*.*")

struct export_material_test
(  
    private 
    originalFileFormatOption,
    exportMaterialTestOptions = USDExporter.CreateOptions(),
    
    public 
    function setup =
    (
        resetmaxfile #noprompt
        exportMaterialTestOptions.FileFormat = #ascii
        exportMaterialTestOptions.RootPrimPath = "/"
        exportMaterialTestOptions.Materials = true
        pyUsdMaterialWriter._material_export_options["relative_texture_paths"] = false
    ),
    
    function validate_multiMtl_binding &stage &prim expectedNumberOfSubset =
    (
        local bindingAPI = pyShade.MaterialBindingAPI(prim)
        subsets = bindingAPI.GetMaterialBindSubsets()
        assert_equal expectedNumberOfSubset subsets.count
        
        for i = 1 to expectedNumberOfSubset do
        (
            local subset1BindingAPI = pyShade.MaterialBindingAPI(subsets[i])
            local mtlBindingRel = subset1BindingAPI.GetDirectBindingRel()
            local bindingTargets = mtlBindingRel.GetTargets()
            assert_equal bindingTargets.count 1
            local mtlPrim = stage.GetPrimAtPath(bindingTargets[1])
            assert_true (pyUsd.Prim.IsValid(mtlPrim))
        )
    ),
    
    function validate_simple_bindings &stage &prim = (
        local bindingAPI = pyShade.MaterialBindingAPI(prim)
        local mtlBindingRel = bindingAPI.GetDirectBindingRel()
        local bindingTargets = mtlBindingRel.GetTargets()
        assert_equal bindingTargets.count 1
        local mtlPrim = stage.GetPrimAtPath(bindingTargets[1])
        assert_true (pyUsd.Prim.IsValid(mtlPrim))
    ),
    
    function get_bound_material_name &stage &prim  = (
        local bindingAPI = pyShade.MaterialBindingAPI(prim)
        local mtlBindingRel = bindingAPI.GetDirectBindingRel()
        local bindingTargets = mtlBindingRel.GetTargets()
        local mtlPrim = stage.GetPrimAtPath(bindingTargets[1])
        return mtlPrim.GetName()
    ),
    
    function test_export_multi_material_bindings = (
        local box001 = Box pos:[0,0,0] name:"box001"
        local multiMtl = multimaterial numsubs:6
        
        box001.material = multiMtl
        
        local exportPath = output_prefix + "export_multi_material_should_have_bindings.usda"
        USDExporter.ExportFile exportPath exportOptions:exportMaterialTestOptions
        
        local stage = pyUsd.Stage.Open(exportPath)
        local boxPrim = stage.GetPrimAtPath("/box001")
        
        validate_multiMtl_binding stage boxPrim 6
    ),

    function test_export_multi_material_bindings_with_exported_object_transform = (
        local box001 = Box pos:[0,0,0] name:"box001"
        local multiMtl = multimaterial numsubs:6
        box001.objectOffsetPos = [1,1,1]
        box001.material = multiMtl
        
        local exportOptions = USDExporter.CreateOptions()
        exportOptions.FileFormat = #ascii
        exportOptions.RootPrimPath = "/"
        exportOptions.BakeObjectOffsetTransform = false
        exportOptions.Materials = true

        local exportPath = output_prefix + "export_multi_material_bindings_with_exported_object_transform.usda"
        USDExporter.ExportFile exportPath exportOptions:exportOptions
        
        local stage = pyUsd.Stage.Open(exportPath)
        local boxPrim = stage.GetPrimAtPath("/box001/box001_Shape")
        
        validate_multiMtl_binding stage boxPrim 6
    ),
    
    function test_export_multi_material_only_one_matid_used = (
        local box001 = Box pos:[0,0,0] name:"box001"
        convertto box001 editable_poly
        numFaces = (getNumFaces box001)
        polyop.SetFaceMatID box001 #(1,2,3,4,5,6) 3
        
        local multiMtl = multimaterial numsubs:6
        multiMtl.materialList[3].name = "mtl3"

        box001.material = multiMtl
        
        local exportPath = output_prefix + "export_multi_material_only_one_matid_used.usda"
        USDExporter.ExportFile exportPath exportOptions:exportMaterialTestOptions
        
        local stage = pyUsd.Stage.Open(exportPath)
        local boxPrim = stage.GetPrimAtPath("/box001")
        
        validate_simple_bindings stage boxPrim
    ),
    
    function test_export_single_material_bindings = (
        local box001 = Box pos:[0,0,0] name:"box001"
        local physicalMtl = Physical_Material name:"mtl1"
        
        box001.material = physicalMtl
        
        local exportPath = output_prefix + "export_single_material_bindings.usda"
        USDExporter.ExportFile exportPath exportOptions:exportMaterialTestOptions
        
        local stage = pyUsd.Stage.Open(exportPath)
        local boxPrim = stage.GetPrimAtPath("/box001")
        
        validate_simple_bindings stage boxPrim
    ),
    
    function test_export_hierarchy_material_bindings = (
        local box1 = Box pos:[0,0,0] name:"box1"
        local box2 = Box pos:[0,0,0] name:"box2"
        box2.parent = box1
        local box3 = Box pos:[0,0,0] name:"box3"
        box3.parent = box2
        
        local materialA = Physical_Material name:"A"
        local materialB = Physical_Material name:"B"
        local materialC = Physical_Material name:"C"
        
        box1.material = materialA
        box2.material = materialB
        box3.material = materialC
        local exportPath = output_prefix + "test_export_hierarchy_material_bindings_alpha.usda"
        USDExporter.ExportFile exportPath exportOptions:exportMaterialTestOptions
        
        local stage = pyUsd.Stage.Open(exportPath)
        local box1Prim = stage.GetPrimAtPath("/box1/box1_Shape")
        local box2Prim = stage.GetPrimAtPath("/box1/box2/box2_Shape")
        local box3Prim = stage.GetPrimAtPath("/box1/box2/box3")
        assert_equal "A" (get_bound_material_name stage box1Prim)
        assert_equal "B" (get_bound_material_name stage box2Prim)
        assert_equal "C" (get_bound_material_name stage box3Prim)
        
        -- Test with different binding (we had an issue with the order in which 
        -- material were processed, caused problems).
        
        box1.material = materialC
        box2.material = materialB
        box3.material = materialA
        
        local exportPath = output_prefix + "test_export_hierarchy_material_bindings_rev_alpha.usda"
        USDExporter.ExportFile exportPath exportOptions:exportMaterialTestOptions
        local stage = pyUsd.Stage.Open(exportPath)
        local box1Prim = stage.GetPrimAtPath("/box1/box1_Shape")
        local box2Prim = stage.GetPrimAtPath("/box1/box2/box2_Shape")
        local box3Prim = stage.GetPrimAtPath("/box1/box2/box3")
        assert_equal "C" (get_bound_material_name stage box1Prim)
        assert_equal "B" (get_bound_material_name stage box2Prim)
        assert_equal "A" (get_bound_material_name stage box3Prim)
        
    ),
    

    function test_export_single_material_bindings_with_exported_object_transform = (
        local box001 = Box pos:[0,0,0] name:"box001"
        local physicalMtl = Physical_Material name:"mtl1"
        box001.objectOffsetPos = [1,1,1]
        box001.material = physicalMtl
        
        local exportOptions = USDExporter.CreateOptions()
        exportOptions.FileFormat = #ascii
        exportOptions.RootPrimPath = "/"
        exportOptions.BakeObjectOffsetTransform = false
        exportOptions.Materials = true
        
        local exportPath = output_prefix + "export_single_material_bindings_with_exported_object_transform.usda"
        USDExporter.ExportFile exportPath exportOptions:exportOptions
        
        local stage = pyUsd.Stage.Open(exportPath)
        local boxPrim = stage.GetPrimAtPath("/box001/box001_Shape")
        
        validate_simple_bindings stage boxPrim
    ),
    
    function test_export_multi_material_missing_slots = (
        local box001 = Box pos:[0,0,0] name:"box001"
        local multiMtl = multimaterial numsubs:6
        
        multiMtl.materialList[1].name = "mtl1"
        multiMtl.materialList[2].name = "mtl2"
        multiMtl.materialList[3] = undefined
        multiMtl.materialList[4].name = "mtl4"
        multiMtl.materialList[5].name = "mtl5"
        multiMtl.materialList[6].name = "mtl6"
        
        box001.material = multiMtl
        
        local exportPath = output_prefix + "export_multi_material_missing_slots.usda"
        USDExporter.ExportFile exportPath exportOptions:exportMaterialTestOptions
        
        local stage = pyUsd.Stage.Open(exportPath)
        local boxPrim = stage.GetPrimAtPath("/box001")
        
        local bindingAPI = pyShade.MaterialBindingAPI(boxPrim)
        subsets = bindingAPI.GetMaterialBindSubsets()
        assert_equal subsets.count 6
    
        for i = 1 to 6 do
        (
            local subset1BindingAPI = pyShade.MaterialBindingAPI(subsets[i])
            local mtlBindingRel = subset1BindingAPI.GetDirectBindingRel()
            local bindingTargets = mtlBindingRel.GetTargets()
            if i == 3 then 
            (
                assert_equal bindingTargets.count 0
            )
            else
            (
                assert_equal bindingTargets.count 1
                local mtlPrim = stage.GetPrimAtPath(bindingTargets[1])
                assert_true (pyUsd.Prim.IsValid(mtlPrim))
            )
        )
    ),
    
    function test_export_multi_material_unused_mtlId = (
        local box001 = Box pos:[0,0,0] name:"box001"
        local multiMtl = multimaterial numsubs:10
        
        multiMtl.materialList[1].name = "mtl1"
        multiMtl.materialList[2].name = "mtl2"
        multiMtl.materialList[3].name = "mtl2"
        multiMtl.materialList[4].name = "mtl4"
        multiMtl.materialList[5].name = "mtl5"
        multiMtl.materialList[6].name = "mtl6"
        
        box001.material = multiMtl
        
        local exportPath = output_prefix + "export_multi_material_unused_mtlId.usda"
        USDExporter.ExportFile exportPath exportOptions:exportMaterialTestOptions
        
        local stage = pyUsd.Stage.Open(exportPath)
        local boxPrim = stage.GetPrimAtPath("/box001")
        
        local bindingAPI = pyShade.MaterialBindingAPI(boxPrim)
        validate_multiMtl_binding stage boxPrim 6
    ),
    
    function test_export_material_on_instances_with_different_materials = (
        -- box001 will not have any material assigned
        local box001 = Box pos:[0,0,0] name:"box001"
        
        -- box002 will have a physical material assigned
        maxOps.cloneNodes #(box001) cloneType:#instance actualNodeList:&list newNodes:&newNodes
        newNodes[1].name = "box002"
        newNodes[1].material = PhysicalMaterial()
        
        -- box003 will have a multi material assigned
        maxOps.cloneNodes #(box001) cloneType:#instance actualNodeList:&list newNodes:&newNodes
        newNodes[1].name = "box003"
        newNodes[1].material = multimaterial numsubs:6

        local exportPath = output_prefix + "export_material_on_instances_with_different_materials.usda"
        USDExporter.ExportFile exportPath exportOptions:exportMaterialTestOptions
        
        local stage = pyUsd.Stage.Open(exportPath)
        local box1Prim = stage.GetPrimAtPath("/box001")
        local box2Prim = stage.GetPrimAtPath("/box002")
        local box3Prim = stage.GetPrimAtPath("/box003")
        
        -- Only box 3 should have broken instancing
        assert_true (box1Prim.IsInstance())
        assert_true (box2Prim.IsInstance())
        assert_false (box3Prim.IsInstance())
        
        -- validate box 1 has no bindings
        local bindingAPI = pyShade.MaterialBindingAPI(box1Prim)
        local mtlBindingRel = bindingAPI.GetDirectBindingRel()
        local bindingTargets = mtlBindingRel.GetTargets()
        assert_equal bindingTargets.count 0
            
        -- validate box 2 binding on the instance prim
        validate_simple_bindings stage box2Prim
            
        -- Validate box 3 binding for multimaterial
        local box3PrimChild = stage.GetPrimAtPath("/box003/box001")
        validate_multiMtl_binding stage box3PrimChild 6
    ),
    
    function test_export_material_on_instances_with_different_materials_when_only_one_matId_used = (
        -- box001 will not have any material assigned
        local box001 = Box pos:[0,0,0] name:"box001"
        convertto box001 editable_poly
        numFaces = (getNumFaces box001)
        polyop.SetFaceMatID box001 #(1,2,3,4,5,6) 3
        
        -- box002 will have a physical material assigned
        maxOps.cloneNodes #(box001) cloneType:#instance actualNodeList:&list newNodes:&newNodes
        newNodes[1].name = "box002"
        newNodes[1].material = PhysicalMaterial()
        
        -- box003 will have a multi material assigned
        maxOps.cloneNodes #(box001) cloneType:#instance actualNodeList:&list newNodes:&newNodes
        newNodes[1].name = "box003"
        newNodes[1].material = multimaterial numsubs:6

        local exportPath = output_prefix + "export_material_on_instances_with_different_materials_only_one_matId_used.usda"
        USDExporter.ExportFile exportPath exportOptions:exportMaterialTestOptions
        
        local stage = pyUsd.Stage.Open(exportPath)
        local box1Prim = stage.GetPrimAtPath("/box001")
        local box2Prim = stage.GetPrimAtPath("/box002")
        local box3Prim = stage.GetPrimAtPath("/box003")
        
        -- No box should have broken instancing
        assert_true (box1Prim.IsInstance())
        assert_true (box2Prim.IsInstance())
        assert_true (box3Prim.IsInstance())
        
        -- validate box 1 has no bindings
        local bindingAPI = pyShade.MaterialBindingAPI(box1Prim)
        local mtlBindingRel = bindingAPI.GetDirectBindingRel()
        local bindingTargets = mtlBindingRel.GetTargets()
        assert_equal bindingTargets.count 0
            
        -- validate box 2 binding on the instance prim
        validate_simple_bindings stage box2Prim
            
        -- Validate box 3 binding on the instance prim
        validate_simple_bindings stage box3Prim
    ),
    
    function test_export_multi_material_on_instances_with_same_material = (
        local box001 = Box pos:[0,0,0] name:"box001"
        multiMtl = multimaterial numsubs:6
        box001.material = multiMtl
        
        maxOps.cloneNodes #(box001) cloneType:#instance actualNodeList:&list newNodes:&newNodes
        newNodes[1].name = "box002"

        local exportPath = output_prefix + "export_multi_material_on_instances_with_same_material.usda"
        USDExporter.ExportFile exportPath exportOptions:exportMaterialTestOptions
        
        local stage = pyUsd.Stage.Open(exportPath)
        local box1Prim = stage.GetPrimAtPath("/box001")
        local box2Prim = stage.GetPrimAtPath("/box002")
        
        assert_true (box1Prim.IsInstance())
        assert_true (box2Prim.IsInstance())
            
        local box1PrimChild = stage.GetPrimAtPath("/box001/box001")
        local box2PrimChild = stage.GetPrimAtPath("/box002/box001")
            
        validate_multiMtl_binding stage box1PrimChild 6
        validate_multiMtl_binding stage box2PrimChild 6
    ),
    
    function test_export_simple_material_on_instances_with_same_material = (
        local box001 = Box pos:[0,0,0] name:"box001"
        multiMtl = PhysicalMaterial()
        box001.material = multiMtl
        
        maxOps.cloneNodes #(box001) cloneType:#instance actualNodeList:&list newNodes:&newNodes
        newNodes[1].name = "box002"

        local exportPath = output_prefix + "export_simple_material_on_instances_with_same_material.usda"
        USDExporter.ExportFile exportPath exportOptions:exportMaterialTestOptions
        
        local stage = pyUsd.Stage.Open(exportPath)
        local box1Prim = stage.GetPrimAtPath("/box001")
        local box2Prim = stage.GetPrimAtPath("/box002")
        
        assert_true (box1Prim.IsInstance())
        assert_true (box2Prim.IsInstance())
            
        local box1ChildPrim = stage.GetPrimAtPath("/box001/box001")
        local box2ChildPrim = stage.GetPrimAtPath("/box002/box001")
            
        validate_simple_bindings stage box1ChildPrim
        validate_simple_bindings stage box2ChildPrim
    ),
    
    function test_export_material_on_single_selected_instance = (
        local box001 = Box pos:[0,0,0] name:"box001"
        multiMtl = PhysicalMaterial()
        box001.material = multiMtl
        
        maxOps.cloneNodes #(box001) cloneType:#instance actualNodeList:&list newNodes:&newNodes
        newNodes[1].name = "box002"

        local exportPath = output_prefix + "export_material_on_single_selected_instance.usda"
        select $box002
        USDExporter.ExportFile exportPath exportOptions:exportMaterialTestOptions contentSource:#selected
        
        local stage = pyUsd.Stage.Open(exportPath)
        local box2Prim = stage.GetPrimAtPath("/box002")
        
        -- Single Max instance not exported as a USD instance.
        assert_false (box2Prim.IsInstance())
        local box2 = stage.GetPrimAtPath("/box002")
        validate_simple_bindings stage box2
    ),
    
    function test_export_instance_with_object_transform = (
        Box pos:[0,0,0] name:"box001"
        MaxOps.cloneNodes #($box001) cloneType:#instance actualNodeList:&list newNodes:&newNodes
        newNodes[1].name = "box002"
        
        $box001.material = PhysicalMaterial()
        $box002.material = multimaterial numsubs:6
        
        $box001.objectOffsetPos = [1,1,1]
        $box002.objectOffsetPos = [1,1,1]
        
        local exportOptions = USDExporter.CreateOptions()
        exportOptions.FileFormat = #ascii
        exportOptions.BakeObjectOffsetTransform = false
        exportOptions.Materials = true
        
        local exportPath = output_prefix + "export_instance_with_object_transform.usda"
        USDExporter.ExportFile exportPath exportOptions:exportMaterialTestOptions
        
        local stage = pyUsd.Stage.Open(exportPath)
        local box1Prim = stage.GetPrimAtPath("/box001/box001_Shape")
        local box2Prim = stage.GetPrimAtPath("/box002/box002_Shape/box001")

        validate_simple_bindings stage box1Prim
        validate_multiMtl_binding stage box2Prim 6
    ),
    
    function test_export_physical_material_values = (
        local box001 = Box pos:[0,0,0] name:"box001"
        local physicalMtl = PhysicalMaterial name:"usdPhysTestMtl"
        box001.material = physicalMtl
        
        setupValuesOnPhysMtl physicalMtl
        
        local exportPath = output_prefix + "export_physical_material_values.usda"
        USDExporter.ExportFile exportPath exportOptions:exportMaterialTestOptions
        
        local stage = pyUsd.Stage.Open(exportPath)
        local shaderPrim = pyShade.Shader.Get stage "/mtl/usdPhysTestMtl/usdPhysTestMtl"
        
        validateValueOnPhysMtl shaderPrim
    ),
    
    function test_export_previewsurface_material_values = (
        local box001 = Box pos:[0,0,0] name:"box001"
        local mtl = MaxUsdPreviewSurface()
        mtl.name = "usdTestMtl"
        box001.material = mtl
        
        setupValuesOnPreviewSurfaceMtl mtl
        
        local exportPath = output_prefix + "export_usdPreviewSurface_material_values.usda"
        USDExporter.ExportFile exportPath exportOptions:exportMaterialTestOptions
        
        local stage = pyUsd.Stage.Open(exportPath)
        local shaderPrim = pyShade.Shader.Get stage "/mtl/usdTestMtl/usdTestMtl"
        
        validateValueOnPreviewSurfaceMtl shaderPrim
    ),    
    
    function pathToForwardSlash path = (
        return (substituteString path @"\" @"/")
    ),
    
    function getTexturePrim connectedSource = (
        local rgbOutput = (connectedSource).GetOutput("rgb")
        local valProducingAttr = (rgbOutput.GetValueProducingAttributes())[1]
        return pyShade.Shader((valProducingAttr).GetPrim())
    ),
    
    function test_export_physical_material_bitmap_maps = (
        local box001 = Box pos:[0,0,0] name:"box001"
        local physicalMtl = PhysicalMaterial name:"usdPhysMapTestMtl"
        box001.material = physicalMtl
        
        local baseColorBitmapPath = @"C:\folderX\basecolor.png"
        local emitColorBitmapPath = @"C:\emitcolor.png"
        local displacementBitmapPath = @"D:\displacement.jpg"
        local roughnessBitmapPath = @"T:\roughness"
        local metalnessBitmapPath = @"c:\metalness.png"
        local coatingBitmapPath = @"E:\coating.tiff"
        local coatRoughnessBitmapPath = @"C:\roughness.png"
        local bumpBitmapPath = @"C:\bump.jpg"
        local iorBitmapPath = @"C:\ior.png"
        local cutoutBitmapPath = @"\\somedrive\cutout.jpg"
        
        physicalMtl.base_color_map = Bitmaptexture fileName:baseColorBitmapPath
        physicalMtl.emission_map = Bitmaptexture fileName:emitColorBitmapPath
        physicalMtl.displacement_map = Bitmaptexture fileName:displacementBitmapPath
        physicalMtl.roughness_map = Bitmaptexture fileName:roughnessBitmapPath
        physicalMtl.metalness_map = Bitmaptexture fileName:metalnessBitmapPath
        physicalMtl.coat_map = Bitmaptexture fileName:coatingBitmapPath
        physicalMtl.coat_rough_map = Bitmaptexture fileName:coatRoughnessBitmapPath        
        -- bump maps will be skipped unless going through a normal bump node!
        physicalMtl.bump_map = Normal_Bump()
        physicalMtl.bump_map.normal_map = Bitmaptexture fileName:bumpBitmapPath
        physicalMtl.trans_ior_map = Bitmaptexture fileName:iorBitmapPath
        physicalMtl.cutout_map = Bitmaptexture fileName:cutoutBitmapPath
        
        local exportPath = output_prefix + "export_physical_material_bitmap_maps_on.usda"
        USDExporter.ExportFile exportPath exportOptions:exportMaterialTestOptions
        
        local stage = pyUsd.Stage.Open(exportPath)
        local shaderPrim = pyShade.Shader.Get stage "/mtl/usdPhysMapTestMtl/usdPhysMapTestMtl"
        
        assert_true ((shaderPrim.GetInput("diffuseColor")).HasConnectedSource())
        assert_true ((shaderPrim.GetInput("emissiveColor")).HasConnectedSource())
        assert_true ((shaderPrim.GetInput("displacement")).HasConnectedSource())
        assert_true ((shaderPrim.GetInput("opacity")).HasConnectedSource())
        assert_true ((shaderPrim.GetInput("roughness")).HasConnectedSource())
        assert_true ((shaderPrim.GetInput("metallic")).HasConnectedSource())
        assert_true ((shaderPrim.GetInput("clearcoat")).HasConnectedSource())
        assert_true ((shaderPrim.GetInput("clearcoatRoughness")).HasConnectedSource())
        assert_true ((shaderPrim.GetInput("normal")).HasConnectedSource())
        assert_true ((shaderPrim.GetInput("ior")).HasConnectedSource())
            
        local connectedSourceTuple = ((shaderPrim.GetInput("diffuseColor")).GetConnectedSource())
        local texturePrim = getTexturePrim (connectedSourceTuple[1])
        local inputFile = (texturePrim.GetInput("file")).Get() as string
        assert_equal ("@" + (pathToForwardSlash baseColorBitmapPath)+ "@") inputFile
            
        connectedSourceTuple = ((shaderPrim.GetInput("emissiveColor")).GetConnectedSource())
        texturePrim= getTexturePrim (connectedSourceTuple[1])
        inputFile = (texturePrim.GetInput("file")).Get() as string
        assert_equal ("@" + (pathToForwardSlash emitColorBitmapPath) + "@") inputFile
            
        connectedSourceTuple = ((shaderPrim.GetInput("displacement")).GetConnectedSource())
        texturePrim = getTexturePrim (connectedSourceTuple[1])
        inputFile = (texturePrim.GetInput("file")).Get() as string
        assert_equal ("@" + (pathToForwardSlash displacementBitmapPath) + "@") inputFile     
        
        connectedSourceTuple = ((shaderPrim.GetInput("opacity")).GetConnectedSource())
        texturePrim = getTexturePrim (connectedSourceTuple[1])
        inputFile = (texturePrim.GetInput("file")).Get() as string
        -- Note : pathlib.as_posix(), used to convert paths to forward slashes, adds a trailing "/" on UNC paths,
        -- it still resolves ok to that file on windows.
        assert_equal ("@" + (pathToForwardSlash cutoutBitmapPath) + "/" + "@") inputFile
            
        connectedSourceTuple = ((shaderPrim.GetInput("roughness")).GetConnectedSource())
        texturePrim = getTexturePrim (connectedSourceTuple[1])
        inputFile = (texturePrim.GetInput("file")).Get() as string
        assert_equal ("@" + (pathToForwardSlash roughnessBitmapPath) + "@") inputFile
        
        connectedSourceTuple = ((shaderPrim.GetInput("metallic")).GetConnectedSource())
        texturePrim = getTexturePrim (connectedSourceTuple[1])
        inputFile = (texturePrim.GetInput("file")).Get() as string
        assert_equal ("@" + (pathToForwardSlash metalnessBitmapPath) + "@") inputFile
            
        connectedSourceTuple = ((shaderPrim.GetInput("clearcoat")).GetConnectedSource())
        texturePrim = getTexturePrim (connectedSourceTuple[1])
        inputFile = (texturePrim.GetInput("file")).Get() as string
        assert_equal ("@" + (pathToForwardSlash coatingBitmapPath) + "@") inputFile
        
        connectedSourceTuple = ((shaderPrim.GetInput("clearcoatRoughness")).GetConnectedSource())
        texturePrim = getTexturePrim (connectedSourceTuple[1])
        inputFile = (texturePrim.GetInput("file")).Get() as string
        assert_equal ("@" + (pathToForwardSlash coatRoughnessBitmapPath) + "@") inputFile
            
        connectedSourceTuple = ((shaderPrim.GetInput("normal")).GetConnectedSource())
        texturePrim = getTexturePrim (connectedSourceTuple[1])
        inputFile = (texturePrim.GetInput("file")).Get() as string
        assert_equal ("@" + (pathToForwardSlash bumpBitmapPath) + "@") inputFile

        connectedSourceTuple = ((shaderPrim.GetInput("ior")).GetConnectedSource())
        texturePrim = getTexturePrim (connectedSourceTuple[1])
        inputFile = (texturePrim.GetInput("file")).Get() as string
        assert_equal ("@" + (pathToForwardSlash iorBitmapPath) + "@") inputFile
            
        physicalMtl.base_color_map_on = false
        physicalMtl.emission_map_on = false
        physicalMtl.displacement_map_on = false
        physicalMtl.transparency_map_on = false
        physicalMtl.roughness_map_on = false
        physicalMtl.metalness_map_on = false
        physicalMtl.coat_map_on = false
        physicalMtl.coat_rough_map_on = false
        physicalMtl.bump_map_on = false
        physicalMtl.trans_ior_map_on = false
        physicalMtl.cutout_map = undefined -- Remove cutout, otherwise the transparency value is not used.
            
        setupValuesOnPhysMtl physicalMtl
            
        exportPath = output_prefix + "export_physical_material_bitmap_maps_off.usda"
        USDExporter.ExportFile exportPath exportOptions:exportMaterialTestOptions

        local stageMapOff = pyUsd.Stage.Open(exportPath)
        shaderPrim = pyShade.Shader.Get stageMapOff "/mtl/usdPhysMapTestMtl/usdPhysMapTestMtl"
        
        validateValueOnPhysMtl shaderPrim
    ),
    
    function create_osl_multi_channel osl_texture channelIndex = (
        multiChannelMap = MultiOutputChannelTexmapToTexmap()
        multiChannelMap.sourceMap = osl_texture
        multiChannelMap.outputChannelIndex = channelIndex
        return multiChannelMap
    ),
    
    function test_multi_channel_texture_material_connections = (
                
        -- Channel indices
        -- rgb: 1
        -- r  : 2
        -- g  : 3
        -- b  : 4
        -- a  : 5
        
        oslTexture = OSLMap()
        oslTexture.name = "Dummy"
        max_root = symbolicPaths.getPathValue("$max")

        maxver = maxversion()
        if maxver[1] >= 26000 then (  -- 3ds Max 2024 and up
            oslTexture.OSLPath = (max_root + "/OSL/UberBitmap2.osl")
        ) else (
            oslTexture.OSLPath = (max_root + "/OSL/UberBitmap.osl")
        )
        oslTexture.OSLAutoUpdate = True
        
        local scriptDir = getFilenamePath (getThisScriptFilename())
        local uv_checker_png = (scriptDir + @"data\uv_checker.png")
        
        oslTexture.filename = uv_checker_png
        
        box1 = convertToPoly (Box())
        polyop.defaultMapFaces box1 1
            
        usdpsMaterial = MaxUsdPreviewSurface()

        usdpsMaterial.diffuseColor_map = create_osl_multi_channel oslTexture  1 -- rgb
        usdpsMaterial.metallic_map = create_osl_multi_channel oslTexture 2 -- r
        usdpsMaterial.roughness_map = create_osl_multi_channel oslTexture 3 -- g 
        usdpsMaterial.displacement_map = create_osl_multi_channel oslTexture 4 -- b
        usdpsMaterial.opacity_map = create_osl_multi_channel oslTexture 5 -- a
        
        box1.material = usdpsMaterial
            
        exportPath = output_prefix + "test_multi_channel_texture_material_connections.usda"
                
        USDExporter.ExportFile exportPath exportOptions:exportMaterialTestOptions        
        
        -- usdchecker should flag this file valid
        assert_true (check_usd_file_valid exportPath)
            
        local stage = pyUsd.Stage.Open(exportPath)
        
        local shaderPrim = pyShade.Shader.Get stage "/mtl/MaxUsdPreviewSurface/MaxUsdPreviewSurface"
        
        -- Connected the diffuse color to the rgb output of the bitmap.
        local diffuseSource = ((shaderPrim.GetInput("diffuseColor")).GetRawConnectedSourcePaths())[1]
        assert_equal "/mtl/MaxUsdPreviewSurface/Dummy.outputs:rgb" (diffuseSource as string)
        
        -- Connected the metalness to the r output of the bitmap.
        local metallicSource  = ((shaderPrim.GetInput("metallic")).GetRawConnectedSourcePaths())[1]
        assert_equal "/mtl/MaxUsdPreviewSurface/Dummy.outputs:r" (metallicSource as string)
        
        -- Connected the roughness to the g output of the bitmap.
        local roughnessSource = ((shaderPrim.GetInput("roughness")).GetRawConnectedSourcePaths())[1]
        assert_equal "/mtl/MaxUsdPreviewSurface/Dummy.outputs:g" (roughnessSource as string)
        
        -- Connected the displacement to the 'b' output of the bitmap.
        local displacementSource = ((shaderPrim.GetInput("displacement")).GetRawConnectedSourcePaths())[1]
        assert_equal "/mtl/MaxUsdPreviewSurface/Dummy.outputs:b" (displacementSource as string)
        
        -- Connected the opacity to the 'a' output of the bitmap.
        local opacitySource = ((shaderPrim.GetInput("opacity")).GetRawConnectedSourcePaths())[1]
        assert_equal "/mtl/MaxUsdPreviewSurface/Dummy.outputs:a" (opacitySource as string)
    ),
    
    -- We had a defect where some data was not cleared between exports, causing problems if the same materials were being 
    -- exported more than once. This tests 
    function test_consecutive_material_export = (
        local box001 = Box pos:[0,0,0] name:"box001"
        local physicalMtl = PhysicalMaterial name:"usdPhysMapTestMtl"
        box001.material = physicalMtl
        
        local baseColorBitmapPath = @"C:\folderX\basecolor.png"
        physicalMtl.base_color_map = Bitmaptexture fileName:baseColorBitmapPath
        
        local exportPath = output_prefix + "test_consecutive_material_export.usda"
        USDExporter.ExportFile exportPath exportOptions:exportMaterialTestOptions
        
        local stage = pyUsd.Stage.Open(exportPath)
        local textureNode = stage.GetPrimAtPath("/mtl/usdPhysMapTestMtl/Bitmaptexture/Bitmaptexture")
        assert_true (textureNode.IsValid())
        assert_true (textureNode.IsA(pyShade.Shader)) 
        local primvarReaderNode = stage.GetPrimAtPath("/mtl/usdPhysMapTestMtl/Bitmaptexture/PrimvarReader_st")
        assert_true (primvarReaderNode.IsValid())
        assert_true (primvarReaderNode.IsA(pyShade.Shader))
            
        local exportPath = output_prefix + "test_consecutive_material_export_1.usda"
        USDExporter.ExportFile exportPath exportOptions:exportMaterialTestOptions
        
        -- Prior to the fix, we had missing shader nodes on the second export.
        local stage = pyUsd.Stage.Open(exportPath)
        local textureNode = stage.GetPrimAtPath("/mtl/usdPhysMapTestMtl/Bitmaptexture/Bitmaptexture")
        assert_true (textureNode.IsValid())
        assert_true (textureNode.IsA(pyShade.Shader)) 
        local primvarReaderNode = stage.GetPrimAtPath("/mtl/usdPhysMapTestMtl/Bitmaptexture/PrimvarReader_st")
        assert_true (primvarReaderNode.IsValid())
        assert_true (primvarReaderNode.IsA(pyShade.Shader))
    ),
    
    -- Tests that a texture map which is used by two different material is only exported once.
    function test_shared_texture_map = (
        -- Create two boxes and two materials.
        local box001 = Box pos:[0,0,0] name:"box001"
        local physicalMtl = PhysicalMaterial name:"usdPhysMapTestMtl"
        box001.material = physicalMtl
        
        local box002 = Box pos:[0,0,20] name:"box002"
        local physicalMtl2 = PhysicalMaterial name:"usdPhysMapTestMtl2"
        box002.material = physicalMtl2
        
        -- Assign the same BitmapTexture to both materials..
        local baseColorBitmapPath = @"C:\folderX\basecolor.png"
        local texMap = Bitmaptexture fileName:baseColorBitmapPath
        physicalMtl.base_color_map = texMap
        physicalMtl2.base_color_map = texMap
        
        local exportPath = output_prefix + "test_shared_texture_map.usda"
        USDExporter.ExportFile exportPath exportOptions:exportMaterialTestOptions
        
        local stage = pyUsd.Stage.Open(exportPath)
        local materials = stage.GetPrimAtPath("/mtl")
        -- Should only have 3 prims under material. One for each material + one for the map.
        assert_equal 3 ((materials.GetAllChildren()).Count)
        
        local mat1 = stage.GetPrimAtPath("/mtl/usdPhysMapTestMtl")
        assert_true (mat1.IsValid())
        assert_true (mat1.IsA(pyShade.Material))
        local mat2 = stage.GetPrimAtPath("/mtl/usdPhysMapTestMtl2")
        assert_true (mat2.IsValid())
        assert_true (mat2.IsA(pyShade.Material))
        local textureNode = stage.GetPrimAtPath("/mtl/Bitmaptexture")
        assert_true (textureNode.IsValid())
        assert_true (textureNode.IsA(pyShade.NodeGraph))
    ),
    
    function test_export_unknown_material_to_dummy = (
        -- Create a box with a material unknown to our shader writers.        
        local theBox = Box()
        local adskMtl = AutodeskMaterial()
        -- Setup the color, it is the only thing that will carry over to UsdPreviewSurface.
        adskMtl.generic_color  = color 255 0 0
        theBox.material = adskMtl
                
        local expOptions = USDExporter.CreateOptions()
        expOptions.FileFormat = #ascii
        expOptions.RootPrimPath = "/"
        
        local exportPath = output_prefix + "test_export_unknown_material_to_dummy.usda"
        USDExporter.ExportFile exportPath exportOptions:expOptions
        
        local stage = pyUsd.Stage.Open(exportPath)
        -- We should have generated a material, with the diffuse color assigned, as fallback.
        local shaderPrim = pyShade.Shader.Get stage "/mtl/Autodesk_Material/Autodesk_Material"
        assert_equal ((pyGf.Vec3f 1 0 0) as string) (((shaderPrim.GetInput("diffuseColor")).Get()) as string)
        
        -- If materials are disabled, no dummy material generated.
        expOptions.materials = false
        local exportPath = output_prefix + "test_export_unknown_material_to_dummy_materials_disabled.usda"
        USDExporter.ExportFile exportPath exportOptions:expOptions
        
        local stage = pyUsd.Stage.Open(exportPath)
        local shaderPrim = stage.GetPrimAtPath("/mtl/Autodesk_Material/Autodesk_Material")
        assert_false (shaderPrim.IsValid())
            
        -- If materials are enabled, but UsdPreviewSurface is not a target, no dummmy material generated.
        expOptions.materials = true
        expOptions.AllMaterialTargets = #()
        local exportPath = output_prefix + "test_export_unknown_material_to_dummy_usdps_disabled.usda"
        USDExporter.ExportFile exportPath exportOptions:expOptions
        
        local stage = pyUsd.Stage.Open(exportPath)
        local shaderPrim = stage.GetPrimAtPath("/mtl/Autodesk_Material/Autodesk_Material")
        assert_false (shaderPrim.IsValid())            
    ),
    
    -- Test making sure that materials name conflicts are properly handled, material names
    -- do not need to be unique on the 3dsMax side, but paths must be unique on the USD side.
    function test_material_name_conflict = (
        
        local box1 = Box name:"box1"
        local box2 = Box name:"box2"
        local box3 = Box name:"box3"
        
        local mat1 = PhysicalMaterial name:"conflict"
        mat1.base_color = (color 255 0 0)
        local mat2 = PhysicalMaterial name:"conflict"
        mat2.base_color = (color 0 255 0)
        local mat3 = PhysicalMaterial name:"conflict"
        mat3.base_color = (color 0 0 255)
        
        box1.material = mat1
        box2.material = mat2
        box3.material = mat3
        
        local expOptions = USDExporter.CreateOptions()
        expOptions.FileFormat = #ascii
        expOptions.RootPrimPath = "/"
        local exportPath = output_prefix + "test_material_name_conflict.usda"
        USDExporter.ExportFile exportPath exportOptions:expOptions
        
        local stage = pyUsd.Stage.Open(exportPath)
        stage.Reload()

        local box1Prim = stage.GetPrimAtPath("/box1")
        assert_true (box1Prim.IsValid())
            
        local boundMaterial = (((pyShade.MaterialBindingAPI(box1Prim)).GetDirectBindingRel()).GetTargets())[0]
        assert_equal "/mtl/conflict" (boundMaterial as string)
            
        local box2Prim = stage.GetPrimAtPath("/box2")
        assert_true (box2Prim.IsValid())
        local boundMaterial2 = (((pyShade.MaterialBindingAPI(box2Prim)).GetDirectBindingRel()).GetTargets())[0]
        assert_equal "/mtl/conflict1" (boundMaterial2 as string)
    
        local box3Prim = stage.GetPrimAtPath("/box3")
        assert_true (box3Prim.IsValid())
        local boundMaterial3 = (((pyShade.MaterialBindingAPI(box3Prim)).GetDirectBindingRel()).GetTargets())[0]
        assert_equal "/mtl/conflict2" (boundMaterial3 as string)
    ),    
    
    -- Test material ID "overflow" behavior. For example if a mesh has a face with material
    -- ID 3, but the assigned multi-sub only has 2 slots, the face should use the material 
    -- id : matId = (faceMaterialId % maxSlot) = 1
    function test_export_multimaterial_ids_overflow = (
        local theBox = Box name:"box"
        
        -- Create a multimaterial, 2 slots, 2 phys materials.
        local multiSub = multiSubMaterial numsubs:2
        multiSub.numSubs = 2
        local mat1 = PhysicalMaterial()
        mat1.base_color = (color 255 0 0)
        multiSub.materialList[1] = mat1
        local mat2 = PhysicalMaterial()
        mat2.base_color = (color 0 255 0)
        multiSub.materialList[2] = mat2      
        theBox.material = multiSub
        
        -- Export
        local expOptions = USDExporter.CreateOptions()
        expOptions.FileFormat = #ascii
        expOptions.RootPrimPath = "/"
        local exportPath = output_prefix + "test_export_multimaterial_ids_overflow.usda"
        USDExporter.ExportFile exportPath exportOptions:expOptions
        
        local stage = pyUsd.Stage.Open(exportPath)
        stage.Reload()
        
        -- Get the exported box prim, and validate the bindings on all the subsets.
        local boxPrim = stage.GetPrimAtPath("/box")
        assert_true (boxPrim.IsValid())
        local bindingAPI = pyShade.MaterialBindingAPI(boxPrim)
        subsets = bindingAPI.GetMaterialBindSubsets()
        assert_equal 6 subsets.count
        
        -- For all 6 subsets, bindings will alternate between PhysicalMaterial and 
        -- PhysicalMaterial1. For faces 3-6 (which have matching matIds), this is via 
        -- the overflow behavior, as the multimaterial only has 2 sub materials.
        local bindings = #("/mtl/PhysicalMaterial", "/mtl/PhysicalMaterial1")
        for i = 1 to 6 do
        (
            local subset1BindingAPI = pyShade.MaterialBindingAPI(subsets[i])
            local mtlBindingRel = subset1BindingAPI.GetDirectBindingRel()
            local bindingTargets = mtlBindingRel.GetTargets()
            assert_equal bindingTargets.count 1
            assert_equal (bindings[((mod (i-1) 2) + 1)]) ((bindingTargets[0]) as string)
            local mtlPrim = stage.GetPrimAtPath(bindingTargets[0])
            assert_true (pyUsd.Prim.IsValid(mtlPrim))
        )
    ),
    
    -- Similar to the test above. This case covers the case where all the materialIds
    -- are the same on a mesh, in this case we don't create material geomSubsets, 
    -- but we still need to handle the id overflow if the unique id goes beyong the
    -- the number of subMtls in the assigned multimaterial, as we read the id from the 
    -- metadata and need to figure out what subMtl to bind.
    function test_export_multimaterial_single_id_overflow = (
        
        local theBox = Box name:"box"
        
        -- Multimaterial with 2 slots.        
        local multiSub = multiSubMaterial numsubs:2
        multiSub.numSubs = 2
        local mat1 = PhysicalMaterial name:"sub1"
        mat1.base_color = (color 255 0 0)
        multiSub.materialList[1] = mat1
        local mat2 = PhysicalMaterial name:"sub2"
        mat2.base_color = (color 0 255 0)
        multiSub.materialList[2] = mat2      
        theBox.material = multiSub
        
        -- Make all faces use matId 5 on the mesh.
        matMod = Materialmodifier()
        matMod.materialId = 5
        addModifier theBox matMod
        
        -- We should observe the following binding (converting to 0 based, and back...): 
        -- ((5-1) % 2) + 1 = 1 -> mesh should be bound to the first submtls
        
        local expOptions = USDExporter.CreateOptions()
        expOptions.FileFormat = #ascii
        expOptions.RootPrimPath = "/"
        local exportPath = output_prefix + "test_export_multimaterial_single_id_5_overflow.usda"
        USDExporter.ExportFile exportPath exportOptions:expOptions
        
        local stage = pyUsd.Stage.Open(exportPath)
        stage.Reload()
        
        -- Validate the binding.
        local boxPrim = stage.GetPrimAtPath("/box")
        assert_true (boxPrim.IsValid())        
        local bindingAPI = pyShade.MaterialBindingAPI(boxPrim)
        assert_equal "/mtl/sub1" ((((bindingAPI.GetDirectBindingRel()).GetTargets())[0]) as string)
        
        -- Now make all faces use matId 8 on the mesh...
        matMod.materialId = 8
        
        local exportPath = output_prefix + "test_export_multimaterial_single_id_8_overflow.usda"
        USDExporter.ExportFile exportPath exportOptions:expOptions
        -- We should now observe the following binding (converting to 0 based, and back...): 
        -- ((8-1) % 2) + 1 = 2 -> mesh should be bound to the second submtls

        local stage = pyUsd.Stage.Open(exportPath)
        stage.Reload()
        
        -- Validate the binding.
        local boxPrim = stage.GetPrimAtPath("/box")
        assert_true (boxPrim.IsValid())        
        local bindingAPI = pyShade.MaterialBindingAPI(boxPrim)
        assert_equal "/mtl/sub2" ((((bindingAPI.GetDirectBindingRel()).GetTargets())[0]) as string)
    ),
    
    -- Test material ID "overflow" behavior when materialIds do not match the sub-indices.
    function test_export_multimaterial_ids_mismatch_overflow = (
        local theBox = Box name:"box"
        
        -- Create a multimaterial, 2 slots, 2 phys materials.
        local multiSub = multiSubMaterial numSubs:2
        
        local mat1 = PhysicalMaterial()
        mat1.base_color = (color 255 0 0)
        multiSub.materialList[1] = mat1
        local mat2 = PhysicalMaterial()
        mat2.base_color = (color 0 255 0)
        multiSub.materialList[2] = mat2      
        theBox.material = multiSub
        
        -- Change the material ids. 
        -- Using material Id 3 will force numSubs = 3
        -- MatId = 1 will have no material.
        multiSub.materialIDList[1] = 2
        multiSub.materialIDList[2] = 3
                
        -- Export
        local expOptions = USDExporter.CreateOptions()
        expOptions.FileFormat = #ascii
        expOptions.RootPrimPath = "/"
        local exportPath = output_prefix + "test_export_multimaterial_ids_mismatch_overflow.usda"
        USDExporter.ExportFile exportPath exportOptions:expOptions
        local stage = pyUsd.Stage.Open(exportPath)
        stage.Reload()
        
        -- Get the exported box prim, and validate the bindings on all the subsets.
        local boxPrim = stage.GetPrimAtPath("/box")
        assert_true (boxPrim.IsValid())
        local bindingAPI = pyShade.MaterialBindingAPI(boxPrim)
        subsets = bindingAPI.GetMaterialBindSubsets()
        assert_equal 6 subsets.count
        
        -- For all 6 subsets, bindings will alternate between no binding, PhysicalMaterial and 
        -- PhysicalMaterial1.
        local bindings = #("", "/mtl/PhysicalMaterial", "/mtl/PhysicalMaterial1")
        for i = 1 to 6 do
        (
            local subset1BindingAPI = pyShade.MaterialBindingAPI(subsets[i])
            local mtlBindingRel = subset1BindingAPI.GetDirectBindingRel()
            local bindingTargets = mtlBindingRel.GetTargets()
            mtlPath = bindings[((mod (i-1) multiSub.numSubs) + 1)]
            local expectedBindingTargetCount = 0
            if mtlPath != "" then expectedBindingTargetCount = 1
            assert_equal bindingTargets.count expectedBindingTargetCount
            if expectedBindingTargetCount == 0 do 
            (
                continue
            )            
            assert_equal mtlPath ((bindingTargets[0]) as string)
            local mtlPrim = stage.GetPrimAtPath(bindingTargets[0])
            assert_true (pyUsd.Prim.IsValid(mtlPrim))
        )
    ),
    
    -- Tests exporting different combinations of materials targets.
    function test_multi_material_targets = (
        -- Start with creating two boxes, one with a usdpreview surface material, 
        -- the other with a materialX material.
        local boxWithUSDPS = Box name:"usdps"
        
        local usdps = MaxUsdPreviewSurface name:"usdps1"
        boxWithUSDPS.material = usdps
        
        local boxWithMtlX = Box name:"mtlx"
        boxWithMtlX.pos = [0,0,50] -- just to see it!
        local mtlx = MaterialXMaterial name:"mtlx1"
        local scriptDir = getFilenamePath (getThisScriptFilename())
        local mtlxDoc = (scriptDir + @"data\Iberian_Blue_Ceramic_Tiles_1k_8b\Iberian_Blue_Ceramic_Tiles.mtlx")
        mtlx.importMaterial mtlxDoc
        boxWithMtlX.material = mtlx
                
        local expOptions = USDExporter.CreateOptions()
        expOptions.FileFormat = #ascii
        expOptions.RootPrimPath = "/"
            
        -- 1) Test export with only UsdPreviewSurface target enabled: 
            
        expOptions.AllMaterialTargets = #("UsdPreviewSurface")
        local exportPath = output_prefix + "test_multi_material_targets_only_usdps.usda"
        USDExporter.ExportFile exportPath exportOptions:expOptions
        local stage = pyUsd.Stage.Open(exportPath)
        stage.Reload()
        -- If we have UsdPreviewSurface as target, all materials will be converted - if no supporting shader
        -- writer exists for a material type, a dummy writer is used, which only looks at the display color.
        -- MaterialXMaterial -> UsdPreviewSurface UsdShade, via dummy writer.
        local mtlxMatPrim = stage.GetPrimAtPath("/mtl/mtlx1")
        assert_true (mtlxMatPrim.IsValid())
        assert_true (mtlxMatPrim.IsA(pyShade.Material))
        local mtlxShaderPrim = stage.GetPrimAtPath("/mtl/mtlx1/mtlx1")
        assert_true (mtlxShaderPrim.IsValid())
        assert_true (mtlxShaderPrim.IsA(pyShade.Shader))
        -- MaxUsdPreviewSurface -> UsdPreviewSurface UsdShade directly supported by a shader writer.
        local usdpsMatPrim = stage.GetPrimAtPath("/mtl/usdps1")
        assert_true (usdpsMatPrim.IsValid())
        assert_true (usdpsMatPrim.IsA(pyShade.Material))
        local usdpsShaderPrim = stage.GetPrimAtPath("/mtl/usdps1/usdps1")
        assert_true (usdpsShaderPrim.IsValid())
        assert_true (usdpsShaderPrim.IsA(pyShade.Shader))
        
        -- 2) Test export with only MaterialX target enabled:
            
        expOptions.AllMaterialTargets = #("MaterialX")
        local exportPath = output_prefix + "test_multi_material_targets_only_mtlx.usda"
        USDExporter.ExportFile exportPath exportOptions:expOptions
        local stage = pyUsd.Stage.Open(exportPath)
        stage.Reload()
        -- MaterialXMaterial -> MaterialX UsdShade, directly supported by a shader writer.
        local mtlxMatPrim = stage.GetPrimAtPath("/mtl/mtlx1")
        assert_true (mtlxMatPrim.IsValid())
        assert_true (mtlxMatPrim.IsA(pyShade.Material))
        local mtlxShaderPrim = stage.GetPrimAtPath("/mtl/mtlx1/Materials/Iberian_Blue_Ceramic_Tiles/ND_standard_surface_surfaceshader")
        assert_true (mtlxShaderPrim.IsValid())
        assert_true (mtlxShaderPrim.IsA(pyShade.Shader))
        -- MaxUsdPreviewSurface -> MaterialX UsdShade -> not handled.
        local usdpsMatPrim = stage.GetPrimAtPath("/mtl/usdps1")
        assert_false (usdpsMatPrim.IsValid())
        
        -- 3) Export with both UsdPreviewSurface and MaterialX targets enabled.
       
        expOptions.AllMaterialTargets = #("UsdPreviewSurface", "MaterialX")
        local exportPath = output_prefix + "test_multi_material_targets_usdps_and_mtlx.usda"
        USDExporter.ExportFile exportPath exportOptions:expOptions
        local stage = pyUsd.Stage.Open(exportPath)
        stage.Reload()
            
        -- Shading data generated from the MaterialXMaterial..
        local mtlxMatPrim = stage.GetPrimAtPath("/mtl/mtlx1")
        assert_true (mtlxMatPrim.IsValid())
        assert_true (mtlxMatPrim.IsA(pyShade.Material))
        -- MaterialX UsdShade node graph...
        local mtlxContextPrim = stage.GetPrimAtPath("/mtl/mtlx1/MaterialX")
        assert_true (mtlxContextPrim.IsValid())
        assert_true (mtlxContextPrim.IsA(pyShade.NodeGraph))
        local mtlxShaderPrim = stage.GetPrimAtPath("/mtl/mtlx1/MaterialX/Materials/Iberian_Blue_Ceramic_Tiles/ND_standard_surface_surfaceshader")
        assert_true (mtlxShaderPrim.IsValid())
        assert_true (mtlxShaderPrim.IsA(pyShade.Shader))
        -- UsdPreviewSurface UsdShade node graph...
        local uspsContextPrim = stage.GetPrimAtPath("/mtl/mtlx1/UsdPreviewSurface")
        assert_true (uspsContextPrim.IsValid())
        assert_true (uspsContextPrim.IsA(pyShade.NodeGraph))
        local mtlxUsdPsShaderPrim = stage.GetPrimAtPath("/mtl/mtlx1/UsdPreviewSurface/mtlx1")
        assert_true (mtlxUsdPsShaderPrim.IsValid())
        assert_true (mtlxUsdPsShaderPrim.IsA(pyShade.Shader))
            
        -- Shading data generated from the MaxUsdPreviewSurface material..
        local usdpsMatPrim = stage.GetPrimAtPath("/mtl/usdps1")
        assert_true (mtlxMatPrim.IsValid())
        assert_true (mtlxMatPrim.IsA(pyShade.Material))
        -- MaterialX UsdShade node graph...
        local mtlxContextPrim = stage.GetPrimAtPath("/mtl/usdps1/MaterialX")
        -- This node was empty, should have been cleaned up, verify that it's not there.
        assert_false (mtlxContextPrim.IsValid())
        -- UsdPreviewSurface UsdShade node graph...
        local uspsContextPrim = stage.GetPrimAtPath("/mtl/usdps1/UsdPreviewSurface")
        assert_true (uspsContextPrim.IsValid())
        assert_true (uspsContextPrim.IsA(pyShade.NodeGraph))
        -- Exported UsdPreviewSurface data is under the UsdPreviewSurface rendering context...
        local mtlxUsdPsShaderPrim = stage.GetPrimAtPath("/mtl/usdps1/UsdPreviewSurface/usdps1")
        assert_true (mtlxUsdPsShaderPrim.IsValid())
        assert_true (mtlxUsdPsShaderPrim.IsA(pyShade.Shader))

        --4) Multi context with last resort fallback disabled

        expOptions.UseLastResortUSDPreviewSurfaceWriter = false
        local exportPath = output_prefix + "test_disable_last_resort_usdps_fallback.usda"
        USDExporter.ExportFile exportPath exportOptions:expOptions
        local stage = pyUsd.Stage.Open(exportPath)
        stage.Reload()
            
        -- Shading data generated from the MaxUsdPreviewSurface material..
        local usdpsMatPrim = stage.GetPrimAtPath("/mtl/usdps1")
        assert_true (mtlxMatPrim.IsValid())
        assert_true (mtlxMatPrim.IsA(pyShade.Material))
        -- MaterialX UsdShade node graph...
        local mtlxContextPrim = stage.GetPrimAtPath("/mtl/usdps1/MaterialX")
        -- This node was empty, should have been cleaned up, verify that it's not there.
        assert_false (mtlxContextPrim.IsValid())
        -- UsdPreviewSurface UsdShade node graph...
        local uspsContextPrim = stage.GetPrimAtPath("/mtl/usdps1/UsdPreviewSurface")
        assert_true (uspsContextPrim.IsValid())
        assert_true (uspsContextPrim.IsA(pyShade.NodeGraph))
        -- Exported UsdPreviewSurface data is under the UsdPreviewSurface rendering context...
        local mtlxUsdPsShaderPrim = stage.GetPrimAtPath("/mtl/usdps1/UsdPreviewSurface/usdps1")
        assert_true (mtlxUsdPsShaderPrim.IsValid())
        assert_true (mtlxUsdPsShaderPrim.IsA(pyShade.Shader))
            
        -- Shading data generated from the MaterialXMaterial..
        local mtlxMatPrim = stage.GetPrimAtPath("/mtl/mtlx1")
        assert_true (mtlxMatPrim.IsValid())
        assert_true (mtlxMatPrim.IsA(pyShade.Material))
        -- MaterialX UsdShade node graph...
        local mtlxContextPrim = stage.GetPrimAtPath("/mtl/mtlx1/MaterialX")
        assert_true (mtlxContextPrim.IsValid())
        assert_true (mtlxContextPrim.IsA(pyShade.NodeGraph))
        local mtlxShaderPrim = stage.GetPrimAtPath("/mtl/mtlx1/MaterialX/Materials/Iberian_Blue_Ceramic_Tiles/ND_standard_surface_surfaceshader")
        assert_true (mtlxShaderPrim.IsValid())
        assert_true (mtlxShaderPrim.IsA(pyShade.Shader))
        -- UsdPreviewSurface UsdShade node graph...
        -- The last resort fallback is disabled we should have nothing there.
        local uspsContextPrim = stage.GetPrimAtPath("/mtl/mtlx1/UsdPreviewSurface")
        assert_false (uspsContextPrim.IsValid())
    ),
    
    function test_export_usd_preview_surface_material = (
        -- TODO test that every PBR spec/gloss mtl map are correctly exported
    ),
    
    function test_export_PBR_spec_gloss_material = (
        -- TODO test that every PBR spec/gloss mtl map are correctly exported
    ),
    
    function test_export_PBR_metal_Rough_material = (
        -- TODO test that every PBR metal/rought mtl map are correctly exported
    ),
    
    -- Test Separate material layer in conjunction with custom prim scope
    -- mtl1 export to a separate layer and a customized scope, via custom attributes.
    -- mtl2 does the same but with different values, via custom attributes.
    -- mtl3 export to a separate layer and custom scope, both defined in the Export options.
    -- mtl4 export to the Export option specified layer, but customize it's scope path via the custom attributes.
    -- mtl5 export to a separate layer with a invalid file path containing unicode characters.
    function test_export_material_separate_files = (

        local teapot001 = teapot name:"teapot001"
        local teapot002 = teapot name:"teapot002"
        local teapot003 = teapot name:"teapot003"
        local teapot004 = teapot name:"teapot004"
        local teapot005 = teapot name:"teapot005"

        local baseColorBitmapPath1 = @"C:\folderX\basecolor.png"
        local baseColorBitmapPath2 = @"C:\AnotherfolderX\basecolor.png"
        local baseColorBitmapPath3 = @"C:\YetAnotherfolderX\basecolor.png"
        local baseColorBitmapPath4 = @"C:\YetAnotherDifferentfolderX\basecolor.png"
        local baseColorBitmapPath5 = @"C:\YetAnotherDifferentNewfolderX\basecolor.png"

        local mtl1 = MaxUsdPreviewSurface()
        mtl1.name = "matTeapot001"
        mtl1.diffuseColor_map = Bitmaptexture fileName:baseColorBitmapPath1

        local mtl2 = MaxUsdPreviewSurface()
        mtl2.name = "matTeapot002"
        mtl2.diffuseColor_map = Bitmaptexture fileName:baseColorBitmapPath2

        local mtl3 = MaxUsdPreviewSurface()
        mtl3.name = "matTeapot003"
        mtl3.diffuseColor_map = Bitmaptexture fileName:baseColorBitmapPath3
        
        local mtl4 = MaxUsdPreviewSurface()
        mtl4.name = "matTeapot004"
        mtl4.diffuseColor_map = Bitmaptexture fileName:baseColorBitmapPath4
		
        local mtl5 = MaxUsdPreviewSurface()
        mtl5.name = "matTeapot005"
        mtl5.diffuseColor_map = Bitmaptexture fileName:baseColorBitmapPath5
		
        teapot001.material = mtl1
        teapot002.material = mtl2
        teapot003.material = mtl3
        teapot004.material = mtl4
        teapot005.material = mtl5

        select $*
        macros.run "USD" "AddUsdCustomAttrToObjectSelection"
        custAttrFileName = output_prefix + "custAttrMaterial.usda"
        mtl1.UsdMaterialAttributeHolder.pathMethod = 2
        mtl2.UsdMaterialAttributeHolder.pathMethod = 2
        mtl1.UsdMaterialAttributeHolder.separateLayer = true
        mtl2.UsdMaterialAttributeHolder.separateLayer = true
        mtl1.UsdMaterialAttributeHolder.filePath = custAttrFileName
        mtl1.UsdMaterialAttributeHolder.primPath = "Hello"
        mtl2.UsdMaterialAttributeHolder.filePath = custAttrFileName
        mtl2.UsdMaterialAttributeHolder.primPath = "World"
        
        mtl4.UsdMaterialAttributeHolder.pathMethod = 2
        mtl4.UsdMaterialAttributeHolder.primPath = "Test"
		
        mtl5.UsdMaterialAttributeHolder.filePath = output_prefix + "custAttrMaterialééé.usda"

        local exportOptions = USDExporter.CreateOptions()
        exportOptions.MaterialPrimPath = "Materials"
        exportOptions.FileFormat = #ascii
        exportOptions.UseSeparateMaterialLayer = true

        local exportPath = output_prefix + "export_material_separate_files.usda"
        USDExporter.ExportFile exportPath exportOptions:exportOptions
        local stage = pyUsd.Stage.Open(exportPath)

        local shaderPrim = pyShade.Shader.Get stage "/root/Hello/matTeapot001/matTeapot001"
        local connectedSourceTuple = ((shaderPrim.GetInput("diffuseColor")).GetConnectedSource())
        local texturePrim = getTexturePrim (connectedSourceTuple[1])
        local inputFile = (texturePrim.GetInput("file")).Get() as string
        assert_equal ("@" + (pathToForwardSlash baseColorBitmapPath1)+ "@") inputFile

        local shaderPrim = pyShade.Shader.Get stage "/root/World/matTeapot002/matTeapot002"
        local connectedSourceTuple = ((shaderPrim.GetInput("diffuseColor")).GetConnectedSource())
        local texturePrim = getTexturePrim (connectedSourceTuple[1])
        local inputFile = (texturePrim.GetInput("file")).Get() as string
        assert_equal ("@" + (pathToForwardSlash baseColorBitmapPath2)+ "@") inputFile

        local shaderPrim = pyShade.Shader.Get stage "/root/Materials/matTeapot003/matTeapot003"
        local connectedSourceTuple = ((shaderPrim.GetInput("diffuseColor")).GetConnectedSource())
        local texturePrim = getTexturePrim (connectedSourceTuple[1])
        local inputFile = (texturePrim.GetInput("file")).Get() as string
        assert_equal ("@" + (pathToForwardSlash baseColorBitmapPath3)+ "@") inputFile
            
        local shaderPrim = pyShade.Shader.Get stage "/root/Test/matTeapot004/matTeapot004"
        local connectedSourceTuple = ((shaderPrim.GetInput("diffuseColor")).GetConnectedSource())
        local texturePrim = getTexturePrim (connectedSourceTuple[1])
        local inputFile = (texturePrim.GetInput("file")).Get() as string
        assert_equal ("@" + (pathToForwardSlash baseColorBitmapPath4)+ "@") inputFile
			
        local shaderPrim = pyShade.Shader.Get stage "/root/Materials/matTeapot005/matTeapot005"
        local connectedSourceTuple = ((shaderPrim.GetInput("diffuseColor")).GetConnectedSource())
        local texturePrim = getTexturePrim (connectedSourceTuple[1])
        local inputFile = (texturePrim.GetInput("file")).Get() as string
        assert_equal ("@" + (pathToForwardSlash baseColorBitmapPath5)+ "@") inputFile
        assert_equal mtl5.UsdMaterialAttributeHolder.filePath (output_prefix + "custAttrMaterial___.usda")

        layerStack = stage.GetLayerStack(false)
        assert_true (pysdf.Layer.FindOrOpen(exportPath) == layerStack[1])
        assert_true (pysdf.Layer.FindOrOpen(output_prefix + "custAttrMaterial.usda") == layerStack[2])
        assert_true (pysdf.Layer.FindOrOpen(output_prefix + "export_material_separate_files_mtl.usda") == layerStack[3])
    ),
    
    -- Similar to test above, but we the load the resulting layers in memory, and make sure exporting
    -- works as expected.
    function test_export_material_separate_layers_already_loaded = (

        local teapot001 = teapot name:"teapot001"
        local teapot002 = teapot name:"teapot002"

        local baseColorBitmapPath1 = @"C:\folderX\basecolor.png"
        local baseColorBitmapPath2 = @"C:\AnotherfolderX\basecolor.png"
        
        local mtl1 = MaxUsdPreviewSurface()
        mtl1.name = "matTeapot001"
        mtl1.diffuseColor_map = Bitmaptexture fileName:baseColorBitmapPath1

        local mtl2 = MaxUsdPreviewSurface()
        mtl2.name = "matTeapot002"
        mtl2.diffuseColor_map = Bitmaptexture fileName:baseColorBitmapPath2
        
        teapot001.material = mtl1
        teapot002.material = mtl2
        
        select $*
        macros.run "USD" "AddUsdCustomAttrToObjectSelection"
        custAttrFileName = output_prefix + "custAttrMaterial.usda"
        mtl1.UsdMaterialAttributeHolder.pathMethod = 2
        mtl1.UsdMaterialAttributeHolder.separateLayer = true
        mtl1.UsdMaterialAttributeHolder.filePath = custAttrFileName
        mtl1.UsdMaterialAttributeHolder.primPath = "Foo"
        
        local exportOptions = USDExporter.CreateOptions()
        exportOptions.MaterialLayerPath = output_prefix + "splitedMaterial_already_loaded.usda"
        exportOptions.MaterialPrimPath = "mtl"
        exportOptions.FileFormat = #ascii
        exportOptions.UseSeparateMaterialLayer = true
        exportOptions.OpenInUsdView = true
        local exportPath = output_prefix + "test_export_material_separate_layers_already_loaded.usda"
        USDExporter.ExportFile exportPath exportOptions:exportOptions
    
        -- Load the stage and check the result.
        local stage = pyUsd.Stage.Open(exportPath)
        stage.Reload()
        local shaderPrim = pyShade.Shader.Get stage "/root/Foo/matTeapot001/matTeapot001"
        local connectedSourceTuple = ((shaderPrim.GetInput("diffuseColor")).GetConnectedSource())
        local texturePrim = getTexturePrim (connectedSourceTuple[1])
        local inputFile = (texturePrim.GetInput("file")).Get() as string
        assert_equal ("@" + (pathToForwardSlash baseColorBitmapPath1)+ "@") inputFile

        local shaderPrim = pyShade.Shader.Get stage "/root/mtl/matTeapot002/matTeapot002"
        local connectedSourceTuple = ((shaderPrim.GetInput("diffuseColor")).GetConnectedSource())
        local texturePrim = getTexturePrim (connectedSourceTuple[1])
        local inputFile = (texturePrim.GetInput("file")).Get() as string
        assert_equal ("@" + (pathToForwardSlash baseColorBitmapPath2)+ "@") inputFile

        layerStack = stage.GetLayerStack(false)
        assert_true (pysdf.Layer.FindOrOpen(exportPath) == layerStack[1])
        assert_true (pysdf.Layer.FindOrOpen(output_prefix + "custAttrMaterial.usda") == layerStack[2])
        assert_true (pysdf.Layer.FindOrOpen(output_prefix + "splitedMaterial_already_loaded.usda") == layerStack[3])
            
        -- Change some value, and reexport. This validates behavior when target layers are already in memory.
        mtl1.diffuseColor_map = Bitmaptexture fileName:baseColorBitmapPath2
        mtl2.diffuseColor_map = Bitmaptexture fileName:baseColorBitmapPath1    
        -- Also change a target path, this way we make sure the layer is correctly overwritten.
        mtl1.UsdMaterialAttributeHolder.primPath = "Bar"
            
        USDExporter.ExportFile exportPath exportOptions:exportOptions

        local stage = pyUsd.Stage.Open(exportPath)
        stage.Reload()
        
        -- New path... previous prim no longer exists in layer.    
        local shaderPrim = pyShade.Shader.Get stage "/root/Bar/matTeapot001/matTeapot001"
        local previousShaderPrim = stage.GetPrimAtPath "/root/Foo/matTeapot001/matTeapot001"

        assert_false(previousShaderPrim.IsValid())
        local connectedSourceTuple = ((shaderPrim.GetInput("diffuseColor")).GetConnectedSource())
        local texturePrim = getTexturePrim (connectedSourceTuple[1])
        local inputFile = (texturePrim.GetInput("file")).Get() as string
        -- New property value.
        assert_equal ("@" + (pathToForwardSlash baseColorBitmapPath2)+ "@") inputFile

        local shaderPrim = pyShade.Shader.Get stage "/root/mtl/matTeapot002/matTeapot002"
        local connectedSourceTuple = ((shaderPrim.GetInput("diffuseColor")).GetConnectedSource())
        local texturePrim = getTexturePrim (connectedSourceTuple[1])
        local inputFile = (texturePrim.GetInput("file")).Get() as string
        -- New property value.
        assert_equal ("@" + (pathToForwardSlash baseColorBitmapPath1)+ "@") inputFile
        layerStack = stage.GetLayerStack(false)
        assert_true (pysdf.Layer.FindOrOpen(exportPath) == layerStack[1])
        assert_true (pysdf.Layer.FindOrOpen(output_prefix + "custAttrMaterial.usda") == layerStack[2])
        assert_true (pysdf.Layer.FindOrOpen(output_prefix + "splitedMaterial_already_loaded.usda") == layerStack[3])
    ),
    
    -- Test material export scope customization
    -- mtl1 is targeting an absolute scope of "/Hello"
    -- mtl2 is targeting a relative scope of "World"
    -- mtl3 is handled by the export options using an absolute scope of "/Materials"
    function test_export_material_custom_scope = (
        
        local teapot001 = teapot name:"teapot001"
        local teapot002 = teapot name:"teapot002"
        local teapot003 = teapot name:"teapot003"

        local baseColorBitmapPath1 = @"C:\folderX\basecolor.png"
        local baseColorBitmapPath2 = @"C:\AnotherfolderX\basecolor.png"
        local baseColorBitmapPath3 = @"C:\YetAnotherfolderX\basecolor.png"

        local mtl1 = MaxUsdPreviewSurface()
        mtl1.name = "matTeapot001"
        mtl1.diffuseColor_map = Bitmaptexture fileName:baseColorBitmapPath1

        local mtl2 = MaxUsdPreviewSurface()
        mtl2.name = "matTeapot002"
        mtl2.diffuseColor_map = Bitmaptexture fileName:baseColorBitmapPath2

        local mtl3 = MaxUsdPreviewSurface()
        mtl3.name = "matTeapot003"
        mtl3.diffuseColor_map = Bitmaptexture fileName:baseColorBitmapPath3
        
        teapot001.material = mtl1
        teapot002.material = mtl2
        teapot003.material = mtl3
        
        select $*
        macros.run "USD" "AddUsdCustomAttrToObjectSelection"
        mtl1.UsdMaterialAttributeHolder.pathMethod = 2
        mtl2.UsdMaterialAttributeHolder.pathMethod = 2
        mtl1.UsdMaterialAttributeHolder.primPath = "/Hello"
        mtl2.UsdMaterialAttributeHolder.primPath = "World"
        
        local exportOptions = USDExporter.CreateOptions()
        exportOptions.MaterialPrimPath = "/Materials"
        exportOptions.FileFormat = #ascii
        
        local exportPath = output_prefix + "export_material_custom_scope.usda"
        USDExporter.ExportFile exportPath exportOptions:exportOptions
        local stage = pyUsd.Stage.Open(exportPath)

        local shaderPrim = pyShade.Shader.Get stage "/Hello/matTeapot001/matTeapot001"
        local connectedSourceTuple = ((shaderPrim.GetInput("diffuseColor")).GetConnectedSource())
        local texturePrim = getTexturePrim (connectedSourceTuple[1])
        local inputFile = (texturePrim.GetInput("file")).Get() as string
        assert_equal ("@" + (pathToForwardSlash baseColorBitmapPath1)+ "@") inputFile

        local shaderPrim = pyShade.Shader.Get stage "/root/World/matTeapot002/matTeapot002"
        local connectedSourceTuple = ((shaderPrim.GetInput("diffuseColor")).GetConnectedSource())
        local texturePrim = getTexturePrim (connectedSourceTuple[1])
        local inputFile = (texturePrim.GetInput("file")).Get() as string
        assert_equal ("@" + (pathToForwardSlash baseColorBitmapPath2)+ "@") inputFile

        local shaderPrim = pyShade.Shader.Get stage "/Materials/matTeapot003/matTeapot003"
        local connectedSourceTuple = ((shaderPrim.GetInput("diffuseColor")).GetConnectedSource())
        local texturePrim = getTexturePrim (connectedSourceTuple[1])
        local inputFile = (texturePrim.GetInput("file")).Get() as string
        assert_equal ("@" + (pathToForwardSlash baseColorBitmapPath3)+ "@") inputFile
    ),
	
	-- Test the full path of the material scope to see if the root and mtl paths are respected
	function test_export_material_scope_path = (
            local tp = teapot name:"tp"
            local mtl = MaxUsdPreviewSurface()
            local baseColorBitmapPath1 = @"C:\folderX\basecolor.png"
            
            mtl.name = "matTeapot"
            mtl.diffuseColor_map = Bitmaptexture fileName:baseColorBitmapPath1
            tp.material = mtl
            
            local exportOptions = USDExporter.CreateOptions()
            exportOptions.MaterialPrimPath = "materials"
            exportOptions.FileFormat = #ascii
            
            local exportPath = output_prefix + "export_material_scope_path.usda"
            USDExporter.ExportFile exportPath exportOptions:exportOptions
            local stage = pyUsd.Stage.Open(exportPath)
            
            local mtlPrim = stage.GetPrimAtPath("/root/materials")
            assert_true (pyUsd.Prim.IsValid(mtlPrim))
	    assert_true (mtlPrim.IsA(pyUsdGeom.Scope))
	),
	
	-- Test that we do not create the mtl scope if there's no materials to export
	function test_export_material_no_scope = (
            local exportOptions = USDExporter.CreateOptions()
            exportOptions.FileFormat = #ascii
            
            local exportPath = output_prefix + "export_material_no_scope.usda"
            USDExporter.ExportFile exportPath exportOptions:exportOptions
            local stage = pyUsd.Stage.Open(exportPath)
            
            local mtlScopePrim = stage.GetPrimAtPath("/root/mtl")
            assert_false (pyUsd.Prim.IsValid(mtlScopePrim))
	),
	
    
    function teardown = (
        pyUsdMaterialWriter._material_export_options["relative_texture_paths"] = true
    ),

    Tests = #(
	test_export_previewsurface_material_values,
        test_export_single_material_bindings,
        test_export_single_material_bindings_with_exported_object_transform,
        test_export_hierarchy_material_bindings,
        test_export_multi_material_bindings,
        test_export_multi_material_bindings_with_exported_object_transform,
        test_export_multi_material_only_one_matid_used,
        test_export_multi_material_missing_slots,
        test_export_multi_material_unused_mtlId,
        test_export_multi_material_on_instances_with_same_material,
        test_export_simple_material_on_instances_with_same_material,
        test_export_material_on_instances_with_different_materials,
        test_export_material_on_instances_with_different_materials_when_only_one_matId_used,
        test_export_material_on_single_selected_instance,
        test_export_instance_with_object_transform,
        test_export_physical_material_values,
        test_export_physical_material_bitmap_maps,
        test_multi_channel_texture_material_connections,
        test_consecutive_material_export,
        test_shared_texture_map,
        test_export_unknown_material_to_dummy,
        test_material_name_conflict,
        test_export_multimaterial_ids_overflow,
        test_export_multimaterial_single_id_overflow,
        test_export_multimaterial_ids_mismatch_overflow,
        test_multi_material_targets,
        test_export_material_separate_files,
        test_export_material_separate_layers_already_loaded,
        test_export_material_custom_scope,
        test_export_material_scope_path,
        test_export_material_no_scope
    )
)

runUsdTestFixture export_material_test (getThisScriptFilename())