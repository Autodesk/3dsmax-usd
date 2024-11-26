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

output_prefix = pathConfig.appendPath (getDir #temp) "/EXPORT_GEOMETRY_TEST_"
deleteTestFiles(output_prefix + "*.*")

struct export_geometry_test
(  
    private 
    exportOptions = USDExporter.CreateOptions(),
    
    public 
    function setup =
    (
        resetmaxfile #noprompt
        exportOptions.FileFormat = #ascii
        exportOptions.RootPrimPath = "/"
        exportOptions.UsdStagesAsReferences = false
    ),
    
    -- Test that the edge visibility of triangular meshes is adjusted for export so that
    -- the conversion to poly part of the USD export produces the right results if we are exporting
    -- poly meshes.
    function export_triobj_with_bad_edgevis_test = (
        local boxNode = box()
        boxNode.name = "box"
        local boxTriMesh = convertToMesh boxNode
        -- Set all edge invisible. This would usually break the conversion to Poly that happens 
        -- on export without additional work (which is what we are testing here!)
        for i = 1 to boxTriMesh.numfaces do (
            setEdgeVis boxTriMesh i 1 false
            setEdgeVis boxTriMesh i 2 false
            setEdgeVis boxTriMesh i 3 false
        )
        local exportPath = output_prefix + "test_hidden_tri_edges.usd"
        exportOptions.MeshFormat = #polyMesh
        USDExporter.ExportFile exportPath exportOptions:exportOptions
        local stage = pyUsd.Stage.Open(exportPath)
        local boxPrim = stage.GetPrimAtPath("/" + boxNode.name)
        assert_true (pyUsd.Prim.IsValid(boxPrim)) message:"The box was not properly exported to a USD prim."
        assert_true (boxPrim.IsA(pyUsdGeom.Mesh)) message:"The box should have been exported to a mesh prim."

        -- Make sure that prior to export, "corner" edges were set to visible.
        local mesh = pyUsdGeom.Mesh(boxPrim)
        local vertexCounts = ((boxPrim.GetAttribute("faceVertexCounts")).Get())
        assert_equal 6 vertexCounts.count messsage:"The box should have been exported to 6 faces."
            
        -- Make sure this was done in a copy, and did not affect the mesh in the scene.
        local edgesShouldBeHiddenError = "The mesh's edges should still be hidden"
        for i = 1 to boxTriMesh.numfaces do (
            assert_false (getEdgeVis boxTriMesh i 1) message:edgesShouldBeHiddenError
            assert_false (getEdgeVis boxTriMesh i 2) message:edgesShouldBeHiddenError
            assert_false (getEdgeVis boxTriMesh i 3) message:edgesShouldBeHiddenError
        )
    ),

    -- Make sure that all of max's geometric objects, which can be converted to either 
    -- triangular meshes or poly meshes are exported to USD.
    function export_all_geometric_types_test = (
        local allObjects = #()
        allObjects = for o in GeometryClass.classes where o.creatable collect o

        local expectedMeshObjects = #()
        for i = 1 to allObjects.count do
        (
            -- Align the objects in a grid for easy visual inspection...
            local x = (mod i 10) * 100
            local y = ((i / 10) as integer) * 100
            local obj = (allObjects[i] height:10 length:10 width:10)
            obj.pos.x = x
            obj.pos.y = y
            obj.pos.z = 0
    
            if ((canConvertTo obj Editable_mesh) or (canConvertTo obj Editable_poly)) then (
                append expectedMeshObjects obj                
            )
        )
        
        local exportPath = output_prefix + "export_all_geometric_objects.usd"
        USDExporter.ExportFile exportPath exportOptions:exportOptions
        local stage = pyUsd.Stage.Open(exportPath)
        root = stage.GetPrimAtPath("/")
        assert_equal expectedMeshObjects.count (root.GetAllChildren()).count message:"Unexpected prim count when exporting all geometric objects."
    
        for i = 1 to expectedMeshObjects.count do (
            local name = expectedMeshObjects[i].name
            local objPrim = stage.GetPrimAtPath("/" + pyUsdTf.MakeValidIdentifier(name))
            assert_true (pyUsd.Prim.IsValid(objPrim)) message:(name + " of class " + ((classOf expectedMeshObjects[i]) as string) + " was not properly exported to a USD mesh.")
            assert_true (objPrim.IsA(pyUsdGeom.Mesh)) message:(name + " of class " + ((classOf expectedMeshObjects[i]) as string) + " should have been exported to a mesh type prim.")
        )
    ),

    function teardown = (
    ),

    Tests = #(export_triobj_with_bad_edgevis_test,
        export_all_geometric_types_test)
)

runUsdTestFixture export_geometry_test (getThisScriptFilename())