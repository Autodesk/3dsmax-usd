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

#include <log.h>

class MockLogSys : public LogSys
{
public:
    virtual void         SetQuietMode(bool quiet) { }
    virtual bool         GetQuietMode() { return true; }
    virtual void         SetEnabledMode(bool enabled) { }
    virtual bool         GetEnabledMode() { return false; }
    virtual void         SetSessionLogName(const MCHAR* logName) { }
    virtual const MCHAR* GetSessionLogName() { return L""; }
    virtual const MCHAR* NetLogName() { return L""; }

    virtual void LogEntry(DWORD type, BOOL dialogue, const MCHAR* title, const MCHAR* format, ...)
    {
    }
    virtual void SaveState(void) { }
    virtual void LoadState(void) { }

#if MAX_RELEASE >= 26900
    void
    LogEntry(DWORD type, BOOL dialogue, const MSTR& title, const wchar_t* format, ...) override { };
    void LogEntry(DWORD type, BOOL dialogue, const MSTR& title, MSTR format, ...) override { };
#endif
};