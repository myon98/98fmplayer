#include "pacc-win.h"
#include <d3d9.h>
#include <stdlib.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>

#include "hlsl/blit.vs.inc"
#include "hlsl/copy.ps.inc"
#include "hlsl/color.ps.inc"
#include "hlsl/color_trans.ps.inc"

struct pacc_ctx {
  int w, h;
  uint8_t pal[256*3];
  bool pal_changed;
  uint8_t clearpal;
  uint8_t color;
  uint8_t color_changed;
  bool render_enabled;
  enum pacc_mode curr_mode;
  atomic_bool killthread;
  HWND hwnd;
  HMODULE d3d9m;
  IDirect3D9 *d3d9;
  IDirect3DDevice9 *d3d9d;
  IDirect3DTexture9 *tex_pal;
  IDirect3DVertexDeclaration9 *vdecl;
  IDirect3DVertexShader9 *vs;
  IDirect3DPixelShader9 *ps_modes[pacc_mode_count];
  IDirect3DStateBlock9 *state_common;
  IDirect3DStateBlock9 *state_modes[pacc_mode_count];
  pacc_rendercb *rendercb;
  void *renderptr;
  HANDLE renderthread;
  HANDLE rendermtx;
  UINT msg_reset;
  HWND msg_wnd;
};

struct pacc_buf {
  IDirect3DVertexBuffer9 *buf_obj;
  float *buf;
  struct pacc_tex *tex;
  int len;
  int buflen;
  int bufobjlen;
  bool changed;
};

struct pacc_tex {
  int w, h;
  bool changed;
  atomic_flag flag;
  IDirect3DTexture9 *tex_obj;
  uint8_t *buf;
};

enum {
  PACC_BUF_DEF_LEN = 32,
  PRINTBUFLEN = 160,
};

static void pacc_buf_delete(struct pacc_buf *pb) {
  if (pb) {
    if (pb->buf_obj) pb->buf_obj->lpVtbl->Release(pb->buf_obj);
    free(pb->buf);
    free(pb);
  }
}

static struct pacc_buf *pacc_gen_buf(
    struct pacc_ctx *pc, struct pacc_tex *pt, enum pacc_buf_mode mode) {
  (void)mode;
  struct pacc_buf *pb = malloc(sizeof(*pb));
  if (!pb) goto err;
  *pb = (struct pacc_buf) {
    .buflen = PACC_BUF_DEF_LEN,
    .bufobjlen = PACC_BUF_DEF_LEN,
    .tex = pt,
  };
  pb->buf = malloc(sizeof(*pb->buf) * pb->buflen);
  if (!pb->buf) goto err;
  HRESULT res;
  res = pc->d3d9d->lpVtbl->CreateVertexBuffer(
      pc->d3d9d,
      sizeof(*pb->buf) * pb->buflen,
      D3DUSAGE_DYNAMIC,
      0,
      D3DPOOL_DEFAULT,
      &pb->buf_obj,
      0);
  if (res != D3D_OK) goto err;
  return pb;
err:
  pacc_buf_delete(pb);
  return 0;
}

static bool buf_reserve(struct pacc_buf *pb, int len) {
  if (pb->len + len > pb->buflen) {
    int newlen = pb->buflen;
    while (pb->len + len > newlen) newlen *= 2;
    float *newbuf = realloc(pb->buf, newlen * sizeof(pb->buf[0]));
    if (!newbuf) return false;
    pb->buflen = newlen;
    pb->buf = newbuf;
  }
  return true;
}

static void pacc_calc_scale(float *ret, int w, int h, int wdest, int hdest) {
  ret[0] = ((float)w) / wdest;
  ret[1] = ((float)h) / hdest;
}

static void pacc_calc_off_tex(float *ret,
                                     int tw, int th, int xsrc, int ysrc) {
  ret[0] = ((float)xsrc) / tw;
  ret[1] = ((float)ysrc) / th;
}

static void pacc_calc_off(
    float *ret, int xdest, int ydest, int w, int h, int wdest, int hdest) {
  ret[0] = ((float)(xdest * 2 + w - wdest)) / wdest;
  ret[1] = ((float)(ydest * 2 + h - hdest)) / hdest;
}

static void pacc_buf_rect_off(
    const struct pacc_ctx *pc, struct pacc_buf *pb,
    int x, int y, int w, int h, int xoff, int yoff) {
  float scale[2];
  float off[2];
  float tscale[2];
  float toff[2];
  pacc_calc_off(off, x, y, w, h, pc->w, pc->h);
  pacc_calc_scale(scale, w, h, pc->w, pc->h);
  pacc_calc_off_tex(toff, pb->tex->w, pb->tex->h, xoff, yoff);
  pacc_calc_scale(tscale, w, h, pb->tex->w, pb->tex->h);
  float coord[16] = {
    -1.0f * scale[0] + off[0], -1.0f * scale[1] - off[1],
     0.0f * tscale[0] + toff[0],  1.0f * tscale[1] + toff[1],

    -1.0f * scale[0] + off[0],  1.0f * scale[1] - off[1],
     0.0f * tscale[0] + toff[0],  0.0f * tscale[1] + toff[1],

     1.0f * scale[0] + off[0], -1.0f * scale[1] - off[1],
     1.0f * tscale[0] + toff[0],  1.0f * tscale[1] + toff[1],

     1.0f * scale[0] + off[0],  1.0f * scale[1] - off[1],
     1.0f * tscale[0] + toff[0],  0.0f * tscale[1] + toff[1],
  };
  if (!buf_reserve(pb, 24)) return;
  int indices[6] = {0, 1, 2, 2, 1, 3};
  for (int i = 0; i < 6; i++) {
    for (int j = 0; j < 4; j++) {
      pb->buf[pb->len+i*4+j] = coord[indices[i]*4+j];
    }
  }
  pb->len += 24;
  pb->changed = true;
}

static void pacc_buf_vprintf(
    const struct pacc_ctx *pc, struct pacc_buf *pb,
    int x, int y, const char *fmt, va_list ap) {
  uint8_t printbuf[PRINTBUFLEN+1];
  vsnprintf((char *)printbuf, sizeof(printbuf), fmt, ap);
  int len = strlen((const char *)printbuf);
  float scale[2];
  float off[2];
  int w = pb->tex->w / 256;
  int h = pb->tex->h;
  pacc_calc_scale(scale, w, h, pc->w, pc->h);
  pacc_calc_off(off, x, y, w, h, pc->w, pc->h);
  if (!buf_reserve(pb, len*24)) return;
  float *coords = pb->buf + pb->len;
  for (int i = 0; i < len; i++) {
    coords[24*i+0*4+0]                      = (-1.0f + 2.0f*i) * scale[0] + off[0];
    coords[24*i+0*4+1]                      = -1.0f * scale[1] - off[1];
    coords[24*i+1*4+0] = coords[24*i+4*4+0] = (-1.0f + 2.0f*i) * scale[0] + off[0];
    coords[24*i+1*4+1] = coords[24*i+4*4+1] = 1.0f * scale[1] - off[1];
    coords[24*i+2*4+0] = coords[24*i+3*4+0] = (1.0f + 2.0f*i) * scale[0] + off[0];
    coords[24*i+2*4+1] = coords[24*i+3*4+1] = -1.0f * scale[1] - off[1];
    coords[24*i+5*4+0]                      = (1.0f + 2.0f*i) * scale[0] + off[0];
    coords[24*i+5*4+1]                      = 1.0f * scale[1] - off[1];
    coords[24*i+0*4+2]                      = ((float)printbuf[i]) / 256.0f;
    coords[24*i+0*4+3]                      = 1.0f;
    coords[24*i+1*4+2] = coords[24*i+4*4+2] = ((float)printbuf[i]) / 256.0f;
    coords[24*i+1*4+3] = coords[24*i+4*4+3] = 0.0f;
    coords[24*i+2*4+2] = coords[24*i+3*4+2] = ((float)(printbuf[i]+1)) / 256.0f;
    coords[24*i+2*4+3] = coords[24*i+3*4+3] = 1.0f;
    coords[24*i+5*4+2]                      = ((float)(printbuf[i]+1)) / 256.0f;
    coords[24*i+5*4+3]                      = 0.0f;
  }
  pb->len += len * 24;
  pb->changed = true;
}

static void pacc_buf_printf(
    const struct pacc_ctx *pc, struct pacc_buf *pb,
    int x, int y, const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  pacc_buf_vprintf(pc, pb, x, y, fmt, ap);
  va_end(ap);
}

static void pacc_buf_rect(
    const struct pacc_ctx *pc, struct pacc_buf *pb,
    int x, int y, int w, int h) {
  pacc_buf_rect_off(pc, pb, x, y, w, h, 0, 0);
}

static void pacc_buf_clear(struct pacc_buf *pb) {
  pb->len = 0;
  pb->changed = true;
}

static void pacc_palette(struct pacc_ctx *pc, const uint8_t *rgb, int colors) {
  memcpy(pc->pal, rgb, colors*3);
  pc->pal_changed = true;
}

static void pacc_color(struct pacc_ctx *pc, uint8_t pal) {
  pc->color = pal;
  pc->color_changed = true;
}

static void pacc_begin_clear(struct pacc_ctx *pc) {
  if (pc->pal_changed) {
    D3DLOCKED_RECT lockrect;
    if (SUCCEEDED(pc->tex_pal->lpVtbl->LockRect(
            pc->tex_pal, 0, &lockrect, 0, D3DLOCK_DISCARD))) {
      uint32_t *palpixel = lockrect.pBits;
      for (int i = 0; i < 256; i++) {
        palpixel[i] =
          ((uint32_t)pc->pal[i*3+0]) << 16 |
          ((uint32_t)pc->pal[i*3+1]) << 8 |
          pc->pal[i*3+2];
      }
      pc->tex_pal->lpVtbl->UnlockRect(
          pc->tex_pal, 0);
    }
    pc->pal_changed = false;
  }
  pc->d3d9d->lpVtbl->Clear(
      pc->d3d9d,
      0, 0, D3DCLEAR_TARGET,
      D3DCOLOR_RGBA(
        pc->pal[0], pc->pal[1], pc->pal[2], 0xff
        ), 1.0, 0);
  pc->state_common->lpVtbl->Apply(pc->state_common);
  float offsets[4] = {
    -0.5f / pc->w, 0.5f / pc->h, 0.0f, 0.0f,
  };
  pc->d3d9d->lpVtbl->SetVertexShaderConstantF(
      pc->d3d9d,
      0, offsets, 1);
  pc->curr_mode = pacc_mode_count;
}

static void pacc_draw(struct pacc_ctx *pc, struct pacc_buf *pb, enum pacc_mode mode) {
  if (mode >= pacc_mode_count) return;
  if (pc->curr_mode != mode) {
    pc->state_modes[mode]->lpVtbl->Apply(pc->state_modes[mode]);
    pc->curr_mode = mode;
  }
  if (mode != pacc_mode_copy && pc->color_changed) {
    float fbuf[4] = {pc->color / 255.f};
    pc->d3d9d->lpVtbl->SetPixelShaderConstantF(pc->d3d9d, 0, fbuf, 1);
    pc->color_changed = false;
  }
  if (pb->tex->changed) {
    D3DLOCKED_RECT lockrect;
    if (SUCCEEDED(pb->tex->tex_obj->lpVtbl->LockRect(
            pb->tex->tex_obj, 0, &lockrect, 0, 0))) {
      while (atomic_flag_test_and_set_explicit(&pb->tex->flag, memory_order_acquire));
      for (int y = 0; y < pb->tex->h; y++) {
        memcpy(
            (char *)lockrect.pBits + lockrect.Pitch * y,
            pb->tex->buf + pb->tex->w * y,
            pb->tex->w);
      }
      atomic_flag_clear_explicit(&pb->tex->flag, memory_order_release);
      pb->tex->tex_obj->lpVtbl->UnlockRect(
          pb->tex->tex_obj, 0);
    }
    pb->tex->changed = false;
  }
  if (pb->changed) {
    if (pb->buflen > pb->bufobjlen) {
      IDirect3DVertexBuffer9 *newbufobj;
      HRESULT res;
      res = pc->d3d9d->lpVtbl->CreateVertexBuffer(
          pc->d3d9d,
          sizeof(*pb->buf) * pb->buflen,
          D3DUSAGE_DYNAMIC,
          0,
          D3DPOOL_DEFAULT,
          &newbufobj,
          0);
      if (res != D3D_OK) return;
      pb->buf_obj->lpVtbl->Release(pb->buf_obj);
      pb->buf_obj = newbufobj;
      pb->bufobjlen = pb->buflen;
    }
    float *objbuf;
    if (SUCCEEDED(pb->buf_obj->lpVtbl->Lock(
            pb->buf_obj,
            0, sizeof(float)*pb->len,
            (void **)&objbuf,
            D3DLOCK_DISCARD))) {
      memcpy(objbuf, pb->buf, pb->len*sizeof(pb->buf[0]));
      pb->buf_obj->lpVtbl->Unlock(pb->buf_obj);
    }
    pb->changed = false;
  }
  DWORD texaddru, texaddrv;
  if (pb->tex->w & (pb->tex->w - 1)) {
    texaddru = D3DTADDRESS_CLAMP;
  } else {
    texaddru = D3DTADDRESS_WRAP;
  }
  if (pb->tex->h & (pb->tex->h - 1)) {
    texaddrv = D3DTADDRESS_CLAMP;
  } else {
    texaddrv = D3DTADDRESS_WRAP;
  }
  pc->d3d9d->lpVtbl->SetSamplerState(
      pc->d3d9d, 1, D3DSAMP_ADDRESSU, texaddru);
  pc->d3d9d->lpVtbl->SetSamplerState(
      pc->d3d9d, 1, D3DSAMP_ADDRESSV, texaddrv);
  pc->d3d9d->lpVtbl->SetTexture(
      pc->d3d9d, 1, (IDirect3DBaseTexture9 *)pb->tex->tex_obj);
  pc->d3d9d->lpVtbl->SetStreamSource(
      pc->d3d9d,
      0,
      pb->buf_obj,
      0, 4*sizeof(float));
  pc->d3d9d->lpVtbl->DrawPrimitive(
      pc->d3d9d,
      D3DPT_TRIANGLELIST,
      0, pb->len/(3*4));
}

static uint8_t *pacc_tex_lock(struct pacc_tex *pt) {
  while (atomic_flag_test_and_set_explicit(&pt->flag, memory_order_acquire));
  return pt->buf;
}

static void pacc_tex_unlock(struct pacc_tex *pt) {
  pt->changed = true;
  atomic_flag_clear_explicit(&pt->flag, memory_order_release);
}

static void pacc_tex_delete(struct pacc_tex *pt) {
  if (pt) {
    if (pt->tex_obj) pt->tex_obj->lpVtbl->Release(pt->tex_obj);
    free(pt->buf);
    free(pt);
  }
}

static struct pacc_tex *pacc_gen_tex(struct pacc_ctx *pc, int w, int h) {
  struct pacc_tex *pt = malloc(sizeof(*pt));
  if (!pt) goto err;
  *pt = (struct pacc_tex) {
    .w = w,
    .h = h,
    .buf = calloc(w*h, 1),
  };
  if (!pt->buf) goto err;
  atomic_flag_clear_explicit(&pt->flag, memory_order_release);
  HRESULT res;
  res = pc->d3d9d->lpVtbl->CreateTexture(
      pc->d3d9d,
      w, h,
      1,
      D3DUSAGE_DYNAMIC,
      D3DFMT_L8,
      D3DPOOL_DEFAULT,
      &pt->tex_obj,
      0);
  if (res != D3D_OK) goto err;
  return pt;
err:
  pacc_tex_delete(pt);
  return 0;
}

static void pacc_delete(struct pacc_ctx *pc) {
  if (pc) {
    if (pc->renderthread) {
      atomic_store_explicit(&pc->killthread, true, memory_order_relaxed);
      WaitForSingleObject(pc->renderthread, INFINITE);
    }
    for (int i = 0; i < pacc_mode_count; i++) {
      if (pc->ps_modes[i]) pc->ps_modes[i]->lpVtbl->Release(pc->ps_modes[i]);
      if (pc->state_modes[i]) pc->state_modes[i]->lpVtbl->Release(pc->state_modes[i]);
    }
    if (pc->state_common) pc->state_common->lpVtbl->Release(pc->state_common);
    if (pc->vs) pc->vs->lpVtbl->Release(pc->vs);
    if (pc->vdecl) pc->vdecl->lpVtbl->Release(pc->vdecl);
    if (pc->tex_pal) pc->tex_pal->lpVtbl->Release(pc->tex_pal);
    if (pc->d3d9d) pc->d3d9d->lpVtbl->Release(pc->d3d9d);
    if (pc->d3d9) pc->d3d9->lpVtbl->Release(pc->d3d9);
    if (pc->d3d9m) FreeLibrary(pc->d3d9m);
    if (pc->rendermtx) CloseHandle(pc->rendermtx);
    free(pc);
  }
}

static DWORD WINAPI pacc_renderproc(void *ptr) {
  struct pacc_ctx *pc = ptr;
  bool waitdevice = false;
  while (!atomic_load_explicit(&pc->killthread, memory_order_relaxed)) {
    if (WaitForSingleObject(pc->rendermtx, INFINITE) == WAIT_OBJECT_0) {
      if (!waitdevice) {
        HRESULT r = pc->d3d9d->lpVtbl->TestCooperativeLevel(pc->d3d9d);
        if (r != D3D_OK) {
          waitdevice = true;
        } else {
          if (SUCCEEDED(pc->d3d9d->lpVtbl->BeginScene(pc->d3d9d))) {
            if (pc->render_enabled) {
              pc->rendercb(pc->renderptr);
            } else {
              pc->d3d9d->lpVtbl->Clear(
                  pc->d3d9d,
                  0, 0, D3DCLEAR_TARGET,
                  D3DCOLOR_RGBA(0x00, 0x00, 0x00, 0xff),
                  1.0, 0);
            }
            pc->d3d9d->lpVtbl->EndScene(pc->d3d9d);
            HRESULT res = pc->d3d9d->lpVtbl->Present(pc->d3d9d, 0, 0, 0, 0);
            if (res == D3DERR_DEVICELOST) {
              waitdevice = true;
            }
          }
        }
      } else {
        HRESULT r = pc->d3d9d->lpVtbl->TestCooperativeLevel(pc->d3d9d);
        if (r == D3DERR_DEVICENOTRESET) {
          PostMessage(pc->msg_wnd, pc->msg_reset, 0, 0);
          ReleaseMutex(pc->rendermtx);
          return 0;
        }
        Sleep(100);
      }
      ReleaseMutex(pc->rendermtx);
    }
  }
  return 0;
}

static void pacc_renderctrl(struct pacc_ctx *pc, bool enable) {
  while (WaitForSingleObject(pc->rendermtx, INFINITE) != WAIT_OBJECT_0);
  pc->render_enabled = enable;
  ReleaseMutex(pc->rendermtx);
}

static struct pacc_vtable pacc_d3d9_vtable = {
  .pacc_delete = pacc_delete,
  .gen_buf = pacc_gen_buf,
  .gen_tex = pacc_gen_tex,
  .buf_delete = pacc_buf_delete,
  .tex_lock = pacc_tex_lock,
  .tex_unlock = pacc_tex_unlock,
  .tex_delete = pacc_tex_delete,
  .buf_rect = pacc_buf_rect,
  .buf_rect_off = pacc_buf_rect_off,
  .buf_vprintf = pacc_buf_vprintf,
  .buf_printf = pacc_buf_printf,
  .buf_clear = pacc_buf_clear,
  .palette = pacc_palette,
  .color = pacc_color,
  .begin_clear = pacc_begin_clear,
  .draw = pacc_draw,
};

struct pacc_win_vtable pacc_win_vtable = {
  .renderctrl = pacc_renderctrl,
};

struct pacc_ctx *pacc_init_d3d9(
    HWND hwnd,
    int w, int h,
    pacc_rendercb *rendercb, void *renderptr,
    struct pacc_vtable *vt, struct pacc_win_vtable *winvt,
    UINT msg_reset, HWND msg_wnd) {
  struct pacc_ctx *pc = malloc(sizeof(*pc));
  if (!pc) goto err;
  *pc = (struct pacc_ctx) {
    .w = w,
    .h = h,
    .hwnd = hwnd,
    .rendercb = rendercb,
    .renderptr = renderptr,
    .msg_reset = msg_reset,
    .msg_wnd = msg_wnd,
  };
  atomic_init(&pc->killthread, false);
  pc->rendermtx = CreateMutex(0, FALSE, 0);
  if (!pc->rendermtx) goto err;
  pc->d3d9m = LoadLibraryW(L"d3d9");
  if (!pc->d3d9m) goto err;
  IDirect3D9 * WINAPI (*d3d9create)(UINT) = (IDirect3D9 * WINAPI (*)(UINT))GetProcAddress(pc->d3d9m, "Direct3DCreate9");
  if (!d3d9create) goto err;
  pc->d3d9 = d3d9create(D3D_SDK_VERSION);
  if (!pc->d3d9) goto err;

  D3DCAPS9 dcaps;
  HRESULT res;
  res = pc->d3d9->lpVtbl->GetDeviceCaps(
      pc->d3d9,
      D3DADAPTER_DEFAULT,
      D3DDEVTYPE_HAL,
      &dcaps);
  if (res != D3D_OK) goto err;

  D3DMULTISAMPLE_TYPE ms;
#ifdef PACC_DEBUG_MSAA
  ms = D3DMULTISAMPLE_8_SAMPLES;
#else
  ms = D3DMULTISAMPLE_NONE;
#endif
  D3DPRESENT_PARAMETERS dp = {
    .MultiSampleType = ms,
    .BackBufferWidth = pc->w * 2,
    .BackBufferHeight = pc->h * 2,
    .SwapEffect = D3DSWAPEFFECT_DISCARD,
    .hDeviceWindow = pc->hwnd,
    .Windowed = TRUE,
  };
  DWORD behavior = D3DCREATE_MULTITHREADED;
  /* from SDL_renderer_d3d.c
     fallback to software vertex processing when hardware vertex processing not available
   */
  behavior |= (dcaps.DevCaps & D3DDEVCAPS_HWTRANSFORMANDLIGHT) ?
    D3DCREATE_HARDWARE_VERTEXPROCESSING :
    D3DCREATE_SOFTWARE_VERTEXPROCESSING;
  res = pc->d3d9->lpVtbl->CreateDevice(
      pc->d3d9,
      D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL,
      0,
      behavior,
      &dp, &pc->d3d9d);
  if (res != D3D_OK) goto err;
  res = pc->d3d9d->lpVtbl->CreateTexture(
      pc->d3d9d,
      256, 1,
      1,
      D3DUSAGE_DYNAMIC,
      D3DFMT_X8R8G8B8,
      D3DPOOL_DEFAULT,
      &pc->tex_pal,
      0);
  if (res != D3D_OK) goto err;
  res = pc->d3d9d->lpVtbl->TestCooperativeLevel(pc->d3d9d);
  if (res != D3D_OK) goto err;

  D3DVERTEXELEMENT9 vertexdecls[] = {
    {
      .Stream = 0,
      .Offset = 0,
      .Type = D3DDECLTYPE_FLOAT4,
      .Method = D3DDECLMETHOD_DEFAULT,
      .Usage = D3DDECLUSAGE_POSITION,
      .UsageIndex = 0,
    },
    D3DDECL_END()
  };
  res = pc->d3d9d->lpVtbl->CreateVertexDeclaration(
      pc->d3d9d,
      vertexdecls,
      &pc->vdecl);
  if (res != D3D_OK) goto err;
  res = pc->d3d9d->lpVtbl->CreateVertexShader(
      pc->d3d9d,
      (const DWORD *)vs20_blit, &pc->vs);
  if (res != D3D_OK) goto err;
  res = pc->d3d9d->lpVtbl->CreatePixelShader(
      pc->d3d9d,
      (const DWORD *)ps20_copy, &pc->ps_modes[pacc_mode_copy]);
  if (res != D3D_OK) goto err;
  res = pc->d3d9d->lpVtbl->CreatePixelShader(
      pc->d3d9d,
      (const DWORD *)ps20_color, &pc->ps_modes[pacc_mode_color]);
  if (res != D3D_OK) goto err;
  res = pc->d3d9d->lpVtbl->CreatePixelShader(
      pc->d3d9d,
      (const DWORD *)ps20_color_trans, &pc->ps_modes[pacc_mode_color_trans]);
  if (res != D3D_OK) goto err;

  res = pc->d3d9d->lpVtbl->BeginStateBlock(pc->d3d9d);
  if (res != D3D_OK) goto err;
  res = pc->d3d9d->lpVtbl->SetVertexDeclaration(
      pc->d3d9d, pc->vdecl);
  if (res != D3D_OK) goto err;
  res = pc->d3d9d->lpVtbl->SetRenderState(
      pc->d3d9d, D3DRS_CULLMODE, D3DCULL_NONE);
  if (res != D3D_OK) goto err;
  res = pc->d3d9d->lpVtbl->SetVertexShader(
      pc->d3d9d, pc->vs);
  if (res != D3D_OK) goto err;
  res = pc->d3d9d->lpVtbl->SetSamplerState(
      pc->d3d9d, 0, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
  if (res != D3D_OK) goto err;
  res = pc->d3d9d->lpVtbl->SetSamplerState(
      pc->d3d9d, 0, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);
  if (res != D3D_OK) goto err;
  res = pc->d3d9d->lpVtbl->SetSamplerState(
      pc->d3d9d, 1, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
  if (res != D3D_OK) goto err;
  res = pc->d3d9d->lpVtbl->SetSamplerState(
      pc->d3d9d, 1, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);
  if (res != D3D_OK) goto err;
#ifdef PACC_DEBUG_MSAA
  res = pc->d3d9d->lpVtbl->SetSamplerState(
      pc->d3d9d, 0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
  if (res != D3D_OK) goto err;
#endif
  res = pc->d3d9d->lpVtbl->SetTexture(
      pc->d3d9d, 0, (IDirect3DBaseTexture9 *)pc->tex_pal);
  if (res != D3D_OK) goto err;
  res = pc->d3d9d->lpVtbl->EndStateBlock(pc->d3d9d, &pc->state_common);
  if (res != D3D_OK) goto err;

  res = pc->d3d9d->lpVtbl->BeginStateBlock(pc->d3d9d);
  if (res != D3D_OK) goto err;
  res = pc->d3d9d->lpVtbl->SetPixelShader(pc->d3d9d, pc->ps_modes[pacc_mode_copy]);
  if (res != D3D_OK) goto err;
  res = pc->d3d9d->lpVtbl->EndStateBlock(pc->d3d9d, &pc->state_modes[pacc_mode_copy]);
  if (res != D3D_OK) goto err;

  res = pc->d3d9d->lpVtbl->BeginStateBlock(pc->d3d9d);
  if (res != D3D_OK) goto err;
  res = pc->d3d9d->lpVtbl->SetPixelShader(pc->d3d9d, pc->ps_modes[pacc_mode_color]);
  if (res != D3D_OK) goto err;
  res = pc->d3d9d->lpVtbl->EndStateBlock(pc->d3d9d, &pc->state_modes[pacc_mode_color]);
  if (res != D3D_OK) goto err;

  res = pc->d3d9d->lpVtbl->BeginStateBlock(pc->d3d9d);
  if (res != D3D_OK) goto err;
  res = pc->d3d9d->lpVtbl->SetPixelShader(pc->d3d9d, pc->ps_modes[pacc_mode_color_trans]);
  if (res != D3D_OK) goto err;
  res = pc->d3d9d->lpVtbl->EndStateBlock(pc->d3d9d, &pc->state_modes[pacc_mode_color_trans]);
  if (res != D3D_OK) goto err;

  pc->renderthread = CreateThread(0, 0, pacc_renderproc, pc, 0, 0);
  if (!pc->renderthread) goto err;
  *vt = pacc_d3d9_vtable;
  *winvt = pacc_win_vtable;
  return pc;
err:
  pacc_delete(pc);
  return 0;
}
