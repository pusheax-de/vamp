#pragma once
// PipelineStates.h - Root signatures and Pipeline State Objects for all render passes

#include "EngineTypes.h"
#include <d3d12.h>
#include <wrl/client.h>
#include <string>

namespace engine
{

using Microsoft::WRL::ComPtr;

// ---------------------------------------------------------------------------
// PipelineStates - creates and owns all PSOs and root signatures
// ---------------------------------------------------------------------------
class PipelineStates
{
public:
    bool Init(ID3D12Device* device, const std::wstring& shaderDir);
    void Shutdown();

    // Root signatures
    ID3D12RootSignature* GetMainRootSig() const { return m_mainRootSig.Get(); }
    ID3D12RootSignature* GetLightRootSig() const { return m_lightRootSig.Get(); }
    ID3D12RootSignature* GetCompositeRootSig() const { return m_compositeRootSig.Get(); }

    // PSOs
    ID3D12PipelineState* GetSpritePSO() const { return m_spritePSO.Get(); }
    ID3D12PipelineState* GetSpriteCutoutPSO() const { return m_spriteCutoutPSO.Get(); }
    ID3D12PipelineState* GetSpriteScreenPSO() const { return m_spriteScreenPSO.Get(); }
    ID3D12PipelineState* GetSpriteScreenPointPSO() const { return m_spriteScreenPointPSO.Get(); }
    ID3D12PipelineState* GetShadowVolumePSO() const { return m_shadowVolumePSO.Get(); }
    ID3D12PipelineState* GetLightRadialPSO() const { return m_lightRadialPSO.Get(); }
    ID3D12PipelineState* GetCompositePSO() const { return m_compositePSO.Get(); }
    ID3D12PipelineState* GetGridOverlayPSO() const { return m_gridOverlayPSO.Get(); }

    // Sampler descriptor heap (static samplers are baked into root sig, but
    // we also expose the heap for custom sampling)
    ID3D12DescriptorHeap* GetSamplerHeap() const { return m_samplerHeap.Get(); }

private:
    bool CreateMainRootSignature(ID3D12Device* device);
    bool CreateLightRootSignature(ID3D12Device* device);
    bool CreateCompositeRootSignature(ID3D12Device* device);

    bool CreateSpritePSO(ID3D12Device* device, const std::wstring& shaderDir);
    bool CreateSpriteCutoutPSO(ID3D12Device* device, const std::wstring& shaderDir);
    bool CreateSpriteScreenPSO(ID3D12Device* device, const std::wstring& shaderDir);
    bool CreateSpriteScreenPointPSO(ID3D12Device* device, const std::wstring& shaderDir);
    bool CreateShadowVolumePSO(ID3D12Device* device, const std::wstring& shaderDir);
    bool CreateLightRadialPSO(ID3D12Device* device, const std::wstring& shaderDir);
    bool CreateCompositePSO(ID3D12Device* device, const std::wstring& shaderDir);
    bool CreateGridOverlayPSO(ID3D12Device* device, const std::wstring& shaderDir);

    bool CreateSamplerHeap(ID3D12Device* device);

    static ComPtr<ID3DBlob> CompileShader(const std::wstring& path,
                                           const char* entryPoint,
                                           const char* target);
    static ComPtr<ID3DBlob> LoadCompiledShader(const std::wstring& path);

    // Root signatures
    ComPtr<ID3D12RootSignature> m_mainRootSig;      // Sprite, grid, shadow
    ComPtr<ID3D12RootSignature> m_lightRootSig;      // Light radial (CBV b0 + b1)
    ComPtr<ID3D12RootSignature> m_compositeRootSig;  // Composite (CBV + 4 SRVs)

    // PSOs
    ComPtr<ID3D12PipelineState> m_spritePSO;
    ComPtr<ID3D12PipelineState> m_spriteCutoutPSO;
    ComPtr<ID3D12PipelineState> m_spriteScreenPSO;  // Same as sprite but targets UNORM backbuffer
    ComPtr<ID3D12PipelineState> m_spriteScreenPointPSO; // UNORM backbuffer + point sampling
    ComPtr<ID3D12PipelineState> m_shadowVolumePSO;
    ComPtr<ID3D12PipelineState> m_lightRadialPSO;
    ComPtr<ID3D12PipelineState> m_compositePSO;
    ComPtr<ID3D12PipelineState> m_gridOverlayPSO;

    // Sampler heap
    ComPtr<ID3D12DescriptorHeap> m_samplerHeap;
};

} // namespace engine
