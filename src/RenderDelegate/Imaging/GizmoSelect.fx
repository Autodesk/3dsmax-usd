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

// Configurable wire color.
float4 LineColor = float4(1,0,0,1.0);
float ZBias = 0.0001;

float4x4 __object_to_ndc : WorldViewProjection;
float4x4 worldViewProjection : WorldViewProjection;
float2 ViewSize : viewportpixelsize;

float4 iPcPriority(float3 pm, float zPriority, float4x4 worldViewProjection)
{
	float4 P = mul(float4(pm, 1), worldViewProjection);
	P.z -= abs(zPriority) * sign(P.w);
	return P;
}

// From 3dsMax vertex buffers.
struct appdata
{
	float3 vertex : POSITION;
};

// Vertex shader to geometry shader.
struct v2g
{
	float4 vertex : SV_POSITION;
};

// Geometry shader to fragment shader.
struct g2f
{	
	float4 vertex : SV_POSITION;
};

v2g vert (appdata v)
{
	v2g o;
	o.vertex = iPcPriority(v.vertex, ZBias, __object_to_ndc);
	return o;
}

float4 fragLine(g2f input) : SV_Target
{
	return LineColor;
}

technique11 Gizmo
{
	pass P0
	{
		SetVertexShader(CompileShader(vs_5_0, vert()));
		SetPixelShader(CompileShader(ps_5_0, fragLine()));
	}
}