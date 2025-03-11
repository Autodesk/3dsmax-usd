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
#include <maxtypes.h>

/**
 * \brief Mock for 3ds Max's Modifier interface.
 */
class MockModifier : public Modifier
{
public:

	/**
	 * The following members inherited from the Modifier interface are not implemented.
	 * Their return values should not be considered, and can cause undefined
	 * side-effects.
	*/
	ChannelMask ChannelsUsed() override { return 0; }
	ChannelMask ChannelsChanged() override { return 0; }
	Class_ID InputType() override { return Class_ID(0, 0); }
	void ModifyObject(TimeValue /*t*/, ModContext& /*mc*/, ObjectState* /*os*/, INode* /*node*/) override { return; }
	CreateMouseCallBack* GetCreateMouseCallBack() override { return nullptr; }
	RefResult NotifyRefChanged(const Interval& /*changeInt*/, RefTargetHandle /*hTarget*/, PartID& /*partID*/, RefMessage /*message*/, BOOL /*propagate*/) override { return REF_SUCCEED; }
};
