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
@echo off

goto :ParseArgs

:Usage
    echo.
    echo Usage:
    echo   %~nx0 ^<vs_version^> [winsdk_version]
    echo. 
    echo   vs_version       Visual Studio version ^<2012^|2015^|2017^|2019^|latest^>
    echo                    latest: Selects the latest available VS 2017 or greater
    echo. 
    echo   winsdk_version   ^(optional^) The Windows SDK version to use
    echo                    If unspecified, let Visual Studio configure the default version.
    echo                    Ignored (not supported) for VS 2012.
    echo.
    echo Configures the developer command prompt for Visual Studio.
    echo If installed, uses the latest compatible version of Build Tools for Visual Studio ^(minimal installation^).
    echo Otherwise, uses the specified version of Visual Studio ^(full installation^).
    echo. 
    echo Only supports Build Tools for VS 2017 or greater.
    echo Build Tools for Visual Studio only supports toolset versions v140 or greater.
    echo. 
    echo Returns non-zero on error.
    echo. 
    echo Examples: 
    echo   %~nx0 latest
    echo   %~nx0 2022
    echo   %~nx0 2019 
    echo   %~nx0 2017 10.0.17134.0
    echo. 
    exit  /b 1

:ParseArgs

set VS_VERSION=%~1
for %%v in ("" "2017" "2019" "2022" "latest") do (
    if /I "%VS_VERSION%"=="%%~v" goto :Continue
)
echo ERROR: vs_version '%VS_VERSION%' is not supported
goto :Usage

:Continue

if "%VS_VERSION%" == "" (
    set VS_VERSION=latest
)

set WINSDK_VERSION=%~2

REM Additional setup for VS 2017 or greater
set "VSWHERE_LATEST_CMD="%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -prerelease -latest"

REM Select Visual Studio version
if "%VS_VERSION%" GEQ "2015" goto :BuildToolsLatest
goto :%VS_VERSION%

:BuildToolsLatest
REM Find latest installation of Build Tools for Visual Studio (2017 or greater) (tested with 2019)
for /f "tokens=*" %%i in ('%VSWHERE_LATEST_CMD% -products Microsoft.VisualStudio.Product.BuildTools -property productPath') do (
    set VSDEVCMD_PATH=%%~dpiVsDevCmd.bat
)
if not exist "%VSDEVCMD_PATH%" (
    echo Build Tools for Visual Studio is not installed. Using full Visual Studio installation.
    goto :%VS_VERSION%
)
for /f "tokens=*" %%i in ('%VSWHERE_LATEST_CMD% -products Microsoft.VisualStudio.Product.BuildTools -property installationVersion') do (
    set VS_LATEST_VERSION=%%i
)
if "%VS_LATEST_VERSION:~0,2%" GEQ "16" (
    if "%WINSDK_VERSION%" NEQ "" set "WINSDK_VERSION=-winsdk=%WINSDK_VERSION%"
)
call "%VSDEVCMD_PATH%" %WINSDK_VERSION%
goto :End

:latest
REM Find latest installation of Microsoft Visual Studio 2017 or greater
for /f "tokens=*" %%i in ('%VSWHERE_LATEST_CMD% -property installationPath') do set VS_PATH=%%i
if not exist "%VS_PATH%" (
    echo ERROR: Can't find latest installation of Visual Studio 2017 or greater
    exit /b 2
)
for /f "tokens=*" %%i in ('%VSWHERE_LATEST_CMD% -property installationVersion') do (
    set VS_LATEST_VERSION=%%i
)
if "%VS_LATEST_VERSION:~0,2%" GEQ "16" (
    if "%WINSDK_VERSION%" NEQ "" set "WINSDK_VERSION=-winsdk=%WINSDK_VERSION%"
)
call "%VS_PATH%\VC\Auxiliary\Build\vcvarsall.bat" amd64 %WINSDK_VERSION%
goto :End

:2022
set VS_VERSION_RANGELO=17.0
set VS_VERSION_RANGEHI=17.9
goto :vswhere_vcvarsall

:2019
set VS_VERSION_RANGELO=16.0
set VS_VERSION_RANGEHI=17.0
goto :vswhere_vcvarsall

:2017
set VS_VERSION_RANGELO=15.0
set VS_VERSION_RANGEHI=16.0
goto :vswhere_vcvarsall

:vswhere_vcvarsall
REM Use vswhere to find Microsoft Visual Studio installation
for /f "tokens=*" %%i in ('%VSWHERE_LATEST_CMD% -version ^[%VS_VERSION_RANGELO%^,%VS_VERSION_RANGEHI%^) -property installationPath') do set VS_PATH=%%i
if not exist "%VS_PATH%" (
    echo ERROR: Can't find Visual Studio 2017 installation
    exit /b 2
)
call "%VS_PATH%\VC\Auxiliary\Build\vcvarsall.bat" amd64 %WINSDK_VERSION%

:End

set vserror=%errorlevel%
if "%vserror%" NEQ "0" (
    echo ERROR: Can't configure developer command prompt for Visual Studio
    exit /b %vserror%
)
