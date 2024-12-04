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

#include <strclass.h>
// MAXX-63363: VS2019 v142: <experimental/filesystem> is deprecated and superseded by the C++17
// <filesystem> header
#if _MSVC_LANG > 201402L
#include <filesystem>
namespace fs = std::filesystem;
#else
#include <experimental/filesystem>
namespace fs = std::experimental::filesystem;
#endif

#include <MaxUsd/MaxUSDAPI.h>

class MaxUSDAPI USDCore
{
public:
    static void initialize();
    static fs::path
    sanitizedFilename(const MCHAR* filePath, const MCHAR* defaultExtension = nullptr);
    static fs::path sanitizedFilename(
        const std::string& filePath,
        const std::string& defaultExtension = std::string());

#if MAX_VERSION_MAJOR < 26
    static std::string relativePath(const fs::path& path, const fs::path& relative_to);
#endif
};
