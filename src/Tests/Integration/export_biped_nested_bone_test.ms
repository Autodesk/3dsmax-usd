--
-- Copyright 2026 Autodesk
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
-- Regression test for issue #42: "Cannot export Point bone under a Biped"
-- https://github.com/Autodesk/3dsmax-usd/issues/42
--
-- A helper node (Point/Dummy) parented inside a Biped is part of that skeleton's
-- joint hierarchy. With the "Include All Bones" option on, it must be exported as
-- a UsdSkel joint even when it has no skin/morpher of its own. Before the fix such
-- a helper matched no prim writer and was silently dropped from the export.
--
-- A helper sitting at the scene root (outside any skeleton) is the boundary case:
-- it has no bone ancestor and must NOT be turned into a joint.
--

FileIn "helpers/usd_test_utils.mxs"

setupUsdPlugins()

pyUsd = python.import("pxr.Usd")
pyUsdSkel = python.import("pxr.UsdSkel")

struct export_biped_nested_bone_test
(
    output_prefix = pathConfig.appendPath (getDir #temp) "/EXPORT_BIPED_NESTED_BONE_",

    public
    function setup =
    (
        quickReset()
        units.SystemType = #Centimeters
        units.SystemScale = 1
        animationRange = interval 0 10
    ),

    function teardown = (),

    function findNodeContaining namePart =
    (
        local found = undefined
        for n in objects do (
            if (findString n.name namePart) != undefined then (
                found = n
                exit
            )
        )
        return found
    ),

    function skinTo bonesArray maxNode =
    (
        local sk = skin()
        addModifier maxNode sk
        local u = 0
        for i = 1 to bonesArray.count do (
            if i == bonesArray.count then u = -1
            skinOps.addbone sk bonesArray[i] u node:maxNode
        )
        select maxNode
        max modify mode
        skinOps.voxelWeighting sk 0 0.2 4 256 false true node:maxNode
    ),

    function dumpUsdaFile filePath =
    (
        format "\n===== BEGIN EXPORTED USDA (%) =====\n" filePath
        local f = openFile filePath mode:"r"
        if f != undefined then (
            while not (eof f) do ( format "%\n" (readLine f) )
            close f
        ) else (
            format "  <could not open file>\n"
        )
        format "===== END EXPORTED USDA =====\n\n"
    ),

    function readFileToString filePath =
    (
        local f = openFile filePath mode:"r"
        if f == undefined then return ""
        local s = ""
        while not (eof f) do ( s += (readLine f) + "\n" )
        close f
        return s
    ),

    -- Returns the 'joints' token array of the first Skeleton found, or an empty
    -- array if none resolves at the expected path.
    function readSkeletonJoints stage =
    (
        local skelPrim = stage.GetPrimAtPath "/root/Bones"
        if skelPrim == undefined or not (skelPrim.IsValid()) then return #()
        local jointsAttr = skelPrim.GetAttribute "joints"
        if jointsAttr == undefined then return #()
        local joints = jointsAttr.Get()
        if joints == undefined then return #()
        return joints
    ),

    -- True if any joint token contains 'namePart' as a substring.
    function jointsContain joints namePart =
    (
        for i = 1 to joints.count do (
            if (findString joints[i] namePart) != undefined then return true
        )
        return false
    ),

    function buildCommonExportOptions =
    (
        local opts = USDExporter.CreateOptions()
        opts.FileFormat = #ascii
        opts.Meshes = true
        opts.Skin = true
        opts.Morpher = true
        opts.IncludeAllBones = true
        opts.PreserveBoneMeshes = false
        opts.TimeMode = #current
        opts.RootPrimPath = "/root"
        return opts
    ),

    --
    -- The actual bug (issue #42): an UNSKINNED helper nested under a Biped bone.
    --
    function export_unskinned_helper_under_biped =
    (
        -- 1) A Biped provides the skeleton.
        local theBip = biped.createNew 100 0 [0,0,0]
        local pelvis = findNodeContaining "Pelvis"
        assert_defined pelvis message:"Could not find the Biped pelvis node."

        -- 2) Helpers parented under the biped, with NO skin and NO morpher.
        local nestedPoint = point name:"NestedHelperPoint" size:10
        nestedPoint.transform = (transMatrix [0,0,20])
        nestedPoint.parent = pelvis

        local nestedDummy = dummy name:"NestedHelperDummy" boxsize:[10,10,10]
        nestedDummy.transform = (transMatrix [0,0,30])
        nestedDummy.parent = pelvis

        -- 3) Boundary control: a helper at the scene root, outside any skeleton.
        local standalonePoint = point name:"StandaloneHelperPoint" size:10 pos:[100,0,20]

        -- 4) Export.
        local exportPath = output_prefix + "unskinned_helper_under_biped.usda"
        assert_true (USDExporter.ExportFile exportPath exportOptions:(buildCommonExportOptions())) \
            message:"USD export failed."

        dumpUsdaFile exportPath

        -- 5) Assertions on the exported skeleton joints.
        local stage = pyUsd.Stage.Open exportPath
        assert_defined stage message:"Could not open the exported USD stage."
        local joints = readSkeletonJoints stage

        assert_true (joints.count > 0) \
            message:"No skeleton joints were exported (expected the Biped skeleton)."

        -- The fix: the nested helpers must be present as joints.
        assert_true (jointsContain joints "NestedHelperPoint") \
            message:"ISSUE #42: unskinned Point nested under a Biped is missing from the exported joints."
        assert_true (jointsContain joints "NestedHelperDummy") \
            message:"ISSUE #42: unskinned Dummy nested under a Biped is missing from the exported joints."

        -- Boundary: a helper with no bone ancestor must NOT be promoted to a joint.
        -- This is the invariant the fix must preserve: only helpers nested inside a
        -- skeleton get rescued, never arbitrary scene-root helpers.
        assert_false (jointsContain joints "StandaloneHelperPoint") \
            message:"A helper with no bone ancestor must not be promoted to a joint."

        -- It is still exported though - just as an ordinary transform outside the
        -- skeleton, untouched by the joint-rescue logic.
        local usdaText = readFileToString exportPath
        assert_true ((findString usdaText "StandaloneHelperPoint") != undefined) \
            message:"A helper outside any skeleton should still be exported as an ordinary transform."
    ),

    --
    -- Guards the pre-existing path: a SKINNED helper nested under a Biped bone.
    -- This already worked before the fix (the skin dependency makes the writer
    -- claim the node) and must keep working.
    --
    function export_skinned_helper_under_biped =
    (
        local theBip = biped.createNew 100 0 [0,0,0]
        local pelvis = findNodeContaining "Pelvis"
        assert_defined pelvis message:"Could not find the Biped pelvis node."

        local nestedPoint = point name:"SkinnedNestedPoint" size:10
        nestedPoint.transform = (transMatrix [0,0,20])
        nestedPoint.parent = pelvis

        local bipedBox = box name:"BipedBox" width:8 length:8 height:40 pos:[0,0,0]
        convertToPoly bipedBox
        skinTo #(pelvis, nestedPoint) bipedBox

        local exportPath = output_prefix + "skinned_helper_under_biped.usda"
        assert_true (USDExporter.ExportFile exportPath exportOptions:(buildCommonExportOptions())) \
            message:"USD export failed."

        dumpUsdaFile exportPath

        local stage = pyUsd.Stage.Open exportPath
        assert_defined stage message:"Could not open the exported USD stage."
        local joints = readSkeletonJoints stage
        assert_true (jointsContain joints "SkinnedNestedPoint") \
            message:"A skinned Point nested under a Biped must be exported as a joint."
    ),

    Tests = #(
        export_unskinned_helper_under_biped,
        export_skinned_helper_under_biped
    )
)

runUsdTestFixture export_biped_nested_bone_test(getThisScriptFilename())
