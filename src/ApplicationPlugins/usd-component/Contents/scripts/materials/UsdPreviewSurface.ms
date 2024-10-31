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

-- A scripted implementation of a USD Preview Surface
-- You are free to modify this file to make derivative versions to match your workflow/engine.
-- Remember to change the ClassID if you do...
--    https://github.com/ADN-DevTech/3dsMax-OSL-Shaders/blob/master/LICENSE.txt

plugin material MaxUsdPreviewSurface
name:"USD Preview Surface"
classID:#(0x6afa4933, 0x4787f1c7)
category:"Materials\USD"
extends:PhysicalMaterial replaceUI:true version:2019.1215
(
    local params
    local settings
    
    local _opacityThresholdFloatOp
    local _opacityThresholdMapOp
    
    fn setGamma1 map =
    (
        if (classof map == bitmapTex) do
        (
            -- Accessing .bitmap of a missing map yeilds an exception
            try (
                if (map.bitmap != undefined) do
                (
                    if (map.bitmap.gamma != 1.0) do
                    (
                        local gamma1bmp = undefined
                        local version = maxversion()
                        if version[1] >= 26000 then (  -- 3ds Max 2024 and up, require color space.
                            gamma1bmp = openBitmap map.bitmap.filename gamma:1.0 colorspace:ColorPipelineMgr.DataColorSpace
                        )
                        else (
                            gamma1bmp = openBitmap map.bitmap.filename gamma:1.0
                        )                        
                        if (gamma1bmp != undefined) do (
                            USDImporter.SetTexmapParamByName map "bitmap" gamma1bmp
                        )
                    )
                )
            )
            catch() -- do-nothing catch
        )
        if (classOf map == OSLMap) do
        (
            if (hasProperty map "autoGamma") do (
                USDImporter.SetTexmapParamByName map "autogamma" false
            )

            if (hasProperty map "manualGamma") do (
                USDImporter.SetTexmapParamByName map "manualGamma" 1.0
            )
            
            -- Set any parameter ending in "_ColorSpace" to "" (Raw)
            local props = getPropNames map
            for prop in props do if ((findString (prop as string) "_ColorSpace") != undefined) do setProperty map (prop as string) ""
        )
        if (classOf map == MultiOutputChannelTexmapToTexmap) do
        (
            setGamma1(map.sourceMap)
        )
    )
    
    -- Returns the opacity threshold operation OSL node. If it does not exist yet, it is created.
    -- This custom OSL node implements UsdPreviewSurface's "opacityThreshold" for scalar values.
    -- It has two imputs, opacity and threshold. If the threshold is non-zero, it will either output 
    -- a final opacity of 0.0 or 1.0, depending on whether the input opacity exceeds the threshold or not.
    -- If the threshold is zero, the input opacity is returned as is.
    fn getOpacityThresholdFloatOpNode = (
        if (_opacityThresholdFloatOp == undefined) do (
            local code =
            "shader OpacityThresholdFloat
            (
                float Opacity = 0.0 ,
                float Threshold = 0.0,
                output float Out = 0.0,
            )
            {
                Out = Threshold > 0.0 ? (Opacity >= Threshold ? 1.0 : 0.0) : Opacity;
            }
            "
            _opacityThresholdFloatOp = OSLmap()
            USDImporter.SetTexmapParamByName _opacityThresholdFloatOp "OSLCode" code
            USDImporter.SetTexmapParamByName _opacityThresholdFloatOp "OSLAutoUpdate" True
            
            USDImporter.SetTexmapParamByName _opacityThresholdFloatOp "Opacity" 1.0
            USDImporter.SetTexmapParamByName _opacityThresholdFloatOp "Threshold" 0.0
        )
        return _opacityThresholdFloatOp
    )
    
    -- Similar implemenation as getOpacityThresholdFloatOpNode for map values. The RGB intensity is used to
    -- compare against the threshold.
    fn getOpacityThresholdMapOpNode = (
        if (_opacityThresholdMapOp == undefined) do (
            local codeOpacityThresholdMap =
            "shader OpacityThresholdMap
            (
                color Opacity = 0.0,
                float Threshold = 0.0,
                output color Out = 0.0,
            )
            {
                float intensity = (Opacity[0] + Opacity[1] + Opacity[2]) / 3;
                Out = Threshold > 0.0 ? (intensity >= Threshold ? 1.0 : 0.0) : Opacity;
            }
            "
            _opacityThresholdMapOp = OSLmap()
            USDImporter.SetTexmapParamByName _opacityThresholdMapOp "OSLCode" codeOpacityThresholdMap
            USDImporter.SetTexmapParamByName _opacityThresholdMapOp "OSLAutoUpdate" True
        )
        return _opacityThresholdMapOp
    )
    
    fn specGlossToPhysMap init:false = (
        if not (init) and (delegate.roughness_map != undefined) do
        (
            -- if not intializing return existing map
            if (delegate.roughness_map.sourceMap != undefined) do
                return delegate.roughness_map.sourceMap
        )
        local code =
        "shader SpecGlossToPhysical
        (
            color        In     = 0.02,
            float        Gloss  = 1.0,
            float        AO     = 1.0,
            int          Invert = 1,
            output float Reflectivity = 0.0,
            output color ReflColor = 0.0,
            output float Roughness = 0.0
        )
        {
            Roughness = Invert?1.0 - Gloss:Gloss;

            Reflectivity = max(max(In[0],In[1]),In[2]);
            if (Reflectivity > 0.0)
                ReflColor = In / Reflectivity;

            float fres   = pow(1-abs(dot(I, N)),5.0) * (1.0-Roughness);
            Reflectivity = mix(Reflectivity, 1.0, fres);
            ReflColor    = mix(ReflColor, 1.0, fres) * AO;
        }"

        local tm = OSLmap()
        tm.OSLCode = code

        local mul = MultiOutputChannelTexmapToTexmap()
        USDImporter.SetTexmapParamByName mul "sourceMap" tm
        USDImporter.SetTexmapParamByName mul "outputChannelIndex" 1
        USDImporter.SetMaterialParamByName delegate "reflectivity_map" mul

        mul = MultiOutputChannelTexmapToTexmap()
        USDImporter.SetTexmapParamByName mul "sourceMap" tm
        USDImporter.SetTexmapParamByName mul "outputChannelIndex" 2
        USDImporter.SetMaterialParamByName delegate "refl_color_map" mul

        mul = MultiOutputChannelTexmapToTexmap()
        USDImporter.SetTexmapParamByName mul "sourceMap" tm
        USDImporter.SetTexmapParamByName mul "outputChannelIndex" 3
        USDImporter.SetMaterialParamByName delegate "roughness_map" mul
        return tm
    )

    parameters settings rollout:settings
    (
        ao_affects_diffuse     type:#boolean ui:aoDiffuse default:true
        ao_affects_reflection type:#boolean ui:aoReflect default:true
        normal_flip_red         type:#boolean ui:normFlipR default:false
        normal_flip_green      type:#boolean ui:normFlipG default:false
    )


    parameters main rollout:params
    (
        useSpecularWorkflow type:#boolean default:false
        diffuseColor type:#frgba default:[0.18,0.18,0.18,1.0] ui:cpCol
        diffuseColor_map type:#TextureMap ui:mapBaseColor localizedname:"Diffuse Color Map"
        metallic type:#float default:0.0 ui:spnmetallic
        metallic_map type:#TextureMap ui:mapMetallic localizedname:"Metallic Map"
        specularColor type:#frgba default:[0.0,0.0,0.0,1.0] ui:cpSpecular
        specularColor_map type:#TextureMap ui:mapSpecular localizedname:"Specular Map"
        roughness type:#float default:0.5 ui:spnRoughness
        roughness_map type:#TextureMap ui:mapRoughness localizedname:"Roughness Map"
        occlusion type:#float default:1.0 ui:spnOcclusion
        occlusion_map type:#TextureMap ui:mapAmbientOcclusion localizedname:"Occlusion Map"
        normal type:#frgba default:[0.0,0.0,1.0,1.0] ui:cpNormal
        bump_map_amt type:#float default:1.0 ui:spnBumpAmount 
        normal_map type:#TextureMap ui:mapNormal localizedname:"Normal Map"
        emissiveColor type:#frgba default:black ui:cpEmCol
        emissiveColor_map type:#TextureMap ui:mapEmitColor localizedName:"Emissive Color Map"

        opacity type:#float ui:spnOpacity default:1.0
        opacity_map type:#TextureMap ui:mapOpacity localizedname:"Opacity Map"
        opacityThreshold type:#float ui:spnOpacityThreshold default:0.0

        displacement type:#float default:0.0 ui:spnDispAmount
        displacement_map type:#TextureMap ui:mapDisplace localizedname:"Displacement Map"

        ior type:#float ui:spnIor default:1.5
        ior_map type:#TextureMap ui:mapIor localizedname:"IOR Map"
        clearcoat type:#float ui:spnClearcoat default:0.0
        clearcoat_map type:#TextureMap ui:mapClearcoat localizedname:"Clearcoat Map"
        clearcoatRoughness type:#float ui:spnClearcoatRoughness default:0.01
        clearcoatRoughness_map type:#TextureMap ui:mapClearcoatRoughness localizedname:"Clearcoat Roughness Map"

        on useSpecularWorkflow set val do (
            if val == true then (
                local x = specGlossToPhysMap init:True
                USDImporter.SetTexmapParamByName x "In" specularColor
                USDImporter.SetTexmapParamByName x "In_map" specularColor_map
                if (ao_affects_reflection) then
                    USDImporter.SetTexmapParamByName x "AO_map" occlusion_map
                else
                    USDImporter.SetTexmapParamByName x "AO_map" undefined
                USDImporter.SetMaterialParamByName delegate "metalness" 0.0
                USDImporter.SetMaterialParamByName delegate "trans_ior" 50.0
                USDImporter.SetTexmapParamByName x "Gloss_map" roughness_map
                USDImporter.SetTexmapParamByName x "Gloss" roughness
                USDImporter.SetTexmapParamByName x "Invert" False
            ) else (
                USDImporter.SetMaterialParamByName delegate "metalness" metallic
                USDImporter.SetMaterialParamByName delegate "trans_ior" ior
                USDImporter.SetMaterialParamByName delegate "roughness_map" roughness_map
                USDImporter.SetMaterialParamByName delegate "refl_color_map" undefined
                if (ao_affects_reflection) then
                    USDImporter.SetMaterialParamByName delegate "reflectivity_map" occlusion_map
                else
                    USDImporter.SetMaterialParamByName delegate "reflectivity_map" undefined
            )
        )
        on diffuseColor set val do (USDImporter.SetMaterialParamByName delegate "base_color" val)
        on diffuseColor_map set val do (USDImporter.SetMaterialParamByName delegate "base_color_map" val)
        on specularColor set val do (
            if useSpecularWorkflow do (
                local x = specGlossToPhysMap()
                USDImporter.SetTexmapParamByName x "In" val
            )
        )
        on specularColor_map set val do (
            if useSpecularWorkflow do (
                local x = specGlossToPhysMap()
                USDImporter.SetTexmapParamByName x "In_map" val
            )
        )

        on metallic set val do USDImporter.SetMaterialParamByName delegate "metalness" val
        on metallic_map set val do (
            setGamma1(val)
            USDImporter.SetMaterialParamByName delegate "metalness_map" val
        )
        on roughness set val do (
            if useSpecularWorkflow then (
                local x = specGlossToPhysMap()
                USDImporter.SetTexmapParamByName x "Gloss" val
                USDImporter.SetTexmapParamByName x "Invert" False
            ) else (
                USDImporter.SetMaterialParamByName delegate "roughness" val
            )
        )

        on roughness_map set val do (
            setGamma1(val)
            if useSpecularWorkflow then (
                local x = specGlossToPhysMap()
                USDImporter.SetTexmapParamByName x "Gloss_map" val
            ) else (
                USDImporter.SetMaterialParamByName delegate "roughness_Map" val
            )
        )

        on occlusion_map set val do (
            setGamma1(val)

            if (ao_affects_diffuse) then
                USDImporter.SetMaterialParamByName delegate "base_weight_map" val
            else
                USDImporter.SetMaterialParamByName delegate "base_weight_map" undefined

            if useSpecularWorkflow then (
                local x = specGlossToPhysMap()

                if (ao_affects_reflection) then
                    USDImporter.SetTexmapParamByName x "AO_map" val
                else
                    USDImporter.SetTexmapParamByName x "AO_map" undefined
            ) else (
                if (ao_affects_reflection) then
                    USDImporter.SetMaterialParamByName delegate "reflectivity_map" val
                else
                    USDImporter.SetMaterialParamByName delegate "reflectivity_map" undefined
            )
        )
        on opacityThreshold set val do (
            -- Set the threshold value in both the float and map threshold op nodes.
            local opacityThresholdFloatOp = getOpacityThresholdFloatOpNode()
            USDImporter.SetTexmapParamByName opacityThresholdFloatOp "Threshold" val
            
            local opacityThresholdMapOp = getOpacityThresholdMapOpNode()
            USDImporter.SetTexmapParamByName opacityThresholdMapOp "Threshold" val        
            
        )
        
        on opacity set val do (
            local opacityThresholdFloatOp = getOpacityThresholdFloatOpNode()
            -- If the value is not fully opaque, and if still have nothing connected in the cutout_map, 
            -- connect our custom osl node, which will forward the correct opacity to the delegate, given 
            -- the current opacity threshold.
            if (val < 1.0) then (
                if (delegate.cutout_map == undefined) then (
                    USDImporter.SetMaterialParamByName delegate "cutout_map" opacityThresholdFloatOp
                )
            )
            -- Fully opaque, remove the cutout_map entirely if the opacity value is what is being used currently.
            else (
                if (delegate.cutout_map == opacityThresholdFloatOp) do (
                    USDImporter.SetMaterialParamByName delegate "cutout_map" undefined
                )
            )
            -- Set the opacity value in the OSL node.
            USDImporter.SetTexmapParamByName opacityThresholdFloatOp "Opacity" val
        )
        on opacity_map set val do (
            if (val != undefined) do (
                setGamma1(val)
            )
            
            
            -- Connected a new map...
            if (val != undefined) then (
                -- Setup the custom OSL node to take into account the opacityThreshold, and set it as 
                -- the opacity map for the delegate.
                local opacityThresholdMapOp = getOpacityThresholdMapOpNode()
                opacityThresholdMapOp.Opacity_map = val
                USDImporter.SetMaterialParamByName delegate "cutout_map" opacityThresholdMapOp
            -- Disconnected 
            ) else (
                -- If we currently have a non fully opaque value for the opacity, connect the float opacity threshold
                -- op node. It's opacity/threshold input will already be correct.
                if (opacity < 1.0) then (
                    local opacityThresholdFloatOp = getOpacityThresholdFloatOpNode()
                    USDImporter.SetMaterialParamByName delegate "cutout_map" opacityThresholdFloatOp
                )
                -- Otherwise, disconnect the opacity map in the delegate.
                else (
                    USDImporter.SetMaterialParamByName delegate "cutout_map" undefined
                )
            )
        )
        
        on displacement_map set val do (
            setGamma1(val)
            USDImporter.SetMaterialParamByName delegate "displacement_map" val
            USDImporter.SetMaterialParamByName delegate "displacement_map_amt" 1.0
        )

        on displacement set val do USDImporter.SetMaterialParamByName delegate "displacement_map_amt" val
        on bump_map_amt set val do delegate.bump_map_amt = val
        on normal_map set val do (
            if (val != undefined) then (
                setGamma1(val)
                local bm = Gnormal()
                USDImporter.SetTexmapParamByName bm "normal_map" val
                USDImporter.SetTexmapParamByName bm "flipred" normal_flip_red
                USDImporter.SetTexmapParamByName bm "flipgreen" normal_flip_green
                USDImporter.SetMaterialParamByName delegate "bump_map" bm
            )
            else
            (
                USDImporter.SetMaterialParamByName delegate "bump_map" undefined
            )
        )
        on emissiveColor set val do (USDImporter.SetMaterialParamByName delegate "emit_color" val)
        on emissiveColor_map set val do (USDImporter.SetMaterialParamByName delegate "emit_color_map" val)
        on ior set val do (
            if not useSpecularWorkflow do (
                USDImporter.SetMaterialParamByName delegate "trans_ior" val
            )
        )
        on ior_map set val do (
            if not useSpecularWorkflow do (
                USDImporter.SetMaterialParamByName delegate "trans_ior_map" val
            )
        )
        on clearcoat set val do (
            USDImporter.SetMaterialParamByName delegate "coating" val
        )
        on clearcoat_map set val do (
            USDImporter.SetMaterialParamByName delegate "coat_map" val
        )
        on clearcoatRoughness set val do (
            USDImporter.SetMaterialParamByName delegate "coat_roughness" val
        )
        on clearcoatRoughness_map set val do (
            USDImporter.SetMaterialParamByName delegate "coat_rough_map" val
        )

    )

    rollout params "USD Preview Surface Parameters"
    (
        group "Material Parameters" (
            label lblWorkflow "Workflow:" across:2
            radiobuttons rdoWorkflow labels:#("Metallic", "Specular") default:1 align:#left

            colorpicker cpCol "Diffuse Color" color:white across:2 fieldWidth:45 offset:[-45,6] align:#right
            mapbutton mapBaseColor "" align:#right width:190 offset:[-5,5]

            spinner spnmetallic "Metallic" range:[0.0,1.0,0.0] scale:0.01 across:2 fieldWidth:35 offset:[-45,5] align:#right
            mapbutton mapMetallic "" align:#right width:190 offset:[-5,2]

            colorpicker cpSpecular "Specular Color" color:white across:2 fieldWidth:45 offset:[-45,3] align:#right
            mapbutton mapSpecular "" align:#right width:190 offset:[-5,2]

            spinner spnRoughness "Roughness" range:[0.0,1.0,0.0] scale:0.01 across:2 fieldWidth:35 offset:[-45,5] align:#right
            mapbutton mapRoughness "" align:#right width:190 offset:[-5,2]

            spinner spnOcclusion "Occlusion" range:[0.0,1.0,0.0] scale:0.01 across:2 fieldWidth:35 offset:[-45,5] align:#right
            mapbutton mapAmbientOcclusion "" align:#right width:190 offset:[-5,2]
            
            colorpicker cpNormal "Normal" color:blue across:2 fieldWidth:45 offset:[-45,3] align:#right
            mapbutton mapNormal "" align:#right width:190 offset:[-5,2]
            
            spinner spnBumpAmount "Amount" range:[-5.0,5.0,1.0] scale:0.01 fieldwidth:35 offset:[-45,5] align:#right

            colorpicker cpEmCol "Emissive Color" color:black across:2 fieldWidth:45 offset:[-45,3] align:#right
            mapbutton mapEmitColor "" align:#right width:190 offset:[-5,2]

            spinner spnDispAmount "Displacement" range:[0.0,999.0,1.0] scale:0.01 fieldwidth:35 across:2 offset:[-45,5] align:#right
            mapbutton mapDisplace "" align:#right width:190 offset:[-5,2]

            spinner spnOpacity "Opacity" range:[0.0,1.0,0.0] scale:0.01 fieldwidth:35 across:2 offset:[-45,5] align:#right
            mapbutton mapOpacity "" align:#right width:190 offset:[-5,2]

            spinner spnOpacityThreshold "Opacity Threshold" range:[0.0,1.0,0.1] scale:0.01 fieldwidth:35 offset:[-45,5] align:#right

            spinner spnIor "IOR" range:[0.0, 999.0, 1.0] scale:0.01 fieldwidth:35 offset:[-45,5] align:#right across:2
            mapbutton mapIor "" align:#right width:190 offset:[-5,2]

            spinner spnClearcoat "Clearcoat" range:[0.0,999.0,1.0] scale:0.01 fieldwidth:35 across:2 offset:[-45,5] align:#right
            mapbutton mapClearcoat "" align:#right width:190 offset:[-5,2]

            spinner spnClearcoatRoughness "CC Roughness" range:[0.0,999.0,1.0] scale:0.01 fieldwidth:35 across:2 offset:[-45,5] align:#right
            mapbutton mapClearcoatRoughness "" align:#right width:190 offset:[-5,2]
        )
        on params open do
        (
            params.setUIText()
        )

        fn setUIText =
        (
            if useSpecularWorkflow then
            (
                params.rdoWorkflow.state = 2
                params.spnmetallic.enabled = false
                params.cpSpecular.enabled = true
            )
            else
            (
                params.rdoWorkflow.state = 1
                params.spnmetallic.enabled = true
                params.cpSpecular.enabled = false
            )
        )


        on rdoWorkflow changed state do
        (
            useSpecularWorkflow = (state == 2)
            params.setUIText()
        )

    )

    rollout settings "Settings"
    (
        checkbox aoDiffuse "AO affects Diffuse" offset:[20,0] across:2
        checkBox normFlipR "Normal Flip Red" align:#left
        checkbox aoReflect "AO affects Reflection" offset:[20,0] across:2
        checkBox normFlipG "Normal Flip Green" align:#left
        label lblSpc "" height:2

        on aoDiffuse changed state do occlusion_map = occlusion_map
        on aoReflect changed state do occlusion_map = occlusion_map
        on normFlipR changed state do normal_map = normal_map
        on normFlipG changed state do normal_map = normal_map
    )

    on create do
    (
        USDImporter.SetMaterialParamByName delegate "base_color" white
        USDImporter.SetMaterialParamByName delegate "metalness" 0.0
        USDImporter.SetMaterialParamByName delegate "roughness" 0.5
        USDImporter.SetMaterialParamByName delegate "bump_map_amt" 1.0
        USDImporter.SetMaterialParamByName delegate "roughness_inv" false
        USDImporter.SetMaterialParamByName delegate "reflectivity" 1.0
        USDImporter.SetMaterialParamByName delegate "emit_color" black
        USDImporter.SetMaterialParamByName delegate "emit_color_map_on" true
        USDImporter.SetMaterialParamByName delegate "trans_ior" 1.5
        USDImporter.SetMaterialParamByName delegate "refl_color_map" undefined
        USDImporter.SetMaterialParamByName delegate "displacement_map_amt" 1.0

        custAttributes.add this USDMaterialAttributeHolder
    )
)