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
Material reader methods

It makes use of `.material_conversion` and `.mat_def` user defined json files stored in the "data_files" 
folder located in this directory, or either the "$userTools" or "$userSettings" 3dsMax folders.

"""

import os
import json
import sys
try:
    import typing
except ImportError:
    # this is just for type hints
    pass

from collections import defaultdict

import pymxs
from pxr import Usd, UsdShade, Gf, UsdUtils, UsdGeom, Sdf

import usd_utils

__author__ = r'Autodesk Inc.'
__copyright__ = r'Copyright 2020, Autodesk Inc.'

RT = pymxs.runtime
RT.pluginManager.loadClass(RT.USDImporter)

WARN = RT.Name("warn")
ERROR = RT.Name("error")

maxver = RT.maxversion()
if maxver[0] >= 26000:  # 3ds Max 2024 and up
    _oslbitmapfile = "OSLBitmap2.osl"
    _uberbitmapfile = "UberBitmap2.osl"
else:
    _oslbitmapfile = "OSLBitmap.osl"
    _uberbitmapfile = "UberBitmap.osl"

UBERBITMAP_OUTPUTS = {
    "rgb":1,
    "r":2,
    "g":3,
    "b":4,
    "a":5
}

def get_usd_texture_map_channel(usd_texture, material_import_options):
    # type: (UsdShade.Shader, dict) -> int
    """Get map channel for a usd texture. Fallback to 1 if any unexpected missing connections."""
    st_attr_input = usd_texture.GetInput("st")
    if not st_attr_input:
        RT.UsdImporter.Log(ERROR, "Missing 'st' input for : {0}".format(usd_texture))
        return 1
    primvar_reader_outputs = st_attr_input.GetValueProducingAttributes()
    if not primvar_reader_outputs:
        RT.UsdImporter.Log(ERROR, "Unable to determine primvar input for : {0}".format(usd_texture))
        return 1
    if len(primvar_reader_outputs) > 1:
        RT.UsdImporter.Log(WARN, "USDTextureReader has more than one 'st' input, using first input : {0}".format(usd_texture))

    st_src_prim = primvar_reader_outputs[0].GetPrim()

    st_source = UsdShade.Shader(st_src_prim)
    varname_input = st_source.GetInput("varname")
    id = st_source.GetIdAttr().Get()

    if id == "UsdTransform2d":
        # Then there must be a transform2D
        transform_in_input = st_source.GetInput("in")
        if not transform_in_input:
            RT.UsdImporter.Log(ERROR, "Unable to determine primvar input for : {0}".format(usd_texture))
            return 1
        transform_2d_outputs = transform_in_input.GetValueProducingAttributes()
        outputs_count = len(transform_2d_outputs)
        if not transform_2d_outputs or outputs_count < 1:
            RT.UsdImporter.Log(ERROR, "Unable to determine primvar input for : {0}".format(usd_texture))
            return 1
        st_src_prim = transform_2d_outputs[0].GetPrim()
        usd_primvar_reader = UsdShade.Shader(st_src_prim)
    elif id == "UsdPrimvarReader_float2":
        usd_primvar_reader = st_source
    else:
        RT.UsdImporter.Log(ERROR, "Unable to determine primvar input for : {0}".format(usd_texture))
        return 1

    varname_input = usd_primvar_reader.GetInput("varname")
    varname_value_attributes = varname_input.GetValueProducingAttributes()
    if not varname_value_attributes:
        RT.UsdImporter.Log(ERROR, "Unable to determine primvar input for : {0}".format(usd_texture))
        return 1
    if len(varname_value_attributes) > 1:
        RT.UsdImporter.Log(WARN, "USDPrimvarReader has more than one 'varname' input, using first input : {0}".format(usd_primvar_reader))

    primvar_name = varname_value_attributes[0].Get()

    primvar_channel = material_import_options["usd_import_options"].GetPrimvarChannel(primvar_name)
    if primvar_channel == -1:
        RT.UsdImporter.Log(ERROR, "Channel is not mapped for primvar name: {0} for {1}. Using fallback channel 1.".format(primvar_name, usd_texture))
        return 1

    return primvar_channel

def output_max_texture_osl(usd_texture, max_texture_id, material_import_options):
    # type: (UsdShade.Shader, str, dict) -> pymxs.runtime.Texture
    """Return an OSL texture reader from a USD texture node"""
    if not usd_utils.is_supported_osl(max_texture_id):
        RT.UsdImporter.Log(ERROR, "Unsupported OSL type specified, using {0} instead: {1}".format(_uberbitmapfile, max_texture_id))
        max_texture_id = _uberbitmapfile

    texture = RT.OSLMap()
    max_root = RT.symbolicPaths.getPathValue("$max")
    osl_path = os.path.join(max_root, 'OSL', max_texture_id)
    texture.OSLPath = osl_path
    texture.OSLAutoUpdate = True

    file_attr = usd_texture.GetInput('file')
    if file_attr:
        resolved_file_path = file_attr.Get().resolvedPath
        
        # if file_attr Shader input resolvedPath is an empty string, this may be because it is truly
        # an empty string at the resolvedPath or it can be due to the UDIM case:
        #   https://forum.aousd.org/t/getting-empty-string-when-trying-to-get-resolvedpath-of-udim-path/406/2
        if resolved_file_path == '':
            file_path = file_attr.Get().path
            is_udim = usd_utils.is_filepath_udim(file_path)

            if is_udim:
                dir = os.path.dirname(os.path.abspath(file_path))
                udim_file = os.path.basename(file_path)
                directory_name = os.fsdecode(dir)
                udim_filename = os.fsdecode(udim_file)
                first_udim_filename = usd_utils.find_first_valid_udim_for_filename(directory_name, udim_filename)

                if first_udim_filename is None:
                    RT.UsdImporter.Log(ERROR, "UDIM format specified in imported file but no UDIM valid files found at specified directory : {0}".format(file_path))
                else:
                    list_of_udims_with_udims_in_their_filename = usd_utils.get_all_valid_udims_from_dir_for_filename(directory_name, udim_filename)
                    udim_list_str = ' '.join(list_of_udims_with_udims_in_their_filename)
                    
                    texture.filename = file_path
                    texture.UDIM = 1
                    texture.loadUDIM = os.path.join(directory_name, udim_filename)
                    texture.Filename_UDIMList = udim_list_str

            elif os.path.isfile(file_path):
                # likely user error where they set empty string for "inputs:file" Shader of UsdUVTexture 
                # or could be more cases where resolution fails for whatever reason
                texture.filename = file_path
            else:
                texture.filename = ''
        else:
            texture.filename = resolved_file_path
        

    primvar_channel = get_usd_texture_map_channel(usd_texture, material_import_options)

    st_input = usd_texture.GetInput("st")
    wrap_s_input = usd_texture.GetInput("wrapS")
    wrap_t_input = usd_texture.GetInput("wrapT")

    wrap_s_val = None
    wrap_t_val = None
    scale_val = None
    rotation_val = None
    translation_val = None

    if wrap_s_input:
        wrap_s_val = wrap_s_input.Get()

    if wrap_t_input:
        wrap_t_val = wrap_t_input.Get()

    if st_input and st_input.GetConnectedSource() is not None:
        st_conn_source = st_input.GetConnectedSource()[0]

        scale_input = st_conn_source.GetInput("scale")
        rotation_input = st_conn_source.GetInput("rotation")
        translation_input = st_conn_source.GetInput("translation")

        scale_val = scale_input.Get()
        rotation_val = rotation_input.Get()
        translation_val = translation_input.Get()

    if max_texture_id.lower() == _uberbitmapfile.lower():
        texture.UVSet = primvar_channel

        # determine and apply wrap mode
        if wrap_s_val and wrap_t_val and wrap_s_val != wrap_t_val:
            RT.UsdImporter.Log(WARN, 'No support for differing wrap modes, defined in wrapS and wrapT as defined in {0}. Setting value to "periodic" for import.'.format(usd_texture.GetPrim().GetName()))
            texture.WrapMode = "periodic"
        elif wrap_s_val or wrap_t_val:
            usd_wrap_mode = wrap_s_val or wrap_t_val
            if usd_wrap_mode == "black":
                texture.WrapMode = "black"
            elif usd_wrap_mode == "clamp":
                texture.WrapMode = "clamp"
            elif usd_wrap_mode == "repeat":
                texture.WrapMode = "periodic"            
            elif usd_wrap_mode == "mirror":
                texture.WrapMode = "mirror"
            elif usd_wrap_mode == "useMetadata":
                RT.UsdImporter.Log(WARN, 'No support for wrap mode "useMetadata" found in {0}. Setting value to "periodic" for import.'.format(usd_texture.GetPrim().GetName()))
                texture.WrapMode = "periodic"
        else:
            # default wrap mode in 3dsmax
            texture.WrapMode = "periodic"

        if translation_val:
            # apply translation
            texture.offset = RT.Point3(-translation_val[0], -translation_val[1], 0)

        if rotation_val and not usd_utils.float_almost_equal(rotation_val, 0):
            # apply rotation
            texture.rotate = rotation_val
            texture.RotAxis = RT.Point3(0, 0, 1) # fix rotation axis to (0, 0, 1)
            texture.RotCenter = RT.Point3(0, 0, 0) # fix rotation axis to (0, 0, 0) (instead of (0.5, 0.5, 0) uber bitmap default)

            rot_matrix = RT.RotateZMatrix(-rotation_val)

            usd_translation = RT.Point3(texture.offset[0], texture.offset[1], 0)
            rotated_translation = usd_translation * rot_matrix

            texture.offset = rotated_translation

        if scale_val:
            # apply scaling

            if scale_val[0] == scale_val[1]:
                texture.tiling = RT.Point3(1, 1, 1) # fix tiling to (1, 1, 1)
                texture.scale = texture.tiling[0] / scale_val[0] if scale_val[0] != 0 else sys.float_info.max
                texture.offset = RT.Point3(
                    texture.offset[0] / scale_val[0] if scale_val[0] != 0 else sys.float_info.max, 
                    texture.offset[1] / scale_val[1] if scale_val[1] != 0 else sys.float_info.max, 
                    0)
            else:
                if rotation_val != 0:
                    RT.UsdImporter.Log(WARN, "Non uniform texture scaling with an applied rotation may result in incorrect texture mapping for {0}.".format(usd_texture.GetPrim().GetName()))
                
                texture.scale = 1 # fix scale to 1
                texture.tiling = RT.Point3(
                    scale_val[0] if scale_val[0] != 0 else sys.float_info.max, 
                    scale_val[1] if scale_val[1] != 0 else sys.float_info.max, 
                    0)
                texture.offset = RT.Point3(
                    texture.offset[0] / scale_val[0] if scale_val[0] != 0 else sys.float_info.max, 
                    texture.offset[1] / scale_val[1] if scale_val[1] != 0 else sys.float_info.max, 
                    0)

    elif max_texture_id.lower() == _oslbitmapfile.lower():
        pos_map = RT.OSLMap()
        pos_osl_path = os.path.join(max_root, 'OSL', 'GetUVW.osl')
        pos_map.OSLPath = pos_osl_path
        pos_map.OSLAutoUpdate = True
        texture.Pos_map = pos_map
        pos_map.UVSet = primvar_channel

    return texture

def valid_texture_type(max_texture_type):
    # type: (str) -> bool
    """Is this a valid texture type?"""
    if hasattr(RT, max_texture_type):
        return True
    if max_texture_type.lower().endswith(".osl") and hasattr(RT, 'OSLMap'):
        return True
    return False

def output_max_classic_bitmap(usd_texture, material_import_options):
    # type: (UsdShade.Shader, dict) -> pymxs.MXSWrapperBase
    """Output max bitmap from usd texture"""
    texture = RT.Bitmaptexture()
    file_attr = usd_texture.GetInput('file')
    if file_attr:
        file_path = file_attr.Get().resolvedPath
        texture.filename = file_path
    primvar_channel = get_usd_texture_map_channel(usd_texture, material_import_options)
    texture.coordinates.mapChannel = primvar_channel
    return texture

def output_max_texture(usd_texture, material_import_options, usd_shader_to_max_map={}):
    # type: (UsdShade.Shader, dict, dict) -> pymxs.MXSWrapperBase
    """Output max texture from usd texture, based on material_import_options"""
    max_texture_id = material_import_options.get("texture_target_id", "Bitmaptexture")
    if not valid_texture_type(max_texture_id):
        RT.UsdImporter.Log(WARN, "Invalid texture type specified: {0}".format(max_texture_id))
        return

    max_texture = usd_shader_to_max_map.get(usd_texture.GetPath(), None)
    if not max_texture:
        if max_texture_id.lower().endswith('.osl'):
            max_texture = output_max_texture_osl(usd_texture, max_texture_id, material_import_options)
        else:
            max_texture = output_max_classic_bitmap(usd_texture, material_import_options)

        map_name = usd_texture.GetPath().name
        max_texture.name = map_name
        usd_shader_to_max_map[usd_texture.GetPath()] = max_texture
    return max_texture

def output_max_material(shader, material_import_options, usd_shader_to_max_map={}):
    # type: (UsdShade.Shader, dict, dict) -> pymxs.MXSWrapperBase
    """
    Create a max material and texture connections from a usd shader.
    """
    source_id = usd_utils.get_id_from_mat(shader)
    if not source_id:
        RT.UsdImporter.Log(WARN, "Unknown shader, cannot get an id: {0}".format(shader))
        return
    target_id = material_import_options.get("material_target_id", "MaxUsdPreviewSurface")

    conversion_recipe = usd_utils.get_conversion_recipe(source_id, "usd", target_id, "3dsmax")
    if not conversion_recipe:
        RT.UsdImporter.Log(WARN, "No conversion recipe found for {0}".format(shader))
        return

    # mapping_from_material interperets the JSON conversion_mapping, and returns
    # a dictionary with the calculated values for each parameter for the target material
    conversion = usd_utils.mapping_from_material(shader, conversion_recipe)
    # Bundle the calculated conversion with the whole recipe for passing on
    conversion_recipe["conversion"] = conversion

    target_material_id = conversion_recipe["target_material"]["id"]
    target_mat_domain = conversion_recipe["target_material"]["domain"]

    klass = getattr(RT, target_material_id)
    mat = klass()

    target_mat_def = usd_utils.get_material_def(target_material_id, target_mat_domain)
    target_inputs = target_mat_def["inputs"]

    # Iterate through all the conversion mapping items.
    # The key is expected to be the target parameter of the material,
    # and the value is the mapped value from the source material.
    for target_param, mapped_value in conversion.items():
        if mapped_value == None:
            # The value is None, continue.
            continue

        # Get the input type for the target material parameter, defined by material definition.
        input_type = target_inputs.get(target_param, None)
        if input_type is None:
            RT.UsdImporter.Log(ERROR, "Input type not defined in material definition for target_param: {0} - {1}".format(target_param, target_mat_def))
            continue

        # Handle params that may be mapped to usd attributes
        mapped_to_attribute = isinstance(mapped_value, Usd.Attribute)
        if input_type == "use_map":
            # In max materials usually have a boolean 'use_map' for texture maps.
            # When this maps to some USD Attribute input, this means there
            # is a texture and so this must be true.
            setattr(mat, target_param, True)
            continue
        elif mapped_to_attribute and input_type in ["texturemap", "normalmap"]:
            # The value is a USD Attribute, now we need to get the attribute source.
            usd_input_prim = mapped_value.GetPrim()
            if usd_input_prim.IsValid():
                
                channel_output = UsdShade.Output(mapped_value)
                if not channel_output:
                    RT.UsdImporter.Log(WARN, "Unsupported UsdShade connection:{0}".format(str(mapped_value)))
                    continue
                
                # 2 scenarios. Either we are connected directly to a shader output, or we are 
                # connected to a node graph's output.            
                # If we have a value producing attribute (node graphs), we need to figure out the 
                # actual shader output (digs down node the node graph recursively).
                value_producing_attrs = channel_output.GetValueProducingAttributes()
                # Shader connection
                if len(value_producing_attrs) == 0:
                    usd_shader = UsdShade.Shader(usd_input_prim)
                # Node graph connection
                else:
                    value_attr = value_producing_attrs[0]
                    shader_prim = value_attr.GetPrim()
                    if not shader_prim.IsA(UsdShade.Shader):
                        RT.UsdImporter.Log(WARN, "Unsupported UsdShade connection:{0}".format(str(value_attr)))
                        continue
                    usd_shader = UsdShade.Shader(shader_prim)                
                                
                # Our target material input type is a texturemap (or normalmap)
                # We will need to output a max bitmap texture reader of some kind.
                
                max_texture = output_max_texture(usd_shader, material_import_options, usd_shader_to_max_map)
                if RT.classOf(max_texture) == RT.OSLMap:
                    # with oslmaps we can connect specific texture channels
                    output = UsdShade.Output(mapped_value)
                    channels = None
                    if output:
                        value_producing_attrs = output.GetValueProducingAttributes()
                        # NodeGraph connection
                        if len(value_producing_attrs) > 0:
                            value_attr = value_producing_attrs[0]
                            channels = value_attr.GetBaseName()
                    if not channels:
                        # Shader connection
                        channels = mapped_value.GetBaseName()
                    multi_channel_map = RT.MultiOutputChannelTexmapToTexmap()
                    multi_channel_map.sourceMap = max_texture
                    out_channel_index = UBERBITMAP_OUTPUTS.get(channels, 1)
                    multi_channel_map.outputChannelIndex = out_channel_index
                    multi_channel_map.name = "{0}:{1}".format(max_texture.name, channels)
                    setattr(mat, target_param, multi_channel_map)
                else:
                    # otherwise - a simple connection for standard bitmaps
                    setattr(mat, target_param, max_texture)
            else:
                RT.UsdImporter.Log(WARN, "USD Attribute input prim is not valid:{0}".format(mapped_value))
            continue
        elif not mapped_to_attribute and input_type in ["texturemap", "normalmap"]:
            # this occurs as a side effect of how the mapping conversion system was made:
            # when we have a property that has an associated map property and the property
            # value is a non-attribute, the associated map property still gets processed by
            # function and ends up having no meaning in terms of the import process.
            continue
        elif mapped_to_attribute:
            # Skip any remaining mapped_to_attribute mappings.
            # This is expected because we map N to 1 : max param to one USD param.
            # For example, this is the mapping for USDPreviewSurface to PhysicalMaterial.
            # Keys on the left are the parameters for PhysicalMaterial, right-hand side is USD:
            # "roughness": "roughness"
            # "roughness_map": "roughness"
            # "roughness_map_on": "roughness"
            # All of these might evaluate to a USDAttribute, or they all might evaluate to a float.
            continue

        # handle params mapped to values
        if input_type == "color":
            if isinstance(mapped_value, Gf.Vec3f):
                # Convert USD color (Vec3f) to a Max color
                color = RT.color(mapped_value[0]*255, mapped_value[1]*255, mapped_value[2]*255)
                setattr(mat, target_param, color)
            else:
                # Anything else is unexpected.
                RT.UsdImporter.Log(WARN, "Expected input for color param:{0}".format(mapped_value))
        elif input_type in ["float", "int", "boolean"]:
            # A simple value, can be set directly.
            setattr(mat, target_param, mapped_value)
        else:
            # Anything else is unexpected.
            RT.UsdImporter.Log(WARN, "Unexpected input type or mapped value - {0}:{1}:{2}".format(input_type, mapped_value, target_param))
    return mat

