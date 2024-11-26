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

Param(
    [Parameter(Mandatory=$true, HelpMessage="Source component folder")]
    [String]$SourceFolder,
    [Parameter(Mandatory=$true, HelpMessage="Destination component folder")]
    [String]$DestinationFolder,
    [Parameter(Mandatory=$true, HelpMessage="Max Target Version")]
    [String]$TargetVersion,
    [Parameter(Mandatory=$true, HelpMessage="Component version")]
    [String]$ComponentVersion,
    [Parameter(Mandatory=$true, HelpMessage="Build number")]
    [String]$BuildNumber
)

# Remove test binaries which should not ship with the plugin.
function RemoveTestBinaries {
    $UnitTestExe = "$DestinationFolder/Contents/Bin/USD.Unit.test.exe"
    if (Test-Path -Path $UnitTestExe) {
        Remove-Item $UnitTestExe
    }
}

Write-Host "Destination Folder: $DestinationFolder"

# Early bail out on error
$ErrorActionPreference = "Stop"

robocopy $SourceFolder $DestinationFolder /SL /MIR /NFL /NDL /XF *.ilk *.iobj *.ipdb *.pdb *.test.* gmock*.dll gtest*.dll

# Load the "PackageContents.xml" file to insert the build number into the
# "Patch" segment of the package's semantic version:
$PackageContentsFile = "$DestinationFolder/PackageContents.xml"
[xml]$XmlDocument = Get-Content -Path $PackageContentsFile

RemoveTestBinaries

$XmlDocument.Save($PackageContentsFile)
