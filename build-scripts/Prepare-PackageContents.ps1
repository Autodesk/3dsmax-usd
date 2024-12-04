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
    [Parameter(Mandatory=$true, HelpMessage="Max Target Version")]
    [String]$TargetVersion,
    [Parameter(Mandatory=$false, HelpMessage="Artifacts file")]
    [String]$ArtifactsXmlFile="",
    [Parameter(Mandatory=$false, HelpMessage="Configuration")]
    [String]$Config="Release",
    [Parameter(Mandatory=$false, HelpMessage="Component folder")]
    [String]$SourceFolder="",
    [Parameter(Mandatory=$false, HelpMessage="Build Number")]
    [String]$BuildNumber="0",
    [Parameter(Mandatory=$false, HelpMessage="Prepare For Distribution")]
    [Boolean]$Distrib=$false,
    [Parameter(Mandatory=$true, HelpMessage="Component version")]
    [String]$ComponentVersion
)
if ($SourceFolder.Equals("")) {
    $SourceFolder = "$PSScriptRoot\..\build\bin\x64\$Config\usd-component-$TargetVersion"
}
if ($ArtifactsXmlFile.Equals("")) {
    $ArtifactsXmlFile = "$PSScriptRoot\..\artifacts_$TargetVersion.xml"
}

# Early bail out on error
$ErrorActionPreference = "Stop"
$UnstableSdk = @("2026")

if ($Distrib -And $UnstableSdk -contains $TargetVersion) {
    [xml]$ArtifactsXmlDocument = Get-Content -Path $ArtifactsXmlFile
    $MaxSdkDependency = $ArtifactsXmlDocument.SelectSingleNode("//dependencies/dependency[artifactId='maxsdk']")
    $MaxSdkVersion = $MaxSdkDependency.version
    $MaxSdkClassifier = $MaxSdkDependency.classifier
    $SplitIdentifier = $MaxSdkClassifier.Split("-")
    $MaxSdkIdentifier = $SplitIdentifier[0]
} Else {
    $MaxSdkIdentifier = ""
}

function GetSeriesMinMaxVersion {
    Param(
        [Parameter(Mandatory=$true, HelpMessage="Target Max Version (major) ex 2022")]
        [String]$MaxMajorVersion,
        [Parameter(Mandatory=$true, HelpMessage="Target MaxSDK version ex. 24.0.0")]
        [String]$MaxSdkVersion,
        [Parameter(Mandatory=$true, HelpMessage="MaxSDK classifier ex. H203-57.0")]
        [String]$MaxSdkClassifier
    )
    if ($Distrib -And $UnstableSdk -contains $TargetVersion) {
        $MaxBuildNumber = $MaxSdkClassifier.Split("-")[0]
        $MaxBuildNumber = $MaxBuildNumber.substring(1)
        $MaxSdkVersionInfo = $MaxSdkVersion.Split(".")
        $MaxSdkUpdateVersion = $MaxSdkVersionInfo[1]
        $MaxSdkHotFixVersion = $MaxSdkVersionInfo[2]
        # YYYY.update.hotfix.build
        return $MaxMajorVersion + "." + $MaxSdkUpdateVersion + "." + $MaxSdkHotFixVersion + "." + $MaxBuildNumber
    }

    return $MaxMajorVersion
}

# Load the "PackageContents.xml" file to insert the build number into the 
# "Patch" segment of the package's semantic version:
$PackageContentsFile = "$SourceFolder/PackageContents.xml"
[xml]$XmlDocument = Get-Content -Path $PackageContentsFile


# Update both the "AppVersion" and "FriendlyVersion" attributes of the
# "ApplicationPackage" root node:
$AppVersion = $XmlDocument.SelectSingleNode("//ApplicationPackage/@AppVersion")
$AppVersion.Value = "$ComponentVersion.$BuildNumber"

$FriendlyVersion = $XmlDocument.SelectSingleNode("//ApplicationPackage/@FriendlyVersion")
$FriendlyVersion.Value = "$ComponentVersion.$BuildNumber"

if ($Distrib -And $UnstableSdk -contains $TargetVersion) {
    $MinMaxVersion = GetSeriesMinMaxVersion -MaxMajorVersion $TargetVersion -MaxSdkVersion $MaxSdkVersion -MaxSdkClassifier $MaxSdkClassifier
} Else {
    $MinMaxVersion = $TargetVersion
}

# Update RuntimeRequirements to min=max=TargetVersion
function UpdateMinMaxRuntimeRequirementBasedOnTargetVersion {
    Param(
        [Parameter(Mandatory=$true, HelpMessage="XmlDocument with the content of the 'PackageContents.xml' file")]
        [xml]$XmlDocument,
        [Parameter(Mandatory=$true, HelpMessage="Max Target Version for min/max runtime requirement")]
        [String]$MinMaxVersion
    )
    $XmlDocument.SelectNodes("//RuntimeRequirements") | ForEach-Object {
        $_.Attributes.RemoveNamedItem("SeriesMin")
        $_.Attributes.RemoveNamedItem("SeriesMax")
        $minAttr = $XmlDocument.CreateAttribute("SeriesMin")
        $minAttr.Value = "$MinMaxVersion"
        $maxAttr = $XmlDocument.CreateAttribute("SeriesMax")
        $maxAttr.Value = "$MinMaxVersion"
        $_.Attributes.SetNamedItem($minAttr)
        $_.Attributes.SetNamedItem($maxAttr)
    }
}

function AddMenus {
    Param(
        [Parameter(Mandatory=$true, HelpMessage="XmlDocument with the content of the 'PackageContents.xml' file")]
        [xml]$XmlDocument,
        [Parameter(Mandatory=$true, HelpMessage="Max Target Version for min/max runtime requirement")]
        [String]$TargetVersion
    )
    $newMenuApixmlSnippet = @"

    <RuntimeRequirements OS="Win64" Platform="3ds Max" SeriesMin="2025" SeriesMax="2026" />
    <ComponentEntry ModuleName="./Contents/cui/usdMenu.mnx" />
  
"@
    $legacyMenuApiXmlSnippet = @"

        <RuntimeRequirements OS="Win64" Platform="3ds Max" SeriesMin="2022" SeriesMax="2024" />
        <ComponentEntry ModuleName="./Contents/scripts/registerMenu.ms" />
  
"@

    # Select the ApplicationPackage element
    $applicationPackage = $XmlDocument.SelectSingleNode("ApplicationPackage")
    # Create a new XML element from the snippet
    $newElement = [System.Xml.XmlElement] $XmlDocument.CreateElement("Components")

    if ($TargetVersion -ge 2025) {
        $newElement.SetAttribute("Description", "menu parts")
        $newElement.InnerXml = $newMenuApixmlSnippet
    } else {
        $newElement.SetAttribute("Description", "post-start-up scripts parts")
        $newElement.InnerXml = $legacyMenuApiXmlSnippet
    }
    # Append the new element to the ApplicationPackage element
    $applicationPackage.AppendChild($newElement)
}


AddMenus -XmlDocument $XmlDocument -TargetVersion $TargetVersion
UpdateMinMaxRuntimeRequirementBasedOnTargetVersion -XmlDocument $XmlDocument -MinMaxVersion $MinMaxVersion


# Update UpgradeCodes and AppName
function UpdateAppNameAndUpgradeCodes {
    Param(
        [Parameter(Mandatory=$true, HelpMessage="XmlDocument with the content of the 'PackageContents.xml' file")]
        [xml]$XmlDocument,
        [Parameter(Mandatory=$true, HelpMessage="Max Target Version for min/max runtime requirement")]
        [String]$MaxTargetVersion
    )
    $appPackageNode = $XmlDocument.SelectSingleNode("ApplicationPackage")
    $upgradeCodeAttr = $appPackageNode.Attributes.RemoveNamedItem("UpgradeCode")
    $upgradeCodeAttr.Value = $upgradeCodeAttr.Value.Replace("XXXX", "$MaxTargetVersion")
    $appPackageNode.Attributes.SetNamedItem($upgradeCodeAttr)

    $productCodeAttr = $appPackageNode.Attributes.RemoveNamedItem("ProductCode")
    $productCodeAttr.Value = $productCodeAttr.Value.Replace("XXXX", "$MaxTargetVersion")
    $appPackageNode.Attributes.SetNamedItem($productCodeAttr)

    $currentNameValue = $appPackageNode.Attributes.GetNamedItem("Name").Value
    $currentDescriptionValue = $appPackageNode.Attributes.GetNamedItem("Description").Value

    $appName = "$currentNameValue $TargetVersion"
    if ($Distrib -And $UnstableSdk -contains $TargetVersion) {
        $appName = "$currentNameValue $TargetVersion-$MaxSdkIdentifier"
    }

    $descriptionName = "$currentDescriptionValue $TargetVersion"
    if ($Distrib -And $UnstableSdk -contains $TargetVersion) {
        $descriptionName = "$currentDescriptionValue $TargetVersion-$MaxSdkIdentifier"
    }
    
    $nameNodeAttr = $appPackageNode.Attributes.RemoveNamedItem("Name")
    $nameNodeAttr.Value = $appName
    $appPackageNode.Attributes.SetNamedItem($nameNodeAttr)

    $descriptionNodeAttr = $appPackageNode.Attributes.RemoveNamedItem("Description")
    $descriptionNodeAttr.Value = $descriptionName
    $appPackageNode.Attributes.SetNamedItem($descriptionNodeAttr)
}

UpdateAppNameAndUpgradeCodes -XmlDocument $XmlDocument -MaxTargetVersion $TargetVersion

# Update the "ProductCode" attribute of the "ApplicationPackage" root node:
# For each build, the ProductCode would be newly generated
$NewProductCode = '{'+[guid]::NewGuid().ToString()+'}'
$ProductCode = $XmlDocument.SelectSingleNode("//ApplicationPackage/@ProductCode")
$ProductCode.Value = $NewProductCode.ToUpper()

$XmlDocument.Save($PackageContentsFile)
