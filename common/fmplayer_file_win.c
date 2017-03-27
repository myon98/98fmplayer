#include "common/fmplayer_file.h"
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shlwapi.h>
#include <stdlib.h>
#include <wchar.h>

static void *fileread(const wchar_t *path,
                      size_t maxsize, size_t *filesize,
                      enum fmplayer_file_error *error) {
  HANDLE file = INVALID_HANDLE_VALUE;
  void *buf = 0;
  file = CreateFile(path, GENERIC_READ, 0, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
  if (file == INVALID_HANDLE_VALUE) {
    if (error) *error = FMPLAYER_FILE_ERR_FILEIO;
    goto err;
  }
  LARGE_INTEGER li;
  if (!GetFileSizeEx(file, &li)) {
    if (error) *error = FMPLAYER_FILE_ERR_FILEIO;
    goto err;
  }
  if (li.HighPart || (maxsize && (li.LowPart > maxsize))) {
    if (error) *error = FMPLAYER_FILE_ERR_BADFILE_SIZE;
    goto err;
  }
  buf = malloc(li.LowPart);
  if (!buf) {
    if (error) *error = FMPLAYER_FILE_ERR_NOMEM;
    goto err;
  }
  DWORD readlen;
  if (!ReadFile(file, buf, li.LowPart, &readlen, 0) || (readlen != li.LowPart)) {
    if (error) *error = FMPLAYER_FILE_ERR_FILEIO;
    goto err;
  }
  *filesize = li.QuadPart;
  CloseHandle(file);
  return buf;
err:
  free(buf);
  if (file != INVALID_HANDLE_VALUE) CloseHandle(file);
  return 0;
}

void *fmplayer_fileread(const void *pathptr, const char *pcmname, const char *extension,
                        size_t maxsize, size_t *filesize, enum fmplayer_file_error *error) {
  const wchar_t *path = (const wchar_t *)pathptr;
  wchar_t *wpcmpath = 0, *wpcmname = 0, *wpcmextname = 0;
  if (!pcmname) return fileread(path, maxsize, filesize, error);
  int wpcmnamelen = MultiByteToWideChar(932, 0, pcmname, -1, 0, 0);
  if (!wpcmnamelen) goto err;
  if (extension) {
    int wextensionlen = MultiByteToWideChar(932, 0, extension, -1, 0, 0);
    if (!wextensionlen) goto err;
    wpcmnamelen += wextensionlen;
    wpcmnamelen -= 1;
    wpcmextname = malloc(wextensionlen * sizeof(wchar_t));
    if (!wpcmextname) goto err;
    if (!MultiByteToWideChar(932, 0, extension, -1, wpcmextname, wextensionlen)) goto err;
  }
  wpcmname = malloc(wpcmnamelen * sizeof(wchar_t));
  if (!wpcmname) goto err;
  if (!MultiByteToWideChar(932, 0, pcmname, -1, wpcmname, wpcmnamelen)) goto err;
  if (wpcmextname) wcscat(wpcmname, wpcmextname);
  wpcmpath = malloc((wcslen(path) + 1 + wcslen(wpcmname) + 1) * sizeof(wchar_t));
  if (!wpcmpath) goto err;
  wcscpy(wpcmpath, path);
  PathRemoveFileSpec(wpcmpath);
  wcscat(wpcmpath, L"\\");
  wcscat(wpcmpath, wpcmname);
  void *buf = fileread(wpcmpath, maxsize, filesize, error);
  free(wpcmextname);
  free(wpcmname);
  free(wpcmpath);
  return buf;
err:
  free(wpcmextname);
  free(wpcmname);
  free(wpcmpath);
  return 0;
}

void *fmplayer_path_dup(const void *pathptr) {
  const wchar_t *path = (const wchar_t *)pathptr;
  return wcsdup(path);
}
