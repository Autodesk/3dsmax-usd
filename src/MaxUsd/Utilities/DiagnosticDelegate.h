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

#include <MaxUsd.h>
#include <MaxUsd/MaxUSDAPI.h>

#include <pxr/base/tf/diagnosticMgr.h>
#include <pxr/pxr.h>

#include <stack>

namespace MAXUSD_NS_DEF {
namespace Diagnostics {

/// The buffered Tf diagnostics messages contained in the ScopedDiagnosticDelegate
struct MaxUSDAPI Message
{
    enum class MessageType
    {
        Error,
        Warning,
        Status
    };
    MessageType           type;
    pxr::TfDiagnosticBase message;

    Message(const MessageType& type, const pxr::TfDiagnosticBase& message)
        : type(type)
        , message(message)
    {
    }
};

/// Scoped diagnostic delegate which converts Tf diagnostics messages,
/// to application level messages (logging or otherwise, depending on the concrete
///	delegate used) It can be configured to keep a buffered list of Tf diagnostics message
/// or simply output the messages to mxsUsd Log System (by default)
/// If a scoped delegate is defined within the scoped of another delegate, both are kept active.
/// Static functions will operate on the inner-most scoped delegate, internally they are kept in a stack.
/// One can use the environment variable 'MAXUSD_SHOW_FULL_DIAGNOSTICS' to control
/// the granularity of TF error/warning/status messages being displayed in 3ds Max USD log files
class MaxUSDAPI ScopedDelegate
{
public:
    ~ScopedDelegate();
    ScopedDelegate() = delete;

    bool                        HasMessages() const;
    const std::vector<Message>& GetDiagnosticMessages() const;

    template <class T> static ScopedDelegate Create(bool buffered = false)
    {
        return ScopedDelegate(new T(buffered));
    }

private:
    template <class T> ScopedDelegate(T* diagDelegate) { runningDelegates.push(diagDelegate); }

    // the reference to the diagnostic delegate created for the original scope
    static std::stack<pxr::TfDiagnosticMgr::Delegate*> runningDelegates;
};

//! Abstract delegate, can be derived to forward info/warn/err messages appropriately.
class MaxUSDAPI DiagnosticDelegate : public pxr::TfDiagnosticMgr::Delegate
{
public:
    ~DiagnosticDelegate() override;

    void IssueError(const pxr::TfError& err) override;
    void IssueStatus(const pxr::TfStatus& status) override;
    void IssueWarning(const pxr::TfWarning& warning) override;
    void IssueFatalError(const pxr::TfCallContext& context, const std::string& msg) override;

    bool                        HasMessages() const;
    const std::vector<Message>& GetDiagnosticMessages() const;

protected:
    virtual void WriteError(const std::string& message) = 0;
    virtual void WriteWarning(const std::string& message) = 0;
    virtual void WriteInfo(const std::string& message) = 0;

private:
    // DiagnosticDelegate subclasses can only be created from a ScopedDelegate.
    DiagnosticDelegate(bool buffered);

    bool                 buffered;
    std::vector<Message> diagnosticMessages;
};

//! Delegate to forward diagnostics to the MaxUsd I/O logs.
class MaxUSDAPI LogDelegate : public DiagnosticDelegate
{
    using DiagnosticDelegate::DiagnosticDelegate;
    friend ScopedDelegate;

protected:
    void WriteError(const std::string& message) override;
    void WriteWarning(const std::string& message) override;
    void WriteInfo(const std::string& message) override;
};

//! Delegate to forward diagnostics to the 3dsMax listener.
class MaxUSDAPI ListenerDelegate : public DiagnosticDelegate
{
    using DiagnosticDelegate::DiagnosticDelegate;
    friend ScopedDelegate;

protected:
    void WriteError(const std::string& message) override;
    void WriteWarning(const std::string& message) override;
    void WriteInfo(const std::string& message) override;
};

//! Delegate to forward diagnostics to the 3dsMax render messsage window
class MaxUSDAPI RenderMessageDelegate : public DiagnosticDelegate
{
    using DiagnosticDelegate::DiagnosticDelegate;
    friend ScopedDelegate;

protected:
    void WriteError(const std::string& message) override;
    void WriteWarning(const std::string& message) override;
    void WriteInfo(const std::string& message) override;
};

/// A process-lifetime delegate registered with TfDiagnosticMgr for the duration of the plugin
/// session. Its sole purpose is to keep TfDiagnosticMgr's delegate list non-empty, which prevents
/// USD from falling back to its default fprintf(stderr) output for diagnostics issued from any
/// thread, including TBB worker threads during stage loading. All override methods are no-ops.
/// Instantiate once in MaxUsd::ufe::initialize() and destroy in MaxUsd::ufe::finalize().
class MaxUSDAPI PassiveDiagnosticDelegate : public pxr::TfDiagnosticMgr::Delegate
{
public:
    PassiveDiagnosticDelegate();
    ~PassiveDiagnosticDelegate() override;

    void IssueError(const pxr::TfError& err) override;
    void IssueStatus(const pxr::TfStatus& status) override;
    void IssueWarning(const pxr::TfWarning& warning) override;
    void IssueFatalError(const pxr::TfCallContext& context, const std::string& msg) override;
};

} // namespace Diagnostics
} // namespace MAXUSD_NS_DEF