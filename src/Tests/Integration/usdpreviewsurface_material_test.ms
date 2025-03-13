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

struct usdpreviewsurface_material_test
(
    function setup =
    (
        resetmaxfile #noprompt
    ),

    function to_dict obj prop_names = (
        d = Dictionary()
        for prop_name in prop_names do (
            local prop_val = getProperty obj prop_name
            d[prop_name] = prop_val
        )
        return d
    ),
    
    function assert_dict_equal d1 d2 message: = (
        if d1.count != d2.count do (
            return (assert_equal d1.count d2.count message:(message + " - Key count mismatch."))
        )
        for key in d1.keys do (
            local kv_1 = GetDictValue d1 key
            local kv_2 = GetDictValue d2 key
            if kv_1 != kv_2 do ( 
                return (assert_equal kv_1 kv_2 message:(message + " - Key: " + key as string))
            )
        )
        return true
    ),

    function test_usdpreviewsurface_no_side_effects = (
        local vps = NitrousGraphicsManager.GetActiveViewportSetting()
        local vp_interface = getInterface vps "ViewportViewSettingImpl"
        local vp_prop_names = getPropNames vp_interface
        
        maxver = maxversion()
        if maxver[1] >= 27000 then (
            local vps_before = to_dict (NitrousGraphicsManager.GetActiveViewportSetting()) vp_prop_names
            local color_pipeline_mgr_before = to_dict ColorPipelineMgr (getPropNames ColorPipelineMgr)
            
            local box001 = Box pos:[0,0,0] name:"box001"
            box001.material = MaxUsdPreviewSurface()
            
            local vps_after = to_dict (NitrousGraphicsManager.GetActiveViewportSetting()) vp_prop_names
            local color_pipeline_mgr_after = to_dict ColorPipelineMgr (getPropNames ColorPipelineMgr)

            assert_dict_equal vps_before vps_after message:"ViewportSetting parameter changed"
            assert_dict_equal color_pipeline_mgr_before color_pipeline_mgr_after message:"ColorPipelineMgr parameter changed"
        )
        else (
            local vps_before = to_dict (NitrousGraphicsManager.GetActiveViewportSetting()) vp_prop_names
            local idisplay_before = to_dict idisplayGamma (getPropNames idisplayGamma)

            local box001 = Box pos:[0,0,0] name:"box001"
            box001.material = MaxUsdPreviewSurface()

            local vps_after = to_dict (NitrousGraphicsManager.GetActiveViewportSetting()) vp_prop_names
            local idisplay_after = to_dict idisplayGamma (getPropNames idisplayGamma)

            assert_dict_equal vps_before vps_after message:"ViewportSetting parameter changed"
            assert_dict_equal idisplay_before idisplay_after message:"idisplayGamma parameter changed"
        )
    ),

    function teardown = (
    ),

    tests = #(test_usdpreviewsurface_no_side_effects)
)

runUsdTestFixture usdpreviewsurface_material_test (getThisScriptFilename())