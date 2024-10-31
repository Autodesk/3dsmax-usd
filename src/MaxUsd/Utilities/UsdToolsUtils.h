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

#include <MaxUsd/MaxUSDAPI.h>

#include <MaxUsd.h>
#include <xstring>

namespace MAXUSD_NS_DEF {
namespace UsdToolsUtils {

/**
 * \brief Open a given usd file in usdview.
 * \param filePath The path to the usd file to open.
 * \return true if opening Usdview was successful, false otherwise.
 */
MaxUSDAPI bool OpenInUsdView(const std::wstring& filePath);

/**
 * \brief Run UsdZip on a give input file to generate a zipped copy of it.
 * \param usdzFilePath Name for the USDZ file to be created.
 * \param usdInputFile Input USD file that will be zipped.
 * \return true if running UsdZip was successful, false otherwise.
 */
MaxUSDAPI bool RunUsdZip(const std::wstring& usdzFilePath, const std::wstring& usdInputFile);

/**
 * \brief Run usdchecker and output the result to the specified file
 * \param usdInputFile The path to the usd file to check.
 * \param outputFile The output file, where the usdchecker output will be written.
 * \return true if running usdchecker was successful, false otherwise.
 */
MaxUSDAPI bool RunUsdChecker(const std::wstring& usdInputFile, const std::wstring& outputFile);

/**
 * \brief Check if the given string is a valid windows path.
 * \param path wide string containing the path to be checked.
 * \return true if string is a valid path, false otherwise.
 */
MaxUSDAPI bool IsValidWindowsPath(const std::wstring& path);

/**
 * \brief Get the plugin directory of the current running instance
 * \param outDir wide string containing the directory path.
 * \return true if successfully got the path, false otherwise.
 */
MaxUSDAPI bool GetPluginDirectory(std::wstring& outDir);

/**
 * \brief Run the given command as a new process.
 * \param waitForProcess bool flag to determine to wait for process to close.
 * \param command string command to be executed.
 * \param arguments string arguments to be passed to the command.
 * \return true if able to run the process successfully, false otherwise.
 */
bool CreateProcessAndWait(
    const bool          waitForProcess,
    const std::wstring& command,
    const std::wstring& arguments);

} // namespace UsdToolsUtils
} // namespace MAXUSD_NS_DEF
