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

#include <pxr/usd/usdGeom/mesh.h>

#include <MaxUsd/MeshConversion/MeshConverter.h>

#include <gtest/gtest.h>

namespace TestUtils {

// Simple delegate to catch and fail tests in case USD reports any coding errors or warnings.
class DiagnosticsDelegate : public pxr::TfDiagnosticMgr::Delegate
{
public:
	void IssueError(const pxr::TfError& err) override
	{
		FAIL() << err.GetCommentary();
	}
	void IssueFatalError(const pxr::TfCallContext& context, const std::string& msg) override
	{
		FAIL() << msg;
	}

	void IssueStatus(const pxr::TfStatus& status) override {};
	void IssueWarning(const pxr::TfWarning& warning) override
	{
		FAIL() << warning.GetCommentary();
	}
};

void CompareUsdAndMaxMeshes(const MNMesh& maxMesh, const pxr::UsdGeomMesh &usdMesh);
void CompareVertices(const MNMesh& maxMesh, const pxr::UsdGeomMesh &usdMesh);
void CompareFaceVertexCount(const MNMesh& maxMesh, const pxr::UsdGeomMesh &usdMesh);
void CompareFaceVertices(const MNMesh& maxMesh, const pxr::UsdGeomMesh &usdMesh);
void CompareMaxMeshNormals(MNMesh& maxMesh1, MNMesh& maxMesh2);
void CompareUSDMatrices(const pxr::GfMatrix4d& matrix1, const pxr::GfMatrix4d& matrix2);

MNMesh CreatePlane(int rows, int cols);
MNMesh CreateCube(bool specifyNormals);
MNMesh CreateRoofShape();
MNMesh CreateQuad();

std::string GetOutputDirectory();

// Used to expose protected methods from the MeshConverter so it can be tested.
class MeshConverterTester : public MaxUsd::MeshConverter
{
public:
	MeshConverterTester() : MeshConverter() {}
	using MaxUsd::MeshConverter::ResolveChannelPrimvars;
};

}
