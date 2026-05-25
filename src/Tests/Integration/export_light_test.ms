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
pyUsdLux = python.import("pxr.UsdLux")
pyGf = python.import("pxr.Gf")

output_prefix = pathConfig.appendPath (getDir #temp) "/EXPORT_LIGHT_TEST_"
deleteTestFiles(output_prefix + "*.*")

struct export_light_test
(  
    private 
    exportOptions = USDExporter.CreateOptions(),
    initialUnits,
    initialScale,

    public 
    function setup =
    (
        quickReset()
        exportOptions.FileFormat = #ascii
        exportOptions.RootPrimPath = "/"

        -- properly set system units in order to get expected results from tests
        initialUnits = units.SystemType
        units.SystemType = #inches
        initialScale = units.SystemScale
        units.SystemScale = 1
    ),
    
    -- Tests to validate the USD light type created from a given 3ds Max Photometric light type at export
    -- A USD light is created based on the Photometric Light Shape and its selected light distribution type
    -- Each photometric light shape has a 'target' sibling type (i.e. Point Target, Sphere Target, etc.)
    -- Below is the expected conversion grid :
    ----------------------------------------------------------------------------------------------------
    --                                            3ds Max Photometric Light Shapes
    --                              Point      Sphere     Disc        Line      Cylinder    Rectangle
    -----------------------------------------|--------|-----------|-----------|----------|--------------
    --              |  diffuse  |  DiskLight |   n/a  | DiskLight | RectLight |    n/a   |  RectLight
    --              |-----------|------------|--------|-----------|-----------|----------|--------------
    -- Distribution | spherical |            SphereLight          |           CylinderLight
    --              |-----------|------------|--------|-----------|-----------|----------|--------------
    --     Type     |    Spot   |  DiskLight |   n/a  | DiskLight | RectLight |    n/a   |  RectLight
    --              |-----------|------------|--------|-----------|-----------|----------|--------------
    --              |  Web/IES  |  DiskLight | Sphere | DiskLight | RectLight | Cylinder |  RectLight
    ----------------------------------------------------------------------------------------------------   
    function photometric_light_general_attributes_export_test = (
        local exportPath = output_prefix + "test_photometric_light_gen_attr_default_export.usda"
        local exportOptions = USDExporter.CreateOptions()
        exportOptions.FileFormat = #ascii
        exportOptions.RootPrimPath = "/"
        exportOptions.Lights = true

        -- taking the point light as a general sample for the attributes that affects all photometric lights
        local pointLight = free_light()
        pointLight.type = #Free_Point
        pointLight.distribution = 2 -- uniform hemispherical/diffuse
        -- use default attributes on photometric lights

        USDExporter.ExportFile exportPath exportOptions:exportOptions
        local stage = pyUsd.Stage.Open(exportPath)
        
        usd_light_path = "/" + pointLight.name
        usd_light = pyUsdLux.BoundableLightBase.Get stage usd_light_path
        
        local lightDiffuseAttr = usd_light.GetDiffuseAttr()
        assert_float_equal 1.0 (lightDiffuseAttr.Get()) message:(pointLight.name + " light is expected to have an effect of this light on the diffuse response of materials by default.")
        local lightSpecularAttr = usd_light.GetSpecularAttr()
        assert_float_equal 1.0 (lightSpecularAttr.Get()) message:(pointLight.name + " light is expected to have an effect of this light on the specular response of materials by default.")
        
        local lightNormalizeAttr = usd_light.GetNormalizeAttr()
        assert_true (lightNormalizeAttr.Get()) message:(pointLight.name + " light is expected to use normalized intensity.")
        local lightEnableColorTemperatureAttr = usd_light.GetEnableColorTemperatureAttr()
        assert_false (lightEnableColorTemperatureAttr.Get()) message:(pointLight.name + " light is not expected to use color temprature (Kelvin) by default.")
            
        -- The default color of the 'free_light' is equal to (1.0, 1.0, 1.0)
        local lightColorAttr = usd_light.GetColorAttr()
        local lightColorValue = lightColorAttr.Get()
        assert_float_equal 1.0 lightColorValue[1] message:"The light color value should default at 1.0"
        assert_float_equal 1.0 lightColorValue[2] message:"The light color value should default at 1.0"
        assert_float_equal 1.0 lightColorValue[3] message:"The light color value should default at 1.0"

        local lightShadowAPI = pyUsdLux.ShadowAPI(usd_light)
        local lightShadowAPI_ShadowEnableAttr = lightShadowAPI.GetShadowEnableAttr()
        assert_true (lightShadowAPI_ShadowEnableAttr.Get()) message:(pointLight.name + " light is expected to enable shadows to be cast by the light.")

        -- The default shadow color of the 'free_light' is equal to (0.0, 0.0, 0.0)
        local lightShadowAPI_ShadowColorAttr = lightShadowAPI.GetShadowColorAttr()
        local lightShadowAPI_ShadowColor = lightShadowAPI_ShadowColorAttr.Get()
        assert_float_equal 0.0 lightShadowAPI_ShadowColor[1] message:(pointLight.name + " light shadow color value (R) should default at 0.0")
        assert_float_equal 0.0 lightShadowAPI_ShadowColor[2] message:(pointLight.name + " light shadow color value (G) should default at 0.0")
        assert_float_equal 0.0 lightShadowAPI_ShadowColor[3] message:(pointLight.name + " light shadow color value (B) should default at 0.0")

        -- now changing the default settings
        pointLight.affectDiffuse = 0
        pointLight.affectSpecular = 0
        pointLight.useKelvin = true
        pointLight.shadowColor = color 255 255 255
        pointLight.baseobject.castShadows = false
        pointLight.on = false -- turning the light off zeroes out diffuse and specular in USD
            
        local exportPath = output_prefix + "test_photometric_light_gen_attr_export.usda"
        USDExporter.ExportFile exportPath exportOptions:exportOptions
        local stage = pyUsd.Stage.Open(exportPath)
        
        usd_light_path = "/" + pointLight.name
        usd_light = pyUsdLux.BoundableLightBase.Get stage usd_light_path
        
        lightDiffuseAttr = usd_light.GetDiffuseAttr()
        assert_float_equal 0.0 (lightDiffuseAttr.Get()) message:(pointLight.name + " light is not expected to have an effect of this light on the diffuse response of materials.")
        lightSpecularAttr = usd_light.GetSpecularAttr()
        assert_float_equal 0.0 (lightSpecularAttr.Get()) message:(pointLight.name + " light is not expected to have an effect of this light on the specular response of materials")
        
        -- keep normalized intensity on light (always)
        lightNormalizeAttr = usd_light.GetNormalizeAttr()
        assert_true (lightNormalizeAttr.Get()) message:(pointLight.name + " light is expected to use normalized intensity.")

        lightEnableColorTemperatureAttr = usd_light.GetEnableColorTemperatureAttr()
        assert_true (lightEnableColorTemperatureAttr.Get()) message:(pointLight.name + " light is expected to use color temprature (Kelvin).")

        lightShadowAPI = pyUsdLux.ShadowAPI(usd_light)
        lightShadowAPI_ShadowEnableAttr = lightShadowAPI.GetShadowEnableAttr()
        assert_false (lightShadowAPI_ShadowEnableAttr.Get()) message:(pointLight.name + " light is not expected to enable shadows to be cast by the light.")

        -- The shadow color of the 'free_light' is equal to (1.0, 1.0, 1.0)
        lightShadowAPI_ShadowColorAttr = lightShadowAPI.GetShadowColorAttr()
        lightShadowAPI_ShadowColor = lightShadowAPI_ShadowColorAttr.Get()
        assert_float_equal 1.0 lightShadowAPI_ShadowColor[1] message:(pointLight.name + " light shadow color value (R) should be at 1.0")
        assert_float_equal 1.0 lightShadowAPI_ShadowColor[2] message:(pointLight.name + " light shadow color value (G) should be at 1.0")
        assert_float_equal 1.0 lightShadowAPI_ShadowColor[3] message:(pointLight.name + " light shadow color value (B) should be at 1.0")
    ),

    -- Test that when the photometric light "On" property is set to false,
    -- both diffuse and specular are exported as 0 in USD, regardless of the
    -- affectDiffuse/affectSpecular settings.
    function photometric_light_on_off_export_test = (
        local exportPath = output_prefix + "test_photometric_light_on_off_export.usda"
        local exportOptions = USDExporter.CreateOptions()
        exportOptions.FileFormat = #ascii
        exportOptions.RootPrimPath = "/"
        exportOptions.Lights = true

        -- Create a point light with default diffuse/specular settings (both enabled)
        local pointLight = free_light()
        pointLight.type = #Free_Point
        pointLight.distribution = 2 -- uniform hemispherical/diffuse

        -- Verify defaults: affectDiffuse and affectSpecular are on
        assert_true pointLight.affectDiffuse message:"affectDiffuse should default to true"
        assert_true pointLight.affectSpecular message:"affectSpecular should default to true"

        -- Turn the light off
        pointLight.on = false

        USDExporter.ExportFile exportPath exportOptions:exportOptions
        local stage = pyUsd.Stage.Open(exportPath)

        local usd_light_path = "/" + pointLight.name
        local usd_light = pyUsdLux.BoundableLightBase.Get stage usd_light_path

        -- When the light is off, both diffuse and specular should be 0
        local lightDiffuseAttr = usd_light.GetDiffuseAttr()
        assert_float_equal 0.0 (lightDiffuseAttr.Get()) message:(pointLight.name + " light with 'on' set to false should export diffuse as 0.")
        local lightSpecularAttr = usd_light.GetSpecularAttr()
        assert_float_equal 0.0 (lightSpecularAttr.Get()) message:(pointLight.name + " light with 'on' set to false should export specular as 0.")

        -- Now turn the light back on and verify diffuse/specular are at their defaults (1.0)
        pointLight.on = true

        local exportPathOn = output_prefix + "test_photometric_light_on_off_on_export.usda"
        USDExporter.ExportFile exportPathOn exportOptions:exportOptions
        local stageOn = pyUsd.Stage.Open(exportPathOn)

        local usd_light_on = pyUsdLux.BoundableLightBase.Get stageOn usd_light_path

        local lightDiffuseAttrOn = usd_light_on.GetDiffuseAttr()
        assert_float_equal 1.0 (lightDiffuseAttrOn.Get()) message:(pointLight.name + " light with 'on' set to true should export diffuse as 1.0.")
        local lightSpecularAttrOn = usd_light_on.GetSpecularAttr()
        assert_float_equal 1.0 (lightSpecularAttrOn.Get()) message:(pointLight.name + " light with 'on' set to true should export specular as 1.0.")
    ),

    function photometric_point_light_attributes_export_test = (
        local exportOptions = USDExporter.CreateOptions()
        exportOptions.FileFormat = #ascii
        exportOptions.RootPrimPath = "/"
        exportOptions.Lights = true

        local pointLight = free_light()
        pointLight.type = #Free_Point
        pointLight.distribution = 2 -- uniform hemispherical/diffuse

        local exportPath = output_prefix + "test_photometric_point_light_diffuse_export.usda"
        USDExporter.ExportFile exportPath exportOptions:exportOptions
        local stage = pyUsd.Stage.Open(exportPath)
        
        usd_light_path = "/" + pointLight.name

        local objPrim = stage.GetPrimAtPath usd_light_path
        assert_true (objPrim.IsA(pyUsdLux.DiskLight)) message:(pointLight.name + " light was not properly exported to a USD LuxDiskLight.")

        usd_light = pyUsdLux.DiskLight.Get stage usd_light_path
        
        lightRadiusAttr = usd_light.GetRadiusAttr()
        assert_float_equal 0.001 (lightRadiusAttr.Get()) message:(pointLight.name + " light radius should always be set to`'0.001'")

        -- Now export when point light is using spherical light distribution
        pointLight.distribution = 0

        local exportPath = output_prefix + "test_photometric_point_light_spherical_export.usda"
        USDExporter.ExportFile exportPath exportOptions:exportOptions
        local stage = pyUsd.Stage.Open(exportPath)
        
        local objPrim = stage.GetPrimAtPath usd_light_path
        assert_true (objPrim.IsA(pyUsdLux.SphereLight)) message:(pointLight.name + " light was not properly exported to a USD LuxSphereLight.")

        usd_light = pyUsdLux.SphereLight.Get stage usd_light_path
        
        lightRadiusAttr = usd_light.GetRadiusAttr()
        assert_float_equal 0.001 (lightRadiusAttr.Get()) message:(pointLight.name + " light radius should always be set to`'0.001'")
        lightTreatAsPointAttr = usd_light.GetTreatAsPointAttr()
        assert_true (lightTreatAsPointAttr.Get()) message:(pointLight.name + " light should have its 'treatAsPoint' attribute active")

        -- Now export when point light is using spot light distribution
        pointLight.distribution = 1 -- spot

        local exportPath = output_prefix + "test_photometric_point_light_spot_export.usda"
        USDExporter.ExportFile exportPath exportOptions:exportOptions
        local stage = pyUsd.Stage.Open(exportPath)
        
        local objPrim = stage.GetPrimAtPath usd_light_path
        assert_true (objPrim.IsA(pyUsdLux.DiskLight)) message:(pointLight.name + " light was not properly exported to a USD LuxDiskLight.")

        usd_light = pyUsdLux.DiskLight.Get stage usd_light_path
        
        lightRadiusAttr = usd_light.GetRadiusAttr()
        assert_float_equal 0.001 (lightRadiusAttr.Get()) message:(pointLight.name + " light radius should always be set to`'0.001'")

        -- Now export when point light is using web light distribution
        pointLight.distribution = 3 -- web/IES

        local exportPath = output_prefix + "test_photometric_point_light_web_export.usda"
        USDExporter.ExportFile exportPath exportOptions:exportOptions
        local stage = pyUsd.Stage.Open(exportPath)
        
        local objPrim = stage.GetPrimAtPath usd_light_path
        assert_true (objPrim.IsA(pyUsdLux.DiskLight)) message:(pointLight.name + " light was not properly exported to a USD LuxDiskLight.")

        usd_light = pyUsdLux.DiskLight.Get stage usd_light_path
        
        lightRadiusAttr = usd_light.GetRadiusAttr()
        assert_float_equal 0.001 (lightRadiusAttr.Get()) message:(pointLight.name + " light radius should always be set to`'0.001'")
    ),

    function photometric_point_light_attributes_animation_export_test = (
        local exportOptions = USDExporter.CreateOptions()
        exportOptions.FileFormat = #ascii
        exportOptions.RootPrimPath = "/"
        exportOptions.Lights = true

        local pointLight = free_light()
        pointLight.type = #Free_Point
        pointLight.distribution = 2 -- uniform hemispherical/diffuse
        pointLight.useKelvin = true
        pointLight.useMultiplier = true
        
        with animate on 
        (
            -- Animate some attributes
            at time 10 move pointLight [0,0,10]
            at time 10 pointLight.kelvin = 5600
            -- when using color temperature, the filter color is the only attribute
            -- the composes the USD light color
            at time 10 pointLight.filterColor = color 155 155 155
            at time 10 pointLight.multiplier = 50  -- dimming 50%; influence intensity
        )
        
        -- Testing #animationRange as TimeMode
        -- Configure the time slider to go from 2 to 8.        
        local rangeStart = 2
        local rangeEnd = 8
        
        animationRange = interval rangeStart rangeEnd
        local exportPath = output_prefix + "test_photometric_point_light_animationrange_export.usda"
        exportOptions.TimeMode = #animationRange
        
        USDExporter.ExportFile exportPath exportOptions:exportOptions
        local stage = pyUsd.Stage.Open(exportPath)
        
        usd_light_path = "/" + pointLight.name
        usd_light = pyUsdLux.DiskLight.Get stage usd_light_path
        
        local xformable = pyUsdGeom.Xformable(usd_light)
        local lightRadiusAttr = usd_light.GetRadiusAttr()
        local lightColorAttr = usd_light.GetColorAttr()
        local lightColorTemperatureAttr = usd_light.GetColorTemperatureAttr()
        local lightIntensityAttr = usd_light.GetIntensityAttr()
        -- hardcoding expected value since the computation of the USD intensity is not easy
        expectedIntensity = #(4616.2656,4343.5747,4012.4502,3652.109,3291.7673,2960.6433,2687.9521)

        for timeCode in rangeStart to rangeEnd do (
            local t = at time timeCode pointLight.transform
            assert_true (compareUsdAndMaxTransforms (xformable.GetLocalTransformation(timeCode)) t) message:("The light's transform is wrong when exporting with #animationRange at t = " + timeCode as string)
            assert_float_equal 0.001 (lightRadiusAttr.Get(timeCode)) message:(pointLight.name + " light radius should always be set to`'0.001'")

            local c = at time timeCode pointLight.filterColor
            local lightColorValue = lightColorAttr.Get(timeCode)
            assert_float_equal (c.r/255.0) lightColorValue[1] message:"The light color value (R) should follow light filter color value"
            assert_float_equal (c.g/255.0) lightColorValue[2] message:"The light color value (G) should follow light filter color value"
            assert_float_equal (c.b/255.0) lightColorValue[3] message:"The light color value (B) should follow light filter color value"
            
            local k = at time timeCode pointLight.kelvin
            assert_float_equal k (lightColorTemperatureAttr.Get(timeCode)) message:"The light color temperature value should follow animation settings"
                
            assert_float_equal expectedIntensity[timeCode-1] (lightIntensityAttr.Get(timeCode)) message:"The light intensity should follow dimmed value from animated light settings"
        )
    ),

    function photometric_sphere_light_attributes_export_test = (
        local exportOptions = USDExporter.CreateOptions()
        exportOptions.FileFormat = #ascii
        exportOptions.RootPrimPath = "/"
        exportOptions.Lights = true

        local sphereLight = free_light()
        sphereLight.type = #Free_Sphere
        sphereLight.distribution = 0 -- uniform spherical

        local exportPath = output_prefix + "test_photometric_sphere_light_spherical_export.usda"
        USDExporter.ExportFile exportPath exportOptions:exportOptions
        local stage = pyUsd.Stage.Open(exportPath)
        
        usd_light_path = "/" + sphereLight.name
        
        local objPrim = stage.GetPrimAtPath usd_light_path
        assert_true (objPrim.IsA(pyUsdLux.SphereLight)) message:(sphereLight.name + " light was not properly exported to a USD LuxSphereLight.")

        usd_light = pyUsdLux.SphereLight.Get stage usd_light_path
        
        lightRadiusAttr = usd_light.GetRadiusAttr()
        assert_float_equal sphereLight.radius (lightRadiusAttr.Get()) message:(sphereLight.name + " light radius is not as specified.")

        -- Now export when point light is using web light distribution
        sphereLight.distribution = 3 -- web/IES
        sphereLight.radius = 10

        local exportPath = output_prefix + "test_photometric_sphere_light_web_export.usda"
        USDExporter.ExportFile exportPath exportOptions:exportOptions
        local stage = pyUsd.Stage.Open(exportPath)
        
        local objPrim = stage.GetPrimAtPath usd_light_path
        assert_true (objPrim.IsA(pyUsdLux.SphereLight)) message:(sphereLight.name + " light was not properly exported to a USD LuxSphereLight.")

        usd_light = pyUsdLux.SphereLight.Get stage usd_light_path
        
        lightRadiusAttr = usd_light.GetRadiusAttr()
        assert_float_equal sphereLight.radius (lightRadiusAttr.Get()) message:(sphereLight.name + " light radius is not as specified.")
    ),

    function photometric_sphere_light_attributes_animation_export_test = (
        local exportOptions = USDExporter.CreateOptions()
        exportOptions.FileFormat = #ascii
        exportOptions.RootPrimPath = "/"
        exportOptions.Lights = true

        local sphereLight = free_light()
        sphereLight.type = #Free_Sphere
        sphereLight.distribution = 0 -- uniform spherical

        with animate on 
        (
            -- Animate some attributes
            at time 10 move sphereLight [0,0,10]
            at time 10 sphereLight.radius = 40
            -- see point light animation export for general animatable attributs export
        )
        
        -- Testing #animationRange as TimeMode
        -- Configure the time slider to go from 2 to 8.        
        local rangeStart = 2
        local rangeEnd = 8
        
        animationRange = interval rangeStart rangeEnd
        local exportPath = output_prefix + "photometric_sphere_light_attributes_animation_export_test.usda"
        exportOptions.TimeMode = #animationRange
        
        USDExporter.ExportFile exportPath exportOptions:exportOptions
        local stage = pyUsd.Stage.Open(exportPath)
        
        usd_light_path = "/" + sphereLight.name
        
        local objPrim = stage.GetPrimAtPath usd_light_path
        assert_true (objPrim.IsA(pyUsdLux.SphereLight)) message:(sphereLight.name + " light was not properly exported to a USD LuxSphereLight.")

        usd_light = pyUsdLux.SphereLight.Get stage usd_light_path
        
        local xformable = pyUsdGeom.Xformable(usd_light)
        lightRadiusAttr = usd_light.GetRadiusAttr()
        for timeCode in rangeStart to rangeEnd do (
            local t = at time timeCode sphereLight.transform
            assert_true (compareUsdAndMaxTransforms (xformable.GetLocalTransformation(timeCode)) t) message:("The light's transform is wrong when exporting with #animationRange at t = " + timeCode as string)
            local r = at time timeCode sphereLight.radius
            assert_float_equal r (lightRadiusAttr.Get(timeCode)) message:(sphereLight.name + " light radius is not as specified.")
        )
    ),

    function photometric_disk_light_attributes_export_test = (
        local exportOptions = USDExporter.CreateOptions()
        exportOptions.FileFormat = #ascii
        exportOptions.RootPrimPath = "/"
        exportOptions.Lights = true

        local diskLight = free_light()
        diskLight.type = #Free_Disc
        diskLight.distribution = 2 -- uniform hemispherical/diffuse

        local exportPath = output_prefix + "test_photometric_disk_light_diffuse_export.usda"
        USDExporter.ExportFile exportPath exportOptions:exportOptions
        local stage = pyUsd.Stage.Open(exportPath)
        
        usd_light_path = "/" + diskLight.name

        local objPrim = stage.GetPrimAtPath usd_light_path
        assert_true (objPrim.IsA(pyUsdLux.DiskLight)) message:(diskLight.name + " light was not properly exported to a USD LuxDiskLight.")

        usd_light = pyUsdLux.DiskLight.Get stage usd_light_path
        
        lightRadiusAttr = usd_light.GetRadiusAttr()
        assert_float_equal diskLight.radius (lightRadiusAttr.Get()) message:(diskLight.name + " light radius is not as specified.")

        -- Now export when disk light is using spherical light distribution
        diskLight.distribution = 0
        diskLight.radius = 20

        local exportPath = output_prefix + "test_photometric_disk_light_spherical_export.usda"
        USDExporter.ExportFile exportPath exportOptions:exportOptions
        local stage = pyUsd.Stage.Open(exportPath)
        
        local objPrim = stage.GetPrimAtPath usd_light_path
        assert_true (objPrim.IsA(pyUsdLux.SphereLight)) message:(diskLight.name + " light was not properly exported to a USD LuxSphereLight.")

        usd_light = pyUsdLux.SphereLight.Get stage usd_light_path
        
        lightRadiusAttr = usd_light.GetRadiusAttr()
        assert_float_equal diskLight.radius (lightRadiusAttr.Get()) message:(diskLight.name + " light radius is not as specified.")

        -- Now export when disk light is using spot light distribution
        diskLight.distribution = 1 -- spot

        local exportPath = output_prefix + "test_photometric_disk_light_spot_export.usda"
        USDExporter.ExportFile exportPath exportOptions:exportOptions
        local stage = pyUsd.Stage.Open(exportPath)
        
        local objPrim = stage.GetPrimAtPath usd_light_path
        assert_true (objPrim.IsA(pyUsdLux.DiskLight)) message:(diskLight.name + " light was not properly exported to a USD LuxDiskLight.")

        usd_light = pyUsdLux.DiskLight.Get stage usd_light_path
        
        lightRadiusAttr = usd_light.GetRadiusAttr()
        assert_float_equal diskLight.radius (lightRadiusAttr.Get()) message:(diskLight.name + " light radius is not as specified.")

        -- Now export when disk light is using web light distribution
        diskLight.distribution = 3 -- web/IES

        local exportPath = output_prefix + "test_photometric_disk_light_web_export.usda"
        USDExporter.ExportFile exportPath exportOptions:exportOptions
        local stage = pyUsd.Stage.Open(exportPath)
        
        local objPrim = stage.GetPrimAtPath usd_light_path
        assert_true (objPrim.IsA(pyUsdLux.DiskLight)) message:(diskLight.name + " light was not properly exported to a USD LuxDiskLight.")

        usd_light = pyUsdLux.DiskLight.Get stage usd_light_path
        
        lightRadiusAttr = usd_light.GetRadiusAttr()
        assert_float_equal diskLight.radius (lightRadiusAttr.Get()) message:(diskLight.name + " light radius is not as specified.")
    ),

    function photometric_disk_light_attributes_animation_export_test = (
        local exportOptions = USDExporter.CreateOptions()
        exportOptions.FileFormat = #ascii
        exportOptions.RootPrimPath = "/"
        exportOptions.Lights = true

        local diskLight = free_light()
        diskLight.type = #Free_Disc
        diskLight.distribution = 2 -- uniform hemispherical/diffuse

        with animate on 
        (
            -- Animate some attributes
            at time 10 move diskLight [0,0,10]
            at time 10 diskLight.radius = 50
            -- see point light animation export for general animatable attributs export
        )
        
        -- Testing #animationRange as TimeMode
        -- Configure the time slider to go from 2 to 8.        
        local rangeStart = 2
        local rangeEnd = 8
        
        animationRange = interval rangeStart rangeEnd
        local exportPath = output_prefix + "test_photometric_sphere_light_animationrange_export.usda"
        exportOptions.TimeMode = #animationRange
        USDExporter.ExportFile exportPath exportOptions:exportOptions
        local stage = pyUsd.Stage.Open(exportPath)
        
        usd_light_path = "/" + diskLight.name

        local objPrim = stage.GetPrimAtPath usd_light_path
        assert_true (objPrim.IsA(pyUsdLux.DiskLight)) message:(diskLight.name + " light was not properly exported to a USD LuxDiskLight.")

        usd_light = pyUsdLux.DiskLight.Get stage usd_light_path

        local xformable = pyUsdGeom.Xformable(usd_light)
        lightRadiusAttr = usd_light.GetRadiusAttr()
        for timeCode in rangeStart to rangeEnd do (
            local t = at time timeCode diskLight.transform
            assert_true (compareUsdAndMaxTransforms (xformable.GetLocalTransformation(timeCode)) t) message:("The light's transform is wrong when exporting with #animationRange at t = " + timeCode as string)
            local r = at time timeCode diskLight.radius
            assert_float_equal r (lightRadiusAttr.Get(timeCode)) message:(diskLight.name + " light radius is not as specified.")
        )
    ),

    function photometric_line_light_attributes_export_test = (
        local exportOptions = USDExporter.CreateOptions()
        exportOptions.FileFormat = #ascii
        exportOptions.RootPrimPath = "/"
        exportOptions.Lights = true

        local lineLight = free_light()
        lineLight.type = #Free_Line
        lineLight.distribution = 2 -- uniform hemispherical/diffuse

        local exportPath = output_prefix + "test_photometric_line_light_diffuse_export.usda"
        USDExporter.ExportFile exportPath exportOptions:exportOptions
        local stage = pyUsd.Stage.Open(exportPath)
        
        usd_light_path = "/" + lineLight.name

        local objPrim = stage.GetPrimAtPath usd_light_path
        assert_true (objPrim.IsA(pyUsdLux.RectLight)) message:(lineLight.name + " light was not properly exported to a USD LuxRectLight.")

        usd_light = pyUsdLux.RectLight.Get stage usd_light_path
        
        lightWidthAttr = usd_light.GetWidthAttr()
        assert_float_equal 0.1 (lightWidthAttr.Get()) message:(lineLight.name + " light width should always be set to`'0.1'")
        lightHeightAttr = usd_light.GetHeightAttr()
        assert_float_equal lineLight.length (lightHeightAttr.Get()) message:(lineLight.name + " light height is not as specified with 3ds Max light length.")

        -- Now export when point light is using spherical light distribution
        lineLight.distribution = 0
        lineLight.length = 40  -- changing default length

        local exportPath = output_prefix + "test_photometric_line_light_spherical_export.usda"
        USDExporter.ExportFile exportPath exportOptions:exportOptions
        local stage = pyUsd.Stage.Open(exportPath)
        
        local objPrim = stage.GetPrimAtPath usd_light_path
        -- the cylinder light needs to be reoriented for USD
        -- 3ds Max light main axis is the y-axis and USD expects it on the x-axis
        assert_true (pyUsd.Prim.IsValid(objPrim)) message:(lineLight.name + " light was not properly exported to a USD prim.")
        assert_true (objPrim.IsA(pyUsdGeom.Xform)) message:(lineLight.name + " light needs its xform transfo.")

        usd_light_path = "/" + lineLight.name + "/" + lineLight.name + "_Light"
        objPrim = stage.GetPrimAtPath usd_light_path
        assert_true (objPrim.IsA(pyUsdLux.CylinderLight)) message:(lineLight.name + " light was not properly exported to a USD LuxCylinderLight.")

        usd_light = pyUsdLux.CylinderLight.Get stage usd_light_path
        
        lightRadiusAttr = usd_light.GetRadiusAttr()
        assert_float_equal 0.001 (lightRadiusAttr.Get()) message:(lineLight.name + " light radius should always be set to`'0.001'")
        lightTreatAsLineAttr = usd_light.GetTreatAsLineAttr()
        assert_true (lightTreatAsLineAttr.Get()) message:(lineLight.name + " light should have its 'treatAsLine' attribute active")
        lightLengthAttr = usd_light.GetLengthAttr()
        assert_float_equal lineLight.length (lightLengthAttr.Get()) message:(lineLight.name + " light length is not as specified.")

        -- Now export when point light is using spot light distribution
        lineLight.distribution = 1 -- spot

        local exportPath = output_prefix + "test_photometric_line_light_spot_export.usda"
        USDExporter.ExportFile exportPath exportOptions:exportOptions
        local stage = pyUsd.Stage.Open(exportPath)
        
        usd_light_path = "/" + lineLight.name
        local objPrim = stage.GetPrimAtPath usd_light_path
        assert_true (objPrim.IsA(pyUsdLux.RectLight)) message:(lineLight.name + " light was not properly exported to a USD LuxRectLight.")

        usd_light = pyUsdLux.RectLight.Get stage usd_light_path
        
        lightWidthAttr = usd_light.GetWidthAttr()
        assert_float_equal 0.1 (lightWidthAttr.Get()) message:(lineLight.name + " light radius should always be set to`'0.001'")
        lightHeightAttr = usd_light.GetHeightAttr()
        assert_float_equal lineLight.length (lightHeightAttr.Get()) message:(lineLight.name + " light height is not as specified with 3ds Max light length.")

        -- Now export when point light is using web light distribution
        lineLight.distribution = 3 -- web/IES

        local exportPath = output_prefix + "test_photometric_line_light_web_export.usda"
        USDExporter.ExportFile exportPath exportOptions:exportOptions
        local stage = pyUsd.Stage.Open(exportPath)
        
        local objPrim = stage.GetPrimAtPath usd_light_path
        assert_true (objPrim.IsA(pyUsdLux.RectLight)) message:(lineLight.name + " light was not properly exported to a USD LuxRectLight.")

        usd_light = pyUsdLux.RectLight.Get stage usd_light_path
        
        lightWidthAttr = usd_light.GetWidthAttr()
        assert_float_equal 0.1 (lightWidthAttr.Get()) message:(lineLight.name + " light width should always be set to`'0.1'")
        lightHeightAttr = usd_light.GetHeightAttr()
        assert_float_equal lineLight.length (lightHeightAttr.Get()) message:(lineLight.name + " light height is not as specified with 3ds Max light length.")
    ),

    function photometric_line_light_attributes_animation_export_test = (
        local exportOptions = USDExporter.CreateOptions()
        exportOptions.FileFormat = #ascii
        exportOptions.RootPrimPath = "/"
        exportOptions.Lights = true

        local lineLight = free_light()
        lineLight.type = #Free_Line
        lineLight.distribution = 2 -- uniform hemispherical/diffuse

        with animate on 
        (
            -- Animate some attributes
            at time 10 move lineLight [0,0,10]
            at time 10 lineLight.length = 120
            -- see point light animation export for general animatable attributs export
        )
        
        -- Testing #animationRange as TimeMode
        -- Configure the time slider to go from 2 to 8.        
        local rangeStart = 2
        local rangeEnd = 8
        
        animationRange = interval rangeStart rangeEnd
        local exportPath = output_prefix + "test_photometric_line_light_animationrange_export.usda"
        exportOptions.TimeMode = #animationRange
        USDExporter.ExportFile exportPath exportOptions:exportOptions
        local stage = pyUsd.Stage.Open(exportPath)
        
        usd_light_path = "/" + lineLight.name

        local objPrim = stage.GetPrimAtPath usd_light_path
        assert_true (objPrim.IsA(pyUsdLux.RectLight)) message:(lineLight.name + " light was not properly exported to a USD LuxRectLight.")

        usd_light = pyUsdLux.RectLight.Get stage usd_light_path
        
        local xformable = pyUsdGeom.Xformable(usd_light)
        lightWidthAttr = usd_light.GetWidthAttr()
        lightHeightAttr = usd_light.GetHeightAttr()
        for timeCode in rangeStart to rangeEnd do (
            local t = at time timeCode lineLight.transform
            assert_true (compareUsdAndMaxTransforms (xformable.GetLocalTransformation(timeCode)) t) message:("The light's transform is wrong when exporting with #animationRange at t = " + timeCode as string)
            assert_float_equal 0.1 (lightWidthAttr.Get(timeCode)) message:(lineLight.name + " light width should always be set to`'0.1'")
            local l = at time timeCode lineLight.length
            assert_float_equal l (lightHeightAttr.Get(timeCode)) message:(lineLight.name + " light height is not as specified with 3ds Max light length.")
        )
    ),

    function photometric_cylinder_light_attributes_export_test = (
        local exportOptions = USDExporter.CreateOptions()
        exportOptions.FileFormat = #ascii
        exportOptions.RootPrimPath = "/"
        exportOptions.Lights = true

        local cylinderLight = free_light()
        cylinderLight.type = #Free_Cylinder

        cylinderLight.distribution = 0
        cylinderLight.length = 40  -- changing default length

        local exportPath = output_prefix + "test_photometric_cylinder_light_spherical_export.usda"
        USDExporter.ExportFile exportPath exportOptions:exportOptions
        local stage = pyUsd.Stage.Open(exportPath)
        
        local usd_light_path = "/" + cylinderLight.name
        local objPrim = stage.GetPrimAtPath usd_light_path
        -- the cylinder light needs to be reoriented for USD
        -- 3ds Max light main axis is the y-axis and USD expects it on the x-axis
        assert_true (pyUsd.Prim.IsValid(objPrim)) message:(cylinderLight.name + " light was not properly exported to a USD prim.")
        assert_true (objPrim.IsA(pyUsdGeom.Xform)) message:(cylinderLight.name + " light needs its xform transfo.")

        usd_light_path = "/" + cylinderLight.name + "/" + cylinderLight.name + "_Light"
        objPrim = stage.GetPrimAtPath usd_light_path
        assert_true (objPrim.IsA(pyUsdLux.CylinderLight)) message:(cylinderLight.name + " light was not properly exported to a USD LuxCylinderLight.")

        usd_light = pyUsdLux.CylinderLight.Get stage usd_light_path
        
        lightRadiusAttr = usd_light.GetRadiusAttr()
        assert_float_equal cylinderLight.radius (lightRadiusAttr.Get()) message:(cylinderLight.name + " light radius is not as specified.")
        lightLengthAttr = usd_light.GetLengthAttr()
        assert_float_equal cylinderLight.length (lightLengthAttr.Get()) message:(cylinderLight.name + " light length is not as specified.")

        -- Now export when point light is using web light distribution
        cylinderLight.distribution = 3 -- web/IES

        local exportPath = output_prefix + "test_photometric_cylinder_light_web_export.usda"
        USDExporter.ExportFile exportPath exportOptions:exportOptions
        local stage = pyUsd.Stage.Open(exportPath)
        
        usd_light_path = "/" + cylinderLight.name
        objPrim = stage.GetPrimAtPath usd_light_path
        -- the cylinder light needs to be reoriented for USD
        -- 3ds Max light main axis is the y-axis and USD expects it on the x-axis
        assert_true (pyUsd.Prim.IsValid(objPrim)) message:(cylinderLight.name + " light was not properly exported to a USD prim.")
        assert_true (objPrim.IsA(pyUsdGeom.Xform)) message:(cylinderLight.name + " light needs its xform transfo.")

        usd_light_path = "/" + cylinderLight.name + "/" + cylinderLight.name + "_Light"
        local objPrim = stage.GetPrimAtPath usd_light_path
        assert_true (objPrim.IsA(pyUsdLux.CylinderLight)) message:(cylinderLight.name + " light was not properly exported to a USD LuxCylinderLight.")

        usd_light = pyUsdLux.CylinderLight.Get stage usd_light_path
        
        lightRadiusAttr = usd_light.GetRadiusAttr()
        assert_float_equal cylinderLight.radius (lightRadiusAttr.Get()) message:(cylinderLight.name + " light radius is not as specified.")
        lightLengthAttr = usd_light.GetLengthAttr()
        assert_float_equal cylinderLight.length (lightLengthAttr.Get()) message:(cylinderLight.name + " light length is not as specified.")
    ),

    function photometric_cylinder_light_attributes_animation_export_test = (
        local exportOptions = USDExporter.CreateOptions()
        exportOptions.FileFormat = #ascii
        exportOptions.RootPrimPath = "/"
        exportOptions.Lights = true

        local cylinderLight = free_light()
        cylinderLight.type = #Free_Cylinder

        cylinderLight.distribution = 0
        cylinderLight.length = 40  -- changing default length

        with animate on 
        (
            -- Animate some attributes
            at time 10 move cylinderLight [0,0,10]
            at time 10 rotate cylinderLight (eulerangles 0 0 45)
            at time 10 cylinderLight.length = 100
            at time 10 cylinderLight.radius = 15
            -- see point light animation export for general animatable attributs export
        )
        
        -- Testing #animationRange as TimeMode
        -- Configure the time slider to go from 2 to 8.        
        local rangeStart = 2
        local rangeEnd = 8
        
        animationRange = interval rangeStart rangeEnd
        local exportPath = output_prefix + "test_photometric_cylinder_light_animationrange_export.usda"
        exportOptions.TimeMode = #animationRange
        USDExporter.ExportFile exportPath exportOptions:exportOptions
        local stage = pyUsd.Stage.Open(exportPath)
        
        local usd_light_path = "/" + cylinderLight.name
        local objPrim = stage.GetPrimAtPath usd_light_path
        -- the cylinder light needs to be reoriented for USD
        -- 3ds Max light main axis is the y-axis and USD expects it on the x-axis
        assert_true (pyUsd.Prim.IsValid(objPrim)) message:(cylinderLight.name + " light was not properly exported to a USD prim.")
        assert_true (objPrim.IsA(pyUsdGeom.Xform)) message:(cylinderLight.name + " light needs its xform transfo.")
        local xformable = pyUsdGeom.Xformable(objPrim)
        for timeCode in rangeStart to rangeEnd do (
            local t = at time timeCode cylinderLight.transform
            assert_true (compareUsdAndMaxTransforms (xformable.GetLocalTransformation(timeCode)) t) message:("The light's transform is wrong when exporting with #animationRange at t = " + timeCode as string)
        )

        usd_light_path = "/" + cylinderLight.name + "/" + cylinderLight.name + "_Light"
        objPrim = stage.GetPrimAtPath usd_light_path
        assert_true (objPrim.IsA(pyUsdLux.CylinderLight)) message:(cylinderLight.name + " light was not properly exported to a USD LuxCylinderLight.")

        usd_light = pyUsdLux.CylinderLight.Get stage usd_light_path
        
        lightRadiusAttr = usd_light.GetRadiusAttr()
        lightLengthAttr = usd_light.GetLengthAttr()
        for timeCode in rangeStart to rangeEnd do (
            local r = at time timeCode cylinderLight.radius
            assert_float_equal r (lightRadiusAttr.Get(timeCode)) message:(cylinderLight.name + " light radius is not as specified.")
            local l = at time timeCode cylinderLight.length
            assert_float_equal l (lightLengthAttr.Get(timeCode)) message:(cylinderLight.name + " light length is not as specified.")
        )
    ),

    function photometric_rect_light_attributes_export_test = (
        local exportOptions = USDExporter.CreateOptions()
        exportOptions.FileFormat = #ascii
        exportOptions.RootPrimPath = "/"
        exportOptions.Lights = true

        local rectLight = free_light()
        rectLight.type = #Free_Rectangle
        rectLight.distribution = 2 -- uniform hemispherical/diffuse

        local exportPath = output_prefix + "test_photometric_rect_light_diffuse_export.usda"
        USDExporter.ExportFile exportPath exportOptions:exportOptions
        local stage = pyUsd.Stage.Open(exportPath)
        
        usd_light_path = "/" + rectLight.name

        local objPrim = stage.GetPrimAtPath usd_light_path
        assert_true (objPrim.IsA(pyUsdLux.RectLight)) message:(rectLight.name + " light was not properly exported to a USD LuxRectLight.")

        usd_light = pyUsdLux.RectLight.Get stage usd_light_path
        
        lightWidthAttr = usd_light.GetWidthAttr()
        assert_float_equal rectLight.width (lightWidthAttr.Get()) message:(rectLight.name + " light width is not as specified.")
        lightHeightAttr = usd_light.GetHeightAttr()
        assert_float_equal rectLight.length (lightHeightAttr.Get()) message:(rectLight.name + " light height is not as specified.")

        -- Now export when point light is using spherical light distribution
        rectLight.distribution = 0
        rectLight.length = 40  -- changing default length

        local exportPath = output_prefix + "test_photometric_rect_light_spherical_export.usda"
        USDExporter.ExportFile exportPath exportOptions:exportOptions
        local stage = pyUsd.Stage.Open(exportPath)
        
        usd_light_path = "/" + rectLight.name
        objPrim = stage.GetPrimAtPath usd_light_path
        -- the cylinder light needs to be reoriented for USD
        -- 3ds Max light main axis is the y-axis and USD expects it on the x-axis
        assert_true (pyUsd.Prim.IsValid(objPrim)) message:(rectLight.name + " light was not properly exported to a USD prim.")
        assert_true (objPrim.IsA(pyUsdGeom.Xform)) message:(rectLight.name + " light needs its xform transfo.")

        usd_light_path = "/" + rectLight.name + "/" + rectLight.name + "_Light"
        local objPrim = stage.GetPrimAtPath usd_light_path
        assert_true (objPrim.IsA(pyUsdLux.CylinderLight)) message:(rectLight.name + " light was not properly exported to a USD LuxCylinderLight.")

        usd_light = pyUsdLux.CylinderLight.Get stage usd_light_path
        
        lightRadiusAttr = usd_light.GetRadiusAttr()
        assert_float_equal (rectLight.width / 2.0) (lightRadiusAttr.Get()) message:(rectLight.name + " light radius should be set to half the light width")
        lightLengthAttr = usd_light.GetLengthAttr()
        assert_float_equal rectLight.length (lightLengthAttr.Get()) message:(rectLight.name + " light length is not as specified.")

        -- Now export when point light is using spot light distribution
        rectLight.distribution = 1 -- spot

        local exportPath = output_prefix + "test_photometric_rect_light_spot_export.usda"
        USDExporter.ExportFile exportPath exportOptions:exportOptions
        local stage = pyUsd.Stage.Open(exportPath)
        
        usd_light_path = "/" + rectLight.name
        local objPrim = stage.GetPrimAtPath usd_light_path
        assert_true (objPrim.IsA(pyUsdLux.RectLight)) message:(rectLight.name + " light was not properly exported to a USD LuxRectLight.")

        usd_light = pyUsdLux.RectLight.Get stage usd_light_path
        
        lightWidthAttr = usd_light.GetWidthAttr()
        assert_float_equal rectLight.width (lightWidthAttr.Get()) message:(rectLight.name + " light width is not as specified.")
        lightHeightAttr = usd_light.GetHeightAttr()
        assert_float_equal rectLight.length (lightHeightAttr.Get()) message:(rectLight.name + " light height is not as specified.")

        -- Now export when point light is using web light distribution
        rectLight.distribution = 3 -- web/IES

        local exportPath = output_prefix + "test_photometric_rect_light_web_export.usda"
        USDExporter.ExportFile exportPath exportOptions:exportOptions
        local stage = pyUsd.Stage.Open(exportPath)
        
        local objPrim = stage.GetPrimAtPath usd_light_path
        assert_true (objPrim.IsA(pyUsdLux.RectLight)) message:(rectLight.name + " light was not properly exported to a USD LuxRectLight.")

        usd_light = pyUsdLux.RectLight.Get stage usd_light_path
        
        lightWidthAttr = usd_light.GetWidthAttr()
        assert_float_equal rectLight.width (lightWidthAttr.Get()) message:(rectLight.name + " light width is not as specified.")
        lightHeightAttr = usd_light.GetHeightAttr()
        assert_float_equal rectLight.length (lightHeightAttr.Get()) message:(rectLight.name + " light height is not as specified.")
    ),

    function photometric_rect_light_attributes_animation_export_test = (
        local exportOptions = USDExporter.CreateOptions()
        exportOptions.FileFormat = #ascii
        exportOptions.RootPrimPath = "/"
        exportOptions.Lights = true

        local rectLight = free_light()
        rectLight.type = #Free_Rectangle
        rectLight.distribution = 2 -- uniform hemispherical/diffuse

        with animate on 
        (
            -- Animate some attributes
            at time 10 move rectLight [0,0,20]
            at time 10 rotate rectLight (eulerangles 0 0 -45)
            at time 10 rectLight.length = 100
            at time 10 rectLight.width = 35
            -- see point light animation export for general animatable attributs export
        )
        
        -- Testing #animationRange as TimeMode
        -- Configure the time slider to go from 2 to 8.        
        local rangeStart = 2
        local rangeEnd = 8
        
        animationRange = interval rangeStart rangeEnd
        local exportPath = output_prefix + "test_photometric_rect_light_animationrange_export.usda"
        exportOptions.TimeMode = #animationRange
        USDExporter.ExportFile exportPath exportOptions:exportOptions
        local stage = pyUsd.Stage.Open(exportPath)
        
        usd_light_path = "/" + rectLight.name

        local objPrim = stage.GetPrimAtPath usd_light_path
        assert_true (objPrim.IsA(pyUsdLux.RectLight)) message:(rectLight.name + " light was not properly exported to a USD LuxRectLight.")

        usd_light = pyUsdLux.RectLight.Get stage usd_light_path
        
        local xformable = pyUsdGeom.Xformable(usd_light)
        lightWidthAttr = usd_light.GetWidthAttr()
        lightHeightAttr = usd_light.GetHeightAttr()
        for timeCode in rangeStart to rangeEnd do (
            local t = at time timeCode rectLight.transform
            assert_true (compareUsdAndMaxTransforms (xformable.GetLocalTransformation(timeCode)) t) message:("The light's transform is wrong when exporting with #animationRange at t = " + timeCode as string)
            local w = at time timeCode rectLight.width
            assert_float_equal w (lightWidthAttr.Get(timeCode)) message:(rectLight.name + " light width is not as specified.")
            local l = at time timeCode rectLight.length
            assert_float_equal l (lightHeightAttr.Get(timeCode)) message:(rectLight.name + " light height is not as specified.")
        )
    ),

    function photometric_sun_positioner_export_test = (
        local exportOptions = USDExporter.CreateOptions()
        exportOptions.FileFormat = #ascii
        exportOptions.RootPrimPath = "/"
        exportOptions.Lights = true

        local sun_light = sun_positioner()

        local exportPath = output_prefix + "test_photometric_sun_positioner_export.usda"
        USDExporter.ExportFile exportPath exportOptions:exportOptions
        local stage = pyUsd.Stage.Open(exportPath)
        
        usd_light_path = "/" + sun_light.name

        local objPrim = stage.GetPrimAtPath usd_light_path
        -- the SunPositioner light needs to orient the prim's z-axis as the sun direction
        -- the direction is given based on the instance position
        assert_true (pyUsd.Prim.IsValid(objPrim)) message:(sun_light.name + " light was not properly exported to a USD prim.")
        assert_true (objPrim.IsA(pyUsdGeom.Xform)) message:(sun_light.name + " light needs its xform transfo.")

        usd_light_path = "/" + sun_light.name + "/" + sun_light.name + "_Light"
        
        local objPrim = stage.GetPrimAtPath usd_light_path
        assert_true (objPrim.IsA(pyUsdLux.DistantLight)) message:(sun_light.name + " light was not properly exported to a USD LuxDistantLight.")

        -- by default, the environment map is not installed
        local usd_light = pyUsdLux.DistantLight.Get stage usd_light_path
        local lightColorAttr = usd_light.GetColorAttr()
        assert_false (lightColorAttr.IsAuthored()) message:(sun_light.name + " light should not have its color set")
        local lightIntensityAttr = usd_light.GetIntensityAttr()
        assert_float_equal 50000.f (lightIntensityAttr.Get()) message:(sun_light.name + " light must have its intensity set to '50,000.f' (USD default)")
            
        -- note: unable to create a default environment map thru scripting
        -- using a pre-configured file to have a proper test environment
        quickReset()
        loadmaxfile ((getFilenamePath (getSourceFileName())) + "data\\" + "light-sun-envmap.max") useFileUnits:true
        -- sun positioner entity name
        light_name = "MySun"
        -- light object orientation and position
        -- rotx:-13.471  roty:-41.297  rotz:82.434  (Euler ZYX)
        -- posx:-10  posy:10  posz:10
        -- sun direction is almost oriented on the z-axis (world space)
        local expectedTransform = (pyGf.Matrix4d 0.0989244  0.744752  0.659969  0  -0.943778  0.280452  -0.175015  0  -0.315432  -0.605551  0.730624  0 -10 10 10 1)

        local exportPath = output_prefix + "test_photometric_sun_positioner_envmap_export.usda"
        USDExporter.ExportFile exportPath exportOptions:exportOptions
        local stage = pyUsd.Stage.Open(exportPath)

        usd_light_path = "/" + light_name

        local objPrim = stage.GetPrimAtPath usd_light_path
        -- the SunPositioner light needs to orient the prim's z-axis as the sun direction
        -- the direction is given based on the instance position
        assert_true (pyUsd.Prim.IsValid(objPrim)) message:(light_name + " light was not properly exported to a USD prim.")
        assert_true (objPrim.IsA(pyUsdGeom.Xform)) message:(light_name + " light needs its xform transfo.")
        local xformable = pyUsdGeom.Xformable(objPrim)
        local exportedTransform = xformable.GetLocalTransformation()
        assert_true(compare2dMatrix exportedTransform expectedTransform 4 4) message:"Incorrect sun transform."
        
        usd_light_path = "/" + light_name + "/" + light_name + "_Light"
        
        local objPrim = stage.GetPrimAtPath usd_light_path
        assert_true (objPrim.IsA(pyUsdLux.DistantLight)) message:(light_name + " light was not properly exported to a USD LuxDistantLight.")
        
        local usd_light = pyUsdLux.DistantLight.Get stage usd_light_path
        local lightColorAttr = usd_light.GetColorAttr()
        assert_true (lightColorAttr.IsAuthored()) message:(light_name + " light should have its color set")
        local lightIntensityAttr = usd_light.GetIntensityAttr()
        assert_float_equal 3.f (lightIntensityAttr.Get()) message:(light_name + " light must have its intensity set to '3.f'")
        local light_imageable = pyUsdGeom.Imageable (usd_light)
        local expectedTransform = (pyGf.Matrix4d 0.280316078 0.94320776 -0.00018235648  0  -0.94320214988 0.2803153115  0.0032611217 0 0.0032298288407 -0.0007666744969 0.99999486414 0 -10 10 10 1)
        local exportedWorldTransform = (light_imageable.ComputeLocalToWorldTransform(0))
        assert_true(compare2dMatrix exportedWorldTransform expectedTransform 4 4) message:"Incorrect sun direction in world."
    ),

    -- Helper function to check if USD version supports spline/curves export (24.11+)
    -- Returns true if supported, false otherwise (and prints skip message)
    fn supportsSplineCurves testName = (
        usdVersion = pyUsd.GetVersion()
        local supportCurves = (usdVersion[2] > 24)
        if not supportCurves then (
            format "Skipping %: USD version % does not support curves\n" testName usdVersion
        )
        return supportCurves
    ),

    -- Helper function to create export options for spline animation tests
    fn createSplineExportOptions animationType:#curves = (
        local exportOptions = USDExporter.CreateOptions()
        exportOptions.FileFormat = #ascii
        exportOptions.RootPrimPath = "/"
        exportOptions.Lights = true
        exportOptions.TimeMode = #frameRange
        exportOptions.StartFrame = 0
        exportOptions.EndFrame = 10
        exportOptions.AnimationType = animationType
        return exportOptions
    ),

    -- Helper function to get the spline knot count for an attribute, returns -1 if no spline
    fn getSplineKnotCount prim attrName = (
        local attr = prim.GetAttribute(attrName)
        if attr == undefined or not attr.IsValid() then (
            return -1
        )
        local spline = attr.GetSpline()
        if spline == undefined then (
            return -1
        )
        local knots = spline.GetKnots()
        return knots.count
    ),

    -- Helper function to get the spline knot value at a specific index, returns undefined if not found
    fn getSplineKnotValue prim attrName knotIndex = (
        local attr = prim.GetAttribute(attrName)
        if attr == undefined or not attr.IsValid() then (
            return undefined
        )
        local spline = attr.GetSpline()
        if spline == undefined then (
            return undefined
        )
        local knots = spline.GetKnots()
        local knot = knots[knotIndex]
        if knot == undefined then (
            return undefined
        )
        return knot.GetValue()
    ),

    -- Helper function to validate spline knot count with detailed message
    fn assertSplineKnotCount prim attrName expectedKnotCount lightName = (
        local actualCount = getSplineKnotCount prim attrName
        local message = lightName + " attribute '" + attrName + "' spline knot count: expected " + (expectedKnotCount as string) + ", got " + (actualCount as string)
        assert_equal expectedKnotCount actualCount message:message
    ),

    -- Helper function to validate spline knot value with detailed message
    fn assertSplineKnotValue prim attrName knotIndex expectedValue lightName tolerance:0.001 = (
        local actualValue = getSplineKnotValue prim attrName knotIndex
        local message = lightName + " attribute '" + attrName + "' at knot " + (knotIndex as string) + ": expected " + (expectedValue as string) + ", got " + (if actualValue != undefined then (actualValue as string) else "undefined")
        if actualValue == undefined then (
            assert_true false message:message
        ) else (
            local isClose = (abs(actualValue - expectedValue) < tolerance)
            assert_true isClose message:message
        )
    ),

    -- Test exporting disk light attributes as USD splines
    function photometric_disk_light_spline_animation_export_test = (
        if not (supportsSplineCurves "photometric_disk_light_spline_animation_export_test") then return undefined

        local exportOptions = createSplineExportOptions()
        local diskLight = free_light()
        diskLight.type = #Free_Disc
        diskLight.distribution = 2 -- uniform hemispherical/diffuse (exports as DiskLight)
        diskLight.radius = 10

        with animate on (
            at time 0 diskLight.radius = 10
            at time 5 diskLight.radius = 25
            at time 10 diskLight.radius = 50
        )

        local exportPath = output_prefix + "test_photometric_disk_light_spline_export.usda"
        USDExporter.ExportFile exportPath exportOptions:exportOptions
        local stage = pyUsd.Stage.Open(exportPath)

        local usd_light_path = "/" + diskLight.name
        local objPrim = stage.GetPrimAtPath usd_light_path
        assert_true (objPrim.IsA(pyUsdLux.DiskLight)) message:(diskLight.name + " light was not properly exported to a USD LuxDiskLight.")

        -- Validate that radius attribute has a spline with 3 knots
        assertSplineKnotCount objPrim "inputs:radius" 3 diskLight.name

        -- Validate the spline knot values
        assertSplineKnotValue objPrim "inputs:radius" 1 10.0 diskLight.name
        assertSplineKnotValue objPrim "inputs:radius" 6 25.0 diskLight.name
        assertSplineKnotValue objPrim "inputs:radius" 11 50.0 diskLight.name
    ),

    -- Test exporting sphere light attributes as USD splines
    function photometric_sphere_light_spline_animation_export_test = (
        if not (supportsSplineCurves "photometric_sphere_light_spline_animation_export_test") then return undefined

        local exportOptions = createSplineExportOptions()

        local sphereLight1 = free_light()
        sphereLight1.type = #Free_Sphere
        sphereLight1.distribution = 0 -- uniform spherical (exports as SphereLight)
        sphereLight1.radius = 5
        sphereLight1.useKelvin = true
        sphereLight1.kelvin = 3000
        sphereLight1.useMultiplier = false -- test intensity without dimmer

        local sphereLight2 = free_light()
        sphereLight2.type = #Free_Sphere
        sphereLight2.distribution = 0 -- uniform spherical (exports as SphereLight)
        sphereLight2.radius = 8
        sphereLight2.useKelvin = false -- use RGB color instead of Kelvin
        sphereLight2.useMultiplier = true -- enable dimmer to test combined intensity+multiplier spline

        with animate on (
            -- Animate radius
            at time 0 sphereLight1.radius = 5
            at time 5 sphereLight1.radius = 15
            at time 10 sphereLight1.radius = 30
            at time 0 sphereLight2.radius = 8
            at time 5 sphereLight2.radius = 16
            at time 10 sphereLight2.radius = 24
            -- Animate color temperature (Kelvin)
            at time 0 sphereLight1.kelvin = 3000
            at time 5 sphereLight1.kelvin = 5500
            at time 10 sphereLight1.kelvin = 8000
            -- Animate intensity
            at time 0 sphereLight1.intensity = 500
            at time 5 sphereLight1.intensity = 1000
            at time 10 sphereLight1.intensity = 2000
            at time 0 sphereLight2.intensity = 1000
            at time 5 sphereLight2.intensity = 2000
            at time 10 sphereLight2.intensity = 1000
            -- Animate multiplier (dimmer percentage)
            at time 0 sphereLight2.multiplier = 100
            at time 5 sphereLight2.multiplier = 50
            at time 10 sphereLight2.multiplier = 100
        )

        local exportPath = output_prefix + "test_photometric_sphere_light_spline_export.usda"
        USDExporter.ExportFile exportPath exportOptions:exportOptions
        local stage = pyUsd.Stage.Open(exportPath)

        local usd_light_path1 = "/" + sphereLight1.name
        local usd_light_path2 = "/" + sphereLight2.name
        local objPrim1 = stage.GetPrimAtPath usd_light_path1
        local objPrim2 = stage.GetPrimAtPath usd_light_path2
        assert_true (objPrim1.IsA(pyUsdLux.SphereLight))
        assert_true (objPrim2.IsA(pyUsdLux.SphereLight))

        -- Validate radius attribute spline
        assertSplineKnotCount objPrim1 "inputs:radius" 3 sphereLight1.name
        assertSplineKnotValue objPrim1 "inputs:radius" 1 5.0 sphereLight1.name
        assertSplineKnotValue objPrim1 "inputs:radius" 6 15.0 sphereLight1.name
        assertSplineKnotValue objPrim1 "inputs:radius" 11 30.0 sphereLight1.name

        -- Validate colorTemperature attribute spline (useKelvin=true)
        assertSplineKnotCount objPrim1 "inputs:colorTemperature" 3 sphereLight1.name
        assertSplineKnotValue objPrim1 "inputs:colorTemperature" 1 3000.0 sphereLight1.name
        assertSplineKnotValue objPrim1 "inputs:colorTemperature" 6 5500.0 sphereLight1.name
        assertSplineKnotValue objPrim1 "inputs:colorTemperature" 11 8000.0 sphereLight1.name

        assertSplineKnotCount objPrim1 "inputs:intensity" 3 sphereLight1.name

        local usd_light_path2 = "/" + sphereLight2.name
        local objPrim2 = stage.GetPrimAtPath usd_light_path2
        assert_true (objPrim2.IsA(pyUsdLux.SphereLight))

        -- Validate enableColorTemperature is false (useKelvin=false)
        local usdLight2 = pyUsdLux.BoundableLightBase.Get stage usd_light_path2
        local enableColorTempAttr2 = usdLight2.GetEnableColorTemperatureAttr()
        local enableColorTemp2 = enableColorTempAttr2.Get()
        assert_false enableColorTemp2

        -- Validate radius attribute spline
        assertSplineKnotCount objPrim2 "inputs:radius" 3 sphereLight2.name
        assertSplineKnotValue objPrim2 "inputs:radius" 1 8.0 sphereLight2.name
        assertSplineKnotValue objPrim2 "inputs:radius" 6 16.0 sphereLight2.name
        assertSplineKnotValue objPrim2 "inputs:radius" 11 24.0 sphereLight2.name

        -- Validate intensity attribute has a spline (useMultiplier=true, combined intensity+multiplier spline)
        local intensityKnotCount = getSplineKnotCount objPrim2 "inputs:intensity"
        assert_true (intensityKnotCount > 0) message:(sphereLight2.name + " should have intensity spline when useMultiplier=true, got " + (intensityKnotCount as string) + " knots")
    ),

    -- Test exporting cylinder light attributes as USD splines
    function photometric_cylinder_light_spline_animation_export_test = (
        if not (supportsSplineCurves "photometric_cylinder_light_spline_animation_export_test") then return undefined

        local exportOptions = createSplineExportOptions()
        local cylinderLight = free_light()
        cylinderLight.type = #Free_Cylinder
        cylinderLight.distribution = 0 -- uniform spherical (exports as CylinderLight)
        cylinderLight.length = 20
        cylinderLight.radius = 3
        cylinderLight.intensity = 2000

        with animate on (
            at time 0 (
                cylinderLight.length = 20
                cylinderLight.radius = 3
            )
            at time 5 (
                cylinderLight.length = 40
                cylinderLight.radius = 6
            )
            at time 10 (
                cylinderLight.length = 80
                cylinderLight.radius = 12
            )
        )

        local exportPath = output_prefix + "test_photometric_cylinder_light_spline_export.usda"
        USDExporter.ExportFile exportPath exportOptions:exportOptions
        local stage = pyUsd.Stage.Open(exportPath)

        -- Cylinder lights need xform transformation, so the light is under an xform prim
        local usd_light_path = "/" + cylinderLight.name + "/" + cylinderLight.name + "_Light"
        local objPrim = stage.GetPrimAtPath usd_light_path
        assert_true (objPrim.IsA(pyUsdLux.CylinderLight)) message:(cylinderLight.name + " light was not properly exported to a USD LuxCylinderLight.")

        -- Validate that length attribute has a spline with 3 knots
        assertSplineKnotCount objPrim "inputs:length" 3 cylinderLight.name
        -- Validate that radius attribute has a spline with 3 knots
        assertSplineKnotCount objPrim "inputs:radius" 3 cylinderLight.name

        -- Validate the length spline knot values
        assertSplineKnotValue objPrim "inputs:length" 1 20.0 cylinderLight.name
        assertSplineKnotValue objPrim "inputs:length" 6 40.0 cylinderLight.name
        assertSplineKnotValue objPrim "inputs:length" 11 80.0 cylinderLight.name

        -- Validate the radius spline knot values
        assertSplineKnotValue objPrim "inputs:radius" 1 3.0 cylinderLight.name
        assertSplineKnotValue objPrim "inputs:radius" 6 6.0 cylinderLight.name
        assertSplineKnotValue objPrim "inputs:radius" 11 12.0 cylinderLight.name

        -- Validate that static intensity is present (not animated, so no spline, but should have a value)
        local usdLight = pyUsdLux.CylinderLight.Get stage usd_light_path
        local intensityAttr = usdLight.GetIntensityAttr()
        assert_true (intensityAttr.IsValid()) message:(cylinderLight.name + " should have a valid intensity attribute.")
        assert_true (intensityAttr.IsAuthored()) message:(cylinderLight.name + " static intensity should be authored even with spline export.")
        local intensityValue = intensityAttr.Get()
        assert_true (intensityValue > 0) message:(cylinderLight.name + " static intensity should have a value > 0, got " + (intensityValue as string))
    ),

    -- Test exporting rectangle light attributes as USD splines
    function photometric_rect_light_spline_animation_export_test = (
        if not (supportsSplineCurves "photometric_rect_light_spline_animation_export_test") then return undefined

        local exportOptions = createSplineExportOptions()
        local rectLight = free_light()
        rectLight.type = #Free_Rectangle
        rectLight.distribution = 2 -- uniform hemispherical/diffuse (exports as RectLight)
        rectLight.width = 10
        rectLight.length = 15

        with animate on (
            at time 0 (
                rectLight.width = 10
                rectLight.length = 15
            )
            at time 5 (
                rectLight.width = 20
                rectLight.length = 30
            )
            at time 10 (
                rectLight.width = 40
                rectLight.length = 60
            )
        )

        local exportPath = output_prefix + "test_photometric_rect_light_spline_export.usda"
        USDExporter.ExportFile exportPath exportOptions:exportOptions
        local stage = pyUsd.Stage.Open(exportPath)

        local usd_light_path = "/" + rectLight.name
        local objPrim = stage.GetPrimAtPath usd_light_path
        assert_true (objPrim.IsA(pyUsdLux.RectLight)) message:(rectLight.name + " light was not properly exported to a USD LuxRectLight.")

        -- Validate that width attribute has a spline with 3 knots
        assertSplineKnotCount objPrim "inputs:width" 3 rectLight.name
        -- Validate that height attribute has a spline with 3 knots
        assertSplineKnotCount objPrim "inputs:height" 3 rectLight.name

        -- Validate the width spline knot values
        assertSplineKnotValue objPrim "inputs:width" 1 10.0 rectLight.name
        assertSplineKnotValue objPrim "inputs:width" 6 20.0 rectLight.name
        assertSplineKnotValue objPrim "inputs:width" 11 40.0 rectLight.name

        -- Validate the height spline knot values (maps from 3ds Max 'length')
        assertSplineKnotValue objPrim "inputs:height" 1 15.0 rectLight.name
        assertSplineKnotValue objPrim "inputs:height" 6 30.0 rectLight.name
        assertSplineKnotValue objPrim "inputs:height" 11 60.0 rectLight.name
    ),

    -- Test exporting spotlight beam angle as USD spline
    function photometric_spotlight_beam_angle_spline_animation_export_test = (
        if not (supportsSplineCurves "photometric_spotlight_beam_angle_spline_animation_export_test") then return undefined

        local exportOptions = createSplineExportOptions()
        local spotLight = free_light()
        spotLight.type = #Free_Point
        spotLight.distribution = 1 -- spotlight distribution (exports as DiskLight with ShapingAPI)
        spotLight.beamAngle = 30
        spotLight.intensity = 1500

        with animate on (
            at time 0 spotLight.beamAngle = 30
            at time 5 spotLight.beamAngle = 45
            at time 10 spotLight.beamAngle = 60
        )

        local exportPath = output_prefix + "test_photometric_spotlight_beam_angle_spline_export.usda"
        USDExporter.ExportFile exportPath exportOptions:exportOptions
        local stage = pyUsd.Stage.Open(exportPath)

        local usd_light_path = "/" + spotLight.name
        local objPrim = stage.GetPrimAtPath usd_light_path
        assert_true (objPrim.IsA(pyUsdLux.DiskLight)) message:(spotLight.name + " light was not properly exported to a USD LuxDiskLight.")

        -- Validate that shaping:cone:angle attribute has a spline with 3 knots
        assertSplineKnotCount objPrim "inputs:shaping:cone:angle" 3 spotLight.name

        -- Validate the shaping:cone:angle spline knot values
        assertSplineKnotValue objPrim "inputs:shaping:cone:angle" 1 30.0 spotLight.name
        assertSplineKnotValue objPrim "inputs:shaping:cone:angle" 6 45.0 spotLight.name
        assertSplineKnotValue objPrim "inputs:shaping:cone:angle" 11 60.0 spotLight.name

        -- Validate that static intensity is present (not animated, so no spline, but should have a value)
        local usdLight = pyUsdLux.DiskLight.Get stage usd_light_path
        local intensityAttr = usdLight.GetIntensityAttr()
        assert_true (intensityAttr.IsValid()) message:(spotLight.name + " should have a valid intensity attribute.")
        assert_true (intensityAttr.IsAuthored()) message:(spotLight.name + " static intensity should be authored even with spline export.")
        local intensityValue = intensityAttr.Get()
        assert_true (intensityValue > 0) message:(spotLight.name + " static intensity should have a value > 0, got " + (intensityValue as string))
    ),

    -- Test exporting line light attributes (as RectLight) with USD splines
    function photometric_line_light_spline_animation_export_test = (
        if not (supportsSplineCurves "photometric_line_light_spline_animation_export_test") then return undefined

        local exportOptions = createSplineExportOptions()
        local lineLight = free_light()
        lineLight.type = #Free_Line
        lineLight.distribution = 2 -- uniform hemispherical/diffuse (exports as RectLight)
        lineLight.length = 30
        lineLight.intensity = 1800

        with animate on (
            at time 0 lineLight.length = 30
            at time 5 lineLight.length = 60
            at time 10 lineLight.length = 120
        )

        local exportPath = output_prefix + "test_photometric_line_light_spline_export.usda"
        USDExporter.ExportFile exportPath exportOptions:exportOptions
        local stage = pyUsd.Stage.Open(exportPath)

        local usd_light_path = "/" + lineLight.name
        local objPrim = stage.GetPrimAtPath usd_light_path
        assert_true (objPrim.IsA(pyUsdLux.RectLight)) message:(lineLight.name + " light was not properly exported to a USD LuxRectLight.")

        -- Validate that height attribute has a spline with 3 knots (line light length maps to height)
        assertSplineKnotCount objPrim "inputs:height" 3 lineLight.name

        -- Validate the height spline knot values
        assertSplineKnotValue objPrim "inputs:height" 1 30.0 lineLight.name
        assertSplineKnotValue objPrim "inputs:height" 6 60.0 lineLight.name
        assertSplineKnotValue objPrim "inputs:height" 11 120.0 lineLight.name

        -- Validate that static intensity is present (not animated, so no spline, but should have a value)
        local usdLight = pyUsdLux.RectLight.Get stage usd_light_path
        local intensityAttr = usdLight.GetIntensityAttr()
        assert_true (intensityAttr.IsValid()) message:(lineLight.name + " should have a valid intensity attribute.")
        assert_true (intensityAttr.IsAuthored()) message:(lineLight.name + " static intensity should be authored even with spline export.")
        local intensityValue = intensityAttr.Get()
        assert_true (intensityValue > 0) message:(lineLight.name + " static intensity should have a value > 0, got " + (intensityValue as string))
    ),

    function teardown = (
        units.SystemType = initialUnits
        units.SystemScale = initialScale
    ),

    Tests = #(
        photometric_light_general_attributes_export_test,
        photometric_light_on_off_export_test,
        photometric_point_light_attributes_export_test,
        photometric_point_light_attributes_animation_export_test,
        photometric_sphere_light_attributes_export_test,
        photometric_sphere_light_attributes_animation_export_test,
        photometric_disk_light_attributes_export_test,
        photometric_disk_light_attributes_animation_export_test,
        photometric_line_light_attributes_export_test,
        photometric_line_light_attributes_animation_export_test,
        photometric_cylinder_light_attributes_export_test,
        photometric_cylinder_light_attributes_animation_export_test,
        photometric_rect_light_attributes_export_test,
        photometric_rect_light_attributes_animation_export_test,
        photometric_sun_positioner_export_test,
        -- Spline/Curves animation tests (require USD 24.11+)
        photometric_disk_light_spline_animation_export_test,
        photometric_sphere_light_spline_animation_export_test,
        photometric_cylinder_light_spline_animation_export_test,
        photometric_rect_light_spline_animation_export_test,
        photometric_spotlight_beam_angle_spline_animation_export_test,
        photometric_line_light_spline_animation_export_test
    )
)

runUsdTestFixture export_light_test (getThisScriptFilename())