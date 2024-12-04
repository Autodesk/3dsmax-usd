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
#include "UsdToolsUtils.h"

#include "MaxSupportUtils.h"

#include <IPathConfigMgr.h>
#include <Shlwapi.h>
#include <maxapi.h>
#include <regex>
#include <shellapi.h>

namespace MAXUSD_NS_DEF {
namespace UsdToolsUtils {

static const std::wstring pythonExeFlag = L"--python-exe";

std::wstring _getQuotedPythonPath()
{
    IPathConfigMgr* pathMgr = IPathConfigMgr::GetPathConfigMgr();
#ifdef IS_MAX2025_OR_GREATER
    std::wstring pythonExe = pathMgr->GetDir(APP_MAX_SYS_ROOT_DIR).data();
#else
    std::wstring pythonExe = pathMgr->GetDir(APP_MAX_SYS_ROOT_DIR);
#endif

    // Relative path of the python exe has changed in 2023.
#ifdef IS_MAX2023_OR_GREATER
    pythonExe.append(L"/Python/python.exe");
#else
    pythonExe.append(L"/Python37/python.exe");
#endif

    return std::wstring(L"\"") + pythonExe + std::wstring(L"\"");
}

std::wstring _buildArgString(const std::vector<std::wstring>& args)
{
    return std::accumulate(
        std::begin(args),
        std::end(args),
        std::wstring {},
        [](std::wstring& total, const std::wstring& next) {
            return total.empty() ? next : total + L" " + next;
        });
}

bool OpenInUsdView(const std::wstring& usdFilePath)
{
    std::wstring directory;
    if (!GetPluginDirectory(directory) || !PathFileExistsW(usdFilePath.c_str())) {
        return false;
    }

    std::wstring runUsdviewBatPath { directory };
    runUsdviewBatPath.append(L"RunUsdView.bat");
    const std::wstring quotedBatPath
        = std::wstring(L"\"") + runUsdviewBatPath + std::wstring(L"\"");
    const std::wstring quotedFilePath = std::wstring(L"\"") + usdFilePath + std::wstring(L"\"");
    const auto args = _buildArgString({ pythonExeFlag, _getQuotedPythonPath(), quotedFilePath });

    return CreateProcessAndWait(false, quotedBatPath, args);
}

bool RunUsdZip(const std::wstring& usdzFilePath, const std::wstring& usdInputFile)
{
    std::wstring directory;
    if (!GetPluginDirectory(directory) || !IsValidWindowsPath(usdzFilePath)
        || !PathFileExistsW(usdInputFile.c_str())) {
        return false;
    }

    std::wstring runUsdZipBatPath { directory };
    runUsdZipBatPath.append(L"RunUsdZip.bat");
    const std::wstring quotedBatPath = std::wstring(L"\"") + runUsdZipBatPath + std::wstring(L"\"");

    const std::wstring quotedFilePaths = std::wstring(L"\"") + usdInputFile + std::wstring(L"\" \"")
        + usdzFilePath + std::wstring(L"\"");
    const auto args
        = _buildArgString({ pythonExeFlag, _getQuotedPythonPath(), L"-a", quotedFilePaths });

    return CreateProcessAndWait(true, quotedBatPath, args);
}

bool RunUsdChecker(const std::wstring& usdInputFile, const std::wstring& outputFile)
{
    std::wstring directory;
    if (!GetPluginDirectory(directory) || !IsValidWindowsPath(outputFile)
        || !PathFileExistsW(usdInputFile.c_str())) {
        return false;
    }

    std::wstring runUsdCheckerBatPath { directory };
    runUsdCheckerBatPath.append(L"RunUsdChecker.bat");
    const std::wstring quotedBatPath
        = std::wstring(L"\"") + runUsdCheckerBatPath + std::wstring(L"\"");
    const std::wstring quotedFilePaths = std::wstring(L"\"") + usdInputFile
        + std::wstring(L"\" > \"") + outputFile + std::wstring(L"\"");
    const auto args = _buildArgString({ pythonExeFlag, _getQuotedPythonPath(), quotedFilePaths });

    return CreateProcessAndWait(true, quotedBatPath, args);
}

bool IsValidWindowsPath(const std::wstring& path)
{
    // Check for illegal characters in path
    std::wregex illegalCharsRegex(L"[<>\"/|?*]");
    if (std::regex_search(path, illegalCharsRegex)) {
        return false;
    }

    // Check if the path is too long
    if (path.length() > MAX_PATH) {
        return false;
    }

    // File/Path is valid
    return true;
}

bool GetPluginDirectory(std::wstring& outDir)
{
    HMODULE moduleHandle = nullptr;

    if (GetModuleHandleEx(
            GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            (LPCWSTR)&GetPluginDirectory,
            &moduleHandle)
        == 0) {
        return false;
    }
    wchar_t modulePath[MAX_PATH];
    // Extract the directory...
    if (GetModuleFileName(moduleHandle, modulePath, sizeof(modulePath)) == 0) {
        return false;
    }

    outDir = modulePath;
    const size_t last_slash_idx = outDir.find_last_of(L"\\/");
    if (std::string::npos != last_slash_idx) {
        outDir.erase(last_slash_idx + 1, outDir.size() - 1);
    }

    return true;
}

bool CreateProcessAndWait(
    const bool          waitForProcess,
    const std::wstring& command,
    const std::wstring& arguments)
{
    // in ordert to call CreateProcess on a bat file, one must call cmd.exe with /c (this flag
    // hides the cmd.exe window) and then pass the path to the .bat file withing quotations to
    // prevent tampering. for more information, see:
    // https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-createprocessa#parameters
    const std::wstring cmdcommand = std::wstring(L"\"cmd.exe\" /c ") + std::wstring(L"\"") + command
        + std::wstring(L" ") + arguments + std::wstring(L"\"");

    // startup info structure
    STARTUPINFO si;
    ZeroMemory(&si, sizeof(si));

    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW | STARTF_USESTDHANDLES;
    si.wShowWindow = SW_HIDE;
    si.hStdError = NULL;

    // process info structure
    PROCESS_INFORMATION pi;
    ZeroMemory(&pi, sizeof(pi));

    const bool bResult = CreateProcess(
        NULL,                        // name
        (LPWSTR)cmdcommand.data(),   // cmdline
        NULL,                        // process attrib.
        NULL,                        // thread attrib.
        FALSE,                       // inherit handles
        CREATE_NEW_PROCESS_GROUP,    // flags
        NULL,                        // env. variables
        NULL,                        // directory
        (LPSTARTUPINFO)&si,          // startup info
        (LPPROCESS_INFORMATION)&pi); // potent potables

    // in the event that we are creating a process we want to wait for it to finish, we want
    // to block the process that is creating another process until it returns.
    if (waitForProcess) {
        // the function does not enter a wait state if the object is not signaled;
        // it always returns immediately. If dwMilliseconds is INFINITE,
        // the function will return only when the object is signaled.
        WaitForSingleObject(pi.hProcess, INFINITE);
    }
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    return bResult;
}

} // namespace UsdToolsUtils
} // namespace MAXUSD_NS_DEF
