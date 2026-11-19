//**************************************************************************
// Copyright (c) 2026 Autodesk, Inc.
// All rights reserved.
//
// Use of this software is subject to the terms of the Autodesk license
// agreement provided at the time of installation or download, or which
// otherwise accompanies this software in either electronic or hard copy form.
//**************************************************************************/

#include "SettingsManagement.h"

#ifdef ADSK_RENDER_SETUP_ENABLED

#include "MaxRenderSetupDialog.h"

#include <pxr/base/tf/diagnostic.h>

#include <Qt/QmaxMainWindow.h>

#include <QPointer>
#include <maxapi.h>

PXR_NAMESPACE_USING_DIRECTIVE

namespace RenderSetupManagement {

namespace {

//! AdskUsdRenderSetup.dll is delay-loaded. Max adds our bin dir via Python's AddDllDirectory only,
//! so the delay-load helper's LoadLibrary("AdskUsdRenderSetup.dll") fails unless we preload by full
//! path.
bool _ensureRenderSetupLoaded()
{
    static const HMODULE module = []() -> HMODULE {
        HMODULE self = nullptr;
        if (!GetModuleHandleExW(
                GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS
                    | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                reinterpret_cast<LPCWSTR>(&_ensureRenderSetupLoaded),
                &self)) {
            return nullptr;
        }
        wchar_t     path[MAX_PATH];
        const DWORD len = GetModuleFileNameW(self, path, MAX_PATH);
        if (len == 0 || len == MAX_PATH) {
            return nullptr;
        }
        wchar_t* lastSlash = wcsrchr(path, L'\\');
        if (!lastSlash
            || wcscpy_s(lastSlash + 1, MAX_PATH - (lastSlash + 1 - path), L"AdskUsdRenderSetup.dll")
                != 0) {
            return nullptr;
        }
        return LoadLibraryW(path);
    }();
    return module != nullptr;
}

//! Recreated after close (WA_DeleteOnClose); held via QPointer.
MaxRenderSetupDialog* getDialog()
{
    static QPointer<MaxRenderSetupDialog> dialog;
    if (!dialog) {
        dialog = new MaxRenderSetupDialog(GetCOREInterface()->GetQmaxMainWindow());
    }
    return dialog.data();
}

} // namespace

void ShowDialog()
{
    if (GetCOREInterface()->GetQuietMode()) {
        return;
    }

    if (!_ensureRenderSetupLoaded()) {
        TF_WARN("USD Render Setup: failed to load AdskUsdRenderSetup.dll; the Render "
                "Setup dialog is unavailable.");
        return;
    }

    MaxRenderSetupDialog* dialog = getDialog();
    dialog->refreshStages();
    dialog->raise();
    dialog->activateWindow();
    dialog->show();
}

} // namespace RenderSetupManagement

#endif // ADSK_RENDER_SETUP_ENABLED
