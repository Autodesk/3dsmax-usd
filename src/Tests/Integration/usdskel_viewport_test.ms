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

pyUsd = python.import("pxr.Usd")
pyUsdUtils = python.import("pxr.UsdUtils")

-- Make sure the USD plugin is properly loaded.
setupUsdPlugins()

-- High level viewport playback test for UsdSkel. Use the bounding box at different 
-- frames to make sure the meshes are computed correctly. This test was written because
-- we missed a regression where UsdSkel started crashing on playback.
struct usdskel_viewport_test
(   
    private 
    stageObject,
    initialAnimRange,
    initialFrameRate,
    initialUnits,
    initialScale,
    
    public 
    function setup =
    (
        initialUnits = units.SystemType
        initialScale = units.SystemScale
        
        resetmaxfile #noprompt
        
        -- Maximize the Viewport 
        actionMan.executeAction 0 "50026"

        -- Create the stage.
        stageObject = UsdStageObject()
        
        local testDataDir = getFilenamePath (getThisScriptFilename())
        local usdSkelSamplePath = (testDataDir + @"data\usdskel_sample.usdc")
        stageObject.SetRootLayer usdSkelSamplePath
        stageObject.DisplayMode = 2
            
        -- Zoom to the extents of the stage object.
        select stageObject
        max zoomext sel
        clearSelection()

        -- Setup the timeconfig to match the usd content. Keep track of what was
        -- there initially, to cleanup after ourselves.
        local stageCache = pyUsdUtils.StageCache.Get()
        local stage = stageCache.Find(pyUsd.StageCache.Id.FromLongInt(stageObject.cacheId))

        initialAnimRange = animationRange
        initialFrameRate = frameRate
        stage.GetTimeCodesPerSecond()
        frameRate  = stage.GetTimeCodesPerSecond()
        
        -- If no animation range defined, just run 100 frames.
        start = (stage.GetStartTimeCode())
        end = (stage.GetEndTimeCode())            
        if (start == end) do end = start + 100
                    
        animationRange = interval start end
    ),
        
    function test_usdskel_animation = (
        -- Fix the units, so we are testing the right bounding box values.
        units.SystemType = #Centimeters
        units.SystemScale = 1
        
        local expectedMaximums = #(
            [63.4995,32.5404,133.573],
			[63.4565,29.2865,133.292],
			[63.3221,25.5149,133.625],
			[63.0976,21.2746,134.06],
			[62.8092,16.8009,134.478],
			[62.4641,12.1828,134.846],
			[62.0668,8.86275,135.139],
			[61.6331,8.86275,135.326],
			[61.7988,8.86275,135.362],
			[62.2769,8.86275,135.205],
			[63.0118,8.86275,134.995],
			[63.6768,8.86275,134.689],
			[64.2506,8.86275,134.315],
			[64.7131,8.86275,133.899],
			[65.039,8.86275,133.477],
			[65.1943,8.86275,133.187],
			[65.1701,8.86275,133.517],
			[64.9485,8.86275,133.955],
			[64.5588,8.86275,134.381],
			[64.0202,8.86275,134.763],
			[63.3517,8.86275,135.073],
			[62.5859,8.86275,135.281],
			[62.3767,8.86275,135.339],
			[62.4562,8.86275,135.205],
			[62.788,8.86275,135.02],
			[63.0642,8.86275,134.738],
			[63.2806,8.86275,134.385],
			[63.4315,8.86275,133.986],
			[63.4995,8.86275,133.573]
        )
        
        local expectedMinimums = #(
			[-65.039,-16.5482,-4.15608],
			[-65.1947,-19.3214,-4.50949],
			[-65.1701,-21.5801,-4.21639],
			[-64.9484,-23.3695,-3.74131],
			[-64.5584,-24.9247,-3.25162],
			[-64.0192,-26.334,-2.7922],
			[-63.3502,-27.6464,-2.393],
			[-62.5839,-28.9022,-2.08803],
			[-62.3742,-33.4284,-1.92844],
			[-62.4533,-37.9311,-1.97103],
			[-62.7849,-42.5124,-2.27288],
			[-63.0613,-46.9847,-2.67936],
			[-63.2783,-51.257,-3.15057],
			[-63.4301,-55.2275,-3.65614],
			[-63.4995,-58.7098,-4.15608],
			[-63.4569,-61.4831,-4.50949],
			[-63.3221,-63.7418,-4.21639],
			[-63.0974,-65.5312,-3.74131],
			[-62.8087,-67.0864,-3.25162],
			[-62.4631,-68.4957,-2.7922],
			[-62.0653,-69.8081,-2.393],
			[-61.631,-71.0639,-2.08803],
			[-61.7962,-75.5901,-1.92844],
			[-62.274,-80.0928,-1.97042],
			[-63.0087,-84.6741,-2.27098],
			[-63.6739,-89.1463,-2.6761],
			[-64.2482,-93.4187,-3.14648],
			[-64.7116,-97.3891,-3.65258],
			[-65.039,-100.872,-4.15608]
        )
                
        local currentIndex = 0
        -- Simulate playback.
        for t in (animationrange.start) to (animationRange.end) do;
        (
            slidertime = t
            currentIndex = currentIndex + 1
            -- Test the bounding box (min/max)
            assert_point3_equal expectedMaximums[currentIndex] stageObject.max
            assert_point3_equal expectedMinimums[currentIndex] stageObject.min
        )
        
    ),
    
    function teardown = (
        frameRate  = initialFrameRate
        animationRange = initialAnimRange
        units.SystemType = initialUnits
        units.SystemScale = initialScale        
    ),

    Tests = #(test_usdskel_animation)
)

runUsdTestFixture usdskel_viewport_test (getThisScriptFilename())