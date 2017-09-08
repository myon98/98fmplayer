#ifndef MYON_FMDSP_PACC_H_INCLUDED
#define MYON_FMDSP_PACC_H_INCLUDED

struct fmdsp_pacc;
struct pacc_ctx;
struct pacc_vtable;
struct fmdriver_work work;
struct opna opna;

struct fmdsp_pacc *fmdsp_pacc_init(
    struct pacc_ctx *pc, const struct pacc_vtable *vtable);
void fmdsp_pacc_set(struct fmdsp_pacc *pacc, struct fmdriver_work *work, struct opna *opna);
void fmdsp_pacc_release(struct fmdsp_pacc *fp);
void fmdsp_pacc_render(struct fmdsp_pacc *fp);

#endif // MYON_FMDSP_PACC_H_INCLUDED
