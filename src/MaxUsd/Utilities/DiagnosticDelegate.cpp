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
#include "DiagnosticDelegate.h"

#include "ListenerUtils.h"
#include "Logging.h"
#include "TranslationUtils.h"

#include <pxr/base/arch/threads.h>
#include <pxr/base/tf/envSetting.h>
#include <pxr/base/tf/stackTrace.h>

PXR_NAMESPACE_USING_DIRECTIVE

TF_DEFINE_ENV_SETTING(
    MAXUSD_SHOW_FULL_DIAGNOSTICS,
    false,
    "This env flag controls the granularity of TF error/warning/status messages "
    "being displayed in 3ds Max USD log files.");

namespace MAXUSD_NS_DEF {
namespace Diagnostics {

static std::string _FormatDiagnostic(const TfDiagnosticBase& d)
{
    if (!TfGetEnvSetting(MAXUSD_SHOW_FULL_DIAGNOSTICS)) {
        return d.GetCommentary();
    } else {
        const std::string msg = TfStringPrintf(
            "%s -- %s in %s at line %zu of %s",
            d.GetCommentary().c_str(),
            TfDiagnosticMgr::GetCodeName(d.GetDiagnosticCode()).c_str(),
            d.GetContext().GetFunction(),
            d.GetContext().GetLine(),
            d.GetContext().GetFile());
        return msg;
    }
}

DiagnosticDelegate::DiagnosticDelegate(bool buffered)
    : buffered(buffered)
{
    TfDiagnosticMgr::GetInstance().AddDelegate(this);
}

DiagnosticDelegate::~DiagnosticDelegate() { TfDiagnosticMgr::GetInstance().RemoveDelegate(this); }

void DiagnosticDelegate::IssueError(const TfError& err)
{
    const auto diagnosticMessage = _FormatDiagnostic(err);

    if (ArchIsMainThread()) {
        if (buffered) {
            diagnosticMessages.emplace_back(Message::MessageType::Error, err);
        }
        WriteError(diagnosticMessage);
    }
}

void DiagnosticDelegate::IssueStatus(const TfStatus& status)
{
    const auto diagnosticMessage = _FormatDiagnostic(status);

    if (ArchIsMainThread()) {
        if (buffered) {
            diagnosticMessages.emplace_back(Message::MessageType::Status, status);
        }
        WriteInfo(diagnosticMessage);
    }
}

void DiagnosticDelegate::IssueWarning(const TfWarning& warning)
{
    const auto diagnosticMessage = _FormatDiagnostic(warning);

    if (ArchIsMainThread()) {
        if (buffered) {
            diagnosticMessages.emplace_back(Message::MessageType::Warning, warning);
        }
        WriteWarning(diagnosticMessage);
    }
}

void DiagnosticDelegate::IssueFatalError(const TfCallContext& context, const std::string& msg)
{
    TfLogCrash(
        "FATAL ERROR",
        msg,
        /*additionalInfo*/ std::string(),
        context,
        /*logToDb*/ true);
    _UnhandledAbort();
}

bool DiagnosticDelegate::HasMessages() const { return buffered && !diagnosticMessages.empty(); }

const std::vector<Message>& DiagnosticDelegate::GetDiagnosticMessages() const
{
    return diagnosticMessages;
}

TfDiagnosticMgr::Delegate* ScopedDelegate::runningDelegate { nullptr };

ScopedDelegate ::~ScopedDelegate()
{
    if (runningDelegate) {
        delete runningDelegate;
        runningDelegate = nullptr;
    }
}

bool ScopedDelegate::HasMessages() const
{
    return runningDelegate ? static_cast<DiagnosticDelegate*>(runningDelegate)->HasMessages()
                           : false;
}

const std::vector<Message>& ScopedDelegate::GetDiagnosticMessages() const
{
    if (runningDelegate) {
        return static_cast<DiagnosticDelegate*>(runningDelegate)->GetDiagnosticMessages();
    }
    static std::vector<Message> empty_vector {};
    return empty_vector;
}

void LogDelegate::WriteError(const std::string& message) { Log::Error(message); }

void LogDelegate::WriteWarning(const std::string& message) { Log::Warn(message); }

void LogDelegate::WriteInfo(const std::string& message) { Log::Info(message); }

void ListenerDelegate::WriteError(const std::string& message)
{
    Listener::Write(UsdStringToMaxString(message).data(), true);
}

void ListenerDelegate::WriteWarning(const std::string& message)
{
    Listener::Write(UsdStringToMaxString(message).data(), false);
}

void ListenerDelegate::WriteInfo(const std::string& message)
{
    Listener::Write(UsdStringToMaxString(message).data(), false);
}

} // namespace Diagnostics
} // namespace MAXUSD_NS_DEF