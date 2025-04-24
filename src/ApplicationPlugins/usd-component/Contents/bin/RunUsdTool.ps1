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

# Utility script to run USD tools from an installed 3dsMax USD plugin.
# This script tries to locate a usable python.exe and then runs the underlying USD tool
# with the UsdToolWrapper.py (this wrapper will set the necessary PATH environment variables).

# Parse positional arguments manually, we dont want usd tool arguments starting with '-' to be 
# interpreted as powershell arguments...

if ($Args.Count -eq 0) {
    Write-Output "USAGE:"
    Write-Output "./RunUsdTool.ps1 <usd_tool> [--python-exe C:path/to/3dsmax/python.exe] <args...>"
    Write-Output "Example:"
    Write-Output "./RunUsdTool.ps1 usdview --python-exe 'C:/Program Files/Autodesk/3ds Max 2023/python/python.exe' path_to_usd.usda"
    Write-Output "./RunUsdTool.ps1 usdview path_to_usd.usda"
    Write-Output "./RunUsdTool.ps1 usdcat -o output_path.usd --usdFormat usda path_tp_usdz_file.usdz"
    exit
}

# First argument is the USD tool name (usdview, usdcat, usdzip, etc.)
$USDTool = $Args[0]

$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path

# Check if the python exe was passed in explicitely.
if ($Args[1] -eq "--python-exe") {
	$USDToolArgs = $Args[3..$Args.length]
	$PythonExe = $Args[2]
}
else {
	$USDToolArgs = $Args[1..$Args.length]
	
	# Find the 3dsMax version from a regex on the plugin path.
	
	$pattern = "(\d\d\d\d)"
	if ($ScriptDir -match $pattern) {
		$version = $matches[1]
	} else {
		Write-Output "3dsMax version not found."
		exit
	}
  	
	$IntVer = [int]$version
	switch ($IntVer) {
		2022 { $RegistryPath = "HKLM:\SOFTWARE\Autodesk\3dsMax\24.0" }
		2023 { $RegistryPath = "HKLM:\SOFTWARE\Autodesk\3dsMax\25.0" }
		2024 { $RegistryPath = "HKLM:\SOFTWARE\Autodesk\3dsMax\26.0" }
		2025 { $RegistryPath = "HKLM:\SOFTWARE\Autodesk\3dsMax\27.0" }
		2026 { $RegistryPath = "HKLM:\SOFTWARE\Autodesk\3dsMax\28.0" }
		default { 
			Write-Output "3dsMax version not supported : $IntVer."
			exit
		}
	}

	if (Test-Path $registryPath) {
		$InstallDir = (Get-ItemProperty -Path $registryPath).InstallDir
		if ($InstallDir) {
			if ($IntVer -eq 2022) {
                # 3dsMax 2022 uses a different directory structure for python.
				$PythonExe = Join-Path -Path $installDir -ChildPath "Python37\python.exe"
			}
			else {
				$PythonExe = Join-Path -Path $installDir -ChildPath "Python\python.exe"
			}
		}
	}

    if (-not $PythonExe) {
        Write-Host "Could not find the 3dsMax python executable from the registry."
        exit
    }
}

Push-Location -Path $ScriptDir

# "Reset" QTDIR env variable to avoid usdview QT crash occurring in dev environment
[System.Environment]::SetEnvironmentVariable("QTDIR", $null)

Write-Host "running $PythonExe with args: $UsdTool $USDToolArgs"

# Run the python wrapper...
$ArgList = ,"UsdToolWrapper.py",$USDTool + $USDToolArgs
& $PythonExe $ArgList

Pop-Location