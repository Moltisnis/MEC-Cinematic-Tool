#pragma once
#include <d3d11.h>

// The context's state the pass changes, put back after it (as the menu's
// own drawing does, and the geometry, hull and domain shaders too)
struct D3DStateBackup
{
  UINT ScissorCount, ViewportCount;
  D3D11_RECT Scissors[D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE];
  D3D11_VIEWPORT Viewports[D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE];
  ID3D11RasterizerState* Rasterizer;
  ID3D11BlendState* Blend;
  FLOAT BlendFactor[4];
  UINT SampleMask;
  ID3D11DepthStencilState* Depth;
  UINT StencilRef;
  ID3D11ShaderResourceView* PsResource;
  ID3D11SamplerState* PsSampler;
  ID3D11Buffer* PsConstants;
  ID3D11Buffer* VsConstants;
  ID3D11PixelShader* Ps;
  ID3D11VertexShader* Vs;
  ID3D11GeometryShader* Gs;
  ID3D11HullShader* Hs;
  ID3D11DomainShader* Ds;
  UINT PsInstanceCount, VsInstanceCount, GsInstanceCount, HsInstanceCount, DsInstanceCount;
  ID3D11ClassInstance* PsInstances[256];
  ID3D11ClassInstance* VsInstances[256];
  ID3D11ClassInstance* GsInstances[256];
  ID3D11ClassInstance* HsInstances[256];
  ID3D11ClassInstance* DsInstances[256];
  D3D11_PRIMITIVE_TOPOLOGY Topology;
  ID3D11Buffer* IndexBuffer;
  DXGI_FORMAT IndexFormat;
  UINT IndexOffset;
  ID3D11Buffer* VertexBuffer;
  UINT VertexStride, VertexOffset;
  ID3D11InputLayout* Layout;
  ID3D11RenderTargetView* Targets[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT];
  ID3D11DepthStencilView* DepthTarget;

  void Save(ID3D11DeviceContext* c)
  {
    ScissorCount = ViewportCount = D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE;
    c->RSGetScissorRects(&ScissorCount, Scissors);
    c->RSGetViewports(&ViewportCount, Viewports);
    c->RSGetState(&Rasterizer);
    c->OMGetBlendState(&Blend, BlendFactor, &SampleMask);
    c->OMGetDepthStencilState(&Depth, &StencilRef);
    c->PSGetShaderResources(0, 1, &PsResource);
    c->PSGetSamplers(0, 1, &PsSampler);
    c->PSGetConstantBuffers(0, 1, &PsConstants);
    c->VSGetConstantBuffers(0, 1, &VsConstants);
    PsInstanceCount = VsInstanceCount = GsInstanceCount = HsInstanceCount = DsInstanceCount = 256;
    c->PSGetShader(&Ps, PsInstances, &PsInstanceCount);
    c->VSGetShader(&Vs, VsInstances, &VsInstanceCount);
    c->GSGetShader(&Gs, GsInstances, &GsInstanceCount);
    c->HSGetShader(&Hs, HsInstances, &HsInstanceCount);
    c->DSGetShader(&Ds, DsInstances, &DsInstanceCount);
    c->IAGetPrimitiveTopology(&Topology);
    c->IAGetIndexBuffer(&IndexBuffer, &IndexFormat, &IndexOffset);
    c->IAGetVertexBuffers(0, 1, &VertexBuffer, &VertexStride, &VertexOffset);
    c->IAGetInputLayout(&Layout);
    c->OMGetRenderTargets(D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT, Targets, &DepthTarget);
  }

  void Restore(ID3D11DeviceContext* c)
  {
    c->RSSetScissorRects(ScissorCount, Scissors);
    c->RSSetViewports(ViewportCount, Viewports);
    c->RSSetState(Rasterizer);
    c->OMSetBlendState(Blend, BlendFactor, SampleMask);
    c->OMSetDepthStencilState(Depth, StencilRef);
    c->PSSetShaderResources(0, 1, &PsResource);
    c->PSSetSamplers(0, 1, &PsSampler);
    c->PSSetConstantBuffers(0, 1, &PsConstants);
    c->VSSetConstantBuffers(0, 1, &VsConstants);
    c->PSSetShader(Ps, PsInstances, PsInstanceCount);
    c->VSSetShader(Vs, VsInstances, VsInstanceCount);
    c->GSSetShader(Gs, GsInstances, GsInstanceCount);
    c->HSSetShader(Hs, HsInstances, HsInstanceCount);
    c->DSSetShader(Ds, DsInstances, DsInstanceCount);
    c->IASetPrimitiveTopology(Topology);
    c->IASetIndexBuffer(IndexBuffer, IndexFormat, IndexOffset);
    c->IASetVertexBuffers(0, 1, &VertexBuffer, &VertexStride, &VertexOffset);
    c->IASetInputLayout(Layout);
    c->OMSetRenderTargets(D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT, Targets, DepthTarget);
    // What the Get calls added
    auto release = [](IUnknown* p) { if (p) p->Release(); };
    release(Rasterizer);
    release(Blend);
    release(Depth);
    release(PsResource);
    release(PsSampler);
    release(PsConstants);
    release(VsConstants);
    release(Ps);
    release(Vs);
    release(Gs);
    release(Hs);
    release(Ds);
    for (UINT i = 0; i < PsInstanceCount; ++i) release(PsInstances[i]);
    for (UINT i = 0; i < VsInstanceCount; ++i) release(VsInstances[i]);
    for (UINT i = 0; i < GsInstanceCount; ++i) release(GsInstances[i]);
    for (UINT i = 0; i < HsInstanceCount; ++i) release(HsInstances[i]);
    for (UINT i = 0; i < DsInstanceCount; ++i) release(DsInstances[i]);
    release(IndexBuffer);
    release(VertexBuffer);
    release(Layout);
    for (ID3D11RenderTargetView* target : Targets) release(target);
    release(DepthTarget);
  }
};
