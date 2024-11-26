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

input_prefix = pathConfig.appendPath (getDir #temp) "/IMPORT_LIGHT_TEST_"
deleteTestFiles(input_prefix + "*.*")

struct import_light_test
(  
    private 
    initialUnits,
    initialScale,
    
    public
    
    function resetSceneAndUnits = 
    (
        resetmaxfile #noprompt
        units.SystemType = #inches
        units.SystemScale = 1.0        
    ),
    
    function setup =
    (
        initialUnits = units.SystemType
        initialScale = units.SystemScale
        resetSceneAndUnits()
    ),
    
    -- Tests to validate the USD light types import as expected 3ds Max Photometric light types
    -- The import of a USD light is based on the export table of the Photometric Light Shape and its selected
    -- light distribution type
    -- Below is the conversion grid used by the export for reference :
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
    function usd_lux_light_general_attributes_import_test = (
        -- USD Lux Disk light will be imported back as a Photometric Disk light using a uniform distribution
        local filepath = input_prefix + "test_lux_disk_import.usda"
        local stage = pyUsd.Stage.CreateNew(filepath)
        pyUsdGeom.SetStageMetersPerUnit stage 0.0254  -- default 3ds Max units are in inches
        local lightName = "DiskLightPrim"
        local lightPrim = pyUsdLux.DiskLight.Define stage ("/" + lightName)
        local lightRadius = 2.8
        lightPrim.CreateRadiusAttr(lightRadius)
        
        -- general light attributes
        lightPrim.CreateNormalizeAttr(True)
        lightIntensity = 1623.1595  -- 500 candelas normalized (using 3ds Max default env setting)
        lightPrim.CreateIntensityAttr(lightIntensity)
        lightPrim.CreateEnableColorTemperatureAttr(True)
        local lightKelvin = 7000.0
        lightPrim.CreateColorTemperatureAttr(lightKelvin)
        lightColor = color 255 0 0
        lightPrim.CreateColorAttr((pyGf.Vec3f (lightColor.r/255.0) (lightColor.g/255.0) (lightColor.b/255.0)))
        local lightShadowAPI = pyUsdLux.ShadowAPI(lightPrim)
        lightShadowAPI.CreateShadowEnableAttr(True)
        lightShadowColor = color 0 255 0
        lightShadowAPI.CreateShadowColorAttr((pyGf.Vec3f (lightShadowColor.r/255.0) (lightShadowColor.g/255.0) (lightShadowColor.b/255.0)))
        (stage.GetRootLayer()).Save()
        
        USDImporter.ImportFile filepath
        importedLight = objects[1]
        
        assert_equal lightName importedLight.name  message:(importedLight.name + " light should be named '" + lightName + "'")
        assert_equal #Free_Disc importedLight.type message:(importedLight.name + " light should be of type Photometric Disc Light")
        assert_equal 2 importedLight.distribution message:(importedLight.name + " light should use a Uniform (diffuse) light distribution")
        assert_float_equal lightRadius importedLight.radius message:(importedLight.name + " light radius is expected to be '" + (lightRadius as String) + "'")
        
        assert_true importedLight.useKelvin message:(importedLight.name + " light should be using Kelvin (temperature) color")
        assert_float_equal lightKelvin importedLight.kelvin message:(importedLight.name + " light should report a Kelvin (temperature) color of '" + (lightKelvin as String) + "'")
        assert_equal lightColor importedLight.rgbFilter message:(importedLight.name + " light filter color should be set to the USD light color")
        assert_true importedLight.castShadows message:(importedLight.name + " light should be set to cast shadows")
        assert_equal lightShadowColor importedLight.shadowColor message:(importedLight.name + " light shadow color should be set to the USD light shadow color")
        assert_true importedLight.affectSpecular message:(importedLight.name + " light should affect specular on surfaces")
        assert_true importedLight.affectDiffuse message:(importedLight.name + " light should affect diffuse on surfaces")
        assert_equal 1 importedLight.intensityType message:(importedLight.name + " light should report intensity in candelas")
        assert_float_equal 500.0 importedLight.intensity message:(importedLight.name + " light intensity should be set to 500")

        resetSceneAndUnits()

        -- USD Lux Disk light will be imported back as a Photometric Disk light using a uniform distribution
        -- changing some attributes
        --  - disable color temperature
        --  - turn off shadows
        --  - turn off specular effect
        --  - turn off diffuse effect
        local filepath = input_prefix + "test_lux_disk_second_import.usda"
        local stage = pyUsd.Stage.CreateNew(filepath)
        pyUsdGeom.SetStageMetersPerUnit stage 0.0254  -- default 3ds Max units are in inches
        local lightName = "DiskLightSecondPrim"
        local lightPrim = pyUsdLux.DiskLight.Define stage ("/" + lightName)
        local lightRadius = 1.8
        lightPrim.CreateRadiusAttr(lightRadius)
        
        -- general light attributes
        lightPrim.CreateNormalizeAttr(True)
        lightPrim.CreateEnableColorTemperatureAttr(False)
        lightColor = color 255 0 0
        lightPrim.CreateColorAttr((pyGf.Vec3f (lightColor.r/255.0) (lightColor.g/255.0) (lightColor.b/255.0)))
        local lightShadowAPI = pyUsdLux.ShadowAPI(lightPrim)
        lightShadowAPI.CreateShadowEnableAttr(False)
        lightShadowColor = color 0 255 0
        lightShadowAPI.CreateShadowColorAttr((pyGf.Vec3f (lightShadowColor.r/255.0) (lightShadowColor.g/255.0) (lightShadowColor.b/255.0)))
        lightPrim.CreateSpecularAttr(0.0)
        lightPrim.CreateDiffuseAttr(0.0)
        (stage.GetRootLayer()).Save()
        
        USDImporter.ImportFile filepath
        importedLight = objects[1]
        
        assert_equal lightName importedLight.name  message:(importedLight.name + " light should be named '" + lightName + "'")
        assert_equal #Free_Disc importedLight.type message:(importedLight.name + " light should be of type Photometric Disc Light")
        assert_equal 2 importedLight.distribution message:(importedLight.name + " light should use a Uniform (diffuse) light distribution")
        assert_float_equal lightRadius importedLight.radius message:(importedLight.name + " light radius is expected to be '" + (lightRadius as String) + "'")
        
        assert_false importedLight.useKelvin message:(importedLight.name + " light should not be using Kelvin (temperature) color")
        assert_float_equal 3600.0 importedLight.kelvin message:(importedLight.name + " light should default to 3600 Kelvin if Kelvin color is not enabled")
        assert_equal (lightColor as string) (importedLight.rgbFilter as string) message:(importedLight.name + " light filter color should be set to the USD light color")
        assert_false importedLight.baseObject.castShadows message:(importedLight.name + " light should not be set to cast shadows")
        assert_equal lightShadowColor importedLight.shadowColor message:(importedLight.name + " light shadow color should be set to the USD light shadow color")
        assert_false importedLight.affectSpecular message:(importedLight.name + " light should not affect specular on surfaces")
        assert_false importedLight.affectDiffuse message:(importedLight.name + " light should not affect diffuse on surfaces")
    ),

    function usd_lux_disk_light_import_test = (
        -- USD Lux Disk light will be imported back as a Photometric Disk light using a uniform distribution
        local filepath = input_prefix + "test_lux_disk_import.usda"
        local stage = pyUsd.Stage.CreateNew(filepath)
        pyUsdGeom.SetStageMetersPerUnit stage 0.0254  -- default 3ds Max units are in inches
        local lightName = "DiskLightPrim"
        local lightPrim = pyUsdLux.DiskLight.Define stage ("/" + lightName)
        local lightRadius = 2.8
        lightPrim.CreateRadiusAttr(lightRadius)
        (stage.GetRootLayer()).Save()
        
        USDImporter.ImportFile filepath
        importedLight = objects[1]
        
        assert_equal lightName importedLight.name  message:(importedLight.name + " light should be named '" + lightName + "'")
        assert_equal #Free_Disc importedLight.type message:(importedLight.name + " light should be of type Photometric Disc Light")
        assert_equal 2 importedLight.distribution message:(importedLight.name + " light should use a Uniform (diffuse) light distribution")
        assert_float_equal lightRadius importedLight.radius message:(importedLight.name + " light radius is expected to be '" + (lightRadius as String) + "'")
    ),

    function usd_lux_sphere_light_import_test = (
        -- USD Lux Sphere light will be imported back as a Photometric Sphere light using a spherical distribution
        local filepath = input_prefix + "test_lux_sphere_import.usda"
        local stage = pyUsd.Stage.CreateNew(filepath)
        pyUsdGeom.SetStageMetersPerUnit stage 0.0254  -- default 3ds Max units are in inches
        local lightName = "SphereLightPrim"
        local lightPrim = pyUsdLux.SphereLight.Define stage ("/" + lightName)
        local lightRadius = 15.5
        lightPrim.CreateRadiusAttr(lightRadius)
        (stage.GetRootLayer()).Save()
        
        USDImporter.ImportFile filepath
        importedLight = objects[1]
        
        assert_equal lightName importedLight.name  message:(importedLight.name + " light should be named '" + lightName + "'")
        assert_equal #Free_Sphere importedLight.type message:(importedLight.name + " light should be of type Photometric Sphere Light")
        assert_equal 0 importedLight.distribution message:(importedLight.name + " light should use a Spherical (isotropic) light distribution")
        assert_float_equal lightRadius importedLight.radius message:(importedLight.name + " light radius is expected to be '" + (lightRadius as String) + "'")

        resetSceneAndUnits()

        -- USD Lux Sphere light using a 'treat as point' attribute
        -- will be imported back as a Photometric Point light using a spherical distribution
        local filepath = input_prefix + "test_lux_sphere_treat_as_point_import.usda"
        local stage = pyUsd.Stage.CreateNew(filepath)
        local lightName = "SpherePointLightPrim"
        local lightPrim = pyUsdLux.SphereLight.Define stage ("/" + lightName)
        lightPrim.CreateTreatAsPointAttr(True)
        (stage.GetRootLayer()).Save()
        
        USDImporter.ImportFile filepath
        importedLight = objects[1]
        
        assert_equal lightName importedLight.name  message:(importedLight.name + " light should be named '" + lightName + "'")
        assert_equal #Free_Point importedLight.type message:(importedLight.name + " light should be of type Photometric Point Light")
        assert_equal 0 importedLight.distribution message:(importedLight.name + " light should use a Spherical (isotropic) light distribution")
    ),

    function usd_lux_rect_light_import_test = (
        -- USD Lux Rect light will be imported back as a Photometric Rectangle light using a uniform distribution
        local filepath = input_prefix + "test_lux_rect_import.usda"
        local stage = pyUsd.Stage.CreateNew(filepath)
        pyUsdGeom.SetStageMetersPerUnit stage 0.0254  -- default 3ds Max units are in inches
        local lightName = "RectLightPrim"
        local lightPrim = pyUsdLux.RectLight.Define stage ("/" + lightName)
        local lightWidth = 8.8
        lightPrim.CreateWidthAttr(lightWidth)
        local lightHeight = 2.4
        lightPrim.CreateHeightAttr(lightHeight)
        (stage.GetRootLayer()).Save()
        
        USDImporter.ImportFile filepath
        importedLight = objects[1]
        
        assert_equal lightName importedLight.name  message:(importedLight.name + " light should be named '" + lightName + "'")
        assert_equal #Free_Rectangle importedLight.type message:(importedLight.name + " light should be of type Photometric Rectangle Light")
        assert_equal 2 importedLight.distribution message:(importedLight.name + " light should use a Uniform (diffuse) light distribution")
        assert_float_equal lightWidth importedLight.width message:(importedLight.name + " light width is expected to be '" + (lightWidth as String) + "'")
        assert_float_equal lightHeight importedLight.length message:(importedLight.name + " light length is expected to be '" + (lightHeight as String) + "'")
    ),

    function usd_lux_cylinder_light_import_test = (
        -- USD Lux Cylinder light will be imported back as a Photometric Cylinder light using a spherical distribution
        local filepath = input_prefix + "test_lux_cylinder_import.usda"
        local stage = pyUsd.Stage.CreateNew(filepath)
        pyUsdGeom.SetStageMetersPerUnit stage 0.0254  -- default 3ds Max units are in inches
        local lightName = "CylinderLightPrim"
        local lightPrim = pyUsdLux.CylinderLight.Define stage ("/" + lightName)
        local lightRadius = 5.5
        local lightLength = 30.0
        lightPrim.CreateRadiusAttr(lightRadius)
        lightPrim.CreateLengthAttr(lightLength)
        (stage.GetRootLayer()).Save()
        
        USDImporter.ImportFile filepath
        importedLight = objects[1]
        
        assert_equal lightName importedLight.name  message:(importedLight.name + " light should be named '" + lightName + "'")
        assert_equal #Free_Cylinder importedLight.type message:(importedLight.name + " light should be of type Photometric Cylinder Light")
        assert_equal 0 importedLight.distribution message:(importedLight.name + " light should use a Spherical (isotropic) light distribution")
        assert_float_equal lightRadius importedLight.radius message:(importedLight.name + " light radius is expected to be '" + (lightRadius as String) + "'")
        assert_float_equal lightLength importedLight.length message:(importedLight.name + " light length is expected to be '" + (lightLength as String) + "'")

        resetSceneAndUnits()

        -- USD Lux Cylinder light using a 'treat as line' attribute
        -- will be imported back as a Photometric Line light using a spherical distribution
        local filepath = input_prefix + "test_lux_cylinder_treat_as_line_import.usda"
        local stage = pyUsd.Stage.CreateNew(filepath)
        pyUsdGeom.SetStageMetersPerUnit stage 0.0254  -- default 3ds Max units are in inches
        local lightName = "CylinderLineLightPrim"
        local lightPrim = pyUsdLux.CylinderLight.Define stage ("/" + lightName)
        local lightLength = 22.5
        lightPrim.CreateTreatAsLineAttr(True)
        lightPrim.CreateLengthAttr(lightLength)
        (stage.GetRootLayer()).Save()
        
        USDImporter.ImportFile filepath
        importedLight = objects[1]
        
        assert_equal lightName importedLight.name  message:(importedLight.name + " light should be named '" + lightName + "'")
        assert_equal #Free_Line importedLight.type message:(importedLight.name + " light should be of type Photometric Line Light")
        assert_equal 0 importedLight.distribution message:(importedLight.name + " light should use a Spherical (isotropic) light distribution")
        assert_float_equal lightLength importedLight.length message:(importedLight.name + " light length is expected to be '" + (lightLength as String) + "'")
    ),

    function animated_light_import_test = (
        units.SystemType = #Millimeters
        units.SystemScale = 1

        -- Import animated lights
        local scriptDir = getFilenamePath (getThisScriptFilename())
        local importPath = (scriptDir + @"data\lights_animated.usda")

        importOptions = USDImporter.CreateOptions()
        importOptions.StartTimeCode = 0
        importOptions.EndTimeCode = 10
        importOptions.TimeMode = #CustomRange
        USDImporter.ImportFile importPath importOptions:importOptions

        local cylinderLight = getNodeByName "CylinderLight_Light"
        sliderTime = 0
        assert_equal #Free_Cylinder cylinderLight.type message:(cylinderLight.name + " light should be of type Photometric Cylinder Light")
        assert_equal 0 cylinderLight.distribution message:(cylinderLight.name + " light should use a Spherical (isotropic) light distribution")
        assert_float_equal 20.0f cylinderLight.radius message:(cylinderLight.name + " light radius is expected to be the value specified in the USD file")
        assert_float_equal 100.0f cylinderLight.length message:(cylinderLight.name + " light length is expected to be the value specified in the USD file")
        assert_equal 1 cylinderLight.intensityType message:(cylinderLight.name + " light should report intensity in candelas")
        assert_float_equal 1500.0 cylinderLight.intensity message:(cylinderLight.name + " light intensity should be set to 1500")
        sliderTime = 10
        assert_equal #Free_Cylinder cylinderLight.type message:(cylinderLight.name + " light should be of type Photometric Cylinder Light")
        assert_equal 0 cylinderLight.distribution message:(cylinderLight.name + " light should use a Spherical (isotropic) light distribution")
        assert_float_equal 40.0f cylinderLight.radius message:(cylinderLight.name + " light radius is expected to be the value specified in the USD file")
        assert_float_equal 200.0f cylinderLight.length message:(cylinderLight.name + " light length is expected to be the value specified in the USD file")
        assert_equal 1 cylinderLight.intensityType message:(cylinderLight.name + " light should report intensity in candelas")
        assert_float_equal 750.0 cylinderLight.intensity message:(cylinderLight.name + " light intensity should be set to 750")

        local discLight = getNodeByName "DiscLight"
        sliderTime = 0
        assert_equal #Free_Disc discLight.type message:(discLight.name + " light should be of type Photometric Disc Light")
        assert_equal 2 discLight.distribution message:(discLight.name + " light should use a Uniform (diffuse) light distribution")
        assert_float_equal 20.0f discLight.radius message:(discLight.name + " light radius is expected to be the value specified in the USD file")
        assert_equal 1 discLight.intensityType message:(discLight.name + " light should report intensity in candelas")
        assert_float_equal 1500.0 discLight.intensity message:(discLight.name + " light intensity should be set to 1500")
        sliderTime = 10
        assert_equal #Free_Disc discLight.type message:(discLight.name + " light should be of type Photometric Disc Light")
        assert_equal 2 discLight.distribution message:(discLight.name + " light should use a Uniform (diffuse) light distribution")
        assert_float_equal 40.0f discLight.radius message:(discLight.name + " light radius is expected to be the value specified in the USD file")
        assert_equal 1 discLight.intensityType message:(discLight.name + " light should report intensity in candelas")
        assert_float_equal 750.0 discLight.intensity message:(discLight.name + " light intensity should be set to 750")

        local lineLight = getNodeByName "LineLight"
        sliderTime = 0
        assert_equal #Free_Rectangle lineLight.type message:(lineLight.name + " light should be of type Photometric Rectangle Light")
        assert_equal 2 lineLight.distribution message:(lineLight.name + " light should use a Uniform (diffuse) light distribution")
        assert_float_equal .1f lineLight.width message:(lineLight.name + " light width is expected to be the value specified in the USD file")
        assert_float_equal 50 lineLight.length message:(lineLight.name + " light length is expected to be the value specified in the USD file")
        assert_equal 1 lineLight.intensityType message:(lineLight.name + " light should report intensity in candelas")
        assert_float_equal 1500.0 lineLight.intensity message:(lineLight.name + " light intensity should be set to 1500")
        sliderTime = 10
        assert_equal #Free_Rectangle lineLight.type message:(lineLight.name + " light should be of type Photometric Rectangle Light")
        assert_equal 2 lineLight.distribution message:(lineLight.name + " light should use a Uniform (diffuse) light distribution")
        assert_float_equal .1f lineLight.width message:(lineLight.name + " light width is expected to be the value specified in the USD file")
        assert_float_equal 100 lineLight.length message:(lineLight.name + " light length is expected to be the value specified in the USD file")
        assert_equal 1 lineLight.intensityType message:(lineLight.name + " light should report intensity in candelas")
        assert_float_equal 750.0 lineLight.intensity message:(lineLight.name + " light intensity should be set to 750")

        local pointLight = getNodeByName "PointLight"
        sliderTime = 0
        assert_equal #Free_Disc pointLight.type message:(pointLight.name + " light should be of type Photometric Disc Light")
        assert_equal 2 pointLight.distribution message:(pointLight.name + " light should use a Uniform (diffuse) light distribution")
        assert_float_equal .001 pointLight.radius message:(pointLight.name + " light radius is expected to be the value specified in the USD file")
        assert_equal 1 pointLight.intensityType message:(pointLight.name + " light should report intensity in candelas")
        assert_float_equal 1500.0 pointLight.intensity message:(pointLight.name + " light intensity should be set to 1500")
        assert_true pointLight.useKelvin message:(pointLight.name + " light should be using Kelvin (temperature) color")
        assert_float_equal 3600 pointLight.kelvin message:(pointLight.name + " light should report a Kelvin (temperature) color value specified in the USD file")
        assert_equal (color 255 255 255) pointLight.rgbFilter message:(pointLight.name + " light filter color should be set to the USD light color")
        sliderTime = 10
        assert_equal #Free_Disc pointLight.type message:(pointLight.name + " light should be of type Photometric Disc Light")
        assert_equal 2 pointLight.distribution message:(pointLight.name + " light should use a Uniform (diffuse) light distribution")
        assert_float_equal .001 pointLight.radius message:(pointLight.name + " light radius is expected to be the value specified in the USD file")
        assert_equal 1 pointLight.intensityType message:(pointLight.name + " light should report intensity in candelas")
        assert_float_equal 750.0 pointLight.intensity message:(pointLight.name + " light intensity should be set to 750")
        assert_true pointLight.useKelvin message:(pointLight.name + " light should be using Kelvin (temperature) color")
        assert_float_equal 1000 pointLight.kelvin message:(pointLight.name + " light should report a Kelvin (temperature) color value specified in the USD file")
        assert_float_equal 255.0 (pointLight.rgbFilter.r) message:(pointLight.name + " light filter color (red) should be set to the USD light color")
        assert_float_equal 21.5836 (pointLight.rgbFilter.g) message:(pointLight.name + " light filter color (green) should be set to the USD light color")
        assert_float_equal 21.5836 (pointLight.rgbFilter.b) message:(pointLight.name + " light filter color (blue) should be set to the USD light color")

        local rectangleLight = getNodeByName "RectangleLight"
        sliderTime = 0
        assert_equal #Free_Rectangle rectangleLight.type message:(rectangleLight.name + " light should be of type Photometric Rectangle Light")
        assert_equal 2 rectangleLight.distribution message:(rectangleLight.name + " light should use a Uniform (diffuse) light distribution")
        assert_float_equal 100 rectangleLight.width message:(rectangleLight.name + " light width is expected to be the value specified in the USD file")
        assert_float_equal 100 rectangleLight.length message:(rectangleLight.name + " light length is expected to be the value specified in the USD file")
        assert_equal 1 rectangleLight.intensityType message:(rectangleLight.name + " light should report intensity in candelas")
        assert_float_equal 1500.0 rectangleLight.intensity message:(rectangleLight.name + " light intensity should be set to 1500")
        sliderTime = 10
        assert_equal #Free_Rectangle rectangleLight.type message:(rectangleLight.name + " light should be of type Photometric Rectangle Light")
        assert_equal 2 rectangleLight.distribution message:(rectangleLight.name + " light should use a Uniform (diffuse) light distribution")
        assert_float_equal 50 rectangleLight.width message:(rectangleLight.name + " light width is expected to be the value specified in the USD file")
        assert_float_equal 50 rectangleLight.length message:(rectangleLight.name + " light length is expected to be the value specified in the USD file")
        assert_equal 1 rectangleLight.intensityType message:(rectangleLight.name + " light should report intensity in candelas")
        assert_float_equal 750.0 rectangleLight.intensity message:(rectangleLight.name + " light intensity should be set to 750")

        local sphereLight = getNodeByName "SphereLight"
        sliderTime = 0
        assert_equal #Free_Sphere sphereLight.type message:(sphereLight.name + " light should be of type Photometric Sphere Light")
        assert_equal 0 sphereLight.distribution message:(sphereLight.name + " light should use a Spherical (isotropic) light distribution")
        assert_float_equal 10.0f sphereLight.radius message:(sphereLight.name + " light radius is expected to be the value specified in the USD file")
        assert_equal 1 sphereLight.intensityType message:(sphereLight.name + " light should report intensity in candelas")
        assert_float_equal 1500.0 sphereLight.intensity message:(sphereLight.name + " light intensity should be set to 1500")
        sliderTime = 10
        assert_equal #Free_Sphere sphereLight.type message:(sphereLight.name + " light should be of type Photometric Sphere Light")
        assert_equal 0 sphereLight.distribution message:(sphereLight.name + " light should use a Spherical (isotropic) light distribution")
        assert_float_equal 20.0f sphereLight.radius message:(sphereLight.name + " light radius is expected to be the value specified in the USD file")
        assert_equal 1 sphereLight.intensityType message:(sphereLight.name + " light should report intensity in candelas")
        assert_float_equal 750.0 sphereLight.intensity message:(sphereLight.name + " light intensity should be set to 750")
    ),

    function teardown = (
        units.SystemType = initialUnits
        units.SystemScale = initialScale        
    ),
    Tests = #(
        usd_lux_light_general_attributes_import_test,
        usd_lux_disk_light_import_test,
        usd_lux_sphere_light_import_test,
        usd_lux_rect_light_import_test,
        usd_lux_cylinder_light_import_test,
        animated_light_import_test
    )
)

runUsdTestFixture import_light_test (getThisScriptFilename())