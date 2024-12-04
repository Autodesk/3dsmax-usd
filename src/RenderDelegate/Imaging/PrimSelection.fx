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

// Shader used to display selection on USD prims in the viewport. Works by
// Looking at the COLOR vertex buffer, vertices with COLOR values over 0.5 are 
// assumed to be selected. Selection is displayed using wireframe, with a 
// configurable color (alpha is supported).

// Configurable wire color.
float4 LineColor = float4(1,0,0,1.0);
// Give the selection wireframe some depth bias, to make sure it's displayed on top
// of the shaded geometry.
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
	float3 selected : COLOR;
};

// Vertex shader to geometry shader.
struct v2g
{
	float4 vertex : SV_POSITION;
	float3 selected : COLOR;
};

// Geometry shader to fragment shader.
struct g2f
{	
	float4 vertex : SV_POSITION;
	float3 selected : COLOR;
};

v2g vert (appdata v)
{
	v2g o;
	o.vertex = iPcPriority(v.vertex, ZBias, __object_to_ndc);
	o.selected = v.selected;
	float4 Po = float4(v.vertex.xyz,1);
	return o;
}

float3 instancedPosition( float3 pm, float4 mtx0, float4 mtx1, float4 mtx2 ) 
{ 
	float4x4 world = {float4(mtx0.x, mtx1.x, mtx2.x, 0), 
	float4(mtx0.y, mtx1.y, mtx2.y, 0), 
	float4(mtx0.z, mtx1.z, mtx2.z, 0), 
	float4(mtx0.w, mtx1.w, mtx2.w, 1)}; 
	// For instancing, transform the input position.
	float4 HPm = float4(pm, 1.0f);
	HPm = mul(HPm, world);
	return HPm.xyz; 
} 

v2g vertInstanced (appdata v, float4 row0 : Texcoord10, float4 row1 : Texcoord11, float4 row2 : Texcoord12)
{
	v2g o;
	const float3 pmInstance = instancedPosition(v.vertex, row0, row1, row2);
	o.vertex = iPcPriority(pmInstance, ZBias, __object_to_ndc);
	o.selected = v.selected;
	float4 Po = float4(v.vertex.xyz,1);
	return o;
}

[maxvertexcount(6)]
void triToLines(triangle v2g input[3], inout LineStream<g2f> outStream)
{
	g2f edge1[2];	
	edge1[0].vertex = input[0].vertex;
	edge1[0].selected = input[0].selected;
	edge1[1].vertex = input[1].vertex;
	edge1[1].selected = input[1].selected;
	outStream.Append(edge1[0]);
	outStream.Append(edge1[1]);
	outStream.RestartStrip();
	
	g2f edge2[2];
	edge2[0].vertex = input[1].vertex; 
	edge2[0].selected = input[1].selected;	
	edge2[1].vertex = input[2].vertex;
	edge2[1].selected = input[2].selected;
	outStream.Append(edge2[0]);
	outStream.Append(edge2[1]);
	outStream.RestartStrip();
	
	g2f edge3[2];
	edge3[0].vertex = input[2].vertex;
	edge3[0].selected = input[2].selected;
	edge3[1].vertex = input[0].vertex;
	edge3[1].selected = input[0].selected;	
	outStream.Append(edge3[0]);
	outStream.Append(edge3[1]);
	outStream.RestartStrip();
}

float4 fragLine(g2f input) : SV_Target
{
	if (input.selected.x < 0.5)
	{
		discard;
	}
	return LineColor;
}

// Display triangle wire frame of shaded geometry.
technique11 Shaded
{
	pass P0
	{
		SetVertexShader(CompileShader(vs_5_0, vert()));
		SetGeometryShader(CompileShader(gs_5_0, triToLines()));
		SetPixelShader(CompileShader(ps_5_0, fragLine()));
	}
}

// Display selected lines in a different color.
technique11 Wire
{
	pass P0
	{
		SetVertexShader(CompileShader(vs_5_0, vert()));
		SetPixelShader(CompileShader(ps_5_0, fragLine()));
	}
}

// Display triangle wire frame of shaded instanced geometry.
technique11 Shaded_Instanced
{
	pass P0
	{
		SetVertexShader(CompileShader(vs_5_0, vertInstanced()));
		SetGeometryShader(CompileShader(gs_5_0, triToLines()));
		SetPixelShader(CompileShader(ps_5_0, fragLine()));
	}
}

// Display instanced selected lines in a different color.
technique11 Wire_Instanced
{
	pass P0
	{
		SetVertexShader(CompileShader(vs_5_0, vertInstanced()));
		SetPixelShader(CompileShader(ps_5_0, fragLine()));
	}
}