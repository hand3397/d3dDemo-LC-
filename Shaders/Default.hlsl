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
    uint MatPad0;
};
#ifdef TEX_ARRAY
Texture2DArray gDiffuseMapArray : register(t0, space1); 
#else
Texture2D gDiffuseMap[16] : register(t0, space0);
#endif

// Put in space1, so the texture array does not overlap with these resources.  
// The texture array will occupy registers t0, t1, ..., t3 in space0. 
StructuredBuffer<MaterialData> gMaterialData : register(t16, space1);

SamplerState gsamPointWrap        : register(s0);
SamplerState gsamPointClamp       : register(s1);
SamplerState gsamLinearWrap       : register(s2);
SamplerState gsamLinearClamp      : register(s3);
SamplerState gsamAnisotropicWrap  : register(s4);
SamplerState gsamAnisotropicClamp : register(s5);

// Constant data that varies per frame.
cbuffer cbPerObject : register(b0)
{
    float4x4 gWorld;
	float4x4 gTexTransform;
    uint gMaterialIndex;
    uint gAtlasIndex; 
    uint gObjPad0; // union with gIsBillboardYAxisFixed in billboard shader
    uint gObjPad1;
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
	float3 PosL    : POSITION;
    float3 NormalL : NORMAL;
	float2 TexC    : TEXCOORD;
#ifdef TEX_ARRAY
    uint TexIndex   : TEXINDEX; // Material index for texture array access
#endif
#ifdef SKINNED
    float3 BoneWeights : WEIGHTS;
    uint4 BoneIndices  : BONEINDICES;
#endif
};

struct VertexOut
{
	float4 PosH    : SV_POSITION;
    float3 PosW    : POSITION;
    float3 NormalW : NORMAL;
	float2 TexC    : TEXCOORD;
#ifdef TEX_ARRAY
    // 보간 방지: 지형 인덱스는 정수값 그대로 전달되어야 함
    nointerpolation uint TexIndex : TEXINDEX;
#endif
};

VertexOut VS(VertexIn vin)
{
	VertexOut vout = (VertexOut)0.0f;
	
#ifdef SKINNED
    float weights[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    weights[0] = vin.BoneWeights.x;
    weights[1] = vin.BoneWeights.y;
    weights[2] = vin.BoneWeights.z;
    weights[3] = 1.0f - weights[0] - weights[1] - weights[2];

    float3 posL = float3(0.0f, 0.0f, 0.0f);
    float3 normalL = float3(0.0f, 0.0f, 0.0f);
    for(int i = 0; i < 4; ++i)
    {
        // Assume no nonuniform scaling when transforming normals, so 
        // that we do not have to use the inverse-transpose.

        posL += weights[i] * mul(float4(vin.PosL, 1.0f), gBoneTransforms[vin.BoneIndices[i]]).xyz;
        normalL += weights[i] * mul(vin.NormalL, (float3x3)gBoneTransforms[vin.BoneIndices[i]]);
    }

    vin.PosL = posL;
    vin.NormalL = normalL;
#endif
    
    // Fetch the material data.
    MaterialData matData = gMaterialData[gMaterialIndex];
    
    // Transform to world space.
    float4 posW = mul(float4(vin.PosL, 1.0f), gWorld);
    vout.PosW = posW.xyz;

    // Assumes nonuniform scaling; otherwise, need to use inverse-transpose of world matrix.
    vout.NormalW = mul(vin.NormalL, (float3x3)gWorld);

    // Transform to homogeneous clip space.
    vout.PosH = mul(posW, gViewProj);
	
    // 기본 텍스처 변환 (애니메이션, 타일링 등) 먼저 수행
    float4 texC = mul(float4(vin.TexC, 0.0f, 1.0f), gTexTransform);
    float2 finalTexC = mul(texC, matData.MatTransform).xy;

    // atlas 텍스처 좌표 계산
    if (matData.AtlasWidth > 1 || matData.AtlasHeight > 1)
    {
        uint x = gAtlasIndex % matData.AtlasWidth;
        uint y = gAtlasIndex / matData.AtlasWidth;

        float2 scale = float2(1.0f / (float) matData.AtlasWidth, 1.0f / (float) matData.AtlasHeight);
        float2 offset = float2(x * scale.x, y * scale.y);
       
        finalTexC = frac(finalTexC) * scale + offset;
        // Tip: frac()을 써주면 타일링 옵션이 켜져있어도 아틀라스 조각 내부에서 반복됩니다.
    }

    vout.TexC = finalTexC;
    
#ifdef TEX_ARRAY
    // 정점의 인덱스를 픽셀 셰이더로 전달
    vout.TexIndex = vin.TexIndex;
#endif
    
    return vout;
}

float4 PS(VertexOut pin) : SV_Target
{
    // 재질 데이터 가져오기
    MaterialData matData = gMaterialData[gMaterialIndex];
    float4 diffuseAlbedo = matData.DiffuseAlbedo;
    float3 fresnelR0 = matData.FresnelR0;
    float roughness = matData.Roughness;
    uint diffuseTexIndex = matData.DiffuseMapIndex;
    
#ifdef TEX_ARRAY
    // float3(U, V, SliceIndex)를 사용하여 샘플링
    diffuseAlbedo *= gDiffuseMapArray.Sample(gsamLinearWrap, float3(pin.TexC, pin.TexIndex));
#else
    // 텍스처 샘플링 (Texture2D 배열에서 인덱스로 접근)
    diffuseAlbedo *= gDiffuseMap[diffuseTexIndex].Sample(gsamLinearWrap, pin.TexC);
#endif
    
#ifdef ALPHA_TEST
    // 알파 테스트: 투명도 0.1 미만 픽셀 제거 (조기 종료 최적화)
    clip(diffuseAlbedo.a - 0.1f);
#endif

    // 법선 벡터 정규화
    pin.NormalW = normalize(pin.NormalW);

    // 카메라 관련 벡터 계산 (조명 및 안개 공용)
    float3 toEyeW = gEyePosW - pin.PosW; // 카메라로 향하는 벡터
    float distToEye = length(toEyeW); // 카메라와의 거리
    toEyeW /= distToEye; // 정규화 (normalize 대용)

    // 조명 연산
    float4 ambient = gAmbientLight * diffuseAlbedo;

    const float shininess = 1.0f - roughness;
    Material mat = { diffuseAlbedo, fresnelR0, shininess };
    float3 shadowFactor = 1.0f;
    
    float4 directLight = ComputeLighting(gLights, mat, pin.PosW,
                                        pin.NormalW, toEyeW, shadowFactor);

    float4 litColor = ambient + directLight;

    // 안개 효과 적용
#ifdef FOG
    float fogAmount = saturate((distToEye - gFogStart) / gFogRange);
    litColor = lerp(litColor, gFogColor, fogAmount);
#endif

    // 최종 알파값 설정 (보통 디퓨즈 알파를 따름)
    litColor.a = diffuseAlbedo.a;

    return litColor;
}

