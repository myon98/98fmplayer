#ifndef MYON_FMPLAYER_FMDSP_PLATFORM_INFO_H_INCLUDED
#define MYON_FMPLAYER_FMDSP_PLATFORM_INFO_H_INCLUDED

int fmdsp_cpu_usage(void);

// call once per 30 frames to obtain fps
int fmdsp_fps_30(void);

#endif // MYON_FMPLAYER_FMDSP_PLATFORM_INFO_H_INCLUDED
