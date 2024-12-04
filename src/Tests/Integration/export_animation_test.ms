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
pySdf = python.import("pxr.Sdf")
pyGf = python.import("pxr.Gf")
pyVt = python.import("pxr.Vt")

output_prefix = pathConfig.appendPath (getDir #temp) "/EXPORT_ANIMATION_TEST_"
deleteTestFiles(output_prefix + "*.*")

struct export_animation_test
(    
    public 
    function setup =
    (
        resetmaxfile #noprompt
    ),
    
    function test_animation_metadata_export = (
        local cube = Box name:"cube"
        
        local exportOptions = USDExporter.CreateOptions()
        exportOptions.FileFormat = #ascii
        exportOptions.RootPrimPath = "/"
        
        frameRates = #(1, 10, 15, 30)
        startFrames = #(-5.5, 0.0, 1.5, 5.0)
        endFrames = #(-1.0, 10, 3.0, 15.0)
        exportOptions.TimeMode = #frameRange

        for i in 1 to 4 do (
            frameRate = frameRates[i]
            local exportPath = output_prefix + "animation_metadata_" + i as string + ".usda"
            exportOptions.StartFrame = startFrames[i]
            exportOptions.EndFrame = endFrames[i]
            USDExporter.ExportFile exportPath exportOptions:exportOptions

            local stage = pyUsd.Stage.Open(exportPath)
        
            assert_equal startFrames[i] (stage.GetStartTimeCode()) message:"Unexpected start timeCode set in the USD stage."
            assert_equal endFrames[i] (stage.GetEndTimeCode()) message:"Unexpected end timeCode set in the USD stage."
            
            assert_equal frameRate (stage.GetTimeCodesPerSecond()) message:"Unexpected \"timeCodes per second\" set in the USD stage."
            assert_equal frameRate (stage.GetFramesPerSecond()) message:"Unexpected \"frames per second\" set in the USD stage."
        )
    ),
    
    function test_simple_object_validity_interval = (
        local exportOptions = USDExporter.CreateOptions()
        exportOptions.FileFormat = #ascii
        exportOptions.RootPrimPath = "/"
        
        local nodeName = "cube"
        local cube = Box name:"cube"
        
        convertToPoly cube
        -- Select top face.
        polyop.setFaceSelection cube  #{2}
    
        with animate on 
        (
            -- Animate the geometry : moving the top face.
            at time 10 move cube.selectedFaces [0,0,10]
        )
        
        -- We have 3 meaningful intervals : 
        -- Object is valid in [-infinity, 0]
        -- Object changes every frame in [0,10]
        -- Object is valid [ 10, infinity]
        
        -- Export from -5 to 15.
        local exportPath = output_prefix + "validity_interval.usda"
        exportOptions.TimeMode = #frameRange
        exportOptions.StartFrame = -5
        exportOptions.EndFrame = 15
        exportOptions.SamplesPerFrame = 1.0
        USDExporter.ExportFile exportPath exportOptions:exportOptions
        
        local stage = pyUsd.Stage.Open(exportPath)
        usdCube = stage.GetPrimAtPath("/cube")
        
        local xformable = pyUsdGeom.Xformable(usdCube)
        local mesh = pyUsdGeom.Mesh(usdCube)
        local points = usdCube.GetAttribute("points")    
        
        for timeCode = exportOptions.StartFrame to exportOptions.EndFrame do (
            -- Test that the geometry was animated correctly. 
            local vertices = (points).Get(timeCode)
            for i = 5 to 8 do (
                local posZ = at time timeCode in coordsys local cube.max[3]
                assert_equal posZ vertices[i][3] message:("The cube's top face position is wrong t = " + timeCode as string)
            )
        )
        
        -- From the validity intervals, we should expect 11 authored values. 
        -- Indeed, the mesh doesnt change in [-infinity, 0] nor [10, infinity]
        -- This means that with 1 sample per frame, we will have timeCodes 0,1,2,3,4,5,6,7,8,9,10 authored.
        local timeSamples = points.GetTimeSamples()
        assert_equal 11 timeSamples.Count message:"Unexpected number of timeSamples authored."
        local expectedSamples = #(0.0,1.0,2.0,3.0,4.0,5.0,6.0,7.0,8.0,9.0,10.0)
        for i in 1 to 11 do (
            assert_equal expectedSamples[i] timeSamples[i] message:("Unexpected time sample exported : " + timeSamples[i] as string)
        )
    ),
    
    function get_mesh_attr_samples stage attrName primPath = (
        prim = stage.GetPrimAtPath(primPath)
        local attr = prim.GetAttribute(attrName)
        return attr.GetTimeSamples()
    ),
    
    function validate_time_samples expectedSamples actualSamples = (
        assert_equal expectedSamples.Count actualSamples.Count
        for i in 1 to expectedSamples.Count do (
            assert_equal expectedSamples[i] actualSamples[i]
        )        
    ),
    
    function test_multiple_validity_intervals = (
        local exportOptions = USDExporter.CreateOptions()
        exportOptions.FileFormat = #ascii
        exportOptions.RootPrimPath = "/"
        
        local nodeName = "cube"
        local cube = Box name:"cube"
        
        convertToPoly cube
        -- Select top face.
        polyop.setFaceSelection cube  #{2}
    
        noiseMod = Noisemodifier ()
        addModifier cube noiseMod
        with animate on 
        (
            -- Animate the geometry using noise in [0,10] and [20,30].
            at time 10 noiseMod.strength = [0,0,100.0]
            at time 20 noiseMod.strength = [0,0,100.0]
            at time 30 noiseMod.strength = [0,0,1000.0]
        )
        
        -- We got 5 meaningful intervals : 
        -- Object doesn't change in [-infinity, 0]
        -- Object changes every frame in [0,10]
        -- Object doesn't change in [10, 20]
        -- Object changes every frame in [20, 30]
        -- Object doesn't change in [30, infinity]
        
        -- Export from -5 to 35. The entire animation, going over the range on both ends.
        local exportPath = output_prefix + "validity_multiple_intervals.usda"
        exportOptions.TimeMode = #frameRange
        exportOptions.StartFrame = -5
        exportOptions.EndFrame = 35
        exportOptions.SamplesPerFrame = 1.0
        USDExporter.ExportFile exportPath exportOptions:exportOptions
        
        local stage = pyUsd.Stage.Open(exportPath)
        stage.Reload()
        
        local timeSamples = get_mesh_attr_samples stage "points" "/cube" 
        -- From these validity intervals, we should expect 22 authored values, at 1 sample per frame.
        local expectedSamples = #(0.0,1.0,2.0,3.0,4.0,5.0,6.0,7.0,8.0,9.0,10.0,20.0,21.0,22.0,23.0,24.0,25.0,26.0,27.0,28.0,29.0,30.0)
        validate_time_samples expectedSamples timeSamples
        
        
        -- Test other "limit" cases where we restrict the animation export within different interval types.        
        
        
        -- Export from 10 to 20, within a single interval.
        local exportPath = output_prefix + "test_exported_frames_from_intervals_1.usda"
        exportOptions.TimeMode = #frameRange
        exportOptions.StartFrame = 10
        exportOptions.EndFrame = 20
        exportOptions.SamplesPerFrame = 1.0
        USDExporter.ExportFile exportPath exportOptions:exportOptions
        
        local stage = pyUsd.Stage.Open(exportPath)
        stage.Reload()
        
        local timeSamples = get_mesh_attr_samples stage "points" "/cube"
        --In this range, nothing changes, we should only require one time sample.
        local expectedSamples = #(10.0)
        validate_time_samples expectedSamples timeSamples
        
        -- Export from 28 to 32 - from data changing every frame, to the infinite interval after 30.
        local exportPath = output_prefix + "test_exported_frames_from_intervals_2.usda"
        exportOptions.TimeMode = #frameRange
        exportOptions.StartFrame = 28
        exportOptions.EndFrame = 32
        exportOptions.SamplesPerFrame = 1.0
        USDExporter.ExportFile exportPath exportOptions:exportOptions
        
        local stage = pyUsd.Stage.Open(exportPath)
        stage.Reload()
        
        local timeSamples = get_mesh_attr_samples stage "points" "/cube"
        -- Should only require 3 frames, up to 30.
        local expectedSamples = #(28.0, 29.0, 30.0)
        validate_time_samples expectedSamples timeSamples        
    
        -- Export from 25 to 28 - from data changing every frame.
        local exportPath = output_prefix + "test_exported_frames_from_intervals_3.usda"
        exportOptions.TimeMode = #frameRange
        exportOptions.StartFrame = 25
        exportOptions.EndFrame = 28
        exportOptions.SamplesPerFrame = 1.0
        USDExporter.ExportFile exportPath exportOptions:exportOptions
        
        local stage = pyUsd.Stage.Open(exportPath)
        stage.Reload()
        
        local timeSamples = get_mesh_attr_samples stage "points" "/cube"
        -- Should require all frames between the start/end.
        local expectedSamples = #(25.0, 26.0, 27.0, 28.0)
        validate_time_samples expectedSamples timeSamples                
        
        -- Same test, but with start/end and sample rate that do not match the interval boundaries exactly.
        local exportPath = output_prefix + "test_exported_frames_from_intervals_4.usda"
        exportOptions.TimeMode = #frameRange
        exportOptions.StartFrame = 22.25
        exportOptions.EndFrame = 28.75
        exportOptions.SamplesPerFrame = 0.75
        USDExporter.ExportFile exportPath exportOptions:exportOptions
        
        local stage = pyUsd.Stage.Open(exportPath)
        stage.Reload()
        
        local timeSamples = get_mesh_attr_samples stage "points" "/cube"
        -- Should have the start/end times as samples. And matching the proper sample rate in between.
        local expectedSamples = #(22.25, 23.58125, 24.9125, 26.24375, 27.575, 28.75)
        validate_time_samples expectedSamples timeSamples        
    ),
       
    function test_animate_all_geom_channels = (
        
        local exportOptions = USDExporter.CreateOptions()
        exportOptions.FileFormat = #ascii
        exportOptions.RootPrimPath = "/"
        
        local aSphere = Sphere name:"sphere"
        
        uvMap = UVWmap()
        addModifier aSphere uvMap
        
        with animate on 
        (
            -- Animate the entire topology.
            at time 5 aSphere.segments = 8
            at time 10.5 aSphere.segments  = 8 
            at time 15 aSphere.segments  = 4
        )
        
        local exportPath = output_prefix + "test_animate_all_geom_channels.usda"
        exportOptions.TimeMode = #frameRange
        exportOptions.StartFrame = -3
        exportOptions.EndFrame = 35
        exportOptions.SamplesPerFrame = 1.0
        USDExporter.ExportFile exportPath exportOptions:exportOptions
        
        local stage = pyUsd.Stage.Open(exportPath)
        stage.Reload()
        
        local primPath = "/sphere"
        
        -- Expected samples where a geometry channel's validity never stays valid for longer than 
        -- the object's validity interval.
        local fullSamples = #(0.0, 1.0, 2.0, 3.0, 4.0, 5.0,10.5, 11.5, 12.5, 13.5, 14.5, 15.0)
        
        -- Extents, all animated frames have samples.
        local extentsTimeSamples = get_mesh_attr_samples stage "extent" primPath
        validate_time_samples extentsTimeSamples fullSamples        
        
        -- The topology, all animated frames have samples.
        local faceVertexIndicesSamples = get_mesh_attr_samples stage "faceVertexIndices" primPath
        validate_time_samples faceVertexIndicesSamples fullSamples
        local faceVertexCountsSamples = get_mesh_attr_samples stage "faceVertexCounts" primPath
        validate_time_samples faceVertexCountsSamples fullSamples
        
        -- Points, all animated frames have samples.
        local pointTimeSamples = get_mesh_attr_samples stage "points" primPath
        validate_time_samples pointTimeSamples fullSamples
        
        -- Normals, all animated frames have samples.
        local normalsTimeSamples = get_mesh_attr_samples stage "primvars:normals" primPath
        validate_time_samples normalsTimeSamples fullSamples
        
        -- UVs, all animated frames have samples.
        local uvSamples = get_mesh_attr_samples stage "primvars:st" primPath
        validate_time_samples uvSamples fullSamples
    ),
    
    function test_animated_topology_only = (
        
        local exportOptions = USDExporter.CreateOptions()
        exportOptions.FileFormat = #ascii
        exportOptions.RootPrimPath = "/"
        
        local aSphere = Sphere name:"sphere"
        
        -- UVWMap just so we have some UVs to export.
        uvMap = UVWmap()
        addModifier aSphere uvMap        
        
        -- Material modifier, when animating the materialId, only invalidates the topology.
        matMod = MaterialModifier()
        addModifier aSphere matMod
        
        with animate on 
        (
            at time 5 matMod.materialId = 10
            at time 10.5 matMod.materialId = 10 
            at time 15 matMod.materialId = 20
        )
        
        local exportPath = output_prefix + "test_animated_topology_only.usda"
        exportOptions.TimeMode = #frameRange
        exportOptions.StartFrame = -3
        exportOptions.EndFrame = 35
        exportOptions.SamplesPerFrame = 1.0
        USDExporter.ExportFile exportPath exportOptions:exportOptions
        
        local stage = pyUsd.Stage.Open(exportPath)
        stage.Reload()
        
        local primPath = "/sphere"
        
        -- Expected samples where a geometry channel's validity never stays valid for longer than 
        -- the object's validity interval.
        local fullSamples = #(0.0, 1.0, 2.0, 3.0, 4.0, 5.0,10.5, 11.5, 12.5, 13.5, 14.5, 15.0)
        
        -- Extents, all animated frames have samples.
        local extentsTimeSamples = get_mesh_attr_samples stage "extent" primPath
        validate_time_samples extentsTimeSamples fullSamples
        
        -- The topology, all animated frames have samples.
        local faceVertexIndicesSamples = get_mesh_attr_samples stage "faceVertexIndices" primPath
        validate_time_samples faceVertexIndicesSamples fullSamples
        local faceVertexCountsSamples = get_mesh_attr_samples stage "faceVertexCounts" primPath
        validate_time_samples faceVertexCountsSamples fullSamples
        
        -- Points : Do not depend on topology, non-animated
        local pointTimeSamples = get_mesh_attr_samples stage "points" primPath
        validate_time_samples pointTimeSamples #(0)
        
        -- Normals : can depend on the topology channel, animated.
        local normalsTimeSamples = get_mesh_attr_samples stage "primvars:normals" primPath
        local expectedNormalsSamples = #(0.0, 1.0, 2.0, 3.0, 4.0, 5.0,10.5, 11.5, 12.5, 13.5, 14.5, 15.0)
        validate_time_samples normalsTimeSamples fullSamples
        
        -- UVs : can depend on topology, animated.
        local uvSamples = get_mesh_attr_samples stage "primvars:st" primPath
        validate_time_samples uvSamples fullSamples
    ),
    
    function test_animated_verts_only = (
        local exportOptions = USDExporter.CreateOptions()
        exportOptions.FileFormat = #ascii
        exportOptions.RootPrimPath = "/"
        
        local nodeName = "cube"
        local cube = Box name:"cube"
        
        convertToPoly cube
        polyop.defaultMapFaces cube 1
        
        bendModifier = BendMod ()
        addModifier cube bendModifier
        with animate on 
        (
            -- Animate the verts/normals
            at time 5 bendModifier.angle = 45
            at time 10.5 bendModifier.angle = 45 -- Purposefully animated at a time between two "normal" sampling steps.
            at time 15 bendModifier.angle = 90
        )
        

        local exportPath = output_prefix + "test_animated_verts_only.usda"
        exportOptions.TimeMode = #frameRange
        exportOptions.StartFrame = -5
        exportOptions.EndFrame = 35
        exportOptions.SamplesPerFrame = 1.0
        USDExporter.ExportFile exportPath exportOptions:exportOptions
        local stage = pyUsd.Stage.Open(exportPath)
        stage.Reload()
        
        local primPath = "/cube"
        
        -- Expected samples where a geometry channel's validity never stays valid for longer than 
        -- the object's validity interval.
        local fullSamples = #(0.0, 1.0, 2.0, 3.0, 4.0, 5.0,10.5, 11.5, 12.5, 13.5, 14.5, 15.0)
        
        -- Extents, all animated frames have samples.
        local extentsTimeSamples = get_mesh_attr_samples stage "extent" primPath
        validate_time_samples extentsTimeSamples fullSamples        
        
        -- The topology doesnt change. 
        -- Topology attributes : faceVertexCounts & faceVertexIndices 
        -- Only 0.0 is exported, not animated in 3dsMax.
        local expectedTopoSamples= #(0.0)
        local faceVertexIndicesSamples = get_mesh_attr_samples stage "faceVertexIndices" primPath
        validate_time_samples faceVertexIndicesSamples #(0.0)
        local faceVertexCountsSamples = get_mesh_attr_samples stage "faceVertexCounts" primPath
        validate_time_samples expectedTopoSamples #(0.0)
        
        -- Points, all animated frames have samples.
        local pointTimeSamples = get_mesh_attr_samples stage "points" primPath
        validate_time_samples pointTimeSamples fullSamples
        
        -- Normals, all animated frames have samples.
        local normalsTimeSamples = get_mesh_attr_samples stage "primvars:normals" primPath
        validate_time_samples normalsTimeSamples fullSamples
        
        -- UVs do not change.
        local uvSamples = get_mesh_attr_samples stage "primvars:st" primPath
        validate_time_samples uvSamples #(0.0)
    ),    
    
    -- Generally speaking we use geom channel validity intervals when exporting.
    -- However, if the export process makes changes to the geometry for export (concave/curved/invalid geom), 
    -- we can no longer rely on them. From that point on, within that interval, use
    -- the object validity interval.
    function test_geom_validity_intervals_ignored_on_split_geometry = (
    
        -- Create the following scenario to test this case.
        -- A cube with a twist modifier. Export it with "PreserveEdgeOrientation" - this way, when twisted
        -- faces are split into triangles for the purpose of export and we can't rely on validity intervals.        
        
        local cube = Box name:"cube"
        twistModifier = Twist()
        addModifier cube twistModifier
        addModifier cube (Turn_to_Poly())
        
        with animate on 
        (
            at time 0 twistModifier.angle = 0 -- Twisted from 0 to 5
            at time 3 twistModifier.angle = 45
            at time 5 twistModifier.angle = 0 -- No twist from 5 to 10
            at time 10 twistModifier.angle = 0 -- Twist again from 10 to 15
            at time 15 twistModifier.angle = 45
        )
        
        local exportPath = output_prefix + "test_geom_validity_intervals_ignored_on_split_geometry.usda"
        local exportOptions = USDExporter.CreateOptions()
        exportOptions.TimeMode = #frameRange
        exportOptions.FileFormat = #ascii
        exportOptions.StartFrame = 0
        exportOptions.EndFrame = 20
        exportOptions.RootPrimPath = "/"
        exportOptions.PreserveEdgeOrientation = true
        
        USDExporter.ExportFile exportPath exportOptions:exportOptions
        
        local stage = pyUsd.Stage.Open(exportPath)
        stage.Reload()        
        local primPath = "/cube"
        
        -- We therefor expect the following time samples : 
        -- 0-5 -> Topo doesnt change on the max side, but we change it for export -> 5 samples.
        -- 5-10 -> Topo doesnt change on the max side, exported as is -> 2 samples (start/end of interval)
        -- 10-15 -> Topo doesnt change on the max side, but we change it for export -> 5 samples.
        
        local expected = #(0.0, 1.0, 2.0, 3.0, 4.0, 5.0,10.0, 11.0, 12.0, 13.0, 14.0, 15.0)
        
        local faceVertexIndicesSamples = get_mesh_attr_samples stage "faceVertexIndices" primPath        
        validate_time_samples expected faceVertexIndicesSamples
        local faceVertexCountsSamples = get_mesh_attr_samples stage "faceVertexCounts" primPath
        validate_time_samples expected faceVertexCountsSamples
    ),
    
    -- Similar to test_geom_validity_intervals_ignored_on_split_geometry.
    -- Here, we make sure that cleaned invalid geometry also causes object level validity 
    -- intervals to be used.
    function test_geom_validity_intervals_ignored_on_cleaned_geometry = (
                
        local quad = plane name:"quad" width:10 length:10 widthsegs:1 lengthsegs:1
        convertToMesh quad
        
        -- Setup an invalid face (all indices the same)
        setFace quad 1 1 1 1
        
        -- Animate a vert over 5 fames (object validity interval will be 'instant' over those frames)
        with animate on 
        (
            at time 5 meshop.moveVert quad #{1} [0,0,10]
        )
                    
        local exportPath = output_prefix + "test_geom_validity_intervals_ignored_on_cleaned_geometry.usda"
        local exportOptions = USDExporter.CreateOptions()
        exportOptions.TimeMode = #frameRange
        exportOptions.FileFormat = #ascii
        exportOptions.StartFrame = -5 --purposefully start/end before/after the object anim range.
        exportOptions.EndFrame = 10 
        exportOptions.RootPrimPath = "/"
        
        USDExporter.ExportFile exportPath exportOptions:exportOptions
        
        local stage = pyUsd.Stage.Open(exportPath)
        stage.Reload()        
        local primPath = "/quad"
        
        -- Mesh is cleaned on the first frame, validity interval of the object is used instead of the topo
        -- interval of the mesh.
        local expected = #(0.0, 1.0, 2.0, 3.0, 4.0, 5)
        
        local faceVertexIndicesSamples = get_mesh_attr_samples stage "faceVertexIndices" primPath        
        validate_time_samples expected faceVertexIndicesSamples
        local faceVertexCountsSamples = get_mesh_attr_samples stage "faceVertexCounts" primPath
        validate_time_samples expected faceVertexCountsSamples
    ),
    
    -- If we are converting to poly at export time, we can only use instant validity intervals.
    -- Indeed, the geom channels intervals can't be used, as the converted topology can vary frame to frame
    -- independently from the source topology. Prior to conversion to poly, for the purpose of export, we
    -- show edges between triangles that are not coplanar, this is to avoid bad looking results with curved
    -- faces. The result of that operation can change as the mesh is animated - making max's intervals meaningless.
    -- This test make sure we dont rely object geom channel intervals in those cases.
    function test_geom_validity_intervals_convert_tri_to_poly = (
        local cube = box name:"cube" width:10 length:10 widthsegs:1 lengthsegs:1
        convertToMesh cube
        with animate on 
        (
            at time 5 meshop.moveVert cube #{1} [0,0,10]
        )
        
        local exportPath = output_prefix + "test_geom_validity_intervals_convert_tri_to_poly.usda"
        local exportOptions = USDExporter.CreateOptions()
        exportOptions.TimeMode = #frameRange
        exportOptions.FileFormat = #ascii
        exportOptions.StartFrame = -5 --purposefully start/end before/after the object anim range.
        exportOptions.EndFrame = 10 
        exportOptions.RootPrimPath = "/"
        exportOptions.MeshFormat = #polyMesh
        
        USDExporter.ExportFile exportPath exportOptions:exportOptions
        
        local stage = pyUsd.Stage.Open(exportPath)
        stage.Reload()
        local primPath = "/cube"
        
        -- Conversion to poly -> ignore geom channel intervals, instead use the object's intervals : 
        -- expect samples 0-5.
        local expected = #(0.0, 1.0, 2.0, 3.0, 4.0, 5)
        
        local faceVertexIndicesSamples = get_mesh_attr_samples stage "faceVertexIndices" primPath        
        validate_time_samples expected faceVertexIndicesSamples
        local faceVertexCountsSamples = get_mesh_attr_samples stage "faceVertexCounts" primPath
        validate_time_samples expected faceVertexCountsSamples
    ),    
    
    function test_animated_uvs_only = (
        
        local exportOptions = USDExporter.CreateOptions()
        exportOptions.FileFormat = #ascii
        exportOptions.RootPrimPath = "/"
        
        local cube = Box name:"cube"
        
        uvMap = Uvwmap()
        addModifier cube uvMap
        
        with animate on 
        (
            -- Animate the entire topology.
            at time 5 uvMap.Utile = 2.0
            at time 10.5 uvMap.Utile = 2.0 
            at time 15 uvMap.Utile = 4.0
        )
        
        local exportPath = output_prefix + "test_animated_uvs_only.usda"
        exportOptions.TimeMode = #frameRange
        exportOptions.StartFrame = -3
        exportOptions.EndFrame = 35
        exportOptions.SamplesPerFrame = 1.0
        USDExporter.ExportFile exportPath exportOptions:exportOptions
        
        local stage = pyUsd.Stage.Open(exportPath)
        stage.Reload()
        
        local primPath = "/cube"
        
        -- Expected samples where a geometry channel's validity never stays valid for longer than 
        -- the object's validity interval.
        local fullSamples = #(0.0, 1.0, 2.0, 3.0, 4.0, 5.0,10.5, 11.5, 12.5, 13.5, 14.5, 15.0)
        
        -- Extents, non-animated.
        local extentsTimeSamples = get_mesh_attr_samples stage "extent" primPath
        validate_time_samples extentsTimeSamples #(0.0)        
        
        -- Topology : non-animated.
        local faceVertexIndicesSamples = get_mesh_attr_samples stage "faceVertexIndices" primPath
        validate_time_samples faceVertexIndicesSamples #(0.0)
        local faceVertexCountsSamples = get_mesh_attr_samples stage "faceVertexCounts" primPath
        validate_time_samples faceVertexCountsSamples #(0.0)
        
        -- Points : non-animated.
        local pointTimeSamples = get_mesh_attr_samples stage "points" primPath
        validate_time_samples pointTimeSamples #(0.0)
        
        -- Normals : non-animated.
        local normalsTimeSamples = get_mesh_attr_samples stage "primvars:normals" primPath
        local expectedNormalsSamples = #(0.0, 1.0, 2.0, 3.0, 4.0, 5.0,10.5, 11.5, 12.5, 13.5, 14.5, 15.0)
        validate_time_samples normalsTimeSamples #(0.0)
        
        -- UVs : animated.
        local uvSamples = get_mesh_attr_samples stage "primvars:st" primPath
        validate_time_samples uvSamples fullSamples
    ),    
    
    function test_animated_transforms_intervals = (
        local exportOptions = USDExporter.CreateOptions()
        exportOptions.FileFormat = #ascii
        exportOptions.RootPrimPath = "/"
        
        local nodeName = "cube"
        local cube = Box name:"cube"
               
        convertToPoly cube
        -- Select top face.
        polyop.setFaceSelection cube  #{2}
    
        with animate on 
        (
            -- Animate the geometry : moving the top face.
            at time 5.5 move cube.selectedFaces [0,0,10]                
            -- Move the cube 10 units over 10 frames.
            at time 10 move cube [0,0,10]
        )
        
        local exportPath = output_prefix + "test_animated_transforms_intervals.usda"
        exportOptions.TimeMode = #frameRange
        exportOptions.StartFrame = -5
        exportOptions.EndFrame = 15
        exportOptions.SamplesPerFrame = 1.0
        USDExporter.ExportFile exportPath exportOptions:exportOptions
        
        local stage = pyUsd.Stage.Open(exportPath)
        local usdCube = stage.GetPrimAtPath("/cube")
        local xFormPrim = pyUsdGeom.Xform(usdCube)
        local xformOps = xFormPrim.GetOrderedXformOps()
        
        local timeSamples = (xformOps[1]).GetTimeSamples()
        local expectedSamples = #(-5.0, -4.0, -3.0, -2.0, -1.0, 0.0, 1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0, 9.0, 10.0, 11.0, 12.0, 13.0, 14.0, 15.0)
        validate_time_samples expectedSamples timeSamples
    ),
        
    -- Make sure that in animation, identity transforms are exported, while this is superfluous for 
    -- single frames, in animation, we need those samples to get the right interpolations if the 
    -- transform changes over time.
    function test_animated_transforms_are_exported_even_if_identity = (
        local exportOptions = USDExporter.CreateOptions()
        exportOptions.FileFormat = #ascii
        exportOptions.RootPrimPath = "/"
        
        local nodeName = "cube"
        local cube = Box name:"cube"
        
        with animate on 
        (
            -- Move the cube 10 units over 10 frames.
            at time 10 move cube [0,0,10]
        )
        
        local exportPath = output_prefix + "test_animated_transforms_identity.usda"
        exportOptions.TimeMode = #frameRange
        exportOptions.StartFrame = 0
        exportOptions.EndFrame = 5
        exportOptions.SamplesPerFrame = 1.0
        USDExporter.ExportFile exportPath exportOptions:exportOptions
        
        local stage = pyUsd.Stage.Open(exportPath)
        local usdCube = stage.GetPrimAtPath("/cube")
        local xFormPrim = pyUsdGeom.Xform(usdCube)
        local xformOps = xFormPrim.GetOrderedXformOps()
        
        assert_equal 0.0 ((xformOps[1]).GetTimeSamples())[1] message:"No time sample was exported at time 0.0."
    ),
    
    function test_interpolation_scheme_inference_disabled_in_animation = (
        local planeNode = Plane name:"plane"
        
        local exportOptions = USDExporter.CreateOptions()
        exportOptions.FileFormat = #ascii
        exportOptions.RootPrimPath = "/"
        
        convertToPoly planeNode        
        with animate on 
        (
            -- Move the plane 10 units over 4 frames.
            at time 4 polyop.moveVert planeNode 1 [0,0,10]
        )
        
        -- When exporting a single frame, the primvar interpolation scheme is inferred at export time.
        local exportPath = output_prefix + "test_animated_primvar_interpolation_inference_enabled.usda"
        USDExporter.ExportFile exportPath exportOptions:exportOptions
        local stage = pyUsd.Stage.Open(exportPath)
        local usdCube = stage.GetPrimAtPath("/plane")
        local cubeMesh = pyUsdGeom.Mesh(usdCube)
        local primvarApi = pyUsdGeom.PrimvarsAPI(cubeMesh)
        assert_true (primvarApi.HasPrimvar("st"))            
        local map1 = primvarApi.GetPrimvar("st")
        assert_equal "vertex" (map1.GetInterpolation()) message:"The interpolation scheme should have been inferred to vertex."
        assert_true (primvarApi.HasPrimvar("normals"))            
        local normals = primvarApi.GetPrimvar("normals") 
        assert_equal "constant" (normals.GetInterpolation()) message:"The interpolation scheme should have been inferred to constant."
        
        -- When exporting an animation, the primvar interpolation is not inferred, as the result could 
        -- vary over the course of the animation - the interpolation scheme is not animatable. 
        exportOptions.TimeMode = #frameRange
        exportOptions.StartFrame = 0
        exportOptions.EndFrame = 4
        local exportPath = output_prefix + "test_animated_primvar_interpolation_inference_disabled.usda"
        USDExporter.ExportFile exportPath exportOptions:exportOptions
        local stage = pyUsd.Stage.Open(exportPath)
        local usdCube = stage.GetPrimAtPath("/plane")
        local cubeMesh = pyUsdGeom.Mesh(usdCube)
        -- When exporting animations, the exporter always uses faceVarying interpolation, which is essentially
        -- how the data is mapped in Max.
        local primvarApi = pyUsdGeom.PrimvarsAPI(cubeMesh)
        assert_true (primvarApi.HasPrimvar("st"))            
        local map1 = primvarApi.GetPrimvar("st")
        assert_equal "faceVarying" (map1.GetInterpolation()) message:"The interpolation scheme should have been set to faceVarying."
        assert_true (primvarApi.HasPrimvar("normals"))            
        local normals = primvarApi.GetPrimvar("normals")
        assert_equal "faceVarying" (normals.GetInterpolation()) message:"The interpolation scheme should have been set to faceVarying."
    ),
    
    function test_animated_hierarchy = (
        local exportOptions = USDExporter.CreateOptions()
        exportOptions.FileFormat = #ascii
        exportOptions.RootPrimPath = "/"
        
        local parentBox = Box name:"parent"
        local childBox = BOx name:"child"
        
        childBox.parent = parentBox
        
        with animate on (
            -- Animate the parent box  from frame 5 to 10, move it 10 units upwards.             
            at time 0 parentBox.pos.Z = 0.0
            at time 5 parentBox.pos.Z = 0.0
            at time 10 parentBox.pos.Z = 10.0
            -- Animate the child box over 10 frames, move it 10 units on Y.
            at time 10 childBox.pos.Y = 10.0            
        )  
        
        local exportPath = output_prefix + "test_animated_hierarchy.usda"
        
        exportOptions.TimeMode = #frameRange
        exportOptions.StartFrame = 0
        exportOptions.EndFrame = 10
        
        USDExporter.ExportFile exportPath exportOptions:exportOptions
        local stage = pyUsd.Stage.Open(exportPath)
        parentPrim = stage.GetPrimAtPath("/parent")
        childPrim = stage.GetPrimAtPath("/parent/child")
        
        -- Test transform.
        local parentXformable = pyUsdGeom.Xformable(parentPrim)
        local childXformable = pyUsdGeom.Xformable(childPrim)
        
        for timeCode = exportOptions.StartFrame to exportOptions.EndFrame do (
            local parentTransform = at time timeCode parentBox.transform
            local childTransform = at time timeCode childBox.transform
            assert_true (compareUsdAndMaxTransforms (parentXformable.ComputeLocalToWorldTransform(timeCode)) parentTransform) message:("The parent cube's transform is wrong at t = " + timeCode as string)
            assert_true (compareUsdAndMaxTransforms (childXformable.ComputeLocalToWorldTransform(timeCode)) childTransform) message:("The child cube's transform is wrong at t = " + timeCode as string)
        )
    ),
    
    -- animated mesh + bake object offset = off + object offset != identity
    -- We had a crash in this scenario, because of object offset XformOp was added on every frame, 
    -- it should only be added once.
    function test_object_offset_bake_off_animated = (
        
        local exportOptions = USDExporter.CreateOptions()
        exportOptions.FileFormat = #ascii
        exportOptions.BakeObjectOffsetTransform = false
        exportOptions.RootPrimPath = "/"
        
        local nodeName = "cube"
        local cube = Box name:"cube"
        
        -- Add an object offset.
        cube.objectOffsetPos = [0,0,10]
                
        convertToPoly cube
        -- Select top face.
        polyop.setFaceSelection cube  #{2}
    
        -- Animate the geometry : moving the top face.
        -- This is just to make sure that we will indeed export the mesh at different timecodes, 
        -- i.e. that we don't have a single validity interval over the animation.
        with animate on 
        (
            at time 10 move cube.selectedFaces [0,0,10]
        )        
        
        local exportPath = output_prefix + "object_offset_bake_off_animated.usda"
        exportOptions.TimeMode = #frameRange
        exportOptions.startFrame = 0
        exportOptions.endFrame = 10
        USDExporter.ExportFile exportPath exportOptions:exportOptions
        
        local stage = pyUsd.Stage.Open(exportPath)
        usdCube = stage.GetPrimAtPath("/cube/cube_Shape")
        
        local xformable = pyUsdGeom.Xformable(usdCube)
        local samples = xformable.GetTimeSamples()
        assert_equal 0 samples.Count message:"Object offset should not be animated."

        -- Make sure the transform op for the offset is only added once. I.e. at all time, the total transform for 
        -- this prim should be the object offset (the node is at the identity).
        local scaleOffset = scaleMatrix cube.objectOffsetScale
        local rotationOffset = cube.objectOffsetRot as matrix3
        local translationOffset = transMatrix cube.objectOffsetPos
        local offsetTransform = scaleOffset * rotationOffset * translationOffset
        for i = exportOptions.startFrame to exportOptions.endFrame do (
            assert_true (compareUsdAndMaxTransforms (xformable.ComputeLocalToWorldTransform(i)) offsetTransform) message:("Incorrect prim transform at timeCode = " + i as string)
        )
    ),
    
    function test_material_id_timesamples_match_topo = (
        
        local doubleQuad = plane name:"doubleQuad" width:10 length:10 widthsegs:1 lengthsegs:2
        
        convertToPoly doubleQuad
        -- Set both quads to different material ids.
        polyOp.setFaceMatId doubleQuad #{1} 1
        polyOp.setFaceMatId doubleQuad #{2} 2
        
        -- Animate one of the quads to make it concave at some point during the animation.
        with animate on 
        (
            at time 5 polyop.moveVert doubleQuad #{1} [8, 4, 0]
        )   
        
        local exportPath = output_prefix + "test_material_id_timesamples_match_topo.usda"
        local exportOptions = USDExporter.CreateOptions()
        exportOptions.TimeMode = #frameRange
        exportOptions.FileFormat = #ascii
        exportOptions.StartFrame = -5 --purposefully start/end before/after the object anim range.
        exportOptions.EndFrame = 10 
        exportOptions.RootPrimPath = "/"
        
        USDExporter.ExportFile exportPath exportOptions:exportOptions
        local stage = pyUsd.Stage.Open(exportPath)
        stage.Reload()        
        local primPath = "/doubleQuad"
        
        -- Mesh is cleaned on the first frame, validity interval of the object is used instead of the topo
        -- interval of the mesh.
        local expected = #(0.0, 3.0, 4.0, 5.0)
        
        local faceVertexIndicesSamples = get_mesh_attr_samples stage "faceVertexIndices" primPath        
        validate_time_samples expected faceVertexIndicesSamples
        local faceVertexCountsSamples = get_mesh_attr_samples stage "faceVertexCounts" primPath
        validate_time_samples expected faceVertexCountsSamples
        
        -- Validate subsets indice timesamples.
        local subset1 = "/doubleQuad/_1_"
        
        local attrName = "indices"
        
        local subset1IndicesSamples = get_mesh_attr_samples stage attrName subset1        
        validate_time_samples expected subset1IndicesSamples
        
        subset1prim = stage.GetPrimAtPath(subset1)
        local attr = subset1prim.GetAttribute(attrName)
        assert_equal ((attr.Get(subset1IndicesSamples[1])) as string) ((pyVt.IntArray(#(0))) as string)
        assert_equal ((attr.Get(subset1IndicesSamples[2])) as string) ((pyVt.IntArray(#(0, 2))) as string)
        assert_equal ((attr.Get(subset1IndicesSamples[3])) as string) ((pyVt.IntArray(#(0, 2))) as string)
        assert_equal ((attr.Get(subset1IndicesSamples[4])) as string) ((pyVt.IntArray(#(0, 2))) as string)
        
        local subset2 = "/doubleQuad/_2_"
        local subset2IndicesSamples = get_mesh_attr_samples stage attrName subset2
        validate_time_samples expected subset2IndicesSamples
        subset2prim = stage.GetPrimAtPath(subset2)
        local attr = subset2prim.GetAttribute(attrName)
        assert_equal ((attr.Get(subset2IndicesSamples[1])) as string) ((pyVt.IntArray(#(1))) as string)
        assert_equal ((attr.Get(subset2IndicesSamples[2])) as string) ((pyVt.IntArray(#(1))) as string)
        assert_equal ((attr.Get(subset2IndicesSamples[3])) as string) ((pyVt.IntArray(#(1))) as string)
        assert_equal ((attr.Get(subset2IndicesSamples[4])) as string) ((pyVt.IntArray(#(1))) as string)            
    ),
    
    function teardown = (
    ),

    Tests = #(test_animation_metadata_export,
        test_simple_object_validity_interval,
        test_multiple_validity_intervals,
        test_animate_all_geom_channels,
        test_animated_topology_only,
        test_animated_uvs_only,
        test_animated_verts_only,
        test_animated_transforms_intervals,
        test_animated_transforms_are_exported_even_if_identity,
        test_interpolation_scheme_inference_disabled_in_animation,
        test_animated_hierarchy,
        test_object_offset_bake_off_animated,
        test_geom_validity_intervals_ignored_on_split_geometry,
        test_geom_validity_intervals_ignored_on_cleaned_geometry,
        test_geom_validity_intervals_convert_tri_to_poly,
        test_material_id_timesamples_match_topo
    )
)

runUsdTestFixture export_animation_test (getThisScriptFilename())