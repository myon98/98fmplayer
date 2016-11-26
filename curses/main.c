#include <curses.h>
#include <stdio.h>
#include <stdint.h>
#include "libopna/opna.h"
#include "libopna/opnatimer.h"
#include "fmdriver/fmdriver.h"
#include "fmdriver/fmdriver_fmp.h"
#include <SDL.h>
#include <stdlib.h>
#include <locale.h>
#include <iconv.h>
#include <errno.h>

static uint8_t g_data[0x10000];

enum {
  SRATE = 55467,
};

static void sdl_callback(void *userdata, Uint8 *stream, int len) {
  SDL_memset(stream, 0, len);
  struct opna_timer *timer = (struct opna_timer *)userdata;
  int16_t *buf = (int16_t *)stream;
  unsigned samples = len/2/2;
  opna_timer_mix(timer, buf, samples);
}

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
    const char *datadir = "/.local/share/libopna/";
    dpath = malloc(strlen(home)+strlen(datadir)+strlen(path) + 1);
    if (dpath) {
      strcpy(dpath, home);
      strcat(dpath, datadir);
      strcat(dpath, path);
      path = dpath;
    }
  }
  FILE *rhythm = fopen(path, "r");
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
  FILE *pvifile = fopen(pviname, "r");
  if (pvifile) return pvifile;
  pvifile = fopen(pviname_l, "r");
  if (pvifile) return pvifile;
  char *slash = strrchr(filename, '/');
  if (!slash) return 0;
  char *pvipath = malloc((slash-filename)+1+sizeof(pviname));
  if (!pvipath) return 0;
  memcpy(pvipath, filename, slash-filename+1);
  pvipath[slash-filename+1] = 0;
  strcat(pvipath, pviname);
  pvifile = fopen(pvipath, "r");
  if (pvifile) {
    free(pvipath);
    return pvifile;
  }
  pvipath[slash-filename+1] = 0;
  strcat(pvipath, pviname_l);
  pvifile = fopen(pvipath, "r");
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

int main(int argc, char **argv) {
  if (SDL_Init(SDL_INIT_AUDIO) != 0) {
    fprintf(stderr, "cannot initialize SDL\n");
    return 1;
  }
  if (argc != 2) {
    fprintf(stderr, "invalid arguments\n");
    return 1;
  }
  FILE *file = fopen(argv[1], "rb");
  if (!file) {
    fprintf(stderr, "cannot open file\n");
    return 1;
  }
  if (fseek(file, 0, SEEK_END) != 0) {
    fprintf(stderr, "cannot seek to end\n");
    return 1;
  }
  long filelen = ftell(file);
  if (fseek(file, 0, SEEK_SET) != 0) {
    fprintf(stderr, "cannot seek to beginning\n");
    return 1;
  }
  if ((filelen < 0) || (filelen > 0xffff)) {
    fprintf(stderr, "invalid file length: %ld\n", filelen);
    return 1;
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
  if (!fmp_init(&work, &fmp, g_data, filelen)) {
    fprintf(stderr, "not fmp\n");
    return 1;
  }
  bool pvi_loaded = loadpvi(&work, &fmp, argv[1]);
  bool ppz_loaded = loadppzpvi(&work, &fmp, argv[1]);

  SDL_AudioSpec as = {0};
  as.freq = SRATE;
  as.format = AUDIO_S16SYS;
  as.channels = 2;
  as.callback = sdl_callback;
  as.userdata = &timer;
  as.samples = 2048;

  SDL_AudioDeviceID ad = SDL_OpenAudioDevice(0, 0, &as, 0, 0);
  if (!ad) {
    fprintf(stderr, "cannot open audio device\n");
    return 1;
  }
  SDL_PauseAudioDevice(ad, 0);

  
  setlocale(LC_CTYPE, "");
  enum {
    //TBUFLEN = 80*3*2,
    TBUFLEN = 0x10000
  };
  char titlebuf[TBUFLEN+1] = {0};
  if (work.title) {
    iconv_t cd = iconv_open("//IGNORE", "CP932");
    if (cd != (iconv_t)-1) {
      char titlebufcrlf[TBUFLEN+1] = {0};
      const char *in = work.title;
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
  }
  
  initscr();
  cbreak();
  noecho();
  clear();
  refresh();

  timeout(20);

  mvprintw(0, 0, "PART  PTR  TONE LEN VOL NOTE  DET FREQ PQRAWE");
  mvprintw(14, 61, "PPZ8");
  mvprintw(16, 0, "FM                           RHYTHM             SSG  ADPCM");
  mvprintw(17, 0, "TL ENV                        TL    PTR  LV     LV");
  mvprintw(21, 48, "NZ");
  mvprintw(24, 0, "PPZ: %c%8s PVI: %c%8s",
           ppz_loaded ? ' ' : '!',
           fmp.ppz_name,
           pvi_loaded ? ' ' : '!',
           fmp.pvi_name);
  mvprintw(25, 0, "%s", titlebuf);

  int cont = 1;
  int pause = 0;
  while (cont) {
    switch (getch()) {
    case 'q':
      cont = 0;
      break;
    case 'p':
      pause = !pause;
      SDL_PauseAudioDevice(ad, pause);
      break;
    case ERR:
      update(&ppz8, &opna, &fmp);
      break;
    }
  }

  endwin();
  SDL_Quit();
  return 0;
}

