// The D3DX functions Gfx/DX9 calls: there is no d3dx9 library on Linux. Math matches D3DX's documented formulas.
#include <d3d9.h>
#include <d3dx9shader.h>
#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

D3DXMATRIX* WINAPI D3DXMatrixMultiply(D3DXMATRIX* out, const D3DXMATRIX* a, const D3DXMATRIX* b)
{
	D3DXMATRIX t;
	for (int r = 0; r < 4; ++r)
		for (int c = 0; c < 4; ++c)
			t.m[r][c] = a->m[r][0] * b->m[0][c] + a->m[r][1] * b->m[1][c] + a->m[r][2] * b->m[2][c] + a->m[r][3] * b->m[3][c];
	*out = t;
	return out;
}
D3DXMATRIX* WINAPI D3DXMatrixTranspose(D3DXMATRIX* out, const D3DXMATRIX* in)
{
	D3DXMATRIX t;
	for (int r = 0; r < 4; ++r)
		for (int c = 0; c < 4; ++c)
			t.m[r][c] = in->m[c][r];
	*out = t;
	return out;
}
D3DXMATRIX* WINAPI D3DXMatrixInverse(D3DXMATRIX* out, FLOAT* determinant, const D3DXMATRIX* in)
{
	// Gauss-Jordan with partial pivoting
	double a[4][8];
	for (int r = 0; r < 4; ++r)
		for (int c = 0; c < 4; ++c) { a[r][c] = in->m[r][c]; a[r][c + 4] = r == c; }
	double det = 1.0;
	for (int i = 0; i < 4; ++i)
	{
		int p = i;
		for (int r = i + 1; r < 4; ++r) if (fabs(a[r][i]) > fabs(a[p][i])) p = r;
		if (a[p][i] == 0.0) return NULL;
		if (p != i) { for (int c = 0; c < 8; ++c) { double t = a[p][c]; a[p][c] = a[i][c]; a[i][c] = t; } det = -det; }
		det *= a[i][i];
		double d = a[i][i];
		for (int c = 0; c < 8; ++c) a[i][c] /= d;
		for (int r = 0; r < 4; ++r)
		{
			if (r == i) continue;
			double f = a[r][i];
			for (int c = 0; c < 8; ++c) a[r][c] -= f * a[i][c];
		}
	}
	if (determinant) *determinant = (FLOAT)det;
	for (int r = 0; r < 4; ++r)
		for (int c = 0; c < 4; ++c)
			out->m[r][c] = (FLOAT)a[r][c + 4];
	return out;
}
D3DXMATRIX* WINAPI D3DXMatrixLookAtRH(D3DXMATRIX* out, const D3DXVECTOR3* eye, const D3DXVECTOR3* at, const D3DXVECTOR3* up)
{
	D3DXVECTOR3 z(eye->x - at->x, eye->y - at->y, eye->z - at->z), x, y;
	D3DXVec3Normalize(&z, &z);
	D3DXVec3Cross(&x, up, &z);
	D3DXVec3Normalize(&x, &x);
	D3DXVec3Cross(&y, &z, &x);
	D3DXMatrixIdentity(out);
	out->_11 = x.x; out->_12 = y.x; out->_13 = z.x;
	out->_21 = x.y; out->_22 = y.y; out->_23 = z.y;
	out->_31 = x.z; out->_32 = y.z; out->_33 = z.z;
	out->_41 = -D3DXVec3Dot(&x, eye); out->_42 = -D3DXVec3Dot(&y, eye); out->_43 = -D3DXVec3Dot(&z, eye);
	return out;
}
D3DXMATRIX* WINAPI D3DXMatrixOrthoRH(D3DXMATRIX* out, FLOAT w, FLOAT h, FLOAT zn, FLOAT zf)
{
	D3DXMatrixIdentity(out);
	out->_11 = 2.0f / w; out->_22 = 2.0f / h; out->_33 = 1.0f / (zn - zf); out->_43 = zn / (zn - zf);
	return out;
}
D3DXMATRIX* WINAPI D3DXMatrixPerspectiveRH(D3DXMATRIX* out, FLOAT w, FLOAT h, FLOAT zn, FLOAT zf)
{
	memset(out, 0, sizeof(*out));
	out->_11 = 2.0f * zn / w; out->_22 = 2.0f * zn / h; out->_33 = zf / (zn - zf); out->_34 = -1.0f; out->_43 = zn * zf / (zn - zf);
	return out;
}
D3DXMATRIX* WINAPI D3DXMatrixRotationY(D3DXMATRIX* out, FLOAT angle)
{
	D3DXMatrixIdentity(out);
	out->_11 = cosf(angle); out->_13 = -sinf(angle); out->_31 = sinf(angle); out->_33 = cosf(angle);
	return out;
}
D3DXMATRIX* WINAPI D3DXMatrixTranslation(D3DXMATRIX* out, FLOAT x, FLOAT y, FLOAT z)
{
	D3DXMatrixIdentity(out);
	out->_41 = x; out->_42 = y; out->_43 = z;
	return out;
}
D3DXVECTOR3* WINAPI D3DXVec3Normalize(D3DXVECTOR3* out, const D3DXVECTOR3* in)
{
	FLOAT l = sqrtf(in->x * in->x + in->y * in->y + in->z * in->z);
	if (l == 0.0f) { out->x = out->y = out->z = 0.0f; return out; }
	out->x = in->x / l; out->y = in->y / l; out->z = in->z / l;
	return out;
}
D3DXVECTOR4* WINAPI D3DXVec3Transform(D3DXVECTOR4* out, const D3DXVECTOR3* v, const D3DXMATRIX* m)
{
	D3DXVECTOR4 t;
	t.x = m->_11 * v->x + m->_21 * v->y + m->_31 * v->z + m->_41;
	t.y = m->_12 * v->x + m->_22 * v->y + m->_32 * v->z + m->_42;
	t.z = m->_13 * v->x + m->_23 * v->y + m->_33 * v->z + m->_43;
	t.w = m->_14 * v->x + m->_24 * v->y + m->_34 * v->z + m->_44;
	*out = t;
	return out;
}
D3DXVECTOR3* WINAPI D3DXVec3TransformCoord(D3DXVECTOR3* out, const D3DXVECTOR3* v, const D3DXMATRIX* m)
{
	D3DXVECTOR4 t;
	D3DXVec3Transform(&t, v, m);
	out->x = t.x / t.w; out->y = t.y / t.w; out->z = t.z / t.w;
	return out;
}
D3DXVECTOR3* WINAPI D3DXVec3TransformNormal(D3DXVECTOR3* out, const D3DXVECTOR3* v, const D3DXMATRIX* m)
{
	D3DXVECTOR3 t;
	t.x = m->_11 * v->x + m->_21 * v->y + m->_31 * v->z;
	t.y = m->_12 * v->x + m->_22 * v->y + m->_32 * v->z;
	t.z = m->_13 * v->x + m->_23 * v->y + m->_33 * v->z;
	*out = t;
	return out;
}
D3DXVECTOR4* WINAPI D3DXVec4Transform(D3DXVECTOR4* out, const D3DXVECTOR4* v, const D3DXMATRIX* m)
{
	D3DXVECTOR4 t;
	t.x = m->_11 * v->x + m->_21 * v->y + m->_31 * v->z + m->_41 * v->w;
	t.y = m->_12 * v->x + m->_22 * v->y + m->_32 * v->z + m->_42 * v->w;
	t.z = m->_13 * v->x + m->_23 * v->y + m->_33 * v->z + m->_43 * v->w;
	t.w = m->_14 * v->x + m->_24 * v->y + m->_34 * v->z + m->_44 * v->w;
	*out = t;
	return out;
}

HRESULT D3DXCreateTexture(IDirect3DDevice9* device, UINT width, UINT height, UINT levels, DWORD usage, D3DFORMAT format, D3DPOOL pool, IDirect3DTexture9** texture)
{
	return device->CreateTexture(width, height, levels, usage, format, pool, texture, NULL);
}
HRESULT D3DXLoadSurfaceFromSurface(IDirect3DSurface9* dst, const PALETTEENTRY*, const RECT* dst_rect, IDirect3DSurface9* src, const PALETTEENTRY*, const RECT* src_rect, DWORD, D3DCOLOR)
{
	// The one caller copies a whole A8R8G8B8-style surface of identical size and format.
	D3DSURFACE_DESC sd, dd;
	src->GetDesc(&sd); dst->GetDesc(&dd);
	if (dst_rect || src_rect || sd.Width != dd.Width || sd.Height != dd.Height || sd.Format != dd.Format) return D3DERR_INVALIDCALL;
	D3DLOCKED_RECT s, d;
	if (FAILED(src->LockRect(&s, NULL, D3DLOCK_READONLY))) return D3DERR_INVALIDCALL;
	if (FAILED(dst->LockRect(&d, NULL, 0))) { src->UnlockRect(); return D3DERR_INVALIDCALL; }
	INT row = s.Pitch < d.Pitch ? s.Pitch : d.Pitch;
	for (UINT y = 0; y < sd.Height; ++y)
		memcpy((char*)d.pBits + y * d.Pitch, (const char*)s.pBits + y * s.Pitch, row);
	dst->UnlockRect(); src->UnlockRect();
	return D3D_OK;
}

// ---- ps.1.x assembler ----------------------------------------------------------------------------------------------
namespace {
class Buffer : public ID3DXBuffer
{
	ULONG m_refs; DWORD m_size; void* m_data;
public:
	Buffer(const void* data, DWORD size) : m_refs(1), m_size(size), m_data(malloc(size)) { memcpy(m_data, data, size); }
	virtual ~Buffer() { free(m_data); }
	HRESULT WINAPI QueryInterface(REFIID, void** out) { *out = this; AddRef(); return S_OK; }
	ULONG WINAPI AddRef() { return ++m_refs; }
	ULONG WINAPI Release() { ULONG r = --m_refs; if (!r) delete this; return r; }
	void* WINAPI GetBufferPointer() { return m_data; }
	DWORD WINAPI GetBufferSize() { return m_size; }
};

struct Op { const char* name; DWORD code; int srcs; };
const Op OPS[] = { {"nop",0,-1}, {"mov",1,1}, {"add",2,2}, {"sub",3,2}, {"mad",4,3}, {"mul",5,2}, {"dp3",8,2}, {"dp4",9,2},
	{"lrp",18,3}, {"cnd",80,3}, {"cmp",88,3}, {"tex",66,0}, {"texcoord",64,0} };

const char* skip_space(const char* s) { while (*s == ' ' || *s == '\t' || *s == '\r') ++s; return s; }

// r0, v0, c0, t0 -> register type + number bits, shared by destination and source tokens.
bool parse_register(const char*& s, DWORD& bits)
{
	DWORD type;
	switch (tolower((unsigned char)*s))
	{
	case 'r': type = D3DSPR_TEMP; break;
	case 'v': type = D3DSPR_INPUT; break;
	case 'c': type = D3DSPR_CONST; break;
	case 't': type = D3DSPR_TEXTURE; break;
	default: return false;
	}
	++s;
	if (!isdigit((unsigned char)*s)) return false;
	DWORD n = (DWORD)strtoul(s, (char**)&s, 10);
	bits = 0x80000000u | n | ((type << D3DSP_REGTYPE_SHIFT) & D3DSP_REGTYPE_MASK);
	return true;
}
int component(char c)
{
	switch (tolower((unsigned char)c)) { case 'r': case 'x': return 0; case 'g': case 'y': return 1; case 'b': case 'z': return 2; case 'a': case 'w': return 3; }
	return -1;
}
bool parse_dest(const char*& s, DWORD mods, DWORD& token)
{
	if (!parse_register(s, token)) return false;
	DWORD mask = 0xF;
	if (*s == '.')
	{
		++s; mask = 0;
		for (int c; (c = component(*s)) >= 0; ++s) mask |= 1u << c;
	}
	token |= (mask << 16) | mods;
	return true;
}
bool parse_source(const char*& s, DWORD& token)
{
	s = skip_space(s);
	DWORD mod = D3DSPSM_NONE;
	bool comp = false, neg = false;
	if (s[0] == '1' && s[1] == '-') { comp = true; s += 2; }
	else if (*s == '-') { neg = true; ++s; }
	if (!parse_register(s, token)) return false;
	if (*s == '_')
	{
		if (!strncasecmp(s, "_bias", 5)) { mod = neg ? D3DSPSM_BIASNEG : D3DSPSM_BIAS; s += 5; }
		else if (!strncasecmp(s, "_bx2", 4)) { mod = neg ? D3DSPSM_SIGNNEG : D3DSPSM_SIGN; s += 4; }
		else if (!strncasecmp(s, "_x2", 3)) { mod = neg ? D3DSPSM_X2NEG : D3DSPSM_X2; s += 3; }
		else return false;
	}
	else if (comp) mod = D3DSPSM_COMP;
	else if (neg) mod = D3DSPSM_NEG;
	DWORD swizzle = D3DSP_NOSWIZZLE;
	if (*s == '.')
	{
		++s;
		int c[4], n = 0;
		for (int v; n < 4 && (v = component(*s)) >= 0; ++s) c[n++] = v;
		// One component replicates. ".rgb" reads as no swizzle.
		if (n == 1) swizzle = (c[0] | c[0] << 2 | c[0] << 4 | c[0] << 6) << D3DVS_SWIZZLE_SHIFT;
		else if (n == 4) swizzle = (c[0] | c[1] << 2 | c[2] << 4 | c[3] << 6) << D3DVS_SWIZZLE_SHIFT;
	}
	token |= swizzle | mod;
	return true;
}
HRESULT fail(ID3DXBuffer** err, const char* line)
{
	char msg[256];
	snprintf(msg, sizeof(msg), "ps assembler: cannot parse: %.200s", line);
	fprintf(stderr, "%s\n", msg);
	if (err) *err = new Buffer(msg, (DWORD)strlen(msg) + 1);
	return D3DERR_INVALIDCALL;
}
}

HRESULT D3DXAssembleShader(const char* data, UINT len, const void*, void*, DWORD, ID3DXBuffer** shader, ID3DXBuffer** err)
{
	if (err) *err = NULL;
	DWORD code[512];
	int n = 0;
	const char* p = data;
	const char* end = data + len;
	while (p < end)
	{
		const char* eol = (const char*)memchr(p, '\n', end - p);
		if (!eol) eol = end;
		char line[256];
		size_t ll = (size_t)(eol - p);
		if (ll >= sizeof(line)) ll = sizeof(line) - 1;
		memcpy(line, p, ll); line[ll] = 0;
		p = eol < end ? eol + 1 : end;
		for (char* c = line; *c; ++c) if (*c == ';' || (c[0] == '/' && c[1] == '/')) { *c = 0; break; }
		const char* s = skip_space(line);
		if (!*s) continue;

		if (!strncasecmp(s, "ps.", 3) || !strncasecmp(s, "ps_", 3))
		{
			int major = s[3] - '0', minor = s[5] - '0';
			if (major != 1 || minor < 0 || minor > 4) return fail(err, line);
			code[n++] = D3DPS_VERSION(major, minor);
			continue;
		}
		bool coissue = false;
		if (*s == '+') { coissue = true; s = skip_space(s + 1); }

		char word[32]; int w = 0;
		while ((isalnum((unsigned char)*s) || *s == '_') && w < 31) word[w++] = (char)tolower((unsigned char)*s++);
		word[w] = 0;

		DWORD mods = 0;
		char* mod = strchr(word, '_');
		if (mod)
		{
			*mod++ = 0;
			while (mod && *mod)
			{
				char* next = strchr(mod, '_');
				if (next) *next++ = 0;
				if (!strcmp(mod, "x2")) mods |= 1u << D3DSP_DSTSHIFT_SHIFT;
				else if (!strcmp(mod, "x4")) mods |= 2u << D3DSP_DSTSHIFT_SHIFT;
				else if (!strcmp(mod, "d2")) mods |= 15u << D3DSP_DSTSHIFT_SHIFT;
				else if (!strcmp(mod, "sat")) mods |= D3DSPDM_SATURATE;
				else return fail(err, line);
				mod = next;
			}
		}

		const Op* op = NULL;
		for (size_t i = 0; i < sizeof(OPS) / sizeof(OPS[0]); ++i) if (!strcmp(word, OPS[i].name)) op = &OPS[i];
		if (!op || n + 6 >= 511) return fail(err, line);
		code[n++] = op->code | (coissue ? D3DSI_COISSUE : 0);
		if (op->srcs < 0) continue;

		s = skip_space(s);
		DWORD token;
		if (!parse_dest(s, mods, token)) return fail(err, line);
		code[n++] = token;
		for (int i = 0; i < op->srcs; ++i)
		{
			s = skip_space(s);
			if (*s != ',') return fail(err, line);
			++s;
			if (!parse_source(s, token)) return fail(err, line);
			code[n++] = token;
		}
	}
	code[n++] = D3DPS_END();
	if (shader) *shader = new Buffer(code, n * sizeof(DWORD));
	return D3D_OK;
}
