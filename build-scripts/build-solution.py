#
# Copyright 2024 Autodesk
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
import os
import subprocess
import argparse

# map to handle the dependencies location for the project
#  the key is the script parameter name
#  the value is a pair
#    1) argument to pass to msbuild (the referenced property in vcxproj/props)
#    2) the help text to display when help is invoked for the current script
artifact_map = {'maxsdk':('MaxSDK', 'The path location for the \'MaxSDK\' folder.'),
                'qtinstall':('QtInstall', 'The Qt reference version from QtVsTools (aka \'Qt Installation\').'),
                'materialx':('MaterialXDir', 'The path location for the 3ds Max MaterialX material plugin folder.'),
                'googletest':('GoogleTestDir', 'The path location for the \'gtest\' folder.'),
                'pyopengl':('PyOpenGLDir', 'The path location for the \'OpenGL\' Python module (PyOpenGL).'),
                'maxusddevkit':('MaxUsdDevKit', 'The path location for the 3ds Max USD \'devkit\'.'),
                'spdlog':('SpdlogInc', 'The path location for \'spdlog\' include folder. If not provided, using the path from the \'devkit\' if the \'maxusddevkit\' option is provided.'),
                'python':('PythonLocation', 'The path location for the \'Python\' folder. If not provided, using the path from the \'devkit\' if the \'maxusddevkit\' option is provided.'),
                'pyside':('PySideDir', 'The path location for the \'PySide6\' Python module. If not provided, using the path from the \'devkit\' if the \'maxusddevkit\' option is provided.'),
                'shiboken':('PySideShibokenDir', 'The path location for the \'shiboken6\' Python module. If not provided, using the path from the \'devkit\' if the \'maxusddevkit\' option is provided.'),
                'ufeinc':('UfeInc', 'The path location for the \'Ufe\' include folder. If not provided, using the path from the \'devkit\' if the \'maxusddevkit\' option is provided.'),
                'ufelib':('UfeLib', ' The path location for the \'Ufe\' lib folder. If not provided, using the path from the \'devkit\' if the \'maxusddevkit\' option is provided.'),
                'usdufe':('UsdUfeDir', 'The path location for the \'UsdUfe\' folder. If not provided, using the path from the \'devkit\' if the \'maxusddevkit\' option is provided.'),
                'openusd':('PxrUsdRoot', 'The path location for the \'OpenUSD\' folder. If not provided, using the path from the \'devkit\' if the \'maxusddevkit\' option is provided.'),
                'tbb':('TBBDir', 'The path location for the \'TBB\' folder. If not provided, using the path from the \'OpenUSD\' if the \'openusd\' or \'maxusddevkit\' option is provided.'),
                'boostinc':('BoostInc', 'The path location for the \'Boost\' include folder. If not provided, using the path from the \'OpenUSD\' if the \'openusd\' or \'maxusddevkit\' option is provided.'),
                'boostlib':('BoostLib', ' The path location for the \'Boost\' lib folder. If not provided, using the path from the \'OpenUSD\' if the \'openusd\' or \'maxusddevkit\' option is provided.')}

def parse_arguments() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("configuration", 
                        nargs='?',
                        type=str.lower,
                        choices=['release', 'hybrid'],
                        default='release',
                        help="The build configuration type.")
    parser.add_argument("target", choices=[2022, 2023, 2024, 2025, 2026], help="The 3ds Max version to target.", type=int)
    parser.add_argument("-b", "--build", help="The build number coming from the pipeline.", default=0, type=int)
    parser.add_argument("-v", "--version", help="The 3ds Max USD component version being built.", default='0.0.0')
    parser.add_argument("-w", "--warnaserror", help="Enable the compiler to treat all warnings as errors.", action='store_true')
    parser.add_argument("-r", "--rebuild", help="Rebuild the project.", action='store_true')
    parser.add_argument("-d", "--distrib", help="Prepare for redistribution. Write component version in source headers.", action='store_true')
    parser.add_argument("-p", "--package", help="Prepare the package folder after build.", action='store_true')
    # parse arguments to set the artifact specific paths if any
    for arg, data in artifact_map.items() :
        parser.add_argument(f"--{arg}", help=data[1])
    return parser.parse_args()

def get_script_folder() -> str :
     return os.path.dirname(os.path.abspath(__file__))

def build_command(args:argparse.Namespace) -> list:
    # path to build-scripts
    swd = get_script_folder()
    # set up the build environment
    cmd = [swd + "\\configure-vsdevcmd.bat"]
    if args.target == 2022:
        # 3ds Max 2022 is expected to be compiled with VS2017 and winsdk 10.0.17134.0
        # Visual Studio 2019 can be used as long as VS2017 tooling is available
        #cmd.append("2017")
        #cmd.append("10.0.17134.0")
        cmd.append("2019")
    elif args.target == 2026:
        cmd.append("2022")
    else:
        cmd.append("2019")

    # append the build command
    cmd.append("&&")

    cmd.append('msbuild.exe')
    cmd.append(swd + "\\..\\src\\usd-component.sln")

    if args.rebuild:
        cmd.append('/t:rebuild')
    if args.warnaserror:
        cmd.append('/warnaserror')
    cmd.append(f'/p:Configuration={args.configuration}')
    cmd.append(f'/p:Platform=x64')
    if args.distrib:
        cmd.append(f'/p:BuildType=jenkins')
    cmd.append(f'/p:VersionTarget={args.target}')
    cmd.append(f'/p:BuildNumber={args.build}')
    cmd.append(f'/p:ComponentVersion={args.version}')

    # parse the artifacts variable option list 
    args_dict = vars(args)
    for arg, data in artifact_map.items() :
        if args_dict[arg] is not None:
            cmd.append(f'/p:{data[0]}={args_dict[arg]}')

    return cmd

def build_component(args:argparse.Namespace):
    cmd = build_command(args)
    proc = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, universal_newlines=True)
    print(f"Launching build process\n{proc.args}")
    for raw_line in proc.stdout:
        print(raw_line, end='')
    proc.wait()
    return proc.returncode == 0

def package(config:str, target:int, version:str, build:int):
    # path to build-scripts
    swd = get_script_folder()
    # set up the build environment
    cmd = ['powershell',
           '-ExecutionPolicy',
           'ByPass',
           '-File',
           swd + "\\Prepare-BinPackage.ps1",
           '-SourceFolder',
           f'{swd}\\..\\build\\bin\\x64\\{config}\\usd-component-{target}',
           '-DestinationFolder',
           f'{swd}\\..\\package\\3dsmax-usd-{target}',
           '-ComponentVersion',
           version,
           '-TargetVersion',
           str(target),
           '-BuildNumber',
           str(build)]
    
    proc = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, universal_newlines=True)
    print(f"Launching packaging process\n{proc.args}")
    for raw_line in proc.stdout:
        print(raw_line, end='')
    proc.wait()
    return proc.returncode == 0
    
def main():
    args = parse_arguments()
    if not build_component(args):
        exit(1)
    if args.package:
        if not package(args.configuration, args.target, args.version, args.build):
            exit(1)

if __name__ == "__main__":
    main()