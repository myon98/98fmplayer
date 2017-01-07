#include <string.h>
#include <windows.h>

int memcmp(const void *s1, const void *s2, size_t n) {
  size_t i = RtlCompareMemory(s1, s2, n);
  if (i == n) return 0;
  return ((const unsigned char *)s1)[i] - ((const unsigned char *)s2)[i];
}

void *memset(void *s, int c, size_t n) {
  RtlFillMemory(s, n, c);
  return s;
}

void *memcpy(void *dest, const void *src, size_t n) {
  RtlCopyMemory(dest, src, n);
  return dest;
}

void *memmove(void *dest, const void *src, size_t n) {
  RtlMoveMemory(dest, src, n);
  return dest;
}

int CALLBACK wWinMain(HINSTANCE hinst, HINSTANCE hpinst,
                      wchar_t *cmdline, int cmdshow);

DWORD CALLBACK entry(void *ptr) {
  (void)ptr;
  STARTUPINFO si;
  GetStartupInfo(&si);
  int cmdshow = si.wShowWindow;
  if (si.dwFlags & STARTF_USESHOWWINDOW) cmdshow = SW_SHOWNORMAL;
  DWORD ret = wWinMain(GetModuleHandle(0), 0, 0, cmdshow);
  ExitProcess(ret);
}
