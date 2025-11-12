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
	usdToolPythonPath = os.path.join(scriptPath, r"..\tools-site-packages")
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

def add3dsMaxInstallDirsToPath():
	"""Attempt to prepend the 3ds Max installation root and its bin folder to PATH.

	We infer the install root from the python executable path when the wrapper is
	launched via RunUsdTool.ps1 (which points to Max's bundled python). Typical layout:
	  <InstallDir>\\Python\\python.exe
	Add both <InstallDir> and <InstallDir>\\bin if they exist and are not already in PATH.
	Also call os.add_dll_directory for these locations (Python >=3.8) so current process
	DLL loads succeed even before spawning child processes.
	"""
	try:
		pyExe = os.path.abspath(sys.executable)
		# Go up one level (../) to get the Python folder, then parent for install root
		pyDir = os.path.dirname(pyExe)
		installRoot = os.path.dirname(pyDir)
		candidateDirs = []
		if os.path.isdir(installRoot):
			candidateDirs.append(installRoot)
			binDir = os.path.join(installRoot, 'bin')
			if os.path.isdir(binDir):
				candidateDirs.append(binDir)

		# Some distributions also place essential DLLs directly alongside python.exe
		candidateDirs.append(pyDir)
		# Prepend in reverse order so final PATH has installRoot first
		currentPath = os.environ.get('PATH', '')
		pathParts = currentPath.split(os.pathsep) if currentPath else []
		prepend = []
		for d in candidateDirs:
			if d and d not in pathParts:
				prepend.append(d)
		if prepend:
			os.environ['PATH'] = os.pathsep.join(prepend) + os.pathsep + currentPath
			# Add DLL directories for current process (won't propagate to child, PATH already handles that)
			if hasattr(os, 'add_dll_directory'):
				for d in prepend:
					try:
						os.add_dll_directory(d)
					except Exception:
						pass
	except Exception as e:
		pass

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
	if sys.version_info.major == 3 and sys.version_info.minor >= 11:
		try:
			import PySide6
		except ImportError:
			pysideVersion = "6.5.3"
			if sys.version_info.major == 3 and sys.version_info.minor == 13:
				pysideVersion = "6.8.3"
			print("WARN: PySide6 is not installed, USDView will not work. You can install pip and PySide6 with the scripts below:")
			print('"{}" -m ensurepip --upgrade --user'.format(sys.executable))
			print('"{}" -m pip install --user PySide6=={}'.format(sys.executable, pysideVersion))
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

	# also attempt to add 3ds Max install directories to PATH (root, bin, python dir)
	add3dsMaxInstallDirsToPath()

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

	# Try same path with .exe appended (covers case where caller passed 'UsdChecker')
	exePath = None
	if os.path.exists(cmd + '.exe'):
		exePath = cmd + '.exe'

		# Usd checker has changed to require a new arg to enable the new validation framework
		if cmd.lower() == "usdchecker":
			newArgs.append("--useNewValidationFramework")

	# If an .exe version of the tool exists (newer OpenUSD releases converted some python tools to native exes)
	if exePath:
		import subprocess
		# Run the native executable; pass only the arguments after the tool name.
		toolArgs = newArgs[1:]
		try:
			completed = subprocess.run([exePath] + toolArgs, check=True)
			sys.exit(completed.returncode)
		except Exception as e:
			sys.exit(e.returncode)

	# The command is not a native exe, so we assume it's a python script and try to run it via runpy.
	filename = cmd
	if not os.path.exists(filename):
		filename = "./bin/" + cmd

	import runpy
	try:
		runpy.run_path(filename, run_name='__main__')
		sys.exit(0)
	except ImportError:
		print("The 'PATH' environment variable contains a conflicting path with the 3ds Max USD component binaries.\nReversing the 'PATH' order and trying a second time to launch '{0}'.".format(filename))
		os.environ["PATH"] = ';'.join(reversed(os.getenv('PATH', '').split(os.pathsep)))
		runpy.run_path(filename, run_name='__main__')
		sys.exit(0)
