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

#include <INode.h>
#include <object.h>
#include <control.h>
#include <memory>
#include "MockObject.h"


/**
 * \brief Mock object for 3ds Max's INode interface.
 * \remarks Mock behaviors should be supported by Google Mock (in conjunction
 * with Google Test), but as of this writing the version of the library
 * delivered through Artifactory contains an defect which prevents compilation
 * of Mock objects. Given that no other 3ds Max project use the Google Mocks as
 * part of their test setup, the Baking to Texture tool is currently the only
 * project affected by this. Given the limited resource available, updating the
 * Google Mock library (in parallel with the Google Test library) constitutes
 * an overhead that the Baking to Texture team cannot afford at this time.
 * \remarks To reproduce this defect:
 *  * Create a traditional Google Mock class in the test project.
 *  * Link against "gmock_main" or "gmock_maind".
 *  * Add the "TEST_LINKED_AS_SHARED_LIBRARY" compilation flag to the project.
 *  * Build the test project.
 *  * Check compilation warnings.
 */
class MockINode : public INode
{
protected: 
	Mtl* material{ nullptr };
	std::unique_ptr<Object> object{ new MockObject() };
	std::unique_ptr<ObjectState> objectState{ new ObjectState(object.get()) };

public:
	/**
	 * The following members inherited from the INode interface can be used in
	 * tests to modify the state and behavior of the Mock.
	 */

	const MCHAR* GetName() override { return name; }
	void SetName(const MCHAR* s) override { name = s; }
	Mtl* GetMtl() override { return material; }

	void SetMtl(Mtl* newMaterial) override {
		material = newMaterial; 
	}

	Object* GetObjectRef() override { return object.get(); }
	void SetObjectRef(Object* o) override {
		object.reset(o);
		objectState.reset(new ObjectState(o));
	}

#pragma warning(push)
#pragma warning(disable: 4172)
	const ObjectState& EvalWorldState(TimeValue /*time*/, BOOL /*evalHidden*/ = TRUE) { return *objectState.get(); }
#pragma warning(pop)

public:
	/**
	 * The following members inherited from the INode interface are not implemented.
	 * Their return values should not be considered, and can cause undefined
	 * side-effects.
	 */
	Matrix3 GetNodeTM(TimeValue /*t*/, Interval* /*valid*/ = NULL) { return Matrix3(); }
	void SetNodeTM(TimeValue /*t*/, Matrix3& /*m*/) { }
	void InvalidateTreeTM() { }
	void InvalidateTM() { }
	void InvalidateWS() { }
	void InvalidateRect(TimeValue /*t*/, bool /*oldRect*/) { }
	Matrix3 GetObjectTM(TimeValue /*time*/, Interval* /*valid*/ = NULL) { return Matrix3(); }
	Matrix3 GetObjTMBeforeWSM(TimeValue /*time*/, Interval* /*valid*/ = NULL) { return Matrix3(); }
	Matrix3 GetObjTMAfterWSM(TimeValue /*time*/, Interval* /*valid*/ = NULL) { return Matrix3(); }
	INode* GetParentNode() { return nullptr; }
	void AttachChild(INode* /*n*/, int /*keepTM*/ = 1) {  }
	void Detach(TimeValue /*t*/, int /*keepTM*/ = 1) {  }
	int NumberOfChildren() { return 0; }
	INode* GetChildNode(int /*i*/) { return nullptr; }
	void Delete(TimeValue /*t*/, int /*keepChildPosition*/) { }
	void Hide(BOOL /*onOff*/) { }
	int IsHidden(DWORD /*hflags*/ = 0, BOOL /*forRenderer*/ = FALSE) { return 0; }
	int IsNodeHidden(BOOL /*forRenderer*/ = FALSE) { return 0; }
	void Freeze(BOOL /*onOff*/) { }
	int IsFrozen() { return 0; }
	void SetShowFrozenWithMtl(BOOL /*onOff*/) { }
	int ShowFrozenWithMtl() { return 0; }
	void XRayMtl(BOOL /*onOff*/) { }
	int HasXRayMtl() { return 0; }
	void IgnoreExtents(BOOL /*onOff*/) { }
	int GetIgnoreExtents() { return 0; }
	void BoxMode(BOOL /*onOff*/) { }
	int GetBoxMode() { return 0; }
	void AllEdges(BOOL /*onOff*/) { }
	int GetAllEdges() { return 0; }
	void VertTicks(int /*onOff*/) { }
	int GetVertTicks() { return 0; }
	void BackCull(BOOL /*onOff*/) { }
	int GetBackCull() { return 0; }
	void SetCastShadows(BOOL /*onOff*/) { }
	int CastShadows() { return 0; }
	void SetRcvShadows(BOOL /*onOff*/) { }
	int RcvShadows() { return 0; }
	void SetGenerateCaustics(BOOL /*onOff*/) { }
	int GenerateCaustics() { return 0; }
	void SetRcvCaustics(BOOL /*onOff*/) {  }
	int RcvCaustics() { return 0; }
	void SetApplyAtmospherics(BOOL /*onOff*/) { }
	int ApplyAtmospherics() { return 0; }
	void SetGenerateGlobalIllum(BOOL /*onOff*/) { }
	int GenerateGlobalIllum() { return 0; }
	void SetRcvGlobalIllum(BOOL /*onOff*/) { }
	int RcvGlobalIllum() { return 0; }

	void SetMotBlur(BOOL /*onOff*/) { }
	int MotBlur() { return 0; }

	float GetImageBlurMultiplier(TimeValue /*t*/) { return 0.0f; }
	void SetImageBlurMultiplier(TimeValue /*t*/, float /*m*/) { };
	void SetImageBlurMultController(Control* /*cont*/) { }
	Control* GetImageBlurMultController() { return nullptr; }

	BOOL GetMotBlurOnOff(TimeValue /*t*/) { return FALSE; }
	void SetMotBlurOnOff(TimeValue /*t*/, BOOL /*m*/) { }
	Control* GetMotBlurOnOffController() { return nullptr; }
	void SetMotBlurOnOffController(Control* /*cont*/) { }

	void SetRenderable(BOOL /*onOff*/) { }
	int Renderable() { return 0; }

	void SetPrimaryVisibility(bool /*onOff*/) override { }
	bool GetPrimaryVisibility() const override { return false; }
	void SetSecondaryVisibility(bool /*onOff*/) override { }
	bool GetSecondaryVisibility() const override { return false; }

	void ShowBone(int /*boneVis*/) { }
	void BoneAsLine(int /*onOff*/) { }
	BOOL IsBoneShowing() { return FALSE; }
	BOOL IsBoneOnly() { return FALSE; }
	DWORD GetWireColor() { return 0; }
	void SetWireColor(DWORD /*newcol*/) { }
	int IsRootNode() { return 0; }
	int Selected() { return 0; }
	int Dependent() { return 0; }
	int IsTarget() { return 0; }
	void SetIsTarget(BOOL /*b*/) { }
	BOOL GetTransformLock(int /*type*/, int /*axis*/) { return FALSE; }
	void SetTransformLock(int /*type*/, int /*axis*/, BOOL /*onOff*/) { }
	INode* GetTarget() { return nullptr; }
	INode* GetLookatNode() { return nullptr; }
	Matrix3 GetParentTM(TimeValue /*t*/) { return Matrix3(); }
	int GetTargetTM(TimeValue /*t*/, Matrix3& /*m*/) { return 0; }
	Object* GetObjOrWSMRef() { return nullptr; }
	Control* GetTMController() { return nullptr; }
	BOOL SetTMController(Control* /*m3cont*/) { return FALSE; }
	Control* GetVisController() { return nullptr; }
	void SetVisController(Control* /*cont*/) { }
	float GetVisibility(TimeValue /*t*/, Interval* /*valid*/ = NULL) { return 0.0f; }
	void SetVisibility(TimeValue /*t*/, float /*vis*/) { }
	float GetLocalVisibility(TimeValue /*t*/, Interval* /*valid*/) { return 0.0f; }
	BOOL GetInheritVisibility() { return FALSE; }
	void SetInheritVisibility(BOOL /*onOff*/) { }
	
	virtual void SetRenderOccluded(BOOL /*onOff*/) { }
	virtual BOOL GetRenderOccluded() { return FALSE; }

	Material* Mtls() { return nullptr; }
	int NumMtls() { return 0; }

	RenderData* GetRenderData() { return nullptr; }
	void SetRenderData(RenderData* /*rd*/) { }

	void SetObjOffsetPos(Point3 /*p*/) { }
	Point3 GetObjOffsetPos() { return nullptr; }
	void SetObjOffsetRot(Quat /*q*/) { }
	Quat GetObjOffsetRot() { return nullptr; }
	void FlagForeground(TimeValue /*t*/, BOOL /*notify*/ = TRUE) { }
	int IsActiveGrid() { return 0; }
	void SetNodeLong(LONG_PTR /*l*/) { }
	LONG_PTR GetNodeLong() { return 0; }

	void GetUserPropBuffer(MSTR& /*buf*/) { }
	void SetUserPropBuffer(const MSTR& /*buf*/) { }
	BOOL GetUserPropString(const MSTR& /*key*/, MSTR& /*string*/) { return FALSE; }
	BOOL GetUserPropInt(const MSTR& /*key*/, int& /*val*/) { return FALSE; }
	BOOL GetUserPropFloat(const MSTR& /*key*/, float& /*val*/) { return FALSE; }
	BOOL GetUserPropBool(const MSTR& /*key*/, BOOL& /*b*/) { return FALSE; }
	void SetUserPropString(const MSTR& /*key*/, const MSTR& /*string*/) { }
	void SetUserPropInt(const MSTR& /*key*/, int /*val*/) { }
	void SetUserPropFloat(const MSTR& /*key*/, float /*val*/) { }
	void SetUserPropBool(const MSTR& /*key*/, BOOL /*b*/) { }
	BOOL UserPropExists(const MSTR& /*key*/) { return FALSE; }
	ULONG GetGBufID() { return 0; }
	void SetGBufID(ULONG /*id*/) { }

	UWORD GetRenderID() { return 0; }
	void SetRenderID(UWORD /*id*/) { }

	void SetObjOffsetScale(ScaleValue /*sv*/) { }
	ScaleValue GetObjOffsetScale() { return ScaleValue{}; }

	void CenterPivot(TimeValue /*t*/, BOOL /*moveObject*/) { }
	void AlignPivot(TimeValue /*t*/, BOOL /*moveObject*/) { }
	void WorldAlignPivot(TimeValue /*t*/, BOOL /*moveObject*/) { }
	void AlignToParent(TimeValue /*t*/) { }
	void AlignToWorld(TimeValue /*t*/) { }
	void ResetTransform(TimeValue /*t*/, BOOL /*scaleOnly*/) { }
	void ResetPivot(TimeValue /*t*/) { }
	bool MayResetTransform() { return false; }

	void Move(TimeValue /*t*/, const Matrix3& /*tmAxis*/, const Point3& /*val*/, BOOL /*localOrigin*/ = FALSE, BOOL /*affectKids*/ = TRUE, int /*pivMode*/ = PIV_NONE, BOOL /*ignoreLocks*/ = FALSE) { }
	void Rotate(TimeValue /*t*/, const Matrix3& /*tmAxis*/, const AngAxis& /*val*/, BOOL /*localOrigin*/ = FALSE, BOOL /*affectKids*/ = TRUE, int /*pivMode*/ = PIV_NONE, BOOL /*ignoreLocks*/ = FALSE) { }
	void Rotate(TimeValue /*t*/, const Matrix3& /*tmAxis*/, const Quat& /*val*/, BOOL /*localOrigin*/ = FALSE, BOOL /*affectKids*/ = TRUE, int /*pivMode*/ = PIV_NONE, BOOL /*ignoreLocks*/ = FALSE) { }
	void Scale(TimeValue /*t*/, const Matrix3& /*tmAxis*/, const Point3& /*val*/, BOOL /*localOrigin*/ = FALSE, BOOL /*affectKids*/ = TRUE, int /*pivMode*/ = PIV_NONE, BOOL /*ignoreLocks*/ = FALSE) { }

	BOOL IsGroupMember() { return FALSE; }
	BOOL IsGroupHead() { return FALSE; }
	BOOL IsOpenGroupMember() { return FALSE; }
	BOOL IsOpenGroupHead() { return FALSE; }

	void SetGroupMember(BOOL /*b*/) { }
	void SetGroupHead(BOOL /*b*/) { }
	void SetGroupMemberOpen(BOOL /*b*/) { }
	void SetGroupHeadOpen(BOOL /*b*/) { }

	RefResult NotifyRefChanged(const Interval& /*changeInt*/, RefTargetHandle /*hTarget*/, PartID& /*partID*/, RefMessage /*message*/, BOOL /*propagate*/) { return REF_FAIL; }

protected:
	/// Node name to use for the Mock:
	MSTR name;
};
