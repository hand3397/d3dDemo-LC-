//***************************************************************************************
// d3dUtil.h by Frank Luna (C) 2015 All Rights Reserved.
//
// General helper code.
//***************************************************************************************

#pragma once
#include "stdafx.h"

extern const int gNumFrameResources;

inline void d3dSetDebugName(IDXGIObject* obj, const char* name)
{
    if(obj)
    {
        obj->SetPrivateData(WKPDID_D3DDebugObjectName, lstrlenA(name), name);
    }
}
inline void d3dSetDebugName(ID3D12Device* obj, const char* name)
{
    if(obj)
    {
        obj->SetPrivateData(WKPDID_D3DDebugObjectName, lstrlenA(name), name);
    }
}
inline void d3dSetDebugName(ID3D12DeviceChild* obj, const char* name)
{
    if(obj)
    {
        obj->SetPrivateData(WKPDID_D3DDebugObjectName, lstrlenA(name), name);
    }
}

inline std::wstring AnsiToWString(const std::string& str)
{
    WCHAR buffer[512];
    MultiByteToWideChar(CP_ACP, 0, str.c_str(), -1, buffer, 512);
    return std::wstring(buffer);
}

/*
#if defined(_DEBUG)
    #ifndef Assert
    #define Assert(x, description)                                  \
    {                                                               \
        static bool ignoreAssert = false;                           \
        if(!ignoreAssert && !(x))                                   \
        {                                                           \
            Debug::AssertResult result = Debug::ShowAssertDialog(   \
            (L#x), description, AnsiToWString(__FILE__), __LINE__); \
        if(result == Debug::AssertIgnore)                           \
        {                                                           \
            ignoreAssert = true;                                    \
        }                                                           \
                    else if(result == Debug::AssertBreak)           \
        {                                                           \
            __debugbreak();                                         \
        }                                                           \
        }                                                           \
    }
    #endif
#else
    #ifndef Assert
    #define Assert(x, description) 
    #endif
#endif 		
    */

class d3dUtil
{
public:
    //static std::string ToString(HRESULT hr);

    static UINT CalcConstantBufferByteSize(UINT byteSize)
    {
        // 상수 버퍼(Constant Buffer)는 하드웨어의 최소 할당 크기(보통 256바이트)의 
        // 배수여야 한다. 따라서 가장 가까운 256의 배수로 올려야 한다.
        // 방법: 255를 더한 뒤 하위 8비트(256 미만 비트)를 마스킹한다.
        //
        // 예시: byteSize = 300일 경우
        // (300 + 255) & ~255
        // 555 & ~255
        // 0x022B & ~0x00ff
        // 0x022B & 0xff00
        // 0x0200
        // = 512
        return (byteSize + 255) & ~255;
    }

    static Microsoft::WRL::ComPtr<ID3DBlob> LoadBinary(const std::wstring& filename);

    static Microsoft::WRL::ComPtr<ID3D12Resource> CreateDefaultBuffer(
        ID3D12Device* device,
        ID3D12GraphicsCommandList* cmdList,
        const void* initData,
        UINT64 byteSize,
        Microsoft::WRL::ComPtr<ID3D12Resource>& uploadBuffer);

	static Microsoft::WRL::ComPtr<ID3DBlob> CompileShader(
		const std::wstring& filename,
		const D3D_SHADER_MACRO* defines,
		const std::string& entrypoint,
		const std::string& target);
};

class DxException
{
public:
    DxException() = default;
    DxException(HRESULT hr, const std::wstring& functionName, const std::wstring& filename, int lineNumber);

    std::wstring ToString()const;

    HRESULT ErrorCode = S_OK;
    std::wstring FunctionName;
    std::wstring Filename;
    int LineNumber = -1;
};

struct Light {
    DirectX::XMFLOAT3 Strength = { 0.5f, 0.5f, 0.5f };
    float FalloffStart = 1.0f;                          // point/spot 라이트에만 해당
    DirectX::XMFLOAT3 Direction = { 0.0f, -1.0f, 0.0f };// directional/spot 라이트에만 해당
    float FalloffEnd = 10.0f;                           // point/spot 라이트에만 해당
    DirectX::XMFLOAT3 Position = { 0.0f, 0.0f, 0.0f };  // point/spot 라이트에만 해당
    float SpotPower = 64.0f;                            // spot 라이트에만 해당
};

#define MaxLights 16

struct MaterialConstants {
    // 텍스처 색상(diffuse 색상)
    DirectX::XMFLOAT4 DiffuseAlbedo = { 1.0f, 1.0f, 1.0f, 1.0f };
    // 프레넬 반사 계수
    DirectX::XMFLOAT3 FresnelR0 = { 0.01f, 0.01f, 0.01f };
    float Roughness = 0.25f;

    // 텍스처 매핑에 사용됨.
    DirectX::XMFLOAT4X4 MatTransform = MathHelper::Identity4x4();
};

// 간단한 Material 구조체. 데모 용도로 사용됨.
// 실제 상용 3D 엔진에서는 보통 Material을 클래스 계층 구조로 관리한다.
struct Material {
    Material() = default;
    Material(const std::string& name, int matCBIndex_,
        int diffuseSrvHeapIndex_ = -1, int normalSrvHeapIndex_ = -1,
        const DirectX::XMFLOAT4& diffuseAlbedo = DirectX::XMFLOAT4(1, 1, 1, 1),
        const DirectX::XMFLOAT3& fresnelR0 = DirectX::XMFLOAT3(0.01f, 0.01f, 0.01f),
        float roughness = 0.25f,
        const DirectX::XMFLOAT4X4& matTransform = MathHelper::Identity4x4())
        : name_(name), matCBIndex_(matCBIndex_),
        diffuseSrvHeapIndex_(diffuseSrvHeapIndex_),
        normalSrvHeapIndex_(normalSrvHeapIndex_),
        diffuseAlbedo_(diffuseAlbedo), fresnelR0_(fresnelR0),
        roughness_(roughness), matTransform_(matTransform) {}

    std::string name_;

    // 상수 버퍼(Constant Buffer)에서 이 Material이 참조하는 인덱스
    int matCBIndex_ = -1;

    // Diffuse 텍스처의 SRV 힙 인덱스
    int diffuseSrvHeapIndex_ = -1;

    int normalSrvHeapIndex_ = -1;

    // Dirty 플래그: Material이 수정되었음을 표시. → 상수 버퍼 업데이트 필요
    // FrameResource마다 Material 상수 버퍼가 존재하므로,
    // 수정 시 NumFramesDirty = gNumFrameResources 로 설정해야
    // 모든 프레임 리소스가 업데이트를 받는다.
    int numFramesDirty_ = gNumFrameResources;

    // 셰이딩에 사용되는 Material 상수 버퍼 데이터
    DirectX::XMFLOAT4 diffuseAlbedo_ = { 1.0f, 1.0f, 1.0f, 1.0f };
    DirectX::XMFLOAT3 fresnelR0_ = { 0.01f, 0.01f, 0.01f };
    float roughness_ = 0.25f;

    DirectX::XMFLOAT4X4 matTransform_ = MathHelper::Identity4x4();
};

struct Texture {
    // 이름 (lookup용)
    std::string Name;

    // 파일 이름
    std::wstring Filename;

    uint32_t SrvHeapIndex = 0;

    // GPU 리소스 및 업로드 힙
    Microsoft::WRL::ComPtr<ID3D12Resource> Resource = nullptr;
    Microsoft::WRL::ComPtr<ID3D12Resource> UploadHeap = nullptr;
};

#ifndef ThrowIfFailed
#define ThrowIfFailed(x)                                              \
{                                                                     \
    HRESULT hr__ = (x);                                               \
    std::wstring wfn = AnsiToWString(__FILE__);                       \
    if(FAILED(hr__)) { throw DxException(hr__, L#x, wfn, __LINE__); } \
}
#endif

#ifndef ReleaseCom
#define ReleaseCom(x) { if(x){ x->Release(); x = 0; } }
#endif