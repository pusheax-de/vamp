// PipelineStates.cpp - Root signature and PSO creation

#include "PipelineStates.h"
#include <d3dcompiler.h>
#include <cassert>
#include <fstream>
#include <vector>

#pragma comment(lib, "d3dcompiler.lib")

namespace engine
{

// ===========================================================================
// Shader compilation helpers
// ===========================================================================

ComPtr<ID3DBlob> PipelineStates::CompileShader(const std::wstring& path,
                                                 const char* entryPoint,
                                                 const char* target)
{
    UINT compileFlags = 0;
#ifdef _DEBUG
    compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

    ComPtr<ID3DBlob> shaderBlob;
    ComPtr<ID3DBlob> errorBlob;
    HRESULT hr = D3DCompileFromFile(path.c_str(), nullptr, nullptr,
                                     entryPoint, target, compileFlags, 0,
                                     &shaderBlob, &errorBlob);
    if (FAILED(hr))
    {
        if (errorBlob)
            OutputDebugStringA(static_cast<const char*>(errorBlob->GetBufferPointer()));
        return nullptr;
    }
    return shaderBlob;
}

ComPtr<ID3DBlob> PipelineStates::LoadCompiledShader(const std::wstring& path)
{
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open())
        return nullptr;

    size_t size = static_cast<size_t>(file.tellg());
    file.seekg(0);

    ComPtr<ID3DBlob> blob;
    D3DCreateBlob(size, &blob);
    file.read(static_cast<char*>(blob->GetBufferPointer()), size);
    return blob;
}

// ===========================================================================
// Init / Shutdown
// ===========================================================================

bool PipelineStates::Init(ID3D12Device* device, const std::wstring& shaderDir)
{
    if (!CreateMainRootSignature(device))       return false;
    if (!CreateLightRootSignature(device))      return false;
    if (!CreateCompositeRootSignature(device))  return false;
    if (!CreateSamplerHeap(device))             return false;

    if (!CreateSpritePSO(device, shaderDir))        return false;
    if (!CreateSpriteCutoutPSO(device, shaderDir))  return false;
    if (!CreateSpriteScreenPSO(device, shaderDir))  return false;
    if (!CreateSpriteScreenPointPSO(device, shaderDir)) return false;
    if (!CreateShadowVolumePSO(device, shaderDir))  return false;
    if (!CreateLightRadialPSO(device, shaderDir))   return false;
    if (!CreateCompositePSO(device, shaderDir))     return false;
    if (!CreateGridOverlayPSO(device, shaderDir))   return false;
    if (!CreateGridOverlayScenePSO(device, shaderDir)) return false;

    return true;
}

void PipelineStates::Shutdown()
{
    m_spritePSO.Reset();
    m_spriteCutoutPSO.Reset();
    m_spriteScreenPSO.Reset();
    m_spriteScreenPointPSO.Reset();
    m_shadowVolumePSO.Reset();
    m_lightRadialPSO.Reset();
    m_compositePSO.Reset();
    m_gridOverlayPSO.Reset();
    m_gridOverlayScenePSO.Reset();
    m_mainRootSig.Reset();
    m_lightRootSig.Reset();
    m_compositeRootSig.Reset();
    m_samplerHeap.Reset();
}

// ===========================================================================
// Root Signatures
// ===========================================================================

bool PipelineStates::CreateMainRootSignature(ID3D12Device* device)
{
    // Main root signature: used by sprite, shadow volume, grid overlay
    // Root param 0: CBV (b0) - FrameConstants
    // Root param 1: Descriptor table - SRV array (t0+, unbounded) for textures
    // Static sampler s0: linear wrap

    D3D12_ROOT_PARAMETER rootParams[2] = {};

    // CBV at b0 (inline 32-bit constants or CBV)
    rootParams[0].ParameterType             = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParams[0].Descriptor.ShaderRegister = 0;
    rootParams[0].Descriptor.RegisterSpace  = 0;
    rootParams[0].ShaderVisibility          = D3D12_SHADER_VISIBILITY_ALL;

    // SRV table (bounded array of textures at t0)
    D3D12_DESCRIPTOR_RANGE srvRange = {};
    srvRange.RangeType                         = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    srvRange.NumDescriptors                    = 1024;
    srvRange.BaseShaderRegister                = 0;
    srvRange.RegisterSpace                     = 0;
    srvRange.OffsetInDescriptorsFromTableStart = 0;

    rootParams[1].ParameterType                       = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParams[1].DescriptorTable.NumDescriptorRanges = 1;
    rootParams[1].DescriptorTable.pDescriptorRanges   = &srvRange;
    rootParams[1].ShaderVisibility                    = D3D12_SHADER_VISIBILITY_PIXEL;

    // Static sampler: linear wrap at s0
    D3D12_STATIC_SAMPLER_DESC staticSamplers[2] = {};

    staticSamplers[0].Filter           = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    staticSamplers[0].AddressU         = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    staticSamplers[0].AddressV         = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    staticSamplers[0].AddressW         = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    staticSamplers[0].ShaderRegister   = 0;
    staticSamplers[0].RegisterSpace    = 0;
    staticSamplers[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    staticSamplers[1].Filter           = D3D12_FILTER_MIN_MAG_MIP_POINT;
    staticSamplers[1].AddressU         = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    staticSamplers[1].AddressV         = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    staticSamplers[1].AddressW         = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    staticSamplers[1].ShaderRegister   = 1;
    staticSamplers[1].RegisterSpace    = 0;
    staticSamplers[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    D3D12_ROOT_SIGNATURE_DESC desc = {};
    desc.NumParameters     = 2;
    desc.pParameters       = rootParams;
    desc.NumStaticSamplers = 2;
    desc.pStaticSamplers   = staticSamplers;
    desc.Flags             = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    ComPtr<ID3DBlob> sigBlob, errorBlob;
    HRESULT hr = D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1,
                                              &sigBlob, &errorBlob);
    if (FAILED(hr))
    {
        if (errorBlob)
            OutputDebugStringA(static_cast<const char*>(errorBlob->GetBufferPointer()));
        return false;
    }

    hr = device->CreateRootSignature(0, sigBlob->GetBufferPointer(),
                                      sigBlob->GetBufferSize(),
                                      IID_PPV_ARGS(&m_mainRootSig));
    return SUCCEEDED(hr);
}

bool PipelineStates::CreateLightRootSignature(ID3D12Device* device)
{
    // Light root signature: CBV b0 (FrameConstants) + CBV b1 (LightConstants)
    D3D12_ROOT_PARAMETER rootParams[2] = {};

    rootParams[0].ParameterType             = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParams[0].Descriptor.ShaderRegister = 0;
    rootParams[0].Descriptor.RegisterSpace  = 0;
    rootParams[0].ShaderVisibility          = D3D12_SHADER_VISIBILITY_ALL;

    rootParams[1].ParameterType             = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParams[1].Descriptor.ShaderRegister = 1;
    rootParams[1].Descriptor.RegisterSpace  = 0;
    rootParams[1].ShaderVisibility          = D3D12_SHADER_VISIBILITY_ALL;

    D3D12_ROOT_SIGNATURE_DESC desc = {};
    desc.NumParameters     = 2;
    desc.pParameters       = rootParams;
    desc.NumStaticSamplers = 0;
    desc.Flags             = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    ComPtr<ID3DBlob> sigBlob, errorBlob;
    HRESULT hr = D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1,
                                              &sigBlob, &errorBlob);
    if (FAILED(hr)) return false;

    hr = device->CreateRootSignature(0, sigBlob->GetBufferPointer(),
                                      sigBlob->GetBufferSize(),
                                      IID_PPV_ARGS(&m_lightRootSig));
    return SUCCEEDED(hr);
}

bool PipelineStates::CreateCompositeRootSignature(ID3D12Device* device)
{
    // Composite: CBV b0 + descriptor table with 4 SRVs (t0-t3)
    D3D12_ROOT_PARAMETER rootParams[2] = {};

    rootParams[0].ParameterType             = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParams[0].Descriptor.ShaderRegister = 0;
    rootParams[0].Descriptor.RegisterSpace  = 0;
    rootParams[0].ShaderVisibility          = D3D12_SHADER_VISIBILITY_ALL;

    D3D12_DESCRIPTOR_RANGE srvRange = {};
    srvRange.RangeType                         = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    srvRange.NumDescriptors                    = 4;
    srvRange.BaseShaderRegister                = 0;
    srvRange.RegisterSpace                     = 0;
    srvRange.OffsetInDescriptorsFromTableStart = 0;

    rootParams[1].ParameterType                       = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParams[1].DescriptorTable.NumDescriptorRanges = 1;
    rootParams[1].DescriptorTable.pDescriptorRanges   = &srvRange;
    rootParams[1].ShaderVisibility                    = D3D12_SHADER_VISIBILITY_PIXEL;

    // Two static samplers: point (s0) and linear (s1)
    D3D12_STATIC_SAMPLER_DESC samplers[2] = {};

    samplers[0].Filter           = D3D12_FILTER_MIN_MAG_MIP_POINT;
    samplers[0].AddressU         = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    samplers[0].AddressV         = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    samplers[0].AddressW         = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    samplers[0].ShaderRegister   = 0;
    samplers[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    samplers[1].Filter           = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    samplers[1].AddressU         = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    samplers[1].AddressV         = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    samplers[1].AddressW         = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    samplers[1].ShaderRegister   = 1;
    samplers[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    D3D12_ROOT_SIGNATURE_DESC desc = {};
    desc.NumParameters     = 2;
    desc.pParameters       = rootParams;
    desc.NumStaticSamplers = 2;
    desc.pStaticSamplers   = samplers;
    desc.Flags             = D3D12_ROOT_SIGNATURE_FLAG_NONE;

    ComPtr<ID3DBlob> sigBlob, errorBlob;
    HRESULT hr = D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1,
                                              &sigBlob, &errorBlob);
    if (FAILED(hr)) return false;

    hr = device->CreateRootSignature(0, sigBlob->GetBufferPointer(),
                                      sigBlob->GetBufferSize(),
                                      IID_PPV_ARGS(&m_compositeRootSig));
    return SUCCEEDED(hr);
}

// ===========================================================================
// PSO creation
// ===========================================================================

bool PipelineStates::CreateSpritePSO(ID3D12Device* device, const std::wstring& shaderDir)
{
    auto vs = CompileShader(shaderDir + L"/SpriteVS.hlsl", "main", "vs_5_1");
    auto ps = CompileShader(shaderDir + L"/SpritePS.hlsl", "main", "ps_5_1");
    if (!vs || !ps) return false;

    // Input layout: per-instance data
    D3D12_INPUT_ELEMENT_DESC inputLayout[] =
    {
        { "INST_POS",    0, DXGI_FORMAT_R32G32_FLOAT,       1, 0,  D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 },
        { "INST_SIZE",   0, DXGI_FORMAT_R32G32_FLOAT,       1, 8,  D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 },
        { "INST_UVRECT", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 16, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 },
        { "INST_COLOR",  0, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 32, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 },
        { "INST_ROT",    0, DXGI_FORMAT_R32_FLOAT,          1, 48, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 },
        { "INST_SORTY",  0, DXGI_FORMAT_R32_FLOAT,          1, 52, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 },
        { "INST_TEX",    0, DXGI_FORMAT_R32_UINT,           1, 56, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 },
        { "INST_DEPTH",  0, DXGI_FORMAT_R32_FLOAT,          1, 60, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 },
    };

    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
    psoDesc.pRootSignature        = m_mainRootSig.Get();
    psoDesc.VS                    = { vs->GetBufferPointer(), vs->GetBufferSize() };
    psoDesc.PS                    = { ps->GetBufferPointer(), ps->GetBufferSize() };
    psoDesc.InputLayout           = { inputLayout, _countof(inputLayout) };
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    psoDesc.NumRenderTargets      = 1;
    psoDesc.RTVFormats[0]         = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    psoDesc.DSVFormat             = DXGI_FORMAT_D24_UNORM_S8_UINT;
    psoDesc.SampleDesc            = { 1, 0 };
    psoDesc.SampleMask            = UINT_MAX;
    psoDesc.RasterizerState.FillMode              = D3D12_FILL_MODE_SOLID;
    psoDesc.RasterizerState.CullMode              = D3D12_CULL_MODE_NONE;
    psoDesc.RasterizerState.DepthClipEnable       = TRUE;
    psoDesc.DepthStencilState.DepthEnable         = TRUE;
    psoDesc.DepthStencilState.DepthWriteMask      = D3D12_DEPTH_WRITE_MASK_ALL;
    psoDesc.DepthStencilState.DepthFunc           = D3D12_COMPARISON_FUNC_LESS_EQUAL;
    psoDesc.DepthStencilState.StencilEnable       = FALSE;

    // Alpha blending
    auto& blend = psoDesc.BlendState.RenderTarget[0];
    blend.BlendEnable           = TRUE;
    blend.SrcBlend              = D3D12_BLEND_SRC_ALPHA;
    blend.DestBlend             = D3D12_BLEND_INV_SRC_ALPHA;
    blend.BlendOp               = D3D12_BLEND_OP_ADD;
    blend.SrcBlendAlpha         = D3D12_BLEND_ONE;
    blend.DestBlendAlpha        = D3D12_BLEND_INV_SRC_ALPHA;
    blend.BlendOpAlpha          = D3D12_BLEND_OP_ADD;
    blend.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

    return SUCCEEDED(device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_spritePSO)));
}

bool PipelineStates::CreateSpriteCutoutPSO(ID3D12Device* device, const std::wstring& shaderDir)
{
    auto vs = CompileShader(shaderDir + L"/SpriteVS.hlsl", "main", "vs_5_1");
    auto ps = CompileShader(shaderDir + L"/SpritePS.hlsl", "main", "ps_5_1");
    if (!vs || !ps) return false;

    D3D12_INPUT_ELEMENT_DESC inputLayout[] =
    {
        { "INST_POS",    0, DXGI_FORMAT_R32G32_FLOAT,       1, 0,  D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 },
        { "INST_SIZE",   0, DXGI_FORMAT_R32G32_FLOAT,       1, 8,  D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 },
        { "INST_UVRECT", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 16, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 },
        { "INST_COLOR",  0, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 32, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 },
        { "INST_ROT",    0, DXGI_FORMAT_R32_FLOAT,          1, 48, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 },
        { "INST_SORTY",  0, DXGI_FORMAT_R32_FLOAT,          1, 52, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 },
        { "INST_TEX",    0, DXGI_FORMAT_R32_UINT,           1, 56, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 },
        { "INST_DEPTH",  0, DXGI_FORMAT_R32_FLOAT,          1, 60, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 },
    };

    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
    psoDesc.pRootSignature        = m_mainRootSig.Get();
    psoDesc.VS                    = { vs->GetBufferPointer(), vs->GetBufferSize() };
    psoDesc.PS                    = { ps->GetBufferPointer(), ps->GetBufferSize() };
    psoDesc.InputLayout           = { inputLayout, _countof(inputLayout) };
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    psoDesc.NumRenderTargets      = 1;
    psoDesc.RTVFormats[0]         = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    psoDesc.DSVFormat             = DXGI_FORMAT_D24_UNORM_S8_UINT;
    psoDesc.SampleDesc            = { 1, 0 };
    psoDesc.SampleMask            = UINT_MAX;
    psoDesc.RasterizerState.FillMode              = D3D12_FILL_MODE_SOLID;
    psoDesc.RasterizerState.CullMode              = D3D12_CULL_MODE_NONE;
    psoDesc.RasterizerState.DepthClipEnable       = TRUE;
    psoDesc.DepthStencilState.DepthEnable         = TRUE;
    psoDesc.DepthStencilState.DepthWriteMask      = D3D12_DEPTH_WRITE_MASK_ALL;
    psoDesc.DepthStencilState.DepthFunc           = D3D12_COMPARISON_FUNC_LESS_EQUAL;
    psoDesc.DepthStencilState.StencilEnable       = FALSE;

    auto& blend = psoDesc.BlendState.RenderTarget[0];
    blend.BlendEnable           = FALSE;
    blend.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

    return SUCCEEDED(device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_spriteCutoutPSO)));
}

bool PipelineStates::CreateSpriteScreenPSO(ID3D12Device* device, const std::wstring& shaderDir)
{
    // Same as sprite PSO but targeting UNORM backbuffer (for UI / screen-space rendering)
    auto vs = CompileShader(shaderDir + L"/SpriteVS.hlsl", "main", "vs_5_1");
    auto ps = CompileShader(shaderDir + L"/SpritePS.hlsl", "main", "ps_5_1");
    if (!vs || !ps) return false;

    D3D12_INPUT_ELEMENT_DESC inputLayout[] =
    {
        { "INST_POS",    0, DXGI_FORMAT_R32G32_FLOAT,       1, 0,  D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 },
        { "INST_SIZE",   0, DXGI_FORMAT_R32G32_FLOAT,       1, 8,  D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 },
        { "INST_UVRECT", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 16, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 },
        { "INST_COLOR",  0, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 32, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 },
        { "INST_ROT",    0, DXGI_FORMAT_R32_FLOAT,          1, 48, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 },
        { "INST_SORTY",  0, DXGI_FORMAT_R32_FLOAT,          1, 52, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 },
        { "INST_TEX",    0, DXGI_FORMAT_R32_UINT,           1, 56, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 },
        { "INST_DEPTH",  0, DXGI_FORMAT_R32_FLOAT,          1, 60, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 },
    };

    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
    psoDesc.pRootSignature        = m_mainRootSig.Get();
    psoDesc.VS                    = { vs->GetBufferPointer(), vs->GetBufferSize() };
    psoDesc.PS                    = { ps->GetBufferPointer(), ps->GetBufferSize() };
    psoDesc.InputLayout           = { inputLayout, _countof(inputLayout) };
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    psoDesc.NumRenderTargets      = 1;
    psoDesc.RTVFormats[0]         = DXGI_FORMAT_R8G8B8A8_UNORM;
    psoDesc.SampleDesc            = { 1, 0 };
    psoDesc.SampleMask            = UINT_MAX;
    psoDesc.RasterizerState.FillMode              = D3D12_FILL_MODE_SOLID;
    psoDesc.RasterizerState.CullMode              = D3D12_CULL_MODE_NONE;
    psoDesc.RasterizerState.DepthClipEnable       = TRUE;
    psoDesc.DepthStencilState.DepthEnable         = FALSE;
    psoDesc.DepthStencilState.StencilEnable       = FALSE;

    auto& blend = psoDesc.BlendState.RenderTarget[0];
    blend.BlendEnable           = TRUE;
    blend.SrcBlend              = D3D12_BLEND_SRC_ALPHA;
    blend.DestBlend             = D3D12_BLEND_INV_SRC_ALPHA;
    blend.BlendOp               = D3D12_BLEND_OP_ADD;
    blend.SrcBlendAlpha         = D3D12_BLEND_ONE;
    blend.DestBlendAlpha        = D3D12_BLEND_INV_SRC_ALPHA;
    blend.BlendOpAlpha          = D3D12_BLEND_OP_ADD;
    blend.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

    return SUCCEEDED(device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_spriteScreenPSO)));
}

bool PipelineStates::CreateSpriteScreenPointPSO(ID3D12Device* device, const std::wstring& shaderDir)
{
    // Same as SpriteScreenPSO but uses SpritePointPS (point-sampled) for crisp UI text
    auto vs = CompileShader(shaderDir + L"/SpriteVS.hlsl", "main", "vs_5_1");
    auto ps = CompileShader(shaderDir + L"/SpritePointPS.hlsl", "main", "ps_5_1");
    if (!vs || !ps) return false;

    D3D12_INPUT_ELEMENT_DESC inputLayout[] =
    {
        { "INST_POS",    0, DXGI_FORMAT_R32G32_FLOAT,       1, 0,  D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 },
        { "INST_SIZE",   0, DXGI_FORMAT_R32G32_FLOAT,       1, 8,  D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 },
        { "INST_UVRECT", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 16, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 },
        { "INST_COLOR",  0, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 32, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 },
        { "INST_ROT",    0, DXGI_FORMAT_R32_FLOAT,          1, 48, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 },
        { "INST_SORTY",  0, DXGI_FORMAT_R32_FLOAT,          1, 52, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 },
        { "INST_TEX",    0, DXGI_FORMAT_R32_UINT,           1, 56, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 },
        { "INST_DEPTH",  0, DXGI_FORMAT_R32_FLOAT,          1, 60, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 },
    };

    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
    psoDesc.pRootSignature        = m_mainRootSig.Get();
    psoDesc.VS                    = { vs->GetBufferPointer(), vs->GetBufferSize() };
    psoDesc.PS                    = { ps->GetBufferPointer(), ps->GetBufferSize() };
    psoDesc.InputLayout           = { inputLayout, _countof(inputLayout) };
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    psoDesc.NumRenderTargets      = 1;
    psoDesc.RTVFormats[0]         = DXGI_FORMAT_R8G8B8A8_UNORM;
    psoDesc.SampleDesc            = { 1, 0 };
    psoDesc.SampleMask            = UINT_MAX;
    psoDesc.RasterizerState.FillMode              = D3D12_FILL_MODE_SOLID;
    psoDesc.RasterizerState.CullMode              = D3D12_CULL_MODE_NONE;
    psoDesc.RasterizerState.DepthClipEnable       = TRUE;
    psoDesc.DepthStencilState.DepthEnable         = FALSE;
    psoDesc.DepthStencilState.StencilEnable       = FALSE;

    auto& blend = psoDesc.BlendState.RenderTarget[0];
    blend.BlendEnable           = TRUE;
    blend.SrcBlend              = D3D12_BLEND_SRC_ALPHA;
    blend.DestBlend             = D3D12_BLEND_INV_SRC_ALPHA;
    blend.BlendOp               = D3D12_BLEND_OP_ADD;
    blend.SrcBlendAlpha         = D3D12_BLEND_ONE;
    blend.DestBlendAlpha        = D3D12_BLEND_INV_SRC_ALPHA;
    blend.BlendOpAlpha          = D3D12_BLEND_OP_ADD;
    blend.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

    return SUCCEEDED(device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_spriteScreenPointPSO)));
}

bool PipelineStates::CreateShadowVolumePSO(ID3D12Device* device, const std::wstring& shaderDir)
{
    auto vs = CompileShader(shaderDir + L"/ShadowVolumeVS.hlsl", "main", "vs_5_1");
    if (!vs) return false;

    // Simple 2D position input
    D3D12_INPUT_ELEMENT_DESC inputLayout[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    };

    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
    psoDesc.pRootSignature        = m_mainRootSig.Get();
    psoDesc.VS                    = { vs->GetBufferPointer(), vs->GetBufferSize() };
    // No pixel shader � color writes off
    psoDesc.InputLayout           = { inputLayout, _countof(inputLayout) };
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    psoDesc.NumRenderTargets      = 0; // No color output
    psoDesc.DSVFormat             = DXGI_FORMAT_D24_UNORM_S8_UINT;
    psoDesc.SampleDesc            = { 1, 0 };
    psoDesc.SampleMask            = UINT_MAX;
    psoDesc.RasterizerState.FillMode              = D3D12_FILL_MODE_SOLID;
    psoDesc.RasterizerState.CullMode              = D3D12_CULL_MODE_NONE;
    psoDesc.RasterizerState.DepthClipEnable       = TRUE;

    // Depth off, stencil on: write 1 where shadow quads are rendered
    psoDesc.DepthStencilState.DepthEnable    = FALSE;
    psoDesc.DepthStencilState.StencilEnable  = TRUE;
    psoDesc.DepthStencilState.StencilReadMask  = 0xFF;
    psoDesc.DepthStencilState.StencilWriteMask = 0xFF;

    D3D12_DEPTH_STENCILOP_DESC stencilOp = {};
    stencilOp.StencilFailOp      = D3D12_STENCIL_OP_KEEP;
    stencilOp.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
    stencilOp.StencilPassOp      = D3D12_STENCIL_OP_REPLACE; // Write stencil ref (1)
    stencilOp.StencilFunc        = D3D12_COMPARISON_FUNC_ALWAYS;

    psoDesc.DepthStencilState.FrontFace = stencilOp;
    psoDesc.DepthStencilState.BackFace  = stencilOp;

    // No blend state needed (no color output)

    return SUCCEEDED(device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_shadowVolumePSO)));
}

bool PipelineStates::CreateLightRadialPSO(ID3D12Device* device, const std::wstring& shaderDir)
{
    auto vs = CompileShader(shaderDir + L"/LightRadialVS.hlsl", "main", "vs_5_1");
    auto ps = CompileShader(shaderDir + L"/LightRadialPS.hlsl", "main", "ps_5_1");
    if (!vs || !ps) return false;

    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
    psoDesc.pRootSignature        = m_lightRootSig.Get();
    psoDesc.VS                    = { vs->GetBufferPointer(), vs->GetBufferSize() };
    psoDesc.PS                    = { ps->GetBufferPointer(), ps->GetBufferSize() };
    psoDesc.InputLayout           = { nullptr, 0 }; // No input � uses SV_VertexID
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    psoDesc.NumRenderTargets      = 1;
    psoDesc.RTVFormats[0]         = DXGI_FORMAT_R11G11B10_FLOAT;
    psoDesc.DSVFormat             = DXGI_FORMAT_D24_UNORM_S8_UINT;
    psoDesc.SampleDesc            = { 1, 0 };
    psoDesc.SampleMask            = UINT_MAX;
    psoDesc.RasterizerState.FillMode              = D3D12_FILL_MODE_SOLID;
    psoDesc.RasterizerState.CullMode              = D3D12_CULL_MODE_NONE;
    psoDesc.RasterizerState.DepthClipEnable       = TRUE;

    // Depth off, stencil test: only pass where stencil == 0 (not shadowed)
    psoDesc.DepthStencilState.DepthEnable    = FALSE;
    psoDesc.DepthStencilState.StencilEnable  = TRUE;
    psoDesc.DepthStencilState.StencilReadMask  = 0xFF;
    psoDesc.DepthStencilState.StencilWriteMask = 0x00; // Don't modify stencil

    D3D12_DEPTH_STENCILOP_DESC stencilOp = {};
    stencilOp.StencilFailOp      = D3D12_STENCIL_OP_KEEP;
    stencilOp.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
    stencilOp.StencilPassOp      = D3D12_STENCIL_OP_KEEP;
    stencilOp.StencilFunc        = D3D12_COMPARISON_FUNC_EQUAL; // Pass where stencil == ref (0)

    psoDesc.DepthStencilState.FrontFace = stencilOp;
    psoDesc.DepthStencilState.BackFace  = stencilOp;

    // Additive blending
    auto& blend = psoDesc.BlendState.RenderTarget[0];
    blend.BlendEnable           = TRUE;
    blend.SrcBlend              = D3D12_BLEND_ONE;
    blend.DestBlend             = D3D12_BLEND_ONE;
    blend.BlendOp               = D3D12_BLEND_OP_ADD;
    blend.SrcBlendAlpha         = D3D12_BLEND_ONE;
    blend.DestBlendAlpha        = D3D12_BLEND_ONE;
    blend.BlendOpAlpha          = D3D12_BLEND_OP_ADD;
    blend.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

    return SUCCEEDED(device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_lightRadialPSO)));
}

bool PipelineStates::CreateCompositePSO(ID3D12Device* device, const std::wstring& shaderDir)
{
    auto vs = CompileShader(shaderDir + L"/CompositeVS.hlsl", "main", "vs_5_1");
    auto ps = CompileShader(shaderDir + L"/CompositePS.hlsl", "main", "ps_5_1");
    if (!vs || !ps) return false;

    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
    psoDesc.pRootSignature        = m_compositeRootSig.Get();
    psoDesc.VS                    = { vs->GetBufferPointer(), vs->GetBufferSize() };
    psoDesc.PS                    = { ps->GetBufferPointer(), ps->GetBufferSize() };
    psoDesc.InputLayout           = { nullptr, 0 }; // Fullscreen triangle � no vertex input
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    psoDesc.NumRenderTargets      = 1;
    psoDesc.RTVFormats[0]         = DXGI_FORMAT_R8G8B8A8_UNORM;
    psoDesc.SampleDesc            = { 1, 0 };
    psoDesc.SampleMask            = UINT_MAX;
    psoDesc.RasterizerState.FillMode              = D3D12_FILL_MODE_SOLID;
    psoDesc.RasterizerState.CullMode              = D3D12_CULL_MODE_NONE;
    psoDesc.RasterizerState.DepthClipEnable       = FALSE;
    psoDesc.DepthStencilState.DepthEnable         = FALSE;
    psoDesc.DepthStencilState.StencilEnable       = FALSE;

    // No blending � overwrite
    auto& blend = psoDesc.BlendState.RenderTarget[0];
    blend.BlendEnable           = FALSE;
    blend.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

    return SUCCEEDED(device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_compositePSO)));
}

bool PipelineStates::CreateGridOverlayPSO(ID3D12Device* device, const std::wstring& shaderDir)
{
    auto vs = CompileShader(shaderDir + L"/GridOverlayVS.hlsl", "main", "vs_5_1");
    auto ps = CompileShader(shaderDir + L"/GridOverlayPS.hlsl", "main", "ps_5_1");
    if (!vs || !ps) return false;

    D3D12_INPUT_ELEMENT_DESC inputLayout[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    };

    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
    psoDesc.pRootSignature        = m_mainRootSig.Get();
    psoDesc.VS                    = { vs->GetBufferPointer(), vs->GetBufferSize() };
    psoDesc.PS                    = { ps->GetBufferPointer(), ps->GetBufferSize() };
    psoDesc.InputLayout           = { inputLayout, _countof(inputLayout) };
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE;
    psoDesc.NumRenderTargets      = 1;
    psoDesc.RTVFormats[0]         = DXGI_FORMAT_R8G8B8A8_UNORM;
    psoDesc.SampleDesc            = { 1, 0 };
    psoDesc.SampleMask            = UINT_MAX;
    psoDesc.RasterizerState.FillMode              = D3D12_FILL_MODE_SOLID;
    psoDesc.RasterizerState.CullMode              = D3D12_CULL_MODE_NONE;
    psoDesc.RasterizerState.DepthClipEnable       = TRUE;
    psoDesc.RasterizerState.AntialiasedLineEnable = TRUE;
    psoDesc.DepthStencilState.DepthEnable         = FALSE;
    psoDesc.DepthStencilState.StencilEnable       = FALSE;

    // Alpha blending
    auto& blend = psoDesc.BlendState.RenderTarget[0];
    blend.BlendEnable           = TRUE;
    blend.SrcBlend              = D3D12_BLEND_SRC_ALPHA;
    blend.DestBlend             = D3D12_BLEND_INV_SRC_ALPHA;
    blend.BlendOp               = D3D12_BLEND_OP_ADD;
    blend.SrcBlendAlpha         = D3D12_BLEND_ONE;
    blend.DestBlendAlpha        = D3D12_BLEND_INV_SRC_ALPHA;
    blend.BlendOpAlpha          = D3D12_BLEND_OP_ADD;
    blend.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

    return SUCCEEDED(device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_gridOverlayPSO)));
}

bool PipelineStates::CreateGridOverlayScenePSO(ID3D12Device* device, const std::wstring& shaderDir)
{
    auto vs = CompileShader(shaderDir + L"/GridOverlayVS.hlsl", "main", "vs_5_1");
    auto ps = CompileShader(shaderDir + L"/GridOverlayPS.hlsl", "main", "ps_5_1");
    if (!vs || !ps) return false;

    D3D12_INPUT_ELEMENT_DESC inputLayout[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    };

    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
    psoDesc.pRootSignature        = m_mainRootSig.Get();
    psoDesc.VS                    = { vs->GetBufferPointer(), vs->GetBufferSize() };
    psoDesc.PS                    = { ps->GetBufferPointer(), ps->GetBufferSize() };
    psoDesc.InputLayout           = { inputLayout, _countof(inputLayout) };
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE;
    psoDesc.NumRenderTargets      = 1;
    psoDesc.RTVFormats[0]         = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    psoDesc.DSVFormat             = DXGI_FORMAT_D24_UNORM_S8_UINT;
    psoDesc.SampleDesc            = { 1, 0 };
    psoDesc.SampleMask            = UINT_MAX;
    psoDesc.RasterizerState.FillMode              = D3D12_FILL_MODE_SOLID;
    psoDesc.RasterizerState.CullMode              = D3D12_CULL_MODE_NONE;
    psoDesc.RasterizerState.DepthClipEnable       = TRUE;
    psoDesc.RasterizerState.AntialiasedLineEnable = TRUE;
    psoDesc.DepthStencilState.DepthEnable         = FALSE;
    psoDesc.DepthStencilState.StencilEnable       = FALSE;

    auto& blend = psoDesc.BlendState.RenderTarget[0];
    blend.BlendEnable           = TRUE;
    blend.SrcBlend              = D3D12_BLEND_SRC_ALPHA;
    blend.DestBlend             = D3D12_BLEND_INV_SRC_ALPHA;
    blend.BlendOp               = D3D12_BLEND_OP_ADD;
    blend.SrcBlendAlpha         = D3D12_BLEND_ONE;
    blend.DestBlendAlpha        = D3D12_BLEND_INV_SRC_ALPHA;
    blend.BlendOpAlpha          = D3D12_BLEND_OP_ADD;
    blend.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

    return SUCCEEDED(device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_gridOverlayScenePSO)));
}

// ===========================================================================
// Sampler Heap
// ===========================================================================

bool PipelineStates::CreateSamplerHeap(ID3D12Device* device)
{
    D3D12_DESCRIPTOR_HEAP_DESC desc = {};
    desc.Type           = D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;
    desc.NumDescriptors = 2;
    desc.Flags          = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

    HRESULT hr = device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&m_samplerHeap));
    if (FAILED(hr)) return false;

    UINT increment = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);
    D3D12_CPU_DESCRIPTOR_HANDLE handle = m_samplerHeap->GetCPUDescriptorHandleForHeapStart();

    // Sampler 0: point clamp
    D3D12_SAMPLER_DESC samplerDesc = {};
    samplerDesc.Filter   = D3D12_FILTER_MIN_MAG_MIP_POINT;
    samplerDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    samplerDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    samplerDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    device->CreateSampler(&samplerDesc, handle);

    // Sampler 1: linear clamp
    handle.ptr += increment;
    samplerDesc.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    device->CreateSampler(&samplerDesc, handle);

    return true;
}

} // namespace engine
