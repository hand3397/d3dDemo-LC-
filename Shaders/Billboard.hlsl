//***************************************************************************************
// TreeSprite.hlsl by Frank Luna (C) 2015 All Rights Reserved.
//***************************************************************************************

// Defaults for number of lights.
#ifndef NUM_DIR_LIGHTS
    #define NUM_DIR_LIGHTS 3
#endif

#ifndef NUM_POINT_LIGHTS
    #define NUM_POINT_LIGHTS 0
#endif

#ifndef NUM_SPOT_LIGHTS
    #define NUM_SPOT_LIGHTS 0
#endif

// Include structures and functions for lighting.
#include "LightingUtil.hlsl"

struct MaterialData
{
    float4 DiffuseAlbedo;
    float3 FresnelR0;
    float Roughness;
    float4x4 MatTransform;
    uint DiffuseMapIndex;
    uint AtlasWidth;
    uint AtlasHeight;
    uint MatPad2;
};

// An array of textures, which is only supported in shader model 5.1+.  Unlike Texture2DArray, the textures
// in this array can be different sizes and formats, making it more flexible than texture arrays.
Texture2D gDiffuseMap[16] : register(t0, space0);

// Put in space1, so the texture array does not overlap with these resources.  
// The texture array will occupy registers t0, t1, ..., t3 in space0. 
StructuredBuffer<MaterialData> gMaterialData : register(t16, space1);

SamplerState gsamPointWrap : register(s0);
SamplerState gsamPointClamp : register(s1);
SamplerState gsamLinearWrap : register(s2);
SamplerState gsamLinearClamp : register(s3);
SamplerState gsamAnisotropicWrap : register(s4);
SamplerState gsamAnisotropicClamp : register(s5);

// Constant data that varies per frame.
cbuffer cbPerObject : register(b0)
{
    float4x4 gWorld;
    float4x4 gTexTransform;
    uint gMaterialIndex;
    uint gAtlasIndex;
    uint gIsBillboardYAxisFixed; // 0이면 자유 회전(Spherical), 1이면 Y축 고정(Cylindrical)
    uint gObjPad2;
};

cbuffer cbSkinned : register(b1)
{
    float4x4 gBoneTransforms[96];
};

// Constant data that varies per material.
cbuffer cbPass : register(b2)
{
    float4x4 gView;
    float4x4 gInvView;
    float4x4 gProj;
    float4x4 gInvProj;
    float4x4 gViewProj;
    float4x4 gInvViewProj;
    
    float3 gEyePosW;
    float cbPerObjectPad1;
    float2 gRenderTargetSize;
    float2 gInvRenderTargetSize;
    
    float gNearZ;
    float gFarZ;
    
    float gTotalTime;
    float gDeltaTime;
    
    float4 gAmbientLight;

    // Allow application to change fog parameters once per frame.
	// For example, we may only use fog for certain times of day.
    float4 gFogColor;
    float gFogStart;
    float gFogRange;
    float2 cbPerObjectPad2;
    
    // Indices [0, NUM_DIR_LIGHTS) are directional lights;
    // indices [NUM_DIR_LIGHTS, NUM_DIR_LIGHTS+NUM_POINT_LIGHTS) are point lights;
    // indices [NUM_DIR_LIGHTS+NUM_POINT_LIGHTS, NUM_DIR_LIGHTS+NUM_POINT_LIGHT+NUM_SPOT_LIGHTS)
    // are spot lights for a maximum of MaxLights per object.
    Light gLights[MaxLights];
};

struct VertexIn
{
    float3 PosL : POSITION;
    float2 Size : SIZE;
};

struct VertexOut
{
    float3 CenterW : POSITION;
    float2 Size : SIZE;
};

struct GeoOut
{
	float4 PosH    : SV_POSITION;
    float3 PosW    : POSITION;
    float3 NormalW : NORMAL;
    float2 TexC    : TEXCOORD;
};

VertexOut VS(VertexIn vin)
{
	VertexOut vout;

	// Just pass data over to geometry shader.
    vout.CenterW = mul(float4(vin.PosL.xyz, 1.0f), gWorld);
	vout.Size   = vin.Size;

	return vout;
}
 
 // We expand each point into a quad (4 vertices), so the maximum number of vertices
 // we output per geometry shader invocation is 4.
[maxvertexcount(4)]
void GS(point VertexOut gin[1],
    inout TriangleStream<GeoOut> triStream)
{	
    float3 up, right, look;
    
    if (gIsBillboardYAxisFixed == 1)
    {
    // Y축 고정 빌보드
        up = float3(0.0f, 1.0f, 0.0f);
        look = gEyePosW - gin[0].CenterW;
        look.y = 0.0f; // y-axis aligned, so project to xz-plane
        look = normalize(look);
        right = cross(up, look);
    }
    else
    {
    // 구형 빌보드
        right = gInvView[0].xyz;
        up = gInvView[1].xyz;
        
        look = normalize(gEyePosW - gin[0].CenterW);
    }

	//
	// Compute triangle strip vertices (quad) in world space.
	//
	float halfWidth  = 0.5f*gin[0].Size.x;
	float halfHeight = 0.5f*gin[0].Size.y;
	
	float4 v[4];
    v[0] = float4(gin[0].CenterW + halfWidth * right - halfHeight * up, 1.0f);
    v[1] = float4(gin[0].CenterW + halfWidth * right + halfHeight * up, 1.0f);
    v[2] = float4(gin[0].CenterW - halfWidth * right - halfHeight * up, 1.0f);
    v[3] = float4(gin[0].CenterW - halfWidth * right + halfHeight * up, 1.0f);

	//
	// Transform quad vertices to world space and output 
	// them as a triangle strip.
	//
	
    // Fetch the material data.
    MaterialData matData = gMaterialData[gMaterialIndex];
    
	float2 texCs[4] = 
	{
		float2(0.0f, 1.0f),
		float2(0.0f, 0.0f),
		float2(1.0f, 1.0f),
		float2(1.0f, 0.0f)
	};
	
	GeoOut gout;
	[unroll]
	for(int i = 0; i < 4; ++i)
	{
        gout.PosH = mul(v[i], gViewProj);
        gout.PosW = v[i].xyz;
        gout.NormalW = look;
        
        // Output vertex attributes for interpolation across triangle.
        float4 texC = mul(float4(texCs[i], 0.0f, 1.0f), gTexTransform);
        float2 finalTexC = mul(texC, matData.MatTransform).xy;
		
        // atlas 텍스처 좌표 계산
        if (matData.AtlasWidth > 1 || matData.AtlasHeight > 1)
        {
            uint x = gAtlasIndex % matData.AtlasWidth;
            uint y = gAtlasIndex / matData.AtlasWidth;

            float2 scale = float2(1.0f / (float) matData.AtlasWidth, 1.0f / (float) matData.AtlasHeight);
            float2 offset = float2(x * scale.x, y * scale.y);
       
            finalTexC = finalTexC * scale + offset;
        }
        
        gout.TexC = finalTexC;
        
		triStream.Append(gout);
	}
}



