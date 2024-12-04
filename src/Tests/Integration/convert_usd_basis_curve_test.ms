--
-- Copyright 2024 Autodesk
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
pyUsdUtils = python.import("pxr.UsdUtils")

output_prefix = pathConfig.appendPath (getDir #temp) "/CONVERT_USD_BASIS_CURVE_TEST_"
deleteTestFiles(output_prefix + "*.*")

struct convert_usd_basis_curve_test
(  
    public 
    function setup =
    (
        resetmaxfile #noprompt    
    ),
    
    function test = (

        -- Open the exported layer, and place it into the cache.
        local stage = pyUsd.Stage.CreateInMemory()
        stageCache = pyUsdUtils.StageCache.Get()
        stageCache.Insert(stage)
        stageId = (stageCache.GetId(stage)).ToLongInt()

        local curve_path = "/Curve"
        local some_curve = pyUsdGeom.BasisCurves.Define stage curve_path

        curvesPoints =  #([0,0,0], [1,1,1], [2,1,1], [3,2,2], [4,2,2], [15,3,3])
        curvesType = "cubic"
        curvesBasis = "bezier"
        curvesWrap = "periodic"
        curveVertexCount = #(6)
        some_curve.CreatePointsAttr(curvesPoints)
        some_curve.CreateTypeAttr(curvesType)
        some_curve.CreateWrapAttr(curvesWrap)
        some_curve.CreateBasisAttr(curvesBasis)
        some_curve.CreateCurveVertexCountsAttr(curveVertexCount)

        -- Convert the UsdMesh of the box to a Max mesh.
        local splineShapeObj = USDImporter.ConvertUsdBasisCurve stageId curve_path
            
        assert_equal SplineShape (classof splineShapeObj) message:("Converted Usd BasisCurve is expected to be a 'SplineShape' type.")
        
        local bezierShapeObj = USDImporter.ConvertUsdBasisCurve stageId curve_path asBezierShape:true
        assert_equal BezierShapeValue (classof bezierShapeObj) message:("Converted Usd BasisCurve is expected to be a 'BezierShapeValue' type.")
    ),

    function teardown = (
    ),

    Tests = #(test)
)

runUsdTestFixture convert_usd_basis_curve_test (getThisScriptFilename())