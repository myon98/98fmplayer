#include <curses.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "libopna/opna.h"
#include "libopna/opnatimer.h"
#include "fmdriver/fmdriver.h"
#include "fmdriver/fmdriver_fmp.h"
#include <portaudio.h>
#include <stdlib.h>
#include <locale.h>
#include <unistd.h>
#include <langinfo.h>
#include <iconv.h>
#include <errno.h>
#ifdef HAVE_SAMPLERATE
#include <samplerate.h>
#endif

static uint8_t g_data[0x10000];
enum {
  SRATE = 55467,
};

#ifdef HAVE_SAMPLERATE

enum {
  READFRAMES = 256,
};

struct {
  double src_ratio;
  struct opna_timer *timer;
  SRC_STATE *src;
  float buf_f[READFRAMES*2];
  int16_t buf_i[READFRAMES*2];
  int buf_used_frames;
} g;

static void update_buf(void) {
  for (int i = g.buf_used_frames*2; i < READFRAMES*2; i++) {
    g.buf_f[i-(g.buf_used_frames*2)] = g.buf_f[i];
  }
  for (int i = 0; i < g.buf_used_frames*2; i++) {
    g.buf_i[i] = 0;
  }
  opna_timer_mix(g.timer, g.buf_i, g.buf_used_frames);
  src_short_to_float_array(g.buf_i, g.buf_f+(READFRAMES-g.buf_used_frames)*2,
                           g.buf_used_frames*2);
  g.buf_used_frames = 0;
}

static int pastream_cb_src(const void *inptr, void *outptr,
                           unsigned long frames,
                           const PaStreamCallbackTimeInfo *timeinfo,
                           PaStreamCallbackFlags statusFlags,
                           void *userdata) {
  (void)inptr;
  (void)timeinfo;
  (void)statusFlags;
  (void)userdata;
  float *out_f = outptr;
  SRC_DATA srcdata;
  srcdata.data_in = g.buf_f;
  srcdata.data_out = out_f;
  srcdata.input_frames = READFRAMES;
  srcdata.output_frames = frames;
  srcdata.end_of_input = 0;
  srcdata.src_ratio = g.src_ratio;
  while (srcdata.output_frames) {
    src_process(g.src, &srcdata);
    srcdata.output_frames -= srcdata.output_frames_gen;
    srcdata.data_out += srcdata.output_frames_gen*2;
    g.buf_used_frames = srcdata.input_frames_used;
    update_buf();
  }
  return paContinue;
}

#else // HAVE_SAMPLERATE

static int pastream_cb(const void *inptr, void *outptr, unsigned long frames,
                       const PaStreamCallbackTimeInfo *timeinfo,
                       PaStreamCallbackFlags statusFlags,
                       void *userdata) {
  (void)inptr;
  (void)timeinfo;
  (void)statusFlags;
  struct opna_timer *timer = (struct opna_timer *)userdata;
  int16_t *buf = (int16_t *)outptr;
  memset(outptr, 0, sizeof(int16_t)*frames*2);
  opna_timer_mix(timer, buf, frames);
  return paContinue;
}

#endif // HAVE_SAMPLERATE

static void opna_writereg_libopna(struct fmdriver_work *work, unsigned addr, unsigned data) {
  struct opna_timer *timer = (struct opna_timer *)work->opna;
  opna_timer_writereg(timer, addr, data);
}

static uint8_t opna_status_libopna(struct fmdriver_work *work, bool a1) {
  struct opna_timer *timer = (struct opna_timer *)work->opna;
  uint8_t status = opna_timer_status(timer);
  if (!a1) {
    status &= 0x83;
  }
  return status;
}

static void opna_interrupt_callback(void *userptr) {
  struct fmdriver_work *work = (struct fmdriver_work *)userptr;
  work->driver_opna_interrupt(work);
}

static void opna_mix_callback(void *userptr, int16_t *buf, unsigned samples) {
  struct ppz8 *ppz8 = (struct ppz8 *)userptr;
  ppz8_mix(ppz8, buf, samples);
}

static const char *notestr[12] = {
  "c ",
  "c+",
  "d ",
  "d+",
  "e ",
  "f ",
  "f+",
  "g ",
  "g+",
  "a ",
  "a+",
  "b ",
};

static const char *pdzf_mode_str[3] = {
  "  ",
  "PS",
  "PE"
};

static void printnote(int y, const struct fmp_part *part) {
  if (part->status.rest) {
    mvprintw(y, 24, "    ");
  } else {
    mvprintw(y, 24, "o%c%s",
             (part->prev_note/0xc)+'0',
             notestr[(part->prev_note%0xc)]
    );
  }
}

static void printpart_ssg(int y, const char *name, const struct driver_fmp *fmp, const struct fmp_part *part) {

  mvprintw(y, 0, "%s %04X @%03d %3d %3d      %+4d %04X %c%c%c%c%c%c %c%c%c %c%c%c%c %c%c%c %02X %02X %c%c       %s %d",
    name,
    part->current_ptr,
    part->tone,
    part->tonelen_cnt,
    part->current_vol-1,
    part->detune - ((part->detune & 0x8000) ? 0x10000 : 0),
    part->actual_freq,
    part->lfo_f.p ? 'P' : '_',
    part->lfo_f.q ? 'Q' : '_',
    part->lfo_f.r ? 'R' : '_',
    part->lfo_f.a ? 'A' : '_',
    part->lfo_f.w ? 'W' : '_',
    part->lfo_f.e ? 'E' : '_',
    part->status.tie ? 'T' : '_',
    part->status.tie_cont ? 't' : '_',
    part->status.slur ? 'S' : '_',
    part->u.ssg.env_f.attack ? 'A' : '_',
    part->u.ssg.env_f.decay ? 'D' : '_',
    part->u.ssg.env_f.sustain ? 'S' : '_',
    part->u.ssg.env_f.release ? 'R' : '_',
    part->u.ssg.env_f.portamento ? 'P' : '_',
    part->u.ssg.env_f.noise ? 'N' : '_',
    part->u.ssg.env_f.ppz ? 'Z' : '_',
    part->actual_vol,
    part->u.ssg.vol,
    ((fmp->ssg_mix >> part->opna_keyon_out)&1) ? '_' : 'T',
    ((fmp->ssg_mix >> part->opna_keyon_out)&8) ? '_' : 'N',
    pdzf_mode_str[part->pdzf.mode],
    part->pdzf.env_state.status
  );
  printnote(y, part);
}

static void printpart_fm(int y, const char *name, const struct fmp_part *part) {
  mvprintw(y, 0, "%s %04X @%03d %3d %3d      %+4d %04X %c%c%c%c%c%c %c%c%c %c%c %02X %02X %02X %02X %c%c%c%c%c%c%c%c %s %3d",
    name,
    part->current_ptr,
    part->tone,
    part->tonelen_cnt,
    0x7f-part->actual_vol,
    part->detune - ((part->detune & 0x8000) ? 0x10000 : 0),
    part->actual_freq,
    part->lfo_f.p ? 'P' : '_',
    part->lfo_f.q ? 'Q' : '_',
    part->lfo_f.r ? 'R' : '_',
    part->lfo_f.a ? 'A' : '_',
    part->lfo_f.w ? 'W' : '_',
    part->lfo_f.e ? 'E' : '_',
    part->status.tie ? 'T' : '_',
    part->status.tie_cont ? 't' : '_',
    part->status.slur ? 'S' : '_',
    (part->pan_ams_pms & 0x80) ? 'L' : '_',
    (part->pan_ams_pms & 0x40) ? 'R' : '_',
    part->u.fm.tone_tl[0],
    part->u.fm.tone_tl[1],
    part->u.fm.tone_tl[2],
    part->u.fm.tone_tl[3],
    (part->u.fm.slot_mask & (1<<0)) ? '_' : '1',
    (part->u.fm.slot_mask & (1<<2)) ? '_' : '2',
    (part->u.fm.slot_mask & (1<<1)) ? '_' : '3',
    (part->u.fm.slot_mask & (1<<3)) ? '_' : '4',
    (part->u.fm.slot_mask & (1<<4)) ? '_' : '1',
    (part->u.fm.slot_mask & (1<<5)) ? '_' : '2',
    (part->u.fm.slot_mask & (1<<6)) ? '_' : '3',
    (part->u.fm.slot_mask & (1<<7)) ? '_' : '4',
    pdzf_mode_str[part->pdzf.mode],
    part->pdzf.vol
  );
  printnote(y, part);
}

static void update(const struct ppz8 *ppz8,
                   const struct opna *opna,
                   const struct driver_fmp *fmp) {
  static const char *partname_fm[] = {
    "FM1  ",
    "FM2  ",
    "FM3  ",
    "FM4  ",
    "FM5  ",
    "FM6  ",
    "FMEX1",
    "FMEX2",
    "FMEX3",
  };
  static const char *partname_ssg[] = {
    "SSG1 ",
    "SSG2 ",
    "SSG3 ",
  };
  static const char *partname_drum[] = {
    "BD ",
    "SD ",
    "TOP",
    "HH ",
    "TOM",
    "RIM",
  };
  for (int i = 0; i < 9; i++) {
    const struct fmp_part *part = &fmp->parts[FMP_PART_FM_1+i];
    printpart_fm(i+1, partname_fm[i], part);
    /*
    if (part->type.fm_3) {
      mvprintw(i+1, 60, "%c%c%c%c",
        (part->u.fm.slot_mask & 0x10) ? ' ' : '1',
        (part->u.fm.slot_mask & 0x20) ? ' ' : '2',
        (part->u.fm.slot_mask & 0x40) ? ' ' : '3',
        (part->u.fm.slot_mask & 0x80) ? ' ' : '4'
      );
    }
    */
  }
  for (int i = 0; i < 3; i++) {
    const struct fmp_part *part = &fmp->parts[FMP_PART_SSG_1+i];
    int y = i+10;
    printpart_ssg(y, partname_ssg[i], fmp, part);
  }
  mvprintw(13, 0, "RHY   %04X      %3d",
           fmp->rhythm.part.current_ptr,
           fmp->rhythm.len_cnt);
  {
    const struct fmp_part *part = &fmp->parts[FMP_PART_ADPCM];
    mvprintw(14, 0, "ADPCM %04X @%03d %3d %3d      %+4d %04X        %c%c %c%c",
             part->current_ptr,
             part->tone,
             part->tonelen_cnt,
             part->actual_vol,
             part->detune - ((part->detune & 0x8000) ? 0x10000 : 0),
             part->actual_freq,
             part->status.tie ? 'T' : '_',
             part->status.tie_cont ? 't' : '_',
             (part->pan_ams_pms & 0x80) ? 'L' : '_',
             (part->pan_ams_pms & 0x40) ? 'R' : '_'
    );
    printnote(14, part);
  }
  for (int c = 0; c < 6; c++) {
    for (int s = 0; s < 4; s++) {
      mvprintw(18+c, 7*s, "%02X %03X",
               opna->fm.channel[c].slot[s].tl,
               opna->fm.channel[c].slot[s].env);
    }
  }
  for (int i = 0; i < 3; i++) {
    mvprintw(18+i, 49, "%02X", opna->ssg.regs[8+i]);
  }
  mvprintw(22, 49, "%02X", opna->ssg.regs[6]&0x1f);
  mvprintw(17, 33, "%02X", opna->drum.total_level);
  for (int i = 0; i < 6; i++) {
    mvprintw(18+i, 30, "%s %c %04X %02X %c%c",
             partname_drum[i],
             opna->drum.drums[i].playing ? '@' : '_',
             opna->drum.drums[i].index,
             opna->drum.drums[i].level,
             opna->drum.drums[i].left ? 'L' : '_',
             opna->drum.drums[i].right ? 'R' : '_'
            );
  }
  mvprintw(18, 54, "%02X %c%c",
           opna->adpcm.vol,
           (opna->adpcm.control2 & 0x80) ? 'L' : '_',
           (opna->adpcm.control2 & 0x40) ? 'R' : '_'
  );
  mvprintw(20, 54, "%06X", opna->adpcm.start<<5);
  mvprintw(21, 54, "%06X", ((opna->adpcm.end+1)<<5)-1);
  mvprintw(22, 54, "%06X", opna->adpcm.ramptr>>1);
  
  for (int i = 0; i < 8; i++) {
    mvprintw(16+i, 62, "@%03d %1X %1d %08X",
             ppz8->channel[i].voice,
             ppz8->channel[i].vol,
             ppz8->channel[i].pan,
             (unsigned)(ppz8->channel[i].ptr>>16));
  }
}

static bool readrom(struct opna *opna) {
  const char *path = "ym2608_adpcm_rom.bin";
  const char *home = getenv("HOME");
  char *dpath = 0;
  if (home) {
    const char *datadir = "/.local/share/fmplayer/";
    dpath = malloc(strlen(home)+strlen(datadir)+strlen(path) + 1);
    if (dpath) {
      strcpy(dpath, home);
      strcat(dpath, datadir);
      strcat(dpath, path);
      path = dpath;
    }
  }
  FILE *rhythm = fopen(path, "rb");
  free(dpath);
  if (!rhythm) goto err;
  if (fseek(rhythm, 0, SEEK_END) != 0) goto err_file;
  long size = ftell(rhythm);
  if (size != 0x2000) goto err_file;
  if (fseek(rhythm, 0, SEEK_SET) != 0) goto err_file;
  uint8_t data[0x2000];
  if (fread(data, 1, 0x2000, rhythm) != 0x2000) goto err_file;
  opna_drum_set_rom(&opna->drum, data);
  fclose(rhythm);
  return true;
err_file:
  fclose(rhythm);
err:
  return false;
}

static FILE *pvisearch(const char *filename, const char *pvibase) {
  // TODO: not SJIS aware
  char pviname[8+3+2] = {0};
  char pviname_l[8+3+2] = {0};
  strcpy(pviname, pvibase);
  strcat(pviname, ".PVI");
  strcpy(pviname_l, pviname);
  for (char *c = pviname_l; *c; c++) {
    if (('A' <= *c) && (*c <= 'Z')) {
      *c += ('a' - 'A');
    }
  }
  FILE *pvifile = fopen(pviname, "rb");
  if (pvifile) return pvifile;
  pvifile = fopen(pviname_l, "rb");
  if (pvifile) return pvifile;
  char *slash = strrchr(filename, '/');
  if (!slash) return 0;
  char *pvipath = malloc((slash-filename)+1+sizeof(pviname));
  if (!pvipath) return 0;
  memcpy(pvipath, filename, slash-filename+1);
  pvipath[slash-filename+1] = 0;
  strcat(pvipath, pviname);
  pvifile = fopen(pvipath, "rb");
  if (pvifile) {
    free(pvipath);
    return pvifile;
  }
  pvipath[slash-filename+1] = 0;
  strcat(pvipath, pviname_l);
  pvifile = fopen(pvipath, "rb");
  if (pvifile) {
    free(pvipath);
    return pvifile;
  }
  free(pvipath);
  return 0;
}

static bool loadpvi(struct fmdriver_work *work,
                    struct driver_fmp *fmp,
                    const char *filename) {
  // no need to load, always success
  if(strlen(fmp->pvi_name) == 0) return true;
  FILE *pvifile = pvisearch(filename, fmp->pvi_name);
  if (!pvifile) goto err;
  if (fseek(pvifile, 0, SEEK_END) != 0) goto err_file;
  size_t fsize;
  {
    long size = ftell(pvifile);
    if (size < 0) goto err_file;
    fsize = size;
  }
  if (fseek(pvifile, 0, SEEK_SET) != 0) goto err_file;
  void *data = malloc(fsize);
  if (!data) goto err_file;
  if (fread(data, 1, fsize, pvifile) != fsize) goto err_memory;
  if (!fmp_adpcm_load(work, data, fsize)) goto err_memory;
  free(data);
  fclose(pvifile);
  return true;
err_memory:
  free(data);
err_file:
  fclose(pvifile);
err:
  return false;
}

static bool loadppzpvi(struct fmdriver_work *work,
                    struct driver_fmp *fmp,
                    const char *filename) {
  // no need to load, always success
  if(strlen(fmp->ppz_name) == 0) return true;
  FILE *pvifile = pvisearch(filename, fmp->ppz_name);
  if (!pvifile) goto err;
  if (fseek(pvifile, 0, SEEK_END) != 0) goto err_file;
  size_t fsize;
  {
    long size = ftell(pvifile);
    if (size < 0) goto err_file;
    fsize = size;
  }
  if (fseek(pvifile, 0, SEEK_SET) != 0) goto err_file;
  void *data = malloc(fsize);
  if (!data) goto err_file;
  if (fread(data, 1, fsize, pvifile) != fsize) goto err_memory;
  int16_t *decbuf = calloc(ppz8_pvi_decodebuf_samples(fsize), sizeof(int16_t));
  if (!decbuf) goto err_memory;
  if (!ppz8_pvi_load(work->ppz8, 0, data, fsize, decbuf)) goto err_memory;
  free(data);
  fclose(pvifile);
  return true;
err_memory:
  free(data);
err_file:
  fclose(pvifile);
err:
  return false;
}

static void list_devices(void) {
  PaHostApiIndex al = Pa_GetHostApiCount();
  if (al < 0) {
    fprintf(stderr, "Pa_GetHostApiCount error");
    exit(1);
  }
  const PaHostApiInfo **aa = malloc(sizeof(PaHostApiInfo *)*al);
  if (!aa) {
    fprintf(stderr, "cannot allocate memory for portaudio host API info\n");
    exit(1);
  }
  for (PaHostApiIndex i = 0; i < al; i++) {
    aa[i] = Pa_GetHostApiInfo(i);
    if (!aa[i]) {
      fprintf(stderr, "cannot get portaudio host API info\n");
      exit(1);
    }
  }
  PaDeviceIndex dl = Pa_GetDeviceCount();
  if (dl < 0) {
    fprintf(stderr, "Pa_GetDeviceCount error\n");
    exit(1);
  }
  PaDeviceIndex dd = Pa_GetDefaultOutputDevice();
  printf("ind:       hostapi              name\n");
  for (PaDeviceIndex i = 0; i < dl; i++) {
    const PaDeviceInfo *di = Pa_GetDeviceInfo(i);
    if (!di) {
      fprintf(stderr, "cannot get device information\n");
      exit(1);
    }
    const char *ds = (i == dd) ? "(def)" : "     ";
    printf("%3d: %s %-20s %s\n", i, ds, aa[di->hostApi]->name, di->name);
  }
  exit(0);
}

static void help(const char *name) {
  fprintf(stderr, "Usage: %s [options] file\n", name);
  fprintf(stderr, "currently supported files: FMP(PLAY6)\n");
  fprintf(stderr, "  options:\n");
  fprintf(stderr, "  -h        show help\n");
  fprintf(stderr, "  -l        list portaudio devices\n");
  fprintf(stderr, "  -d index  specify device number\n");
  exit(1);
}

int main(int argc, char **argv) {
  if (Pa_Initialize() != paNoError) {
    fprintf(stderr, "cannot initialize portaudio\n");
    return 1;
  }
  int optchar;
  PaDeviceIndex pi = Pa_GetDefaultOutputDevice();
  while ((optchar = getopt(argc, argv, "hld:")) != -1) {
    switch (optchar) {
    case 'l':
      list_devices();
      break;
    case 'd':
      pi = atoi(optarg);
      break;
    default:
    case 'h':
      help(argv[0]);
      break;
    }
  }
  if (pi == paNoDevice) {
    fprintf(stderr, "no default output device\n");
    return 1;
  } else if ((pi < 0) || (pi >= Pa_GetDeviceCount())) {
    fprintf(stderr, "invalid output device\n");
    return 1;
  }
  if (argc != optind + 1) {
    fprintf(stderr, "invalid arguments\n");
    help(argv[0]);
  }
  const char *filename = argv[optind];
  FILE *file = fopen(filename, "rb");
  if (!file) {
    fprintf(stderr, "cannot open file\n");
    return 1;
  }
  if (fseek(file, 0, SEEK_END) != 0) {
    fprintf(stderr, "cannot seek to end\n");
    return 1;
  }
  size_t filelen;
  {
    long filelen_t = ftell(file);
    if (fseek(file, 0, SEEK_SET) != 0) {
      fprintf(stderr, "cannot seek to beginning\n");
      return 1;
    }
    if ((filelen_t < 0) || (filelen_t > 0xffff)) {
      fprintf(stderr, "invalid file length: %ld\n", filelen_t);
      return 1;
    }
    filelen = filelen_t;
  }
  if (fread(g_data, 1, filelen, file) != filelen) {
    fprintf(stderr, "cannot read file\n");
    return 1;
  }
  fclose(file);
  static struct fmdriver_work work = {0};
  static struct driver_fmp fmp = {0};
  static struct opna opna;
  static struct opna_timer timer;
  static struct ppz8 ppz8;
  ppz8_init(&ppz8, SRATE, 0xa000);
  work.ppz8 = &ppz8;
  work.ppz8_functbl = &ppz8_functbl;
  opna_reset(&opna);
  opna_timer_reset(&timer, &opna);
  readrom(&opna);
  opna_adpcm_set_ram_256k(&opna.adpcm, malloc(OPNA_ADPCM_RAM_SIZE));
  
  opna_timer_set_int_callback(&timer, opna_interrupt_callback, &work);
  opna_timer_set_mix_callback(&timer, opna_mix_callback, &ppz8);
  work.opna_writereg = opna_writereg_libopna;
  work.opna_status = opna_status_libopna;
  work.opna = &timer;
  if (!fmp_load(&fmp, g_data, filelen)) {
    fprintf(stderr, "not fmp\n");
    return 1;
  }
  fmp_init(&work, &fmp);
  bool pvi_loaded = loadpvi(&work, &fmp, filename);
  bool ppz_loaded = loadppzpvi(&work, &fmp, filename);

  PaStream *ps;
  PaError pe;
  const PaDeviceInfo *pdi = Pa_GetDeviceInfo(pi);
  if (!pdi) {
    fprintf(stderr, "cannot get device default samplerate\n");
    return 1;
  }
#ifdef HAVE_SAMPLERATE
  {
    double outrate = pdi->defaultSampleRate;
    double ratio = outrate / SRATE;
    int e;
    g.src_ratio = ratio;
    g.timer = &timer;
    g.src = src_new(SRC_SINC_BEST_QUALITY, 2, &e);
    if (!g.src) {
      fprintf(stderr, "cannot open samplerate converter\n");
      return 1;
    }
    PaStreamParameters psp;
    psp.device = pi;
    psp.channelCount = 2;
    psp.sampleFormat = paFloat32;
    psp.suggestedLatency = pdi->defaultLowOutputLatency;
    psp.hostApiSpecificStreamInfo = 0;
    pe = Pa_OpenStream(&ps, 0, &psp, outrate, 0, 0, &pastream_cb_src, 0);
    if (pe != paNoError) {
      fprintf(stderr, "cannot open audio device\n");
      return 1;
    }
  }
#else // HAVE_SAMPLERATE

  {
    PaStreamParameters psp;
    psp.device = pi;
    psp.channelCount = 2;
    psp.sampleFormat = paInt16;
    psp.suggestedLatency = pdi->defaultLowOutputLatency;
    psp.hostApiSpecificStreamInfo = 0;
    pe = Pa_OpenStream(&ps, 0, &psp, SRATE, 0, 0, &pastream_cb, &timer);
    if (pe != paNoError) {
      fprintf(stderr, "cannot open audio device\n");
      return 1;
    }
  }

#endif // HAVE_SAMPLERATE

  Pa_StartStream(ps);
  setlocale(LC_CTYPE, "");

  initscr();
  cbreak();
  noecho();
  clear();
  refresh();
  curs_set(0);

  timeout(20);

  static const char pdzf_mode_str[3][9] = {
    "OFF", "STANDARD", "ENHANCED"
  };
  mvprintw(0, 0, "PART  PTR  TONE LEN VOL NOTE  DET FREQ PQRAWE");
  mvprintw(14, 61, "PPZ8");
  mvprintw(16, 0, "FM                           RHYTHM             SSG  ADPCM");
  mvprintw(17, 0, "TL ENV                        TL    PTR  LV     LV");
  mvprintw(21, 48, "NZ");
  mvprintw(24, 0, "PPZ: %c%8s PVI: %c%8s PDZF: %s",
           ppz_loaded ? ' ' : '!',
           fmp.ppz_name,
           pvi_loaded ? ' ' : '!',
           fmp.pvi_name,
           pdzf_mode_str[fmp.pdzf.mode]
          );

  enum {
    TBUFLEN = 80*2*2
  };
  char titlebuf[TBUFLEN+1] = {0};
  for (int l = 0; l < 3; l++) {
    iconv_t cd = iconv_open(nl_langinfo(CODESET), "CP932");
    if (cd != (iconv_t)-1) {
      char titlebufcrlf[TBUFLEN+1] = {0};
      ICONV_CONST char *in = (char *)work.comment[l];
      size_t inleft = strlen(in)+1;
      char *out = titlebufcrlf;
      size_t outleft = TBUFLEN;
      for (;;) {
        if (iconv(cd, &in, &inleft, &out, &outleft) == (size_t)-1) {
          *out = 0;
          break;
        }
        if (!inleft) break;
      }
      iconv_close(cd);
      int o = 0;
      for (int i = 0; ; i++) {
        if (titlebufcrlf[i] == 0x0d) continue;
        titlebuf[o++] = titlebufcrlf[i];
        if (!titlebufcrlf[i]) break;
      }
    }
    mvprintw(25+l, 0, "%s", titlebuf);
  }
  
  int cont = 1;
  bool pause = 0;
  while (cont) {
    switch (getch()) {
    case 'q':
      cont = 0;
      break;
    case 'p':
      pause = !pause;
      if (pause) {
        Pa_StopStream(ps);
      } else {
        Pa_StartStream(ps);
      }
      break;
    case ERR:
      update(&ppz8, &opna, &fmp);
      break;
    }
  }

  endwin();
  Pa_Terminate();
  return 0;
}

