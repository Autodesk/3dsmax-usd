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
Defined utility functions used by the `usd_material_import.py` and `usd_material_export.py` scripts
"""

import pymxs
import json
import os, re

from contextlib import contextmanager
from collections import defaultdict

try:
    import typing
except ImportError:
    # this is just for type hints
    pass

from pxr import UsdShade

__author__ = r'Autodesk Inc.'
__copyright__ = r'Copyright 2020, Autodesk Inc.'

@contextmanager
def cwd(path):
    oldpwd = os.getcwd()
    os.chdir(path)
    try:
        yield
    finally:
        os.chdir(oldpwd)

RT = pymxs.runtime
RT.pluginManager.loadClass(RT.USDExporter)

BITMAP = RT.Name("Bitmap")

_THIS_DIR = os.path.dirname(__file__)
DATA_FILES_DIR = os.path.join(_THIS_DIR, "data_files")

max_ver_info = RT.maxversion()
max_ver_year = max_ver_info[0] # RT.maxversion() returns an array where index 0 holds part of the build number

DATA_EXTENSIONS = [".material_conversion", ".mat_def"]
SEARCH_PATHS = {
    "defaults": DATA_FILES_DIR
}

SEARCH_PATHS["UserPath1"] = RT.symbolicPaths.getPathValue("$userTools")
SEARCH_PATHS["UserPath2"] = RT.symbolicPaths.getPathValue("$userSettings")

# data global dicts
_CONFIG_DATA = {} # type: typing.Dict[str, dict]

SUPPORTED_OSL_BITMAPS = ['uberbitmap.osl', 'oslbitmap.osl', 'uberbitmap2.osl', 'oslbitmap2.osl']

def is_supported_osl(osl_name):
    # type: (str) -> bool
    base_name = os.path.basename(osl_name).lower()
    return base_name in SUPPORTED_OSL_BITMAPS

def has_non_ascii_char(str):
    unicodeMatch = re.search("[^\x00-\x7F]", str)
    return unicodeMatch != None

def get_file_path_mxs(filename):
    _, file_path = RT.FileResolutionManager.getFullFilePath(pymxs.byref(filename), BITMAP)
    return file_path

def safe_relpath(path, start):
    """Return relpath if same drive, else return path."""
    path_drive, _ = os.path.splitdrive(os.path.abspath(path))
    start_drive, _ = os.path.splitdrive(os.path.abspath(start))
    if path_drive.lower() == start_drive.lower():
        return os.path.relpath(path, start)
    return path

def _gather_data_files(search_paths=SEARCH_PATHS, data_extensions=DATA_EXTENSIONS):
    """
    Search SEARCH_PATHS for DATA_EXTENSIONS files.
    Return a nested dictionary of data files:
    data_files["path_type"]["extension"]
    """
    data_files = {}
    with cwd(_THIS_DIR):
        for path_type, search_path in search_paths.items():
            path_dict = defaultdict(list)
            data_files[path_type] = path_dict
            for file in os.listdir(search_path):
                for extension in data_extensions:
                    if file.lower().endswith(extension):
                        path_dict[extension].append(os.path.join(search_path, file))
    return data_files

def _gather_data(data_files):
    """
    Get data from data_files and concatenate into dictionaries.
    Return a nested dictionary of :
    conversion_data[extension]
    """
    conversion_data = {}
    for extension in DATA_EXTENSIONS:
        data = {}
        conversion_data[extension] = data
        for path_type in SEARCH_PATHS:
            for data_file in data_files[path_type][extension]:
                json_data = {}
                try:
                    with open(data_file) as f:
                        json_data = json.load(f)
                except:
                    print("Bad json data file: {data_file}")
                data.update(json_data)
    return conversion_data

def get_config_data(update=False):
    """Lazy get of config data."""
    global _CONFIG_DATA
    if not update and _CONFIG_DATA:
        return dict(_CONFIG_DATA)

    config_files = _gather_data_files()
    _CONFIG_DATA = _gather_data(config_files)
    return _CONFIG_DATA

def get_material_def(name, domain):
    """Get a material definition from config data"""
    config_data = get_config_data()

    material_def = None
    material_definitions = config_data[".mat_def"]
    for mat in material_definitions.values():
        if mat.get("id") == name and mat.get("domain") == domain:
            material_def = mat
            break

    if material_def:
        return material_def
    else:
        raise ValueError((name, domain))

def get_conversion_recipe(source_id, source_domain, target_id, target_domain):
    """From a material source_id and source_domain and target id, return conversion mapping dict"""
    config_data = get_config_data()

    conversion_recipe = {}

    # get the full material definitions
    source_def = None
    target_def = None
    parameter_mapping = None

    for mat_def_name, mat_def in config_data[".mat_def"].items():
        mat_id = mat_def.get("id")
        if not mat_id:
            Warning("Material definition missing 'id': {0}".format(mat_def_name))
            continue

        mat_domain = mat_def.get("domain")
        if not mat_domain:
            Warning("Material definition missing 'domain': {0}".format(mat_def_name))
            continue

        if mat_id == source_id and mat_domain == source_domain:
            source_def = dict(mat_def)
        elif mat_id == target_id and mat_domain == target_domain:
            target_def = dict(mat_def)

    if not source_def and target_def:
        # couldn't find both source and target material definitions
        return {}

    for recipe_name, recipe in config_data[".material_conversion"].items():
        recipe_source_material = recipe.get("source_material")
        if not recipe_source_material:
            Warning("Material conversion missing 'source_material': {0}".format(recipe_name))
            continue

        recipe_source_domain = recipe_source_material.get("domain")
        if not recipe_source_domain:
            Warning("Material conversion missing source 'domain': {0}".format(recipe_name))
            continue
        if not recipe_source_domain == source_domain:
            continue

        recipe_target_material = recipe.get("target_material")
        if not recipe_target_material:
            Warning("Material conversion missing 'target_material': {0}".format(recipe_name))
            continue


        recipe_target_id = recipe_target_material.get("id")
        if not recipe_target_id:
            Warning("Material conversion missing target 'id': {0}".format(recipe_name))
            continue

        recipe_target_domain = recipe_target_material.get("domain")
        if not recipe_target_domain:
            Warning("Material conversion missing target 'domain': {0}".format(recipe_name))
            continue
        if not recipe_source_domain == source_domain or not recipe_target_domain == target_domain:
            continue
        recipe_source_id = recipe_source_material.get("id")
        if not recipe_source_id:
            Warning("Material conversion missing source 'id': {0}".format(recipe_name))
            continue
        if recipe_target_id == target_id and recipe_source_id == source_id:
            recipe_param_mapping = recipe.get("parameter_mapping")
            if not recipe_param_mapping:
                Warning("Material conversion missing 'parameter_mapping': {0}".format(recipe_name))
                continue

            parameter_mapping = dict(recipe_param_mapping)
            break

    if source_def and target_def and parameter_mapping:
        conversion_recipe["source_material"] = source_def
        conversion_recipe["target_material"] = target_def
        conversion_recipe["parameter_mapping"] = parameter_mapping
    else:
        # couldn't find both source and target material definitions
        return {}

    return conversion_recipe

def get_id_from_mat(mat):
    """Convenience function to return an id from any given material (USD or MAX)"""
    mat_id = None
    if isinstance(mat, str):
        mat_id  = mat
    elif isinstance(mat, pymxs.MXSWrapperBase):
        mat_id = str(RT.ClassOf(mat))
    elif isinstance(mat, UsdShade.Shader):
        shader_id_attr = mat.GetIdAttr()
        if shader_id_attr:
            mat_id = shader_id_attr.Get()

    if not mat_id:
        raise ValueError(mat)

    return mat_id

def mapping_property_getter(material, param):
    """Get a material property for either max or usd domains
    This makes mapping_from_material() domain agnostic"""
    if isinstance(material, pymxs.MXSWrapperBase):
        return RT.getProperty(material, param)
    elif isinstance(material, UsdShade.Shader):
        _input = material.GetInput(param)

        if not _input:
            return None

        if _input.HasConnectedSource():
            source, sourceName, sourceType = _input.GetConnectedSource()
            attr =  source.GetOutput(sourceName).GetAttr()
            return attr
        else:
            return _input.Get()
    else:
        raise ValueError((material, param))

def max_dict_to_dict(max_dict):
    """Util to convert max dict to python dict."""
    return {key:max_dict[key] for key in max_dict.keys}

def get_src_type(input_param, recipe):
    # type: (str, dict) -> str
    """Get the source input type as string for the parameter"""
    mapping = recipe["parameter_mapping"]["mappings"]
    map_src = mapping.get(input_param)
    if type(map_src) is dict:
        map_src = map_src["map_parameter"]
    return recipe["source_material"]["inputs"].get(map_src)

def mapping_from_material(material, conversion_recipe):
    """
    This function interperets the conversion_recipe dict, and returns
    a dictionary with the calculated values for each parameter of the target material.
    """
    mapping = {}
    parameter_mapping = conversion_recipe["parameter_mapping"]
    if not parameter_mapping["mappings"].items():
        raise Exception("There must be at least 1 mapping defined.")

    for in_param, out_param in parameter_mapping["mappings"].items():
        in_value_param_name = None
        one_minus_param = None
        map_param = None
        use_map_param = None # indicates that there is a param to enable/disable texture map
        map_required = None # indicates that a map is required to be meaningful
        multiplier_param = None
        direct_value = None

        if type(out_param) is dict:
            out_param = dict(out_param)
            case = out_param.get("case", False)
            while case:
                # iterate potentially recursive case params
                case_parameter = case.get("case_parameter")
                if not case_parameter:
                    Warning("A case data block has no case_parameter, skipping : {0}".format(case))
                    break
                case_params = None
                case_val = mapping_property_getter(material, case_parameter)
                case_params = case.get(str(case_val))
                if case_val and not case_params:
                    # case_val is something, so check the "+" case which is "at least one value" - or NOT None
                    case_params = case.get("+")

                if case_params and type(case_params) is dict:
                    out_param.update(case_params)
                    case = case_params.get("case", False)
                else:
                    case = False

            in_value_param_name = out_param.get("value_parameter")
            map_param = out_param.get("map_parameter")
            use_map_param = out_param.get("use_map_parameter")
            one_minus_param = out_param.get("one_minus", False)
            map_required = out_param.get("map_required", False)
            multiplier_param = out_param.get("multiplier_parameter")
            direct_value = out_param.get("value")

        elif type(out_param) is str:
            # if just a string we assume as straight up value mapping
            in_value_param_name = out_param

        if map_param:
            use_map = True
            if use_map_param:
                use_map = mapping_property_getter(material, use_map_param)
            map_value = mapping_property_getter(material, map_param)

            if (use_map_param and use_map and map_value) or (use_map_param == None and map_value):
                mapping[in_param] = map_value
                continue
            if map_required:
                mapping[in_param] = None
                continue

        if in_value_param_name:
            in_param_value = mapping_property_getter(material, in_value_param_name)
            if in_param_value is None:
                #print("None value for in_value_param_name="+in_value_param_name)
                mapping[in_param] = None
                continue
            if one_minus_param:
                in_param_value = 1 - in_param_value
            if multiplier_param:
                mult = mapping_property_getter(material, multiplier_param)
                in_param_value = mult * in_param_value
            if not map_required:
                mapping[in_param] = in_param_value
                continue
        if direct_value:
            mapping[in_param] = direct_value
            continue

    return mapping

def float_almost_equal(val1, val2):
    return abs(val1-val2) < 1e-06

def normalize_angle(angle):
    # Reduce the angle
    angle =  angle % 360.0
    # Make it positive : 0 <= angle < 360
    angle = (angle + 360.0) % 360.0
    # Normalize :  -180 < angle <= 180
    if (angle > 180.0):
        angle -= 360.0
    return angle

def is_filepath_udim(file_path):
    return "<UDIM>" in file_path

def find_first_valid_udim_for_filename(directory_name, target_filename):
    return get_all_valid_udims_from_dir_for_filename(directory_name, target_filename, first_only=True)

def get_all_valid_udims_from_dir_for_filename(directory_name, target_filename, first_only=False):
    u = 0
    v = 0
    udim_val = 1001 # first value based on formula: 1000 + (u + 1) + (v * 10)
    MAX_UDIM_VAL = 9999

    list_of_valid_udims = []
    
    dir_list = os.listdir(directory_name)
    while udim_val <= MAX_UDIM_VAL:
        udim_val_str = str(udim_val)
        udim_target_filename = target_filename.replace("<UDIM>", udim_val_str)
        for file in dir_list:
            filename = os.fsdecode(file)

            if udim_val_str in filename and filename == udim_target_filename:
                if first_only:
                    return udim_val_str
                
                list_of_valid_udims.append(udim_val_str)
                break

        if first_only == True and len(list_of_valid_udims) == 0:
            break

        u = u + 1
        if u + 1 > 10:
            u = 0
            v = v + 1

        udim_val = 1000 + (u + 1) + (v * 10)

    if len(list_of_valid_udims) == 0:
        Warning("No valid udim files found in  \"\{0}\".".format(directory_name))

    return list_of_valid_udims
