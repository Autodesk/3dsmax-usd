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
#include "CreateAtPosition.h"

#include <snap.h>

int CreateAtPosition::proc(ViewExp* vpt, int msg, int point, int flags, IPoint2 m, Matrix3& mat)
{
    switch (msg) { // Handle geometry positioning
    case MOUSE_POINT: {
        switch (point) {
        case 0: { // first click
            Point3 tx = vpt->SnapPoint(m, m, nullptr, SNAP_IN_3D);
            mat.SetTrans(tx);
            return CREATE_STOP;
        }
        default: return CREATE_ABORT;
        }
    }
    case MOUSE_MOVE: {
        switch (point) {
        case 0: {

            Point3 tx = vpt->SnapPoint(m, m, nullptr, SNAP_IN_3D);
            mat.SetTrans(tx);
        }
        }
        break;
    }
    case MOUSE_ABORT: {
        return CREATE_ABORT;
    }
    default: break;
    }
    return CREATE_CONTINUE;
}