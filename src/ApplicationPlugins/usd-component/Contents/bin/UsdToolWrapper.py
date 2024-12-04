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
'''
This file is simply a wrapper to handle setting the windows PATH to locate USD binaries 
and python bindings. It also does some basic verifications such as handling missing PyOpenGL
dependency.
Written to work in an installed 3dsMax USD Plugin context or a standalone zip structure.
'''
from __future__ import print_function
import sys, os

scriptPath = os.path.dirname(os.path.realpath(__file__))

def getUsdPythonBindingsPath():
	usdPythonPath = os.path.join(scriptPath, "python")
	testPaths = [usdPythonPath, os.path.join(scriptPath, "lib", "python")]
	for tp in testPaths:
		if os.path.exists(tp):
			usdPythonPath = tp
			break
	return usdPythonPath
	
def getUsdBinPath():
	usdBinPath = scriptPath
	if not os.path.exists(os.path.join(usdBinPath, "3dsmax_usd.dll")):
		usdBinPath = os.path.join(scriptPath, "bin")
	return usdBinPath

def getUsdLibPath():
	usdLibPath = None
	if os.path.exists(os.path.join(scriptPath, "lib")):
		usdLibPath = os.path.join(scriptPath, "lib")
	elif os.path.exists(scriptPath + r"\..\lib"):
		usdLibPath = scriptPath + r"\..\lib"
	return usdLibPath

def getMtlxLibPath():
	mtlxLibPath = scriptPath + r"\..\libraries"
	return mtlxLibPath

def addUsdToolPythonBindingsToPythonPath():
	usdToolPythonPath = os.path.join(scriptPath, "python-usd-tool-packages")
	if os.path.exists(usdToolPythonPath):
		if not usdToolPythonPath in sys.path:
			sys.path.insert(0, usdToolPythonPath)

def addUsdPythonBindingsToPythonPath():
	usdPythonPath = getUsdPythonBindingsPath()
	# pre-pending to avoid override issues due to PYTHONPATH being set
	# only for standalone. if using installed max python3, PYTHONPATH is
	# not taken into account as the python.exe was overwritten with a 
	# custom version in 2021+
	if not usdPythonPath in sys.path:
		sys.path.insert(0, usdPythonPath)

def addUsdBinariesToWindowsPath():
	usdBinPath = getUsdBinPath()
	usdLibPath = getUsdLibPath()
	sysPath = os.environ["PATH"]
	sysPath = usdBinPath + ";" + sysPath
	if usdLibPath is not None:
		sysPath = usdLibPath + ";" + sysPath
	os.environ["PATH"] = sysPath

def addMtlxLibToPath():
	usdMtlXLibPath = getMtlxLibPath()
	from pxr import Usd
	ver = Usd.GetVersion()
	mtlxLibEnvVarName = None
	if ver == (0,21,11): # Only version we support with the older env var name
		mtlxLibEnvVarName = 'PXR_USDMTLX_STDLIB_SEARCH_PATHS'
	else:
		mtlxLibEnvVarName = 'PXR_MTLX_STDLIB_SEARCH_PATHS'
	# Get the current value of the environment variable, or set it to a default value (empty)
	env_var_value = os.getenv(mtlxLibEnvVarName, '')
	env_var_value = usdMtlXLibPath + os.pathsep + env_var_value
	os.environ[mtlxLibEnvVarName] = env_var_value

def validateUsdViewRequirements():
	status = True
	if sys.version_info.major == 3 and sys.version_info.minor == 11:
		try:
			import PySide6
		except ImportError:
			print("WARN: PySide6 is not installed, USDView will not work. You can install pip and PySide6 with the scripts below:")
			print('"{}" -m ensurepip --upgrade --user'.format(sys.executable))
			print('"{}" -m pip install --user PySide6==6.5.3'.format(sys.executable))
			status = False
	else:
		try:
			import PySide2
		except ImportError:
			print("WARN: PySide2 is not installed, USDView will not work. You can install pip and PySide2 with the scripts below:")
			print('"{}" -m ensurepip --upgrade --user'.format(sys.executable))
			print('"{}" -m pip install --user PySide2==5.15.1'.format(sys.executable))
			status = False

	try:
		import OpenGL
	except ImportError:
		print("WARN: PyOpenGL is not installed, USDView will not work. You can install pip and PyOpenGL with the scripts below:")
		print('"{}" -m ensurepip --upgrade --user'.format(sys.executable))
		print('"{}" -m pip install --user PyOpenGL==3.1.5'.format(sys.executable))
		status = False
	
	return status


if __name__ == "__main__":
	if len(sys.argv) < 2:
		# NOTE: can build a UI here with pyside2 instead of cli usage
		print("ERROR: Incorrect arguments set, first argument should be name of usd tool such as `usdcat`")
		exit()

	# make sure usd python bindings path is in `sys.path`
	addUsdPythonBindingsToPythonPath()
	addUsdToolPythonBindingsToPythonPath()

	# make sure usd binaries are searchable in windows PATH
	addUsdBinariesToWindowsPath()

	# make sure MtlX Libraries are searchable for USD
	addMtlxLibToPath()

	# check if PyOpenGL is installed (only for py3+ and usdview)
	if not sys.version_info.major == 2:
		if not validateUsdViewRequirements() and sys.argv[1] == "usdview":
			print("ERROR: missing required dependencies for running usdview")
			exit()

	if sys.version_info.major == 3 and sys.version_info.minor == 9 and sys.version_info.micro >= 7:
		os.add_dll_directory(os.path.dirname(os.path.dirname(sys.executable)))

	cmd = sys.argv[1]
	newArgs = sys.argv[1: len(sys.argv)]
	sys.argv = newArgs

	filename = cmd
	if not os.path.exists(filename):
		filename = "./bin/" + cmd

	import runpy
	try:
		runpy.run_path(filename, run_name='__main__')
	except ImportError:
		print("The 'PATH' environment variable contains a conflicting path with the 3ds Max USD component binaries.\nReversing the 'PATH' order and trying a second time to launch '{0}'.".format(filename))
		os.environ["PATH"] = ';'.join(reversed(os.getenv('PATH', '').split(os.pathsep)))
		runpy.run_path(filename, run_name='__main__')
