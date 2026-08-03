#ifndef GK_AUDIO_H
#define GK_AUDIO_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum gk_wave {
    GK_WAVE_SINE = 0,
    GK_WAVE_SQUARE,
    GK_WAVE_SAW,
    GK_WAVE_TRIANGLE,
    GK_WAVE_NOISE,
    GK_WAVE_COUNT
} gk_wave;

typedef enum gk_scope_source {
    GK_SRC_GEN = 0,
    GK_SRC_INPUT,
    GK_SRC_MIX
} gk_scope_source;

typedef struct gk_device_info {
    int index;           /* PortAudio device index */
    int is_input;        /* 1 if has input channels */
    int is_output;       /* 1 if has output channels */
    int max_in;
    int max_out;
    double default_sr;
    char name[256];      /* "HostAPI: Device name" */
} gk_device_info;

typedef struct gk_audio gk_audio;

gk_audio *gk_audio_create(void);
void      gk_audio_destroy(gk_audio *a);

/* Pa_Initialize / terminate. Returns 0 on success. */
int  gk_audio_init(gk_audio *a);
void gk_audio_shutdown(gk_audio *a);

/* Enumerate devices. Caller provides array; returns count written (or needed if out==NULL). */
int gk_audio_list_devices(gk_audio *a, gk_device_info *out, int max_out);

int gk_audio_default_input(const gk_audio *a);
int gk_audio_default_output(const gk_audio *a);

/*
 * Open streams. in_dev/out_dev are PortAudio indices, or -1 to skip that side.
 * sample_rate 0 = use device default (prefer 48000).
 * Returns 0 on success.
 */
int  gk_audio_start(gk_audio *a, int in_dev, int out_dev, double sample_rate);
int  gk_audio_stop(gk_audio *a);
int  gk_audio_is_running(const gk_audio *a);

void gk_audio_set_wave(gk_audio *a, gk_wave w);
void gk_audio_set_freq(gk_audio *a, float hz);
void gk_audio_set_amp(gk_audio *a, float amp);       /* 0..1 */
void gk_audio_set_gen_enabled(gk_audio *a, int on);
void gk_audio_set_monitor(gk_audio *a, int on);      /* route input → output */
void gk_audio_set_monitor_gain(gk_audio *a, float g);/* 0..1, default 0.35 */

gk_wave gk_audio_get_wave(const gk_audio *a);
float   gk_audio_get_freq(const gk_audio *a);
float   gk_audio_get_amp(const gk_audio *a);
int     gk_audio_get_gen_enabled(const gk_audio *a);
int     gk_audio_get_monitor(const gk_audio *a);

double gk_audio_sample_rate(const gk_audio *a);
int    gk_audio_frames_per_buffer(const gk_audio *a);
unsigned long gk_audio_xruns(const gk_audio *a);

/* Pull latest scope samples (oldest→newest). source: gen / input / mix. */
size_t gk_audio_pull_scope(gk_audio *a, float *out, size_t n, gk_scope_source src);

const char *gk_audio_last_error(const gk_audio *a);
const char *gk_wave_name(gk_wave w);

#ifdef __cplusplus
}
#endif

#endif /* GK_AUDIO_H */
