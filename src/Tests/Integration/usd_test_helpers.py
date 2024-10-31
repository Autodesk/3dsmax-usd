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
from pxr import UsdShade, Usd, Sdf
import os
from unittest import TestCase
from pymxs import runtime as mxs
from pymxs import byref
from glob import glob

max_ver_info = mxs.maxversion()
max_ver_year = max_ver_info[0] # RT.maxversion() returns an array where index 0 holds part of the build number
BITMAP = mxs.Name("Bitmap")

def get_file_path_mxs(filename):
    _, file_path = mxs.FileResolutionManager.getFullFilePath(byref(filename), BITMAP)
    return file_path


def load_usd_plugins():
    """Make sure usd plugins are loaded"""
    mxs.pluginManager.loadClass(mxs.USDImporter)
    mxs.pluginManager.loadClass(mxs.USDExporter)


def standard_output_prefix(prefix, clean_up=True):
    # type: (str, bool) -> str
    """Setup for standard testcase: set output_prefix and cleanup previous run."""
    tempDir = mxs.getDir(mxs.Name("temp"))
    output_prefix = os.path.join(tempDir, prefix)
    if clean_up:
        # cleanup previous run
        file_list = glob(output_prefix + "*.*")
        for filename in file_list:
            try:
                os.remove(filename)
            except:
                Warning(f"Could not remove: {filename}")

    return output_prefix
    

def test_texture_sampler_ouput(testcase, stage, texture_sampler_path, filename):
    # type: (TestCase, Usd.Stage, Sdf.Path, str) -> None
    """Test expected texture sampler output"""
    prim = stage.GetPrimAtPath(texture_sampler_path)
    testcase.assertIsNotNone(prim, msg="{0}".format(prim))
    testcase.assertTrue(prim.IsValid(), msg="Prim path is invalid: {0}".format(texture_sampler_path))
    
    shader = UsdShade.Shader.Get(stage, texture_sampler_path)
    shader_id_attr = shader.GetIdAttr()
    shader_id = shader_id_attr.Get()
    testcase.assertEqual(shader_id, "UsdUVTexture")

    file_attr = shader.GetInput('file')
    testcase.assertIsNotNone(file_attr)
    
    file_path = file_attr.Get().path
    testcase.assertTrue(os.path.samefile(file_path, filename))

def check_expected_usd_mat_value(testcase, stage, usd_mat_path, input_name, expected_val):
    # type: (TestCase, Usd.Stage, Sdf.Path, str, object) -> None
    """Test expected texture sampler output"""
    msg = f"Expected {usd_mat_path}:{input_name} to be {expected_val} in {stage.GetRootLayer().realPath}"
    usd_mat_shader = UsdShade.Shader.Get(stage, usd_mat_path)
    testcase.assertIsInstance(usd_mat_shader, UsdShade.Shader)
    input = usd_mat_shader.GetInput(input_name)
    testcase.assertIsNotNone(input)
    value = input.Get()

    if isinstance(expected_val, float):
        testcase.assertAlmostEqual(value, float(expected_val), msg=msg)
    elif expected_val is None:
        testcase.assertIsNone(value, msg=msg)        
    else:
        testcase.assertEqual(value, expected_val, msg=msg)
