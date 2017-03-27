#include "common/fmplayer_file.h"
#include <string.h>
#include <stdlib.h>

void fmplayer_file_free(const struct fmplayer_file *fmfileptr) {
  struct fmplayer_file *fmfile = (struct fmplayer_file *)fmfileptr;
  if (!fmfile) return;
  free(fmfile->path);
  free(fmfile->buf);
  free(fmfile->ppzbuf[0]);
  free(fmfile->ppzbuf[1]);
  free(fmfile);
}

struct fmplayer_file *fmplayer_file_alloc(const void *path, enum fmplayer_file_error *error) {
  struct fmplayer_file *fmfile = calloc(1, sizeof(*fmfile));
  if (!fmfile) {
    if (error) *error = FMPLAYER_FILE_ERR_NOMEM;
    goto err;
  }
  fmfile->path = fmplayer_path_dup(path);
  if (!fmfile->path) {
    if (error) *error = FMPLAYER_FILE_ERR_NOMEM;
    goto err;
  }
  size_t filesize;
  fmfile->buf = fmplayer_fileread(path, 0, 0, 0xffff, &filesize, error);
  if (!fmfile->buf) goto err;
  if (pmd_load(&fmfile->driver.pmd, fmfile->buf, filesize)) {
    fmfile->type = FMPLAYER_FILE_TYPE_PMD;
    return fmfile;
  }
  memset(&fmfile->driver, 0, sizeof(fmfile->driver));
  if (fmp_load(&fmfile->driver.fmp, fmfile->buf, filesize)) {
    fmfile->type = FMPLAYER_FILE_TYPE_FMP;
    return fmfile;
  }
  if (error) *error = FMPLAYER_FILE_ERR_BADFILE;
err:
  fmplayer_file_free(fmfile);
  return 0;
}

static void loadppc(struct fmdriver_work *work, struct fmplayer_file *fmfile) {
  if (!strlen(fmfile->driver.pmd.ppcfile)) return;
  size_t filesize;
  void *ppcbuf = fmplayer_fileread(fmfile->path, fmfile->driver.pmd.ppcfile, ".PPC", 0, &filesize, 0);
  if (ppcbuf) {
    fmfile->pmd_ppc_err = !pmd_ppc_load(work, ppcbuf, filesize);
    free(ppcbuf);
  } else {
    fmfile->pmd_ppc_err = true;
  }
}

static bool loadppzpvi(struct fmdriver_work *work, struct fmplayer_file *fmfile, const char *name) {
  size_t filesize;
  void *pvibuf = 0, *ppzbuf = 0;
  pvibuf = fmplayer_fileread(fmfile->path, name, ".PVI", 0, &filesize, 0);
  if (!pvibuf) goto err;
  ppzbuf = calloc(ppz8_pvi_decodebuf_samples(filesize), 2);
  if (!ppzbuf) goto err;
  if (!ppz8_pvi_load(work->ppz8, 0, pvibuf, filesize, ppzbuf)) goto err;
  free(pvibuf);
  free(fmfile->ppzbuf[0]);
  fmfile->ppzbuf[0] = ppzbuf;
  return true;
err:
  free(ppzbuf);
  free(pvibuf);
  return false;
}

static bool loadppzpzi(struct fmdriver_work *work, struct fmplayer_file *fmfile, const char *name) {
  size_t filesize;
  void *pzibuf = 0, *ppzbuf = 0;
  pzibuf = fmplayer_fileread(fmfile->path, name, ".PZI", 0, &filesize, 0);
  if (!pzibuf) goto err;
  ppzbuf = calloc(ppz8_pzi_decodebuf_samples(filesize), 2);
  if (!ppzbuf) goto err;
  if (!ppz8_pzi_load(work->ppz8, 0, pzibuf, filesize, ppzbuf)) goto err;
  free(pzibuf);
  free(fmfile->ppzbuf[0]);
  fmfile->ppzbuf[0] = ppzbuf;
  return true;
err:
  free(ppzbuf);
  free(pzibuf);
  return false;
}

static void loadpmdppz(struct fmdriver_work *work, struct fmplayer_file *fmfile) {
  const char *ppzfile = fmfile->driver.pmd.ppzfile;
  if (!strlen(ppzfile)) return;
  if (!loadppzpvi(work, fmfile, ppzfile) && !loadppzpzi(work, fmfile, ppzfile)) {
    fmfile->pmd_ppz_err = true;
  }
}

static void loadpvi(struct fmdriver_work *work, struct fmplayer_file *fmfile) {
  const char *pvifile = fmfile->driver.fmp.pvi_name;
  if (!strlen(pvifile)) return;
  size_t filesize;
  void *pvibuf = fmplayer_fileread(fmfile->path, pvifile, ".PVI", 0, &filesize, 0);
  if (pvibuf) {
    fmfile->fmp_pvi_err = !fmp_adpcm_load(work, pvibuf, filesize);
    free(pvibuf);
  } else {
    fmfile->fmp_pvi_err = true;
  }
}

static void loadfmpppz(struct fmdriver_work *work, struct fmplayer_file *fmfile) {
  const char *pvifile = fmfile->driver.fmp.ppz_name;
  if (!strlen(pvifile)) return;
  fmfile->fmp_ppz_err = !loadppzpvi(work, fmfile, pvifile);
}

void fmplayer_file_load(struct fmdriver_work *work, struct fmplayer_file *fmfile) {
  switch (fmfile->type) {
  case FMPLAYER_FILE_TYPE_PMD:
    pmd_init(work, &fmfile->driver.pmd);
    loadppc(work, fmfile);
    loadpmdppz(work, fmfile);
    work->pcmerror[0] = fmfile->pmd_ppc_err;
    work->pcmerror[1] = fmfile->pmd_ppz_err;
    break;
  case FMPLAYER_FILE_TYPE_FMP:
    fmp_init(work, &fmfile->driver.fmp);
    loadpvi(work, fmfile);
    loadfmpppz(work, fmfile);
    work->pcmerror[0] = fmfile->fmp_pvi_err;
    work->pcmerror[1] = fmfile->fmp_ppz_err;
    break;
  }
}
#define MSG_FILE_ERR_UNKNOWN "Unknown error"
#define MSG_FILE_ERR_NOMEM "Memory allocation error"
#define MSG_FILE_ERR_FILEIO "File I/O error"
#define MSG_FILE_ERR_BADFILE_SIZE "Invalid file size"
#define MSG_FILE_ERR_BADFILE "Invalid file format"

#define XWIDE(x) L ## x
#define WIDE(x) XWIDE(x)

const char *fmplayer_file_strerror(enum fmplayer_file_error error) {
  if (error >= FMPLAYER_FILE_ERR_COUNT) return MSG_FILE_ERR_UNKNOWN;
  static const char *errtable[] = {
    "",
    MSG_FILE_ERR_NOMEM,
    MSG_FILE_ERR_FILEIO,
    MSG_FILE_ERR_BADFILE_SIZE,
    MSG_FILE_ERR_BADFILE,
  };
  return errtable[error];
}

const wchar_t *fmplayer_file_strerror_w(enum fmplayer_file_error error) {
  if (error >= FMPLAYER_FILE_ERR_COUNT) return WIDE(MSG_FILE_ERR_UNKNOWN);
  static const wchar_t *errtable[] = {
    L"",
    WIDE(MSG_FILE_ERR_NOMEM),
    WIDE(MSG_FILE_ERR_FILEIO),
    WIDE(MSG_FILE_ERR_BADFILE_SIZE),
    WIDE(MSG_FILE_ERR_BADFILE),
  };
  return errtable[error];
}
