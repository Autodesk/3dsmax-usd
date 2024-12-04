::
:: Copyright 2023 Autodesk
::
:: Licensed under the Apache License, Version 2.0 (the "License");
:: you may not use this file except in compliance with the License.
:: You may obtain a copy of the License at
::
::     http://www.apache.org/licenses/LICENSE-2.0
::
:: Unless required by applicable law or agreed to in writing, software
:: distributed under the License is distributed on an "AS IS" BASIS,
:: WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
:: See the License for the specific language governing permissions and
:: limitations under the License.
::
@ECHO OFF
REM Utility script to run USD tools from an installed 3dsMax USD plugin
REM  This script tries to locate a usable python.exe and then runs the underlying USD tool
REM  with the UsdToolWrapper.py (python wrapper script will properly set the PATH environment variable)

REM Check passed arguments. If none, display help/usage.
SET ARG_COUNT=0
setlocal enabledelayedexpansion
FOR %%x in (%*) DO (
	SET "argVec[!ARG_COUNT!]=%%x"
	SET /A ARG_COUNT+=1
)
setlocal disabledelayedexpansion

IF %ARG_COUNT%==0 (
	REM If no arguments are specified, output help / usage.
	ECHO "USAGE:"
	ECHO "RunUsdTool.bat <usd_tool> [--python-exe C:path/to/3dsmax/python.exe] <args...>"
	ECHO "Ex : "
	ECHO "RunUsdTool.bat usdview --python-exe "C:/Program Files/Autodesk/3ds Max 2023/python/python.exe" path_to_usd.usda"
	ECHO "RunUsdTool.bat usdview path_to_usd.usda"
	ECHO "RunUsdTool.bat usdcat -o output_path.usd --usdFormat usda path_tp_usdz_file.usdz"
	EXIT /b
)

REM Clears the PATH to avoid any conflicts. Only keep what we know we need.
SET PATH="C:\windows\system32"

SETLOCAL

PUSHD %~dp0
SET SCRIPT_DIR="%CD%"
POPD

REM Detect the --python-exe switch, it allows to specify the python executable to be used. 
REM Typically it should be the python exe matching the 3dsMax version for which
REM the 3dsMax USD plugin was compiled.
IF "%~2"=="--python-exe" (
	goto PYTHON_ARG
)

REM Otherwise, try looking at registry keys to find the python executable.
REM In this case, the USD tool's args start at %2.
setlocal enabledelayedexpansion
FOR /L %%i in (1,1,%ARG_COUNT%) do (
	SET "ARGS=!ARGS!!argVec[%%i]!"
)
setlocal disabledelayedexpansion
if not x%SCRIPT_DIR:2022=%==x%SCRIPT_DIR% goto MAX2022_REGISTRY
if not x%SCRIPT_DIR:2023=%==x%SCRIPT_DIR% goto MAX2023_REGISTRY
if not x%SCRIPT_DIR:2024=%==x%SCRIPT_DIR% goto MAX2024_REGISTRY
if not x%SCRIPT_DIR:2025=%==x%SCRIPT_DIR% goto MAX2025_REGISTRY
if not x%SCRIPT_DIR:2026=%==x%SCRIPT_DIR% goto MAX2026_REGISTRY
REM Out of options...
goto no_python

:PYTHON_ARG
SET PYTHON_EXE=%~3
REM In this case, the USD tool's args start at %4.

set ALL_ARGS=%*
SET SKIPPED_ARGS=%1 %2 %3 
SETLOCAL enabledelayedexpansion
set ARGS=!ALL_ARGS:%SKIPPED_ARGS%=!
SETLOCAL DisableDelayedExpansion 
goto run_python_exe

:MAX2022_REGISTRY
FOR /F "tokens=2,* skip=2" %%a in ('reg query "HKLM\SOFTWARE\Autodesk\3dsMax\24.0" /V InstallDir') do (
	SET MAX_DIR=%%b
)
SET PYTHON_EXE=%MAX_DIR%python37\python.exe
goto run_python_exe

:MAX2023_REGISTRY
FOR /F "tokens=2,* skip=2" %%a in ('reg query "HKLM\SOFTWARE\Autodesk\3dsMax\25.0" /V InstallDir') do (
	SET MAX_DIR=%%b
)
SET PYTHON_EXE=%MAX_DIR%Python\python.exe
goto run_python_exe

:MAX2024_REGISTRY
FOR /F "tokens=2,* skip=2" %%a in ('reg query "HKLM\SOFTWARE\Autodesk\3dsMax\26.0" /V InstallDir') do (
	SET MAX_DIR=%%b
)
SET PYTHON_EXE=%MAX_DIR%Python\python.exe
goto run_python_exe

:MAX2025_REGISTRY
FOR /F "tokens=2,* skip=2" %%a in ('reg query "HKLM\SOFTWARE\Autodesk\3dsMax\27.0" /V InstallDir') do (
	SET MAX_DIR=%%b
)
SET PYTHON_EXE=%MAX_DIR%Python\python.exe
goto run_python_exe

:MAX2026_REGISTRY
FOR /F "tokens=2,* skip=2" %%a in ('reg query "HKLM\SOFTWARE\Autodesk\3dsMax\28.0" /V InstallDir') do (
	SET MAX_DIR=%%b
)
SET PYTHON_EXE=%MAX_DIR%Python\python.exe
goto run_python_exe

:no_python
	ECHO "Could not find max python based on registry or argument."
	goto end
)

:run_python_exe
PUSHD %SCRIPT_DIR%
ECHO running %PYTHON_EXE% with args:%ARGS%

REM "Reset" QTDIR env variable to avoid usdview QT crash occurring in dev environment
set QTDIR=
"%PYTHON_EXE%" UsdToolWrapper.py %1 %ARGS%

:end
ENDLOCAL
POPD
