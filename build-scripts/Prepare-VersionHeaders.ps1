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

# NOTE: this is a temporary solution until there is a more standard
# solution across component plugins from max-pipeline or devops team

Param(
	[Parameter(Mandatory=$true, HelpMessage="Source component folder")]
	[String]$SourceFolder,
	[Parameter(Mandatory=$true, HelpMessage="Build Number")]
	[String]$BuildNumber="0",
	[Parameter(Mandatory=$true, HelpMessage="Component version")]
	[String]$ComponentVersion
)

$componentVersionHeaderFile = "$SourceFolder/USDComponentVersionNumber.h"
$componentBuildNumberHeaderFile = "$SourceFolder/USDComponentBuildNumber.h"

$componentVersions = $ComponentVersion.Split(".")
Write-Host "$componentVersions"
$componentVersionMajor = $componentVersions[0]
$componentVersionMinor = $componentVersions[1]
$componentVersionMicro = $componentVersions[2]

$replaceVersionMajorRegex = '#define COMPONENT_VERSION_MAJOR.*'
$replaceVersionMinorRegex = '#define COMPONENT_VERSION_MINOR.*'
$replaceVersionMicroRegex = '#define COMPONENT_VERSION_MICRO.*'
$replaceBuildNumberRegex = '#define VERSION_INT.*'

Write-Host "Updating $componentVersionHeaderFile with version $componentVersionMajor $componentVersionMinor $componentVersionMicro"
(Get-Content $componentVersionHeaderFile) -replace $replaceVersionMajorRegex, "#define COMPONENT_VERSION_MAJOR $componentVersionMajor" | Set-Content $componentVersionHeaderFile
(Get-Content $componentVersionHeaderFile) -replace $replaceVersionMinorRegex, "#define COMPONENT_VERSION_MINOR $componentVersionMinor" | Set-Content $componentVersionHeaderFile
(Get-Content $componentVersionHeaderFile) -replace $replaceVersionMicroRegex, "#define COMPONENT_VERSION_MICRO $componentVersionMicro" | Set-Content $componentVersionHeaderFile

Write-Host "Updating $componentBuildNumberHeaderFile with version $BuildNumber"
(Get-Content $componentBuildNumberHeaderFile) -replace $replaceBuildNumberRegex, "#define VERSION_INT $BuildNumber" | Set-Content $componentBuildNumberHeaderFile
