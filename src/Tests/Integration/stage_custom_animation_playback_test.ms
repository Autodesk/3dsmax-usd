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

pyUsd = python.import("pxr.Usd")
pyUsdUtils = python.import("pxr.UsdUtils")

struct stage_custom_animation_playback_test
(  
    private 
    output_prefix = pathConfig.appendPath (getDir #temp) "/STAGE_CUSTOM_ANIMATION_PLAYBACK_",
    
    public
    function setup =
    (
        deleteTestFiles(output_prefix + "*.*")
        resetMaxFile #noprompt
    ),

    function test_source_params = (
        local stageObject = USDStageObject name:"stageAnimPlaybackTestObj"        
        local testDataDir = getFilenamePath (getThisScriptFilename())
        local sampleFile = (testDataDir + @"data\usd_anim_playback_test_sample.usda")
        stageObject.SetRootLayer sampleFile

        -- Set animation mode to original range value which is 0
        stageObject.AnimationMode = 0
        local sourceTPS = stageObject.SourceAnimationTPS
        local sourceStartTimeCode = stageObject.SourceAnimationStartTimeCode
        local sourceEndTimeCode = stageObject.SourceAnimationEndTimeCode
        
        local stageCache = pyUsdUtils.StageCache.Get()
        local pxrStage = stageCache.Find(pyUsd.StageCache.Id.FromLongInt(stageObject.cacheId))
        local pxrStageSourceTPS = pxrStage.GetTimeCodesPerSecond()
        local pxrStageSourceStartTimeCode = pxrStage.GetStartTimeCode()
        local pxrStageSourceEndTimeCode = pxrStage.GetEndTimeCode()

        assert_float_equal pxrStageSourceTPS sourceTPS message:"The pxrStage.GetTimeCodesPerSecond() value should be equal to (our) SourceAnimationTPS."
        assert_float_equal pxrStageSourceStartTimeCode sourceStartTimeCode message:"The pxrStage.GetStartTimeCode() value should be equal to (our) SourceAnimationStartTimeCode."
        assert_float_equal pxrStageSourceEndTimeCode sourceEndTimeCode message:"The pxrStage.GetEndTimeCode() value should be equal to (our) SourceAnimationEndTimeCode."

    ),

    function test_original_range = (
        local stageObject = USDStageObject name:"stageAnimPlaybackTestObj"        
        local testDataDir = getFilenamePath (getThisScriptFilename())
        local sampleFile = (testDataDir + @"data\usd_anim_playback_test_sample.usda")
        stageObject.SetRootLayer sampleFile

        -- Set animation mode to original range value which is 0
        stageObject.AnimationMode = 0
        local sourceTPS = stageObject.SourceAnimationTPS
        local sourceStartTimeCode = stageObject.SourceAnimationStartTimeCode
        local sourceEndTimeCode = stageObject.SourceAnimationEndTimeCode

        -- Make sure the source file we are testing holds this value assumption as the constants
        -- tested in following tests are based on these start and end time codes
        assert_float_equal 101.0 sourceStartTimeCode message:"The SourceAnimationStartTimeCode value should be equal to 101.0"
        assert_float_equal 129.0 sourceEndTimeCode message:"The SourceAnimationEndTimeCode value should be equal to 129.0"
        assert_float_equal 24.0 sourceTPS message:"The SourceAnimationTPS value should be equal to 24.0"

        frameRate = 30        
        local maxStartFrame = stageObject.MaxAnimationStartFrame
        local maxEndFrame = stageObject.MaxAnimationEndFrame
        assert_float_equal 126.25 maxStartFrame message:"The MaxAnimationStartFrame value should be equal to 126.25 at 30 FPS"
        assert_float_equal 161.25 maxEndFrame message:"The MaxAnimationEndFrame value should be equal to 161.25 at 30 FPS"

        frameRate = 24
        maxStartFrame = stageObject.MaxAnimationStartFrame
        maxEndFrame = stageObject.MaxAnimationEndFrame
        assert_float_equal 101.0 maxStartFrame message:"The MaxAnimationStartFrame value should be equal to 101.0 at 24 FPS (matching)"
        assert_float_equal 129.0 maxEndFrame message:"The MaxAnimationEndFrame value should be equal to 129.0 at 24 FPS (matching)"
    ),

    function test_custom_start_and_speed = (
        local stageObject = USDStageObject name:"stageAnimPlaybackTestObj"        
        local testDataDir = getFilenamePath (getThisScriptFilename())
        local sampleFile = (testDataDir + @"data\usd_anim_playback_test_sample.usda")
        stageObject.SetRootLayer sampleFile

        -- Set animation mode to custom start and speed value which is 1
        stageObject.AnimationMode = 1
        local sourceTPS = stageObject.SourceAnimationTPS
        local sourceStartTimeCode = stageObject.SourceAnimationStartTimeCode
        local sourceEndTimeCode = stageObject.SourceAnimationEndTimeCode

        -- Make sure the source file we are testing holds this value assumption as the constants
        -- tested in following tests are based on these start and end time codes
        assert_float_equal 101.0 sourceStartTimeCode message:"The SourceAnimationStartTimeCode value should be equal to 101.0"
        assert_float_equal 129.0 sourceEndTimeCode message:"The SourceAnimationEndTimeCode value should be equal to 129.0"
        assert_float_equal 24.0 sourceTPS message:"The SourceAnimationTPS value should be equal to 24.0"

        -- case 1
        frameRate = 24
        stageObject.CustomAnimationStartFrame = 30
        stageObject.CustomAnimationSpeed = 1.0
        local maxStartFrame = stageObject.MaxAnimationStartFrame
        local maxEndFrame = stageObject.MaxAnimationEndFrame

        assert_float_equal 30.0 maxStartFrame message:"The MaxAnimationStartFrame value should be equal to 30 at 24 FPS (matching)"
        assert_float_equal 58.0 maxEndFrame message:"The MaxAnimationEndFrame value should be equal to 58.0 at 24 FPS (matching)"

        -- case 2
        frameRate = 30
        stageObject.CustomAnimationStartFrame = 30
        stageObject.CustomAnimationSpeed = 1.0
        maxStartFrame = stageObject.MaxAnimationStartFrame
        maxEndFrame = stageObject.MaxAnimationEndFrame

        assert_float_equal 30.0 maxStartFrame message:"The MaxAnimationStartFrame value should be equal to 30.0 at 30 FPS"
        assert_float_equal 65.0 maxEndFrame message:"The MaxAnimationEndFrame value should be equal to 65.0 at 30 FPS"

        -- case 3
        frameRate = 24
        stageObject.CustomAnimationStartFrame = -28
        stageObject.CustomAnimationSpeed = 1.0
        local maxStartFrame = stageObject.MaxAnimationStartFrame
        local maxEndFrame = stageObject.MaxAnimationEndFrame

        assert_float_equal -28.0 maxStartFrame message:"The MaxAnimationStartFrame value should be equal to -28.0 at 24 FPS (matching) for negative value -28.0 for CustomAnimationStartFrame"
        assert_float_equal 0.0 maxEndFrame message:"The MaxAnimationEndFrame value should be equal to 0.0 at 24 FPS (matching) for negative value -28.0 for CustomAnimationStartFrame"

        -- case 4
        frameRate = 24
        stageObject.CustomAnimationStartFrame = 30
        stageObject.CustomAnimationSpeed = 0.0
        local maxStartFrame = stageObject.MaxAnimationStartFrame
        local maxEndFrame = stageObject.MaxAnimationEndFrame

        assert_float_equal 30.0 maxStartFrame message:"The MaxAnimationStartFrame value should be equal to 30.0 at 24 FPS (matching) at speed 0.0"
        assert_float_equal 30.0 maxEndFrame message:"The MaxAnimationEndFrame value should be equal to 30.0 at 24 FPS (matching) at speed 0.0"

        -- case 5
        frameRate = 24
        stageObject.CustomAnimationStartFrame = 28
        stageObject.CustomAnimationSpeed = -1.0
        local maxStartFrame = stageObject.MaxAnimationStartFrame
        local maxEndFrame = stageObject.MaxAnimationEndFrame

        assert_float_equal 28.0 maxStartFrame message:"The MaxAnimationStartFrame value should be equal to 28.0 at 24 FPS (matching) at speed -1.0"
        assert_float_equal 0.0 maxEndFrame message:"The MaxAnimationEndFrame value should be equal to 0.0 at 24 FPS (matching) at speed -1.0"

        -- case 6
        frameRate = 24
        stageObject.CustomAnimationStartFrame = -28
        stageObject.CustomAnimationSpeed = -1.0
        local maxStartFrame = stageObject.MaxAnimationStartFrame
        local maxEndFrame = stageObject.MaxAnimationEndFrame

        assert_float_equal -28.0 maxStartFrame message:"The MaxAnimationStartFrame value should be equal to -28.0 at 24 FPS (matching) for negative value -28.0 for CustomAnimationStartFrame at speed -1.0"
        assert_float_equal -56.0 maxEndFrame message:"The MaxAnimationEndFrame value should be equal to -56.0 at 24 FPS (matching) for negative value -28.0 for CustomAnimationStartFrame at speed -1.0"

        -- case 7
        frameRate = 24
        stageObject.CustomAnimationStartFrame = 28
        stageObject.CustomAnimationSpeed = 2.0
        local maxStartFrame = stageObject.MaxAnimationStartFrame
        local maxEndFrame = stageObject.MaxAnimationEndFrame

        assert_float_equal 28.0 maxStartFrame message:"The MaxAnimationStartFrame value should be equal to 28.0 at 24 FPS (matching) at speed 2.0"
        assert_float_equal 42.0 maxEndFrame message:"The MaxAnimationEndFrame value should be equal to 42.0 at 24 FPS (matching) at speed 2.0"

        -- case 8
        frameRate = 24
        stageObject.CustomAnimationStartFrame = 28
        stageObject.CustomAnimationSpeed = 0.5
        local maxStartFrame = stageObject.MaxAnimationStartFrame
        local maxEndFrame = stageObject.MaxAnimationEndFrame

        assert_float_equal 28.0 maxStartFrame message:"The MaxAnimationStartFrame value should be equal to 28.0 at 24 FPS (matching) at speed 0.5"
        assert_float_equal 84.0 maxEndFrame message:"The MaxAnimationEndFrame value should be equal to 84.0 at 24 FPS (matching) at speed 0.5"
    ),

    function test_custom_range = (
        local stageObject = USDStageObject name:"stageAnimPlaybackTestObj"        
        local testDataDir = getFilenamePath (getThisScriptFilename())
        local sampleFile = (testDataDir + @"data\usd_anim_playback_test_sample.usda")
        stageObject.SetRootLayer sampleFile

        -- Set animation mode to custom range value which is 2
        stageObject.AnimationMode = 2
        local sourceTPS = stageObject.SourceAnimationTPS
        local sourceStartTimeCode = stageObject.SourceAnimationStartTimeCode
        local sourceEndTimeCode = stageObject.SourceAnimationEndTimeCode

        -- Make sure the source file we are testing holds this value assumption as the constants
        -- tested in following tests are based on these start and end time codes
        assert_float_equal 101.0 sourceStartTimeCode message:"The stage.GetStartTimeCode() value should be equal to 101.0"
        assert_float_equal 129.0 sourceEndTimeCode message:"The stage.GetEndTimeCode() value should be equal to 129.0"
        assert_float_equal 24.0 sourceTPS message:"The SourceAnimationTPS value should be equal to 24.0"

        -- case 1
        frameRate = 24
        stageObject.CustomAnimationStartFrame = 30
        stageObject.CustomAnimationEndFrame = 50
        local maxStartFrame = stageObject.MaxAnimationStartFrame
        local maxEndFrame = stageObject.MaxAnimationEndFrame

        assert_float_equal 30.0 maxStartFrame message:"The MaxAnimationStartFrame value should be equal to 30.0 at 24 FPS (matching)"
        assert_float_equal 50.0 maxEndFrame message:"The MaxAnimationEndFrame value should be equal to 50.0 at 24 FPS (matching)"

        -- case 2
        frameRate = 30
        stageObject.CustomAnimationStartFrame = 30
        stageObject.CustomAnimationEndFrame = 50
        maxStartFrame = stageObject.MaxAnimationStartFrame
        maxEndFrame = stageObject.MaxAnimationEndFrame

        assert_float_equal 30.0 maxStartFrame message:"The MaxAnimationStartFrame value should be equal to 30.0 at 30 FPS"
        assert_float_equal 50.0 maxEndFrame message:"The MaxAnimationEndFrame value should be equal to 50.0 at 30 FPS"

        -- case 3
        frameRate = 30
        stageObject.CustomAnimationStartFrame = 30
        stageObject.CustomAnimationEndFrame = 0
        maxStartFrame = stageObject.MaxAnimationStartFrame
        maxEndFrame = stageObject.MaxAnimationEndFrame

        assert_float_equal 30.0 maxStartFrame message:"The MaxAnimationStartFrame value should be equal to 30.0 at 30 FPS for MaxAnimationEndFrame set to 0"
        assert_float_equal 0.0 maxEndFrame message:"The MaxAnimationEndFrame value should be equal to 0.0 at 30 FPS for MaxAnimationEndFrame set to 0"

        -- case 4
        frameRate = 30
        stageObject.CustomAnimationStartFrame = 30
        stageObject.CustomAnimationEndFrame = 30
        maxStartFrame = stageObject.MaxAnimationStartFrame
        maxEndFrame = stageObject.MaxAnimationEndFrame

        assert_float_equal 30.0 maxStartFrame message:"The MaxAnimationStartFrame value should be equal to 30.0 at 30 FPS for MaxAnimationEndFrame set to 30"
        assert_float_equal 30.0 maxEndFrame message:"The MaxAnimationEndFrame value should be equal to 30.0 at 30 FPS for MaxAnimationEndFrame set to 30"

    ),

    function test_custom_timecode_playback = (
        local stageObject = USDStageObject name:"stageAnimPlaybackTestObj"        
        local testDataDir = getFilenamePath (getThisScriptFilename())
        local sampleFile = (testDataDir + @"data\usd_anim_playback_test_sample.usda")
        stageObject.SetRootLayer sampleFile

        -- Set animation mode to custom playback frame to 3
        stageObject.AnimationMode = 3
        local sourceTPS = stageObject.SourceAnimationTPS
        local sourceStartTimeCode = stageObject.SourceAnimationStartTimeCode
        local sourceEndTimeCode = stageObject.SourceAnimationEndTimeCode
        local maxStartFrame = stageObject.MaxAnimationStartFrame
        local maxEndFrame = stageObject.MaxAnimationEndFrame

        -- Make sure the source file we are testing holds this value assumption as the constants
        -- tested in following tests are based on these start and end time codes
        assert_float_equal 101.0 sourceStartTimeCode message:"The stage.GetStartTimeCode() value should be equal to 101.0"
        assert_float_equal 129.0 sourceEndTimeCode message:"The stage.GetEndTimeCode() value should be equal to 129.0"
        assert_float_equal 24.0 sourceTPS message:"The SourceAnimationTPS value should be equal to 24.0"

        -- case 1
        frameRate = 24
        maxStartFrame = stageObject.MaxAnimationStartFrame
        maxEndFrame = stageObject.MaxAnimationEndFrame

        assert_float_equal 101.0 maxStartFrame message:"The MaxAnimationStartFrame value should be equal to 101.0 at 24 FPS (matching)"
        assert_float_equal 129.0 maxEndFrame message:"The MaxAnimationEndFrame value should be equal to 129.0 at 24 FPS (matching)"

        -- case 2
        frameRate = 30
        maxStartFrame = stageObject.MaxAnimationStartFrame
        maxEndFrame = stageObject.MaxAnimationEndFrame

        assert_float_equal 101.0 maxStartFrame message:"The MaxAnimationStartFrame value should be equal to 101.0 at 30 FPS"
        assert_float_equal 129.0 maxEndFrame message:"The MaxAnimationEndFrame value should be equal to 129.0 at 30 FPS"
    ),
    
    -- Test the RenderUsdTimeCode param, informs of the currently renderered USD time code,
    -- considering animation playback configuration.
    function test_usd_render_time = (
        local stageObject = USDStageObject()
        local testDataDir = getFilenamePath (getThisScriptFilename())
        local sampleFile = (testDataDir + @"data\usd_anim_playback_test_sample.usda")
        stageObject.SetRootLayer sampleFile        
        -- High level testing that the value computed reacts correctly to the requested time, 
        -- and animation modes. The computation itself is tested via all the config tests
        -- earlier in this file.            
        assert_equal 0.0 stageObject.RenderUsdTimeCode
        -- Check at frame 10... expect time code 8.0 
        -- 24 timecode/sec in the file VS 3dsMax 30 FPS.
        at time 10 assert_equal 8.0 stageObject.RenderUsdTimeCode
        -- Change the animation mode, and validate again.
        stageObject.AnimationMode = 1
        assert_equal 101.0 stageObject.RenderUsdTimeCode
        at time 10 assert_equal 109.0 stageObject.RenderUsdTimeCode        
    ),    
        
    function teardown = (
        resetMaxFile #noprompt
    ),

    Tests = #(test_source_params, 
        test_original_range, 
        test_custom_start_and_speed, 
        test_custom_range, 
        test_custom_timecode_playback,
        test_usd_render_time)
)

runUsdTestFixture stage_custom_animation_playback_test(getThisScriptFilename())