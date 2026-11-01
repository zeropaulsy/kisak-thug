// Test hooks for unattended runs (from hit-and-run-linux's SHAR_SHOT_EVERY / SHAR_QUIT_AT):
//   KISAK_SHOT_EVERY=N  write the back buffer to $KISAK_SHOT_DIR/shot_<frame>.ppm every N frames (default dir: cwd)
//   KISAK_QUIT_AT=N     exit after N frames
//   KISAK_PAD_SCRIPT    "button@first-last,..." holds SDL_GameControllerButton numbers on a virtual gamepad for those
//                       frames (0 A, 1 B, 2 X, 3 Y, 6 Start, 11 up, 12 down, 13 left, 14 right)
#include <SDL.h>
#include <d3d9.h>
#include <stdio.h>
#include <stdlib.h>

static void write_shot(IDirect3DDevice9* device, unsigned frame)
{
	IDirect3DSurface9* back = NULL;
	IDirect3DSurface9* copy = NULL;
	if (FAILED(device->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &back))) return;
	D3DSURFACE_DESC d;
	back->GetDesc(&d);
	if (SUCCEEDED(device->CreateOffscreenPlainSurface(d.Width, d.Height, d.Format, D3DPOOL_SYSTEMMEM, &copy, NULL))
		&& SUCCEEDED(device->GetRenderTargetData(back, copy)))
	{
		D3DLOCKED_RECT r;
		if (SUCCEEDED(copy->LockRect(&r, NULL, D3DLOCK_READONLY)))
		{
			const char* dir = getenv("KISAK_SHOT_DIR");
			char name[512];
			snprintf(name, sizeof(name), "%s/shot_%06u.ppm", dir ? dir : ".", frame);
			if (FILE* f = fopen(name, "wb"))
			{
				fprintf(f, "P6\n%u %u\n255\n", d.Width, d.Height);
				for (UINT y = 0; y < d.Height; ++y)
				{
					const unsigned char* p = (const unsigned char*)r.pBits + y * r.Pitch;
					for (UINT x = 0; x < d.Width; ++x, p += 4)
					{
						unsigned char rgb[3] = { p[2], p[1], p[0] };
						fwrite(rgb, 1, 3, f);
					}
				}
				fclose(f);
			}
			copy->UnlockRect();
		}
	}
	if (copy) copy->Release();
	back->Release();
}

static SDL_Joystick* s_script_pad = NULL;
namespace SIO { // called from SIO::Manager::Manager
void kisak_attach_script_pad()
{
	if (!getenv("KISAK_PAD_SCRIPT")) return;
	int index = SDL_JoystickAttachVirtual(SDL_JOYSTICK_TYPE_GAMECONTROLLER, 6, 15, 1);
	if (index >= 0) s_script_pad = SDL_JoystickOpen(index);
	fprintf(stderr, "KISAK_PAD_SCRIPT: virtual pad index %d: %s\n", index, s_script_pad ? "open" : SDL_GetError());
}
}
static void run_pad_script(unsigned frame)
{
	if (!s_script_pad) return;
	Uint8 held[15] = {};
	const char* p = getenv("KISAK_PAD_SCRIPT");
	int button, consumed; unsigned first, last;
	while (sscanf(p, "%d@%u-%u%n", &button, &first, &last, &consumed) == 3)
	{
		if (button >= 0 && button < 15 && frame >= first && frame <= last) held[button] = 1;
		p += consumed;
		if (*p == ',') ++p;
	}
	for (int i = 0; i < 15; ++i) SDL_JoystickSetVirtualButton(s_script_pad, i, held[i]);
}

HRESULT kisak_present(IDirect3DDevice9* device, const RECT* a, const RECT* b, HWND c, const RGNDATA* d)
{
	static unsigned s_frame = 0;
	static const char* s_every = getenv("KISAK_SHOT_EVERY");
	static const char* s_quit = getenv("KISAK_QUIT_AT");
	++s_frame;
	run_pad_script(s_frame);
	if (s_every && s_frame % (unsigned)atoi(s_every) == 0) write_shot(device, s_frame);
	HRESULT hr = device->Present(a, b, c, d);
	if (s_quit && s_frame >= (unsigned)atoi(s_quit)) { fprintf(stderr, "KISAK_QUIT_AT: %u frames\n", s_frame); _Exit(0); }
	return hr;
}
