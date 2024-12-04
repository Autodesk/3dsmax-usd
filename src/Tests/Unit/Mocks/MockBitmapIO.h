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

#include <bitmap.h>

class MockBitmapIO_BTTTest : public BitmapIO
{

public:
    /**
     * The following methods are implemented in order to be able to use
     * MockBitmapIO_BTTTest in the tests.
     * NOTE: This is mostly copied from BitmapIO_PNG and was left almost
     * as is (voluntarily) in order to be close to reality.
     */
    int          ExtCount() { return 1; }
    const TCHAR* Ext(int /*n*/) { return _T("png"); }
    BOOL         LoadConfigure(void* ptr, DWORD piDataSize)
    {
        memcpy_s(&btt_cfg, sizeof(BTTTest_cfg), ptr, piDataSize);
        btt_cfg.MockSaved = TRUE;
        return TRUE;
    }
    BOOL SaveConfigure(void* ptr)
    {
        if (ptr) {
            btt_cfg.MockSaved = TRUE;
            memcpy_s(ptr, sizeof(BTTTest_cfg), &btt_cfg, sizeof(BTTTest_cfg));
            return TRUE;
        } else
            return FALSE;
    }
    DWORD EvaluateConfigure() { return sizeof(BTTTest_cfg); }

    /**
     * The following members inherited from the BitmapIO interface are not
     * implemented. Their return values should not be considered, and can
     * cause undefined side-effects.
     */
    const TCHAR* LongDesc() { return nullptr; }
    const TCHAR* ShortDesc() { return nullptr; }
    const TCHAR* AuthorName() { return nullptr; }
    const TCHAR* CopyrightMessage() { return nullptr; }
    unsigned int Version() { return 1; }
    int  Capability() { return BMMIO_READER | BMMIO_WRITER | BMMIO_EXTENSION | BMMIO_CONTROLWRITE; }
    void ShowAbout(HWND) { }
    BMMRES         GetImageInfo(BitmapInfo*) { return BMMRES_SUCCESS; }
    BitmapStorage* Load(BitmapInfo*, Bitmap*, BMMRES*) { return nullptr; }

private:
    struct BTTTest_cfg
    {
        int  MockBitdepth;
        int  MockInterlaced;
        BOOL MockSaved;
    };
    BTTTest_cfg btt_cfg;
};