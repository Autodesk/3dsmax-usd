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

output_prefix = pathConfig.appendPath (getDir #temp) "/EXPORT_SHAPES_TEST_"
deleteTestFiles(output_prefix + "*.*")

struct export_shapes_test
(  
    private 
    exportOptions = USDExporter.CreateOptions(),
    
    public 
    function setup =
    (
        resetmaxfile #noprompt
        exportOptions.FileFormat = #ascii
        exportOptions.RootPrimPath = "/"
    ),
    
    -- Makes sure that we are able to export all shape object types available to UsdGeom curves.
    function export_all_shape_types_test = (
        local allObjects = #()
        allObjects = for o in Shape.classes where o.creatable collect o

        local expectedShapeObjects = #()
        for i = 1 to allObjects.count do
        (
            -- Align the objects in a grid for easy visual inspection...
            local x = (mod i 10) * 100
            local y = ((i / 10) as integer) * 100
            local obj = (allObjects[i] height:10 length:10 width:10)
            obj.pos.x = x
            obj.pos.y = y
            obj.pos.z = 0
            append expectedShapeObjects obj                
        )
        
        local exportPath = output_prefix + "export_all_shape_objects.usd"
        
        USDExporter.ExportFile exportPath exportOptions:exportOptions
        local stage = pyUsd.Stage.Open(exportPath)
        root = stage.GetPrimAtPath("/")
        assert_equal expectedShapeObjects.count (root.GetAllChildren()).count message:"Unexpected prim count when exporting all shape objects."
    
        for i = 1 to expectedShapeObjects.count do (
            local name = expectedShapeObjects[i].name
            local objPrim = stage.GetPrimAtPath("/" + pyUsdTf.MakeValidIdentifier(name))
            assert_true (pyUsd.Prim.IsValid(objPrim)) message:(name + " of class " + ((classOf expectedShapeObjects[i]) as string) + " was not properly exported to a USD prim.")
            assert_true (objPrim.IsA(pyUsdGeom.Curves)) message:(name + " of class " + ((classOf expectedShapeObjects[i]) as string) + " should have been exported to a curves type prim.")
        )
    ),
    
    -- Test that all required curve attributes are exported to properly represent the shape in USD.
    function export_simple_spline_test = (
        
        -- Build a simple spline with 3 knots.
        spline = splineShape name:"spline"
        -- These are the current default values from Max, just making sure the test wont break if defaults change.
        spline.steps = 6
        spline.optimize = true
        spline.adaptive = false
        
        splineIdx = addNewSpline spline
        addKnot spline splineIdx #smooth #curve [0, 10, 0]
        addKnot spline splineIdx #smooth #curve [0, 20, 0]
        addKnot spline splineIdx #smooth #curve [20, 20, 0]

        local exportOptions = USDExporter.CreateOptions()
        exportOptions.FileFormat = #ascii
        exportOptions.RootPrimPath = "/"
        exportOptions.Shapes = true
        
        local exportPath = output_prefix + "export_simple_spline_test.usd"
        USDExporter.ExportFile exportPath exportOptions:exportOptions
        
        local stage = pyUsd.Stage.Open(exportPath)
        local splinePrim = stage.GetPrimAtPath("/spline")
        assert_true (pyUsd.Prim.IsValid(splinePrim)) message:"The spline was not exported to a USD prim as expected."
        assert_true (splinePrim.IsA(pyUsdGeom.BasisCurves)) message:"The spline prim is not a curve."
        
        local splineCurve = pyUsdGeom.BasisCurves(splinePrim)
        local curveVertexCounts = (splineCurve.GetCurveVertexCountsAttr()).Get()
        assert_equal 15 curveVertexCounts[1] message:"Unexpected curve vertex counts."
        
        local expectedPoints = #([0, 10, 0],
            [-1.2244899, 12.040816, 0], 
            [-2.0408165, 13.877552, 0], 
            [-2.4489794, 15.510201, 0], 
            [-2.4489796, 16.938774, 0], 
            [-2.0408163, 18.163265, 0], 
            [-1.2244897, 19.183672, 0], 
            [0, 20, 0], 
            [1.6326532, 20.612246, 0],
            [3.67347, 21.02041, 0], 
            [6.122449, 21.224487, 0], 
            [8.979592, 21.224491, 0], 
            [12.244898, 21.020407, 0], 
            [15.918367, 20.612244, 0], 
            [20, 20, 0])
            
        local points = (splineCurve.GetPointsAttr()).Get()
        for i = 1 to 15 do (
            assert_float_equal expectedPoints[i][1] points[i][1] message:"Unexpected curve point position in X."
            assert_float_equal expectedPoints[i][2] points[i][2] message:"Unexpected curve point position in Y."
            assert_float_equal expectedPoints[i][3] points[i][3] message:"Unexpected curve point position in Z."
        )
        
        local typeAttr = (splineCurve.GetTypeAttr()).Get()
        assert_equal "linear" typeAttr message:"The exported curve type should be \"linear\"."
    ),    
    
    -- Make sure the interpolation parameters specified on splines are respected when exporting to USD.
    function export_splines_interpolation_test = (
        local exportOptions = USDExporter.CreateOptions()
        exportOptions.FileFormat = #ascii
        exportOptions.RootPrimPath = "/"
        exportOptions.Shapes = true
        
        local circleNode = circle name:"circle"
        local exportPath = output_prefix + "circle_interpolation_steps_2.usd"
        
        circleNode.steps = 2
        circleNode.optimize = false
        circleNode.adaptive = false
        
        USDExporter.ExportFile exportPath exportOptions:exportOptions
            
        local stage = pyUsd.Stage.Open(exportPath)
        local circlePrim = stage.GetPrimAtPath("/circle")
        assert_true (pyUsd.Prim.IsValid(circlePrim)) message:"The circle was not exported to a USD prim as expected."
        assert_true (circlePrim.IsA(pyUsdGeom.BasisCurves)) message:"The circle prim is not a curve."
        local circleCurve = pyUsdGeom.BasisCurves(circlePrim)
        local curveVertexCounts = (circleCurve.GetCurveVertexCountsAttr()).Get()
        assert_equal 13 curveVertexCounts[1] message:"Unexpected curve vertex counts with steps = 2."
            
        local exportPath = output_prefix + "circle_interpolation_steps_4.usd"
        circleNode.steps = 4
        circleNode.optimize = false
        circleNode.adaptive = false
        
        USDExporter.ExportFile exportPath exportOptions:exportOptions        
        local stage = pyUsd.Stage.Open(exportPath)
        local circlePrim = stage.GetPrimAtPath("/circle")
        assert_true (pyUsd.Prim.IsValid(circlePrim)) message:"The circle was not exported to a USD prim as expected."
        assert_true (circlePrim.IsA(pyUsdGeom.BasisCurves)) message:"The circle prim is not a curve."
        local circleCurve = pyUsdGeom.BasisCurves(circlePrim)
        local curveVertexCounts = (circleCurve.GetCurveVertexCountsAttr()).Get()
        assert_equal 21 curveVertexCounts[1] message:"Unexpected curve vertex counts with steps = 4."
        
        local exportPath = output_prefix + "circle_interpolation_steps_adaptive.usd"
        circleNode.steps = 4
        circleNode.optimize = false
        circleNode.adaptive = true
        
        USDExporter.ExportFile exportPath exportOptions:exportOptions        
        local stage = pyUsd.Stage.Open(exportPath)
        local circlePrim = stage.GetPrimAtPath("/circle")
        assert_true (pyUsd.Prim.IsValid(circlePrim)) message:"The circle was not exported to a USD prim as expected."
        assert_true (circlePrim.IsA(pyUsdGeom.BasisCurves)) message:"The circle prim is not a curve."
        local circleCurve = pyUsdGeom.BasisCurves(circlePrim)
        local curveVertexCounts = (circleCurve.GetCurveVertexCountsAttr()).Get()
        assert_equal 129 curveVertexCounts[1] message:"Unexpected curve vertex counts with adaptive steps."
    
        -- To test the optimize property, need something other than a circle, as it would have no colinear points.
        resetMaxFile #noprompt
            
        local rectNode = rectangle name:"rectangle"
            
        local exportPath = output_prefix + "rect_interpolation_steps_optimize_on.usd"
        rectNode.steps = 4
        rectNode.optimize = true
        rectNode.adaptive = false
        
        USDExporter.ExportFile exportPath exportOptions:exportOptions        
        local stage = pyUsd.Stage.Open(exportPath)
        local rectPrim = stage.GetPrimAtPath("/rectangle")
        assert_true (pyUsd.Prim.IsValid(rectPrim)) message:"The rectangle was not exported to a USD prim as expected."
        assert_true (rectPrim.IsA(pyUsdGeom.BasisCurves)) message:"The rectangle prim is not a curve."
        local rectCurve = pyUsdGeom.BasisCurves(rectPrim)
        local curveVertexCounts = (rectCurve.GetCurveVertexCountsAttr()).Get()
        assert_equal 5 curveVertexCounts[1] message:"Unexpected curve vertex counts with optimize ON."
        
        local exportPath = output_prefix + "rect_interpolation_steps_optimize_off.usd"
        rectNode.steps = 4
        rectNode.optimize = false
        rectNode.adaptive = false
        
        USDExporter.ExportFile exportPath exportOptions:exportOptions        
        local stage = pyUsd.Stage.Open(exportPath)
        local rectPrim = stage.GetPrimAtPath("/rectangle")
        assert_true (pyUsd.Prim.IsValid(rectPrim)) message:"The rectangle was not exported to a USD prim as expected."
        assert_true (rectPrim.IsA(pyUsdGeom.BasisCurves)) message:"The rectangle prim is not a curve."
        local rectCurve = pyUsdGeom.BasisCurves(rectPrim)
        local curveVertexCounts = (rectCurve.GetCurveVertexCountsAttr()).Get()
        assert_equal 21 curveVertexCounts[1] message:"Unexpected curve vertex counts with optimize OFF."
    ),
    
    function export_animated_spline_test = (
        
        circleNode = circle name:"circle"
        circleNode.steps = 0 -- Simplest possible interpolation, results in 4 segments for a circle.
        circleNode.optimize = false 
        circleNode.adaptive = false
        -- Animate the radius of the circle...
        with animate on (
            at time 0 circleNode.radius = 1
            at time 2 circleNode.radius = 10
        )
        
        -- Export the animation...
        local exportOptions = USDExporter.CreateOptions()
        exportOptions.TimeMode = #frameRange
        exportOptions.StartFrame = 0
        exportOptions.EndFrame = 2
        exportOptions.FileFormat = #ascii
        exportOptions.RootPrimPath = "/"
        exportOptions.Shapes = true
        
        local exportPath = output_prefix + "export_animated_spline_test.usd"
                
        USDExporter.ExportFile exportPath exportOptions:exportOptions
        
        local stage = pyUsd.Stage.Open(exportPath)
        local circlePrim = stage.GetPrimAtPath("/circle")
        assert_true (pyUsd.Prim.IsValid(circlePrim)) message:"The circle was not exported to a USD prim as expected."
        assert_true (circlePrim.IsA(pyUsdGeom.BasisCurves)) message:"The circle prim is not a curve."
        
        local circleCurve = pyUsdGeom.BasisCurves(circlePrim)

        local expectedPoints = #(#([1, 0, 0],[0, 1, 0], [-1, 0, 0], [0, -1, 0]), -- timeCode 0
            #([5.5, 0, 0],[0, 5.5, 0], [-5.5, 0, 0], [0, -5.5, 0]), -- timeCode 1
            #([10, 0, 0],[0, 10, 0], [-10, 0, 0], [0, -10, 0])) -- timeCode 2
        
        for timeCode = 0 to 2 do (
            local points = (circleCurve.GetPointsAttr()).Get(timeCode)
            for i = 1 to 4 do (
                assert_float_equal expectedPoints[timeCode+1][i][1] points[i][1] message:("Unexpected curve point position in X at time " + timeCode as string)
                assert_float_equal expectedPoints[timeCode+1][i][2] points[i][2] message:("Unexpected curve point position in Y at time " + timeCode as string)
                assert_float_equal expectedPoints[timeCode+1][i][3] points[i][3] message:("Unexpected curve point position in Z  at time " + timeCode as string)
            )
        )
    ),
    
    function export_spline_offset = (
        -- Create a hierarchy with transforms and object offsets.
        local parent = PointHelperObj name:"parent"
        parent.position = [-10,-10,-10]
        parent.rotation = (quat  0.3 0 0 0.3)
        
        -- Build a simple spline with 2 knots.
        spline = splineShape name:"spline"
        spline.parent = parent
        
        -- No steps -> We get a single, line from point to point.
        spline.steps = 0
        
        splineIdx = addNewSpline spline
        addKnot spline splineIdx #smooth #curve [0, 0, 0]
        addKnot spline splineIdx #smooth #curve [0, 10, 0]
        
        -- Give it an offset transform.
        spline.objectOffsetPos = [4,5,6]
        spline.objectOffsetScale = [4,5,6]
        spline.objectOffsetRot = quat 4 5 6 1
        
        local exportOptions = USDExporter.CreateOptions()
        exportOptions.RootPrimPath = "/"
        exportOptions.FileFormat = #ascii
        exportOptions.Shapes = true
        
        local exportPath = output_prefix + "spline_with_offset.usd"
        
        USDExporter.ExportFile exportPath exportOptions:exportOptions
        
        local stage = pyUsd.Stage.Open(exportPath)
        local xformPrim = stage.GetPrimAtPath("/parent/spline")
        local splinePrim = stage.GetPrimAtPath("/parent/spline/spline_Shape")
        
        assert_true (pyUsd.Prim.IsValid(xformPrim)) message:("The xform prim was not properly exported, to support the object offset.")
        assert_true (pyUsd.Prim.IsValid(splinePrim)) message:("The spline prim was not properly exported.")
        
        local scaleOffset = scaleMatrix spline.objectOffsetScale
        local rotationOffset = spline.objectOffsetRot as matrix3
        local translationOffset = transMatrix spline.objectOffsetPos
        local offsetTransform = scaleOffset * rotationOffset * translationOffset

        local xformable = pyUsdGeom.Xformable(xformPrim)
        assert_true (compareUsdAndMaxTransforms (xformable.ComputeLocalToWorldTransform(0)) spline.transform) message:"Incorrect transform applied on the parent xform."
        
        splineXformable = pyUsdGeom.Xformable(splinePrim)
        assert_true (compareUsdAndMaxTransforms (splineXformable.GetLocalTransformation()) offsetTransform) message:"Unexpected object offset transform applied on the prim."
    ),
    
    function export_spline_offset_with_wsm = (
        -- Create a hierarchy with transforms and object offsets.
        local parent = PointHelperObj name:"parent"
        parent.position = [-10,-10,-10]
        parent.rotation = (quat  0.3 0 0 0.3)
        
        -- Build a simple spline with 2 knots.
        spline = splineShape name:"spline"
        spline.parent = parent
        
        -- No steps -> We get a single, line from point to point.
        spline.steps = 0
        
        splineIdx = addNewSpline spline
        addKnot spline splineIdx #smooth #curve [0, 0, 0]
        addKnot spline splineIdx #smooth #curve [0, 10, 0]
        
        -- Give it an offset transform.
        spline.objectOffsetPos = [4,5,6]
        spline.objectOffsetScale = [4,5,6]
        spline.objectOffsetRot = quat 4 5 6 1
                
        -- Create a Noise space warp and bind it to the box.
        local sw = SpaceNoise()        
        sw.position = [20,30,40] 
        sw.rotation = (quat  0 0.707 0 0.707)
        sw.strength = [1000,0,0]
        bindSpaceWarp spline sw
                
        local exportOptions = USDExporter.CreateOptions()

        exportOptions.FileFormat = #ascii
        exportOptions.RootPrimPath = "/"
        exportOptions.Shapes = true
        local exportPath = output_prefix + "spline_with_offset_and_wsm.usd"
        
        USDExporter.ExportFile exportPath exportOptions:exportOptions
        
        local stage = pyUsd.Stage.Open(exportPath)
        local splinePrim = stage.GetPrimAtPath("/parent/spline")
    
        assert_true (pyUsd.Prim.IsValid(splinePrim)) message:("The spline prim was not properly exported.")
        assert_true (splinePrim.IsA(pyUsdGeom.BasisCurves)) message:"The spline prim is not a curve."
        
        local splineCurve = pyUsdGeom.BasisCurves(splinePrim)
        local curveVertexCounts = (splineCurve.GetCurveVertexCountsAttr()).Get()
        assert_equal 2 curveVertexCounts[1] message:"Unexpected curve vertex counts."
        
        local expectedPoints = #([4, 5, -34.4081],
            [37.3333, -11.6667, 34.4867])
            
        local points = (splineCurve.GetPointsAttr()).Get()
        for i = 1 to 2 do (
            assert_float_equal expectedPoints[i][1] points[i][1] message:"Unexpected curve point position in X."
            assert_float_equal expectedPoints[i][2] points[i][2] message:"Unexpected curve point position in Y."
            assert_float_equal expectedPoints[i][3] points[i][3] message:"Unexpected curve point position in Z."
        )
    ),
    
    function teardown = (
    ),

    Tests = #(export_all_shape_types_test,
        export_splines_interpolation_test,
        export_simple_spline_test,
        export_animated_spline_test,
        export_spline_offset,
        export_spline_offset_with_wsm)
)

runUsdTestFixture export_shapes_test (getThisScriptFilename())