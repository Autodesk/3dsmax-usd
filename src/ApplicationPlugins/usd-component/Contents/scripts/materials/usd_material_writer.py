#
# Copyright 2023 Autodesk
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

"""
Register export callback used for material conversions.

The following script registers a callback function for the `export_complete_callback` function defined
in the 3dsMax USD component plugin. In this example, we use the export complete callback to define and
apply material export conversion logic. This is achieved with the use of `.material_conversion` and `.mat_def`
json files, which are user defined, which can be stored in the "data_files" folder located in this directory, or
either the "$userTools" or "$userSettings" 3dsMax folders.
"""

import os
import sys
import json
import pathlib

from collections import defaultdict

import itertools
try:
    import typing
except ImportError:
    # this is just for type hints
    pass
import numbers
import pymxs
from pxr import Usd, UsdShade, Sdf, Tf, Gf, UsdUtils

import usd_utils
import maxUsd

__author__ = r'Autodesk Inc.'
__copyright__ = r'Copyright 2020, Autodesk Inc.'

RT = pymxs.runtime
RT.pluginManager.loadClass(RT.USDExporter)

WARN = RT.Name("warn")
ERROR = RT.Name('error')

sdf_type_map = {
    "float": Sdf.ValueTypeNames.Float,
    "int": Sdf.ValueTypeNames.Int,
    "float3": Sdf.ValueTypeNames.Float3,
    "normal3f": Sdf.ValueTypeNames.Normal3f,
    "color3f": Sdf.ValueTypeNames.Color3f
}

OSL_BITMAP_OUTPUTS = {
    1:"rgb",
    2:"r",
    3:"g",
    4:"b",
    5:"a"
}

# default export options
_material_export_options = {
    "bake": False,
    "bake_image_type": "png",
    # Typically a relative sdf path from the configured root primitive, where to export materials.
    # Assuming default export options, this would resolve to /Materials".
    # If the path specified here is absolute, then it is used as is.
    "materials_root": "Materials",
    "bake_dir": "baked_textures",
    "bitmap_dimensions": (512, 512),
    "st_input_name": "map1",
    "target_material_id": "UsdPreviewSurface",
    "relative_texture_paths": True,
    "usd_filename": None
}

def bake_bitmap(from_tex, material_export_options, file_path=None):
    """
    Bake the bitmap for a max texture. Return baked file path
    """
    if file_path:
        bake_dir = os.path.dirname(file_path)
    else:
        export_dir = os.path.dirname(material_export_options["usd_filename"])
        bake_dir = os.path.join(export_dir, material_export_options["bake_dir"])
        file_path = os.path.join(bake_dir, from_tex.name)

    if not os.path.exists(bake_dir):
        os.makedirs(bake_dir, exist_ok=True)

    base_name = os.path.basename(file_path)
    render_name, ext = os.path.splitext(base_name)
    render_ext = material_export_options.get("bake_image_type")
    render_file_path = os.path.join(bake_dir, render_name + '.' + render_ext)

    bitmap_dimensions = material_export_options.get("bitmap_dimensions")
    bitmap = RT.renderMap(from_tex, filename=render_file_path, size=RT.point2(bitmap_dimensions[0], bitmap_dimensions[1]))
    RT.save(bitmap)
    RT.close(bitmap)

    return render_file_path

def set_bitmap_scale_bias_sourcecolorspace(uv_texture_shader, is_normal_map):
    source_color_space_input = uv_texture_shader.CreateInput("sourceColorSpace", Sdf.ValueTypeNames.Token)
    # when this is implemented in 2024, we need update the logic to check if the max bitmap
    # color space is set to "raw" or not, for not this is not implemented yet
    source_color_space_input.Set("raw")
    if is_normal_map:
        scale_input = uv_texture_shader.CreateInput("scale", Sdf.ValueTypeNames.Float4)
        bias_input = uv_texture_shader.CreateInput("bias", Sdf.ValueTypeNames.Float4)
        scale_input.Set(Gf.Vec4f(2, 2, 2, 1))
        bias_input.Set(Gf.Vec4f(-1, -1, -1, 0))

def get_max_map_output_channel(max_map):
    # type: (pymxs.MXSWrapperBase) -> str
    """Get the output channel as string for the max texture map.
    Returns rgb by default if not a MultiOutput map."""
    output_channel = "rgb" # default
    if RT.ClassOf(max_map) == RT.MultiOutputChannelTexmapToTexmap:
        output_channel = OSL_BITMAP_OUTPUTS.get(max_map.outputChannelIndex, "")
        if not output_channel:
            source_map = max_map.sourceMap
            RT.UsdExporter.Log(WARN, "Unsupported output channel index for {0}: {1}. Using 'r' channel instead.".format(source_map, max_map.outputChannelIndex))
            # OSL bitmap lookups index 6-7 are 'luminance' and 'average' which don't have a direct translation.
            # Fallback to r as it's probably reasonably close.
            output_channel = 'r'
    return output_channel

def output_usd_texture(from_max_tex, stage, parent_path, material_export_options, usd_shade_to_max_map, is_normal_map):
    # type: (pymxs.MXSWrapperBase, Usd.Stage, Sdf.Path, dict, dict, bool) -> UsdShade.Shader
    """
    Outupt a usd texture from a max texture.
    """
    file_path = None
    texture_lookup = None
    bake_enabled = material_export_options.get("bake", False)
    texture_graph_name = Tf.MakeValidIdentifier(from_max_tex.name)
    output_channel = "rgb"

    if bake_enabled:
        file_path = bake_bitmap(from_max_tex, material_export_options, file_path=file_path)
    else:
        # see if we can resolve to a texture lookup from this input
        texture_lookup = resolve_texture_lookup(from_max_tex)
        if texture_lookup and hasattr(texture_lookup, 'filename'):
            file_path = usd_utils.get_file_path_mxs(texture_lookup.filename)
            texture_graph_name = Tf.MakeValidIdentifier(texture_lookup.name)
            # When figuring out the used channel (rgb,r,g,b or a), do not use the 
            # resolved texture, as the information is not in that node. It lives 
            # in MultiOutputChannelTexmapToTexmap which will be slotted into the 
            # material.
            output_channel = get_max_map_output_channel(from_max_tex)
        elif not texture_lookup and hasattr(from_max_tex, 'filename'):
            file_path = usd_utils.get_file_path_mxs(from_max_tex.filename)
            texture_lookup = from_max_tex

    if not file_path:
        # no valid file path for this texture, skip it.
        RT.UsdExporter.Log(WARN, "Unsupported texture map: {0}".format(from_max_tex))
        return None, texture_lookup, None, None

    # Make the file path relative.
    if material_export_options.get("relative_texture_paths", True) and material_export_options["usd_filename"]:
        start_path = os.path.dirname(material_export_options["usd_filename"])
        file_path = usd_utils.safe_relpath(file_path, start_path)
    
    texture_graph_path = parent_path.AppendChild(texture_graph_name)
    
    # Keep track of whether we are creating a new texture in this call, or simply adding new outputs/connections.
    is_new = True
    
    # This is where we check if the texture_graph is already created or not.
    # If you connect a texturemap to more than one material input, this code will be run multiple times
    # on the same texture node.
    # But it will only export once, because we will find the existing texture_graph in the usd_shade_to_max_map
    texture_graph = None
    uv_texture_shader = None
    if usd_shade_to_max_map.get(texture_graph_path) == texture_lookup:
        # already exported this texture, but we may need to do more connections
        texture_graph = UsdShade.NodeGraph.Get(stage, texture_graph_path)
        uv_texture_shader = UsdShade.Shader.Get(stage, texture_graph_path.AppendChild(texture_graph_name))
        is_new = False
    if not texture_graph:
        # TODO : uniquifying of the paths doesnt work as expected, as this function can be called multiple times 
        # add new outputs. This will not work correctly as the second call will not find the unique name created here.
        texture_graph_path = uniquify_path(stage, parent_path, texture_graph_name)
        texture_graph = UsdShade.NodeGraph.Define(stage, texture_graph_path)
        
        uv_texture_shader = UsdShade.Shader.Define(stage, texture_graph_path.AppendChild(texture_graph_name))
        
        uv_texture_shader.CreateIdAttr("UsdUVTexture")
        usd_shade_to_max_map[texture_graph.GetPath()] = texture_lookup
        # Set to repeat by default (USD sets to "metadata" by default).
        wrap_s_input = uv_texture_shader.CreateInput("wrapS", Sdf.ValueTypeNames.Token)
        wrap_t_input = uv_texture_shader.CreateInput("wrapT", Sdf.ValueTypeNames.Token)
        wrap_s_input.Set("repeat")
        wrap_t_input.Set("repeat")

        if texture_lookup:
            if RT.classOf(texture_lookup) == RT.Bitmaptexture:
                if texture_lookup.coordinates.U_Tile == True:
                    wrap_s_input.Set("repeat")
                if texture_lookup.coordinates.V_Tile == True:
                    wrap_t_input.Set("repeat")
                if texture_lookup.coordinates.U_Mirror == True:
                    wrap_s_input.Set("mirror")
                if texture_lookup.coordinates.V_Mirror == True:
                    wrap_t_input.Set("mirror")
                if not texture_lookup.coordinates.U_Tile and not texture_lookup.coordinates.U_Mirror:
                    wrap_s_input.Set("black")
                if not texture_lookup.coordinates.V_Tile and not texture_lookup.coordinates.V_Mirror:
                    wrap_t_input.Set("black")

            if RT.classOf(texture_lookup) == RT.OSLMap:
                osl_file_name = os.path.basename(texture_lookup.OSLPath).lower()
                # The two OSL bitmaps we support handle wrapping the same way.
                # U and V wrap modes are not configured individually.
                if osl_file_name == "uberbitmap.osl" or osl_file_name == "oslbitmap.osl" or \
                    osl_file_name == "uberbitmap2.osl" or osl_file_name == "oslbitmap2.osl":
                    # The "default" wrap mode means "whatever the default is in the renderer".
                    # We will assume "black" here, as this is how it shows in the Max viewport, and
                    # it seems to be the most common default.
                    if texture_lookup.WrapMode == "black" or texture_lookup.WrapMode == "default":
                        wrap_s_input.Set("black")
                        wrap_t_input.Set("black")
                    elif texture_lookup.WrapMode == "mirror":
                        wrap_s_input.Set("mirror")
                        wrap_t_input.Set("mirror")
                    elif texture_lookup.WrapMode == "periodic":
                        wrap_s_input.Set("repeat")
                        wrap_t_input.Set("repeat")
                    elif texture_lookup.WrapMode == "clamp":
                        wrap_s_input.Set("clamp")
                        wrap_t_input.Set("clamp")

        if usd_utils.has_non_ascii_char(file_path):
            RT.UsdExporter.Log(WARN, "USD does not support unicode in filepaths. Please remove invalid characters from texture filepaths.")
        else:
            windows_path = pathlib.PureWindowsPath(file_path)
            pathStr = str(windows_path.as_posix())
            # Normalizing the path will strip the ./ from the relative path - as far as the OS is concerned, that is fine.
            # However, the PXR ArDefaultResolver, if not seeing a ./ will also look into any defined search paths, having the ./
            # prefix will make it understand that the relative path is anchored to the layer we are exporting. (see
            # ArDefaultResolver::SetDefaultSearchPath())
            if not windows_path.is_absolute():
                pathStr = "./" + pathStr
            uv_texture_shader.CreateInput("file", Sdf.ValueTypeNames.Asset).Set(pathStr)

    if texture_lookup:
        if RT.classOf(texture_lookup) == RT.Bitmaptexture:
            if RT.isProperty(texture_lookup, "bitmap") and texture_lookup.bitmap and texture_lookup.bitmap.gamma == 1.0:
                set_bitmap_scale_bias_sourcecolorspace(uv_texture_shader, is_normal_map)
        elif RT.classOf(texture_lookup) == RT.OSLMap:
            osl_file_name = os.path.basename(texture_lookup.OSLPath).lower()
            if osl_file_name == "uberbitmap.osl" or osl_file_name == "oslbitmap.osl":
                if texture_lookup.AutoGamma == 0 and texture_lookup.ManualGamma == 1.0:
                    set_bitmap_scale_bias_sourcecolorspace(uv_texture_shader, is_normal_map)
            if osl_file_name == "uberbitmap2.osl" or osl_file_name == "oslbitmap2.osl":
                if texture_lookup.ManualGamma == 1.0:
                    set_bitmap_scale_bias_sourcecolorspace(uv_texture_shader, is_normal_map)

    # Add the output to the texture shader, and wire it up to the node graph output.
    if len(output_channel) == 3:
        uv_texture_shader.CreateOutput(output_channel, Sdf.ValueTypeNames.Float3)
        graph_output = texture_graph.CreateOutput(output_channel, Sdf.ValueTypeNames.Float3)
        graph_output.ConnectToSource(uv_texture_shader.ConnectableAPI(), output_channel)
    elif len(output_channel) == 1:
        uv_texture_shader.CreateOutput(output_channel, Sdf.ValueTypeNames.Float)
        graph_output = texture_graph.CreateOutput(output_channel, Sdf.ValueTypeNames.Float)
        graph_output.ConnectToSource(uv_texture_shader.ConnectableAPI(), output_channel)
    else:
        RT.UsdExporter.Log(RT.Name('error'), "Texture sampler connections of {0} channels unsupported.".format(len(output_channel)))

    return texture_graph, texture_lookup, uv_texture_shader, is_new


def output_usd_uv_reader(stage, parent_path, primvar_name):
    """
    Output usd texture sampler from max texture
    """
    uv_reader_name = "PrimvarReader_{0}".format(primvar_name)
    uv_reader_path = uniquify_path(stage, parent_path, uv_reader_name)
    uv_reader = UsdShade.Shader.Define(stage, uv_reader_path)
    uv_reader.CreateIdAttr("UsdPrimvarReader_float2")
    return uv_reader

def normal_bumps():
    """Returns a list of Normal Bump types."""
    normal_bumps = [RT.Normal_Bump]
    if hasattr(RT, "VRayNormalMap"):
        normal_bumps.append(RT.VRayNormalMap)
    return normal_bumps

def resolve_texture_lookup(in_value):
    # type: (pymxs.MXSWrapperBase) -> pymxs.MXSWrapperBase
    """Given a material input, resolve to the source bitmap lookup if possible."""
    in_type = RT.classOf(in_value)
    if in_type == RT.Bitmaptexture:
        return in_value
    elif in_type == RT.MultiOutputChannelTexmapToTexmap:
        # handle known OSL bitmap lookups.
        source_map = in_value.sourceMap
        if source_map and RT.classOf(source_map) == RT.OSLMap:
            if usd_utils.is_supported_osl(source_map.OSLPath):
                return source_map
    # make sure normal bump has a texture input
    elif in_type in normal_bumps():
        normal_map = in_value.normal_map
        if normal_map:
            return resolve_texture_lookup(normal_map)

def uniquify_path(stage, parent_path, name):
    # type: (Usd.Stage, Sdf.Path, str) -> Sdf.Path
    """Generate a unique path"""
    unique_path = parent_path.AppendChild(Tf.MakeValidIdentifier(name))
    # due to max allowing duplicate names, the following is to deduplicate the paths in
    # case there already exists a prim with said path in the stage (otherwise it will be overriden)
    prim = stage.GetPrimAtPath(unique_path)
    uniq = 1
    while prim.IsValid():
        unique_path = parent_path.AppendChild("{0}_{1}".format(Tf.MakeValidIdentifier(name), uniq))
        uniq += 1
        prim = stage.GetPrimAtPath(unique_path)
    return unique_path

def map_channel_from_texturemap(texturemap):
    # type: (pymxs.texturemap) -> int
    """From a texturemap, get the channel map index"""
    map_channel = None
    warn_msg = ""
    if RT.classOf(texturemap) == RT.Bitmaptexture:
        map_channel = texturemap.coordinates.mapChannel
    elif RT.classOf(texturemap) == RT.MultiOutputChannelTexmapToTexmap:
        source_map = texturemap.sourceMap
        if source_map and RT.classOf(source_map) == RT.OSLMap:
            if hasattr(source_map, 'UVSet'):
                # UVSet is the attribute for input map channel of uberbitmap
                # If doesn't exist, fallback to 1 below
                map_channel = source_map.UVSet
            elif hasattr(source_map, 'Pos_map'):
                if hasattr(source_map.Pos_map, 'UVSet'):
                    map_channel = source_map.Pos_map.UVSet
        else:
            warn_msg + "Could not derive UVSet from: {0}\n".format(source_map)

        if not map_channel:
            warn_msg + "OSL texturemap type does not have UVSet: {0}\n".format(texturemap)
    else:
       warn_msg + "Unknown texturemap type for map channel: {0}\n".format(texturemap)

    if not map_channel:
        # default fallback to channel 1
        RT.UsdExporter.Log(WARN, warn_msg + "Using fallback map channel 1 for texture: {0}.".format(texturemap))
        map_channel = 1

    return map_channel

def primvar_mapping_from_texturemap(texturemap, material_export_options):
    # type: (pymxs.texturemap, dict) -> str
    """From a texturemap, get the primvar mapping string."""
    channel_map = map_channel_from_texturemap(texturemap)
    return material_export_options["usd_export_options"].GetChannelPrimvarName(channel_map)

def check_and_warn_for_non_uniform_scaling_with_rotation(scale_x, scale_y, rotation, texture_name):
    # type: (float, float, float, str) -> None
    """Check if the scaling is not uniform. If it is not, and we have a rotation applied, we might get skewed textures in USD."""
    uniformScaling = usd_utils.float_almost_equal(scale_x, scale_y)
    if rotation != 0.0 and not uniformScaling:
        RT.UsdExporter.Log(WARN, "Non uniform texture scaling with an applied rotation may result in incorrect texture mapping for {0}.".format(texture_name))


def create_2dtransform_for_bitmap(stage, texture_graph, uv_primvar_name, uv_reader, st_input, scale_val = Gf.Vec2f(1.0, 1.0), rotation_val = 0.0, translation_val = Gf.Vec2f(0.0, 0.0)):
    """
    Create UsdTransform2d Shader prim if necessary
    """
    if scale_val != (1.0, 1.0) or rotation_val != 0.0 or translation_val != (0.0, 0.0):
        # Connect a UsdTransform2d node to support texture transforms (offset,translate,rotate).
        transform_2d_name = "TextureTransform_{0}".format(uv_primvar_name)
        transform_2d_path = uniquify_path(stage, texture_graph.GetPath(), transform_2d_name)
        transform_2d_prim = UsdShade.Shader.Define(stage, transform_2d_path)
        
        transform_2d_prim.CreateIdAttr("UsdTransform2d")

        transform_input = transform_2d_prim.CreateInput("in", Sdf.ValueTypeNames.Float2)
        transform_input.ConnectToSource(uv_reader.ConnectableAPI(), "result")
        st_input.ConnectToSource(transform_2d_prim.ConnectableAPI(), "result")

        # Transform inputs...
        if scale_val != (1.0, 1.0):
            scale_input = transform_2d_prim.CreateInput("scale", Sdf.ValueTypeNames.Float2)
            scale_input.Set(scale_val)
        
        if rotation_val != 0.0:
            rotation_input = transform_2d_prim.CreateInput("rotation", Sdf.ValueTypeNames.Float)
            rotation_input.Set(rotation_val)

        if translation_val != (0.0, 0.0):
            translation_input = transform_2d_prim.CreateInput("translation", Sdf.ValueTypeNames.Float2)
            translation_input.Set(translation_val)

def write_usd_material(from_mat, conversion_mapping, stage, shader, shader_path, material_export_options, usd_shade_to_max_map):
    # type: (pymxs.MXSWrapperBase, dict, Usd.Stage, Usd.Shader, Sdf.Path, dict, dict) -> None
    """
    Output usd material from a max material
    """
    target_mat_def = conversion_mapping["target_material"]
    target_inputs = target_mat_def["inputs"]

    mat_path = shader_path.GetParentPath()
    material = UsdShade.Material.Get(stage, mat_path)
    
    # mapping_from_material interperets the JSON conversion_mapping, and returns
    # a dictionary with the calculated values for each parameter for the target material
    mapping = usd_utils.mapping_from_material(from_mat, conversion_mapping)
    # Bundle the calculated conversion with the whole recipe for passing on
    conversion_mapping["conversion"] = mapping

    # Iterate over the target material definition items which are both strings: "target_param_name":"target_input_type"
    for target_param_name, target_input_type in target_inputs.items():
        # store off the calculated in_value from mapping
        in_value = mapping.get(target_param_name)
        if in_value == None:
            # There is no mapping for this target_param_name
            continue
        texture_lookup = False
        # note: If you try to compare types that are coming from a maxscript object within a Python script,
        # disregarding the fact the types could now be Python native type, it will generate a MAXScript
        # exception about an unsupported method being called (the operator '>').
        # (i.e. a float keeps being a Python float and not a MAXScript Value representing a float
        #       which a call to pymxs.runtime.classof ends up doing)
        in_type = type(in_value)
        if in_type == pymxs.MXSWrapperBase:
            in_type = RT.classOf(in_value)
            texture_lookup = resolve_texture_lookup(in_value)
        bake_enabled = material_export_options.get("bake", False)

        texture_src_type = ""
        if in_type == RT.Color:
            # The input type is a max color, convert to USD color.
            value = (in_value.r/255, in_value.g/255, in_value.b/255)
        elif texture_lookup or (RT.superClassOf(in_value) == RT.textureMap and bake_enabled):
            # We've found a texture lookup, or we just want to bake this input texturemap.
            if target_input_type == "normal3f":
                # If our input type is normal3f, we only want to map normal maps, but not bump maps.
                texture_src_type = usd_utils.get_src_type(target_param_name, conversion_mapping)
                if not texture_src_type == "normalmap" and not in_type in normal_bumps():
                    msg = "Texture source must be a normal map for {0}.{1}: {2}".format(from_mat, target_param_name, texture_lookup)
                    RT.UsdExporter.Log(WARN, msg)
                    continue

            uv_primvar_name = primvar_mapping_from_texturemap(in_value, material_export_options)
            if not uv_primvar_name:
                RT.UsdExporter.Log(ERROR, "Texture ({0}) using a channel not mapped to a primvar.".format(in_value))
                continue

            texture_graph, resolved_texture, uv_texture, is_new = output_usd_texture(in_value, stage, mat_path.GetParentPath(), material_export_options, usd_shade_to_max_map, (texture_src_type == "normalmap"))

            if not texture_graph:
                # output_usd_texture could return None
                continue

            # As per USD documentation, shader connections should not cross material boundaries.
            # We want to be able to reuse the texture maps we export accross multiple materials, 
            # so they live outside of the materials. To conform to the connectivity rules, we create 
            # an internal reference from inside the material, to the texture's NodeGraph. The 
            # connection is made to the reference, which in within the material, and therefor we 
            # respect the rules.
            texture_ref = UsdShade.NodeGraph.Define(stage, mat_path.AppendChild(texture_graph.GetPrim().GetName()))
            texture_ref.GetPrim().GetReferences().AddInternalReference(texture_graph.GetPath())
            value = texture_ref
                        
            # If the texture is new (i.e. we were not just adding some outputs), add the UsdTransform2d
            # and UsdPrimvarReader_float2 shading nodes to the graph.
            if is_new:
                uv_reader = output_usd_uv_reader(stage, texture_graph.GetPath(), uv_primvar_name)
                input_frame_name = "frame:{0}".format(uv_primvar_name)

                input_frame_primvar = texture_graph.CreateInput(input_frame_name, Sdf.ValueTypeNames.Token)
                input_frame_primvar.Set(uv_primvar_name)

                input_varname = uv_reader.CreateInput("varname", Sdf.ValueTypeNames.String)
                input_varname.ConnectToSource(input_frame_primvar)

                st_input = uv_texture.CreateInput("st", Sdf.ValueTypeNames.Float2)

                if resolved_texture:
                    st_input.ConnectToSource(uv_reader.ConnectableAPI(), "result")
                    if RT.classOf(texture_lookup) == RT.Bitmaptexture:
                        coords = resolved_texture.coords

                        if coords.mappingType != 0:
                            RT.UsdExporter.Log(WARN, "Unsupported texture mapping type. Only \"Texture\" is supported when exporting to USD.".format(texture_lookup.name))
                        else :
                            if coords.mapping != 0 and coords.mapping != 1:
                                RT.UsdExporter.Log(WARN, "Unsupported texture mapping. Only \"Explicit Map Channel\" or \"Vertex Color Channel\" are supported when exporting to USD.".format(texture_lookup.name))
                            else:
                                scale = Gf.Vec2f(coords.UVTransform.scalepart[0], coords.UVTransform.scalepart[1])
                                offset = Gf.Vec2f(coords.UVTransform.translation[0], coords.UVTransform.translation[1])
                                rotationEuler = RT.QuatToEuler(coords.UVTransform.rotationpart)

                                # Mirroring works differently in BitmapTexture VS USD.
                                # In USD : flip and repeat texture outside unit square.
                                # In BitmapTexture : UV coordinates are mirrored.
                                # Luckily we can repruce max's behavior by scaling the texture by a 2X factor.
                                # Caveat is if we are only mirroring on one axis, we may introduce non-uniform scaling, which doesnt
                                # play well with texture rotations.
                                if coords.U_Mirror:
                                    scale[0] *= 2
                                    offset[0] *= 2
                                if coords.V_Mirror:
                                    scale[1] *= 2
                                    offset[1] *= 2

                                # In .UVTransform, negative scalings are implemented via rotations around U,V,W.
                                # In USD, we only want rotation around W, and there is no issue with negative scaling.
                                # Undo the rotation, and reapply the sign to the scale.
                                if coords.U_Tiling < 0.0 and coords.V_Tiling < 0.0:
                                    scale[0] = -scale[0]
                                    scale[1] = -scale[1]
                                    rotationEuler.Z = usd_utils.normalize_angle(rotationEuler.Z - 180)
                                elif coords.U_Tiling < 0.0:
                                    scale[0] = -scale[0]
                                    rotationEuler.X = usd_utils.normalize_angle(rotationEuler.X - 180)
                                elif coords.V_Tiling < 0.0:
                                    scale[1] = -scale[1]
                                    rotationEuler.X = usd_utils.normalize_angle(rotationEuler.X + 180)
                                    rotationEuler.Z = usd_utils.normalize_angle(rotationEuler.Z + 180)

                                if scale != None:
                                    check_and_warn_for_non_uniform_scaling_with_rotation(scale[0], scale[1], rotationEuler.Z, texture_lookup.name)
                                  
                                if not usd_utils.float_almost_equal(rotationEuler.X, 0.0) or not usd_utils.float_almost_equal(rotationEuler.Y, 0.0):
                                    RT.UsdExporter.Log(WARN, "Unsupported texture rotation axis found on {0}. Only rotations around the Z axis can be exported to USD.".format(texture_lookup.name))
                                
                                create_2dtransform_for_bitmap(stage, texture_graph, uv_primvar_name, uv_reader, st_input, scale, rotationEuler.Z, offset)

                    if RT.classOf(texture_lookup) == RT.OSLMap:
                        osl_file_name = os.path.basename(texture_lookup.OSLPath)

                        if osl_file_name.lower() == "uberbitmap.osl" or osl_file_name.lower() == "uberbitmap2.osl":
                            # Parameters mapped from the OSL nodes inputs cannot be supported as they can be procedural, node dependant, randomized, etc.
                            # Warn the user in those cases...
                            if texture_lookup.Offset_map != None:
                                RT.UsdExporter.Log(WARN, "The OSL UberBitmap Offset input is not supported when exporting to USD. Only static texture transforms are supported.".format(texture_lookup.name))
                            if texture_lookup.RealHeight_map != None:
                                RT.UsdExporter.Log(WARN, "The OSL UberBitmap RealHeight input is not supported when exporting to USD. Only static texture transforms are supported.".format(texture_lookup.name))
                            if texture_lookup.RealWidth_map != None:
                                RT.UsdExporter.Log(WARN, "The OSL UberBitmap RealWidth input is not supported when exporting to USD. Only static texture transforms are supported.".format(texture_lookup.name))
                            if texture_lookup.RotAxis_map != None:
                                RT.UsdExporter.Log(WARN, "The OSL UberBitmap RotAxis input is not supported when exporting to USD. Only static texture transforms are supported.".format(texture_lookup.name))
                            if texture_lookup.RotCenter_map != None:
                                RT.UsdExporter.Log(WARN, "The OSL UberBitmap RotCenter input is not supported when exporting to USD. Only static texture transforms are supported.".format(texture_lookup.name))
                            if texture_lookup.Rotate_map != None:
                                RT.UsdExporter.Log(WARN, "The OSL UberBitmap Rotate input is not supported when exporting to USD. Only static texture transforms are supported.".format(texture_lookup.name))
                            if texture_lookup.Scale_map != None:
                                RT.UsdExporter.Log(WARN, "The OSL UberBitmap Scale input is not supported when exporting to USD. Only static texture transforms are supported.".format(texture_lookup.name))
                            if texture_lookup.Tiling_map != None:
                                RT.UsdExporter.Log(WARN, "The OSL UberBitmap Tiling input is not supported when exporting to USD. Only static texture transforms are supported.".format(texture_lookup.name))
                            if texture_lookup.WrapMode_map != None:
                                RT.UsdExporter.Log(WARN, "The OSL UberBitmap  WrapMode input is not supported when exporting to USD. Only static texture transforms are supported.".format(texture_lookup.name))

                            # Calculate total scaling, given RealWorld scale config, scale and tiling.
                            # Inspired from the UberBitmap.osl implementation.
                            worldScale = [1.0,1.0]
                            if (texture_lookup.RealWorld):
                                worldScale = [texture_lookup.RealWidth, texture_lookup.RealHeight, 1.0];

                            bitmap_u_scaling = texture_lookup.Scale * worldScale[0]
                            bitmap_u_tiling = texture_lookup.tiling[0]
                            if bitmap_u_scaling != 0.0:
                                usd_scale_u = bitmap_u_tiling / bitmap_u_scaling
                            else:
                                usd_scale_u = sys.float_info.max if bitmap_u_tiling > 0.0 else -sys.float_info.max

                            bitmap_v_scaling =  texture_lookup.Scale * worldScale[1]
                            bitmap_v_tiling = texture_lookup.tiling[1]
                            if bitmap_v_scaling != 0.0:
                                usd_scale_v =  bitmap_v_tiling / bitmap_v_scaling
                            else:
                                usd_scale_v = sys.float_info.max if bitmap_v_tiling > 0.0 else -sys.float_info.max

                            # Multiplication order is different in OSL vs USdTransform2D, so need to scale the offset.
                            # The offset is for the texture, whereas we are moving the UVs, so we need to inverse it.
                            usd_offset_u = usd_scale_u * -texture_lookup.offset[0]
                            usd_offset_v = usd_scale_v * -texture_lookup.offset[1]

                            # USdTransform2D only supports rotations around W.
                            if texture_lookup.RotAxis[0] != 0.0:
                                RT.UsdExporter.Log(WARN, "Unsupported texture rotation, X axis value {0} found on {1} - value must be equal to 0.0. Texture rotation will be ignored.".format(texture_lookup.RotAxis[0], texture_lookup.name))
                            elif texture_lookup.RotAxis[1] != 0.0:
                                RT.UsdExporter.Log(WARN, "Unsupported texture rotation, Y axis value {0} found on {1} - value must be equal to 0.0. texture rotation will be ignored.".format(texture_lookup.RotAxis[1], texture_lookup.name))
                            elif texture_lookup.RotAxis[2] <= 0.0:
                                RT.UsdExporter.Log(WARN, "Unsupported texture rotation, Z axis value {0} found on {1} - value must be greater than 0.0. texture rotation will be ignored.".format(texture_lookup.RotAxis[1], texture_lookup.name))
                            else:
                                # Deal with the rotation. We need to adjust the translation to account for the rotation center.
                                rot_matrix = RT.RotateZMatrix(texture_lookup.Rotate)
                                
                                scaled_center_u = texture_lookup.RotCenter[0] * usd_scale_u
                                scaled_center_v = texture_lookup.RotCenter[1] * usd_scale_v

                                rot_center_u = usd_offset_u - scaled_center_u
                                rot_center_v = usd_offset_v - scaled_center_v
                                rotatedCenter = RT.Point3(rot_center_u, rot_center_v, 0) * rot_matrix

                                usd_offset_u = rotatedCenter[0] + scaled_center_u
                                usd_offset_v = rotatedCenter[1] + scaled_center_v

                            scale_value = Gf.Vec2f(usd_scale_u,usd_scale_v)
                            rotate_value = texture_lookup.Rotate
                            translation_value = Gf.Vec2f(usd_offset_u, usd_offset_v)

                            if scale_value != None and texture_lookup.Rotate != None:
                                check_and_warn_for_non_uniform_scaling_with_rotation(scale_value[0], scale_value[1], texture_lookup.Rotate, texture_lookup.name)

                            create_2dtransform_for_bitmap(stage, texture_graph, uv_primvar_name, uv_reader, st_input, scale_value, rotate_value, translation_value)
                        
                        if osl_file_name.lower() == "oslbitmap.osl" or osl_file_name.lower() == "oslbitmap2.osl":
                            # Parameters mapped from the OSL nodes inputs cannot be supported as they can be procedural, node dependant, randomized, etc.
                            # Warn the user in those cases...
                            if texture_lookup.Pos_map != None:
                                RT.UsdExporter.Log(WARN, "The OSL Bitmap Position input is not supported when exporting to USD. Only static texture transforms are supported.".format(texture_lookup.name))
                            if texture_lookup.Scale_map != None:
                                RT.UsdExporter.Log(WARN, "The OSL Bitmap Scale input is not supported when exporting to USD. Only static texture transforms are supported.".format(texture_lookup.name))
                            if texture_lookup.WrapMode_map != None:
                                RT.UsdExporter.Log(WARN, "The OSL Bitmap WrapMode input is not supported when exporting to USD. Only static texture transforms are supported.".format(texture_lookup.name))

                            scale = 1.0
                            if texture_lookup.Scale != 0.0:
                                scale = 1.0 / texture_lookup.Scale

                            scale_value = Gf.Vec2f(scale,scale)

                            create_2dtransform_for_bitmap(stage, texture_graph, uv_primvar_name, uv_reader, st_input, scale_val = scale_value)

                else:
                    st_input.ConnectToSource(uv_reader.ConnectableAPI(), "result")
        else:
            value = in_value

        if in_type == RT.Color and target_input_type == "float":
            # To go from color to float we take the luminance.
            value = 0.2126 * value[0] + 0.7152 * value[1] + 0.0722 * value[2]

        if isinstance(value, UsdShade.NodeGraph):
            channel = get_max_map_output_channel(in_value)
            shader.CreateInput(target_param_name, sdf_type_map[target_input_type]).ConnectToSource(value.ConnectableAPI(), channel)
        elif isinstance(value, numbers.Number) or RT.superClassOf(value) == RT.Value:
            shader.CreateInput(target_param_name, sdf_type_map[target_input_type]).Set(value)



def export_material(material, stage, shader_prim, usd_path, usd_filename, export_options, usd_shade_to_max_map, isUSDZ = False):
    material_export_options = dict(_material_export_options)
    material_export_options["usd_filename"] = usd_filename
    material_export_options["usd_export_options"] = export_options

    # If exporting to USDZ, force absolute paths. There are scenarios in which usdzip completely fails
    # to resolve relative paths (it will sometimes erroneously resolves from the destination folder...)
    # Using absolute paths, we avoid these issues, and it makes no difference, as paths are re-anchored
    # to the USDZ root anyway after the call to usdzip.
    if isUSDZ:
        material_export_options["relative_texture_paths"] = False
    
    export_target_id = material_export_options.get("target_material_id", "UsdPreviewSurface")
    source_id = usd_utils.get_id_from_mat(material)
    if not source_id:
        RT.UsdExporter.Log(WARN, "Unknown material, cannot get an id: {0}".format(material))
        return
    conversion_recipe = usd_utils.get_conversion_recipe(source_id, "3dsmax", export_target_id, "usd")

    if not conversion_recipe:
        RT.UsdExporter.Log(WARN, "No conversion recipe for material: {0}".format(material))
        return

    write_usd_material(material, conversion_recipe, stage, shader_prim, usd_path, material_export_options, usd_shade_to_max_map)