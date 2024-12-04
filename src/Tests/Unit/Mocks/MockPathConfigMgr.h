//
// Copyright 2023 Autodesk
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
#pragma once

#include <IPathConfigMgr.h>

/**
 * \brief Mock object for 3ds Max's IPathConfigMgr interface.
 */
class MockPathConfigMgr : public IPathConfigMgr
{
public:
    /**
     * The following methods are used by the MockCoreInterface in order to
     * control the behavior of the Interface as it is passed to tests.
     */

    const MCHAR* GetDir(int /*which*/) const override { return _T("."); }

public:
    /**
     * The following members inherited from the INode interface are not implemented.
     * Their return values should not be considered, and can cause undefined
     * side-effects.
     */

    bool         LoadPathConfiguration(const MCHAR* /*filename*/) override { return false; }
    bool         MergePathConfiguration(const MCHAR* /*filename*/) override { return false; }
    bool         SavePathConfiguration(const MCHAR* /*filename*/) override { return false; }
    bool         SetDir(int /*which*/, const MCHAR* /*dir*/) override { return false; }
    int          GetPlugInEntryCount() const override { return 0; }
    const MCHAR* GetPlugInDesc(int /*i*/) const override { return nullptr; }
    const MCHAR* GetPlugInDir(int /*i*/) const override { return nullptr; }
    int          GetAssetDirCount(MaxSDK::AssetManagement::AssetType /*assetType*/) const override
    {
        return 0;
    }
    const MCHAR*
    GetAssetDir(int /*i*/, MaxSDK::AssetManagement::AssetType /*assetType*/) const override
    {
        return nullptr;
    }
    bool
    AddAssetDir(const MCHAR* /*dir*/, MaxSDK::AssetManagement::AssetType /*assetType*/) override
    {
        return false;
    }
    bool AddAssetDir(
        const MCHAR* /*dir*/,
        MaxSDK::AssetManagement::AssetType /*assetType*/,
        int /*update*/) override
    {
        return false;
    }
    bool DeleteAssetDir(int /*i*/, MaxSDK::AssetManagement::AssetType /*assetType*/, int /*update*/)
        override
    {
        return false;
    }
    bool AddSessionAssetDir(
        const MCHAR* /*dir*/,
        MaxSDK::AssetManagement::AssetType /*assetType*/,
        int /*update*/) override
    {
        return false;
    }
    int GetSessionAssetDirCount(MaxSDK::AssetManagement::AssetType /*assetType*/) const override
    {
        return 0;
    }
    const MCHAR*
    GetSessionAssetDir(int /*i*/, MaxSDK::AssetManagement::AssetType /*assetType*/) const override
    {
        return nullptr;
    }
    bool DeleteSessionAssetDir(
        int /*i*/,
        MaxSDK::AssetManagement::AssetType /*assetType*/,
        int /*update*/) override
    {
        return false;
    }
    int GetCurAssetDirCount(MaxSDK::AssetManagement::AssetType /*assetType*/) const override
    {
        return 0;
    }
    const MCHAR*
    GetCurAssetDir(int /*i*/, MaxSDK::AssetManagement::AssetType /*assetType*/) const override
    {
        return nullptr;
    }
    void UpdateAssetSection(MaxSDK::AssetManagement::AssetType /*assetType*/) override { }
    MSTR GetMAXIniFile() const override { return nullptr; }
    bool GetResolveUNC() const override { return false; }
    void SetResolveUNC(bool /*aFlag*/) override { }
    bool GetResolveToRelative() const override { return false; }
    void SetResolveToRelative(bool /*aFlag*/) override { }
    void AppendSlash(MCHAR* /*path*/) const override { }
    void RemoveSlash(MCHAR* /*path*/) const override { }
    void AppendSlash(MSTR& /*path*/) const override { }
    void RemoveSlash(MSTR& /*path*/) const override { }
    bool IsUsingProfileDirectories() const override { return false; }
    bool IsUsingRoamingProfiles() const override { return false; }
    const MaxSDK::Util::Path& GetCurrentProjectFolder() const override
    {
        static MaxSDK::Util::Path path;
        return path;
    }
    bool SetCurrentProjectFolder(const MaxSDK::Util::Path& /*aDirectory*/) override
    {
        return false;
    }
    bool SetSessionProjectFolder(const MaxSDK::Util::Path& /*aDirectory*/) override
    {
        return false;
    }
    bool DoProjectSetupSteps() const override { return false; }
    bool DoProjectSetupStepsUsingDirectory(const MaxSDK::Util::Path& /*aDirectory*/) const override
    {
        return false;
    }
    bool IsProjectFolder(const MaxSDK::Util::Path& /*aDirectoryToCheck*/) const override
    {
        return false;
    }
    MaxSDK::Util::Path
    GetProjectFolderPath(const MaxSDK::Util::Path& /*aProjectRoot*/) const override
    {
        return nullptr;
    }
    MaxSDK::Util::Path GetCurrentProjectFolderPath() const override { return nullptr; }
    void MakePathRelativeToProjectFolder(MaxSDK::Util::Path& /*aPath*/) const override { }
    bool CreateDirectoryHierarchy(const MaxSDK::Util::Path& /*aPath*/) const override
    {
        return false;
    }
    bool DoesFileExist(const MaxSDK::Util::Path& /*aPath*/) const override { return false; }
    void NormalizePathAccordingToSettings(MaxSDK::Util::Path& /*aPath*/) const override { }
    void RecordInputAsset(
        const MaxSDK::AssetManagement::AssetUser& /*originalPath*/,
        AssetEnumCallback& /*nameEnum*/,
        DWORD /*vflags*/) const override
    {
    }
    void RecordOutputAsset(
        const MaxSDK::AssetManagement::AssetUser& /*originalAsset*/,
        AssetEnumCallback& /*nameEnum*/,
        DWORD /*vflags*/) const override
    {
    }
    int          GetProjectSubDirectoryCount() const override { return 0; }
    const MCHAR* GetProjectSubDirectory(int /*aIndex*/) const override { return nullptr; }
    void         AddProjectDirectoryCreateFilter(int /*aID*/) override { }
    void         RemoveAllProjectDirectoryCreateFilters() override { }
    void         RemoveProjectDirectoryCreateFilter(int /*aID*/) override { }
};
