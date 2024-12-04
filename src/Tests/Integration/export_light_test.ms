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
        resetmaxfile #noprompt
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
        pointLight.on = false -- turning the light off should not affect the export to USD
            
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
        local exportPath = output_prefix + "test_photometric_sphere_light_animationrange_export.usda"
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
        resetmaxfile #noprompt
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

    function teardown = (
        units.SystemType = initialUnits
        units.SystemScale = initialScale
    ),

    Tests = #(
        photometric_light_general_attributes_export_test,
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
        photometric_sun_positioner_export_test
    )
)

runUsdTestFixture export_light_test (getThisScriptFilename())