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

#include <object.h>

/**
 * \brief Mock object for 3ds Max's Object interface.
 */
class MockObject : public Object
{
public:
    /**
     * The following members inherited from the Object interface can be used in
     * tests to modify the state and behavior of the Mock.
     */

    ObjectState Eval(TimeValue /*t*/) { return ObjectState(this); }
    SClass_ID   SuperClassID() { return GEOMOBJECT_CLASS_ID; }

public:
    /**
     * The following members inherited from the INode interface are not implemented.
     * Their return values should not be considered, and can cause undefined
     * side-effects.
     */

    int                  IsRenderable() { return 0; }
    void                 InitNodeName(MSTR& /*s*/) { }
    CreateMouseCallBack* GetCreateMouseCallBack() { return nullptr; }
    RefResult            NotifyRefChanged(
                   const Interval& /*changeInt*/,
                   RefTargetHandle /*hTarget*/,
                   PartID& /*partID*/,
                   RefMessage /*message*/,
                   BOOL /*propagate*/)
    {
        return REF_FAIL;
    }
};
