/* Linux stand-in for the D3DX header Core/Defines.h includes: only what Gfx/DX9 uses, implemented in d3dx_compat.cpp.
   d3dx9math.h is DXVK's copy (MinGW); there is no d3dx9 library to link on Linux. */
#ifndef KISAK_D3DX9SHADER_H
#define KISAK_D3DX9SHADER_H
#include <d3d9.h>
#define __D3DX9_H__ /* d3dx9math.h pulls in d3dx9.h, whose other headers need GDI types */
#include <limits.h>
#define D3DX_DEFAULT ((UINT)-1)
#define D3DX_FILTER_NONE (1 << 0)
#define D3DX_FILTER_POINT (2 << 0)
#define D3DX_FILTER_LINEAR (3 << 0)
#include <d3dx9math.h>

struct ID3DXBuffer : public IUnknown
{
	virtual void* WINAPI GetBufferPointer() = 0;
	virtual DWORD WINAPI GetBufferSize() = 0;
};
typedef ID3DXBuffer* LPD3DXBUFFER;

/* Assembles the ps.1.x subset the game generates; defines/include/flags are unused by every caller. */
HRESULT D3DXAssembleShader(const char* data, UINT data_len, const void* defines, void* include, DWORD flags,
	ID3DXBuffer** shader, ID3DXBuffer** error_messages);
HRESULT D3DXLoadSurfaceFromSurface(IDirect3DSurface9* dst, const PALETTEENTRY* dst_palette, const RECT* dst_rect,
	IDirect3DSurface9* src, const PALETTEENTRY* src_palette, const RECT* src_rect, DWORD filter, D3DCOLOR color_key);
HRESULT D3DXCreateTexture(IDirect3DDevice9* device, UINT width, UINT height, UINT levels, DWORD usage, D3DFORMAT format,
	D3DPOOL pool, IDirect3DTexture9** texture);
#endif
