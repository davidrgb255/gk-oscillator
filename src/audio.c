#include "audio.h"
#include "ringbuf.h"

#include "../third_party/portaudio.h"

#include <math.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define GK_RING_SECONDS 0.12
#define GK_FRAMES       256
#define GK_MAX_CH       2

/*
 * SPSC monitor FIFO: input callback produces, output callback consumes in order.
 * Using free-running indices (power-of-two capacity) avoids the old "read latest
 * window" path that re-played or skipped blocks when streams drifted.
 */
typedef struct gk_mon_fifo {
    float *data;
    uint32_t cap;   /* power of two */
    uint32_t mask;
    volatile uint32_t w; /* producer (input) */
    volatile uint32_t r; /* consumer (output) */
} gk_mon_fifo;

struct gk_audio {
    int pa_ready;
    int running;

    PaStream *out_stream;
    PaStream *in_stream;
    PaStream *duplex_stream; /* full-duplex when both ends open together */

    int in_dev;
    int out_dev;
    double sample_rate;
    int frames_per_buffer;
    int out_channels;
    int in_channels;

    /* Parameters (audio thread reads; UI writes under lock or atomics-ish) */
    pthread_mutex_t lock;
    gk_wave wave;
    float freq;
    float amp;
    int gen_enabled;
    int monitor;
    float monitor_gain;

    double phase; /* 0..1 */
    unsigned int noise_state;

    gk_ringbuf ring_gen;
    gk_ringbuf ring_in;

    /* Ordered monitor path (not the scope ring) */
    gk_mon_fifo mon;
    float mon_gain_smooth; /* de-click enable/disable and gain changes */
    float mon_dc_x;        /* one-pole DC blocker state */
    float mon_dc_y;
    float mon_env;         /* soft underrun fade */

    unsigned long xruns;
    char last_error[256];
};

static uint32_t next_pow2_u32(uint32_t v)
{
    uint32_t p = 1;
    if (v == 0) {
        return 1;
    }
    while (p < v) {
        p <<= 1;
    }
    return p;
}

static int mon_fifo_init(gk_mon_fifo *f, uint32_t min_cap)
{
    uint32_t cap = next_pow2_u32(min_cap < 1024 ? 1024 : min_cap);
    memset(f, 0, sizeof(*f));
    f->data = (float *)calloc(cap, sizeof(float));
    if (f->data == NULL) {
        return -1;
    }
    f->cap = cap;
    f->mask = cap - 1;
    f->w = 0;
    f->r = 0;
    return 0;
}

static void mon_fifo_free(gk_mon_fifo *f)
{
    if (f == NULL) {
        return;
    }
    free(f->data);
    memset(f, 0, sizeof(*f));
}

static void mon_fifo_reset(gk_mon_fifo *f)
{
    if (f == NULL || f->data == NULL) {
        return;
    }
    f->w = 0;
    f->r = 0;
    memset(f->data, 0, f->cap * sizeof(float));
}

static uint32_t mon_fifo_avail(const gk_mon_fifo *f)
{
    return f->w - f->r;
}

/* Drop oldest samples so fill stays near target (low latency, fewer underruns). */
static void mon_fifo_trim(gk_mon_fifo *f, uint32_t target, uint32_t high)
{
    uint32_t avail = mon_fifo_avail(f);
    if (avail > high) {
        f->r = f->w - target;
    }
}

static void mon_fifo_write(gk_mon_fifo *f, const float *samples, uint32_t n)
{
    uint32_t i;
    uint32_t w;
    uint32_t r;
    if (f == NULL || f->data == NULL || samples == NULL || n == 0) {
        return;
    }
    w = f->w;
    r = f->r;
    for (i = 0; i < n; i++) {
        /* Overrun: drop oldest so we never block the capture callback. */
        if ((uint32_t)(w - r) >= f->cap - 1) {
            r++;
        }
        f->data[w & f->mask] = samples[i];
        w++;
    }
    f->r = r;
    f->w = w;
}

/* Consume n samples in order. Underruns write 0 and return underrun count. */
static uint32_t mon_fifo_read(gk_mon_fifo *f, float *out, uint32_t n)
{
    uint32_t i;
    uint32_t w;
    uint32_t r;
    uint32_t underruns = 0;
    if (f == NULL || f->data == NULL || out == NULL || n == 0) {
        return n;
    }
    w = f->w;
    r = f->r;
    for (i = 0; i < n; i++) {
        if (r != w) {
            out[i] = f->data[r & f->mask];
            r++;
        } else {
            out[i] = 0.0f;
            underruns++;
        }
    }
    f->r = r;
    return underruns;
}

/* Soft clip keeps generator+monitor sums from hard-edge crackles. */
static float soft_clip(float x)
{
    if (x > 1.2f) {
        return 1.0f;
    }
    if (x < -1.2f) {
        return -1.0f;
    }
    /* cubic soft knee: smooth near ±1 */
    return x - (x * x * x) * (1.0f / 3.0f) * 0.35f;
}

const char *gk_wave_name(gk_wave w)
{
    switch (w) {
    case GK_WAVE_SINE:     return "Sine";
    case GK_WAVE_SQUARE:   return "Square";
    case GK_WAVE_SAW:      return "Saw";
    case GK_WAVE_TRIANGLE: return "Triangle";
    case GK_WAVE_NOISE:    return "Noise";
    default:               return "?";
    }
}

static void set_error(gk_audio *a, const char *msg)
{
    if (a == NULL) {
        return;
    }
    if (msg == NULL) {
        a->last_error[0] = '\0';
        return;
    }
    snprintf(a->last_error, sizeof(a->last_error), "%s", msg);
}

static void set_pa_error(gk_audio *a, const char *prefix, PaError err)
{
    char buf[256];
    snprintf(buf, sizeof(buf), "%s: %s", prefix, Pa_GetErrorText(err));
    set_error(a, buf);
}

static float noise_sample(gk_audio *a)
{
    /* xorshift32 */
    unsigned int x = a->noise_state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    a->noise_state = x ? x : 0xA341316Cu;
    return ((float)(a->noise_state & 0xFFFF) / 32768.0f) - 1.0f;
}

static float osc_sample(gk_audio *a, float freq, float amp)
{
    float s = 0.0f;
    double phase = a->phase;
    switch (a->wave) {
    case GK_WAVE_SINE:
        s = (float)sin(2.0 * M_PI * phase);
        break;
    case GK_WAVE_SQUARE:
        s = (phase < 0.5) ? 1.0f : -1.0f;
        break;
    case GK_WAVE_SAW:
        s = (float)(2.0 * phase - 1.0);
        break;
    case GK_WAVE_TRIANGLE:
        s = (float)(1.0 - 4.0 * fabs(phase - 0.5));
        break;
    case GK_WAVE_NOISE:
        s = noise_sample(a);
        break;
    default:
        s = 0.0f;
        break;
    }
    /* advance phase */
    if (a->sample_rate > 0.0 && a->wave != GK_WAVE_NOISE) {
        a->phase += (double)freq / a->sample_rate;
        if (a->phase >= 1.0) {
            a->phase -= floor(a->phase);
        }
    }
    return s * amp;
}

/* Render one output block; optional direct input for duplex (NULL = use FIFO). */
static void render_out_block(gk_audio *a, float *out, unsigned long frameCount,
                             const float *direct_in, int direct_in_ch)
{
    unsigned long i;
    float gen_buf[512];
    float mon_buf[512];
    float freq, amp, mon_gain_target;
    int gen_on, mon_on, ch;
    float smooth;
    float coeff;
    uint32_t underruns = 0;

    pthread_mutex_lock(&a->lock);
    freq = a->freq;
    amp = a->amp;
    gen_on = a->gen_enabled;
    mon_on = a->monitor;
    mon_gain_target = mon_on ? a->monitor_gain : 0.0f;
    pthread_mutex_unlock(&a->lock);

    ch = a->out_channels;
    if (ch < 1) {
        ch = 1;
    }
    if (ch > GK_MAX_CH) {
        ch = GK_MAX_CH;
    }

    if (frameCount > 512) {
        frameCount = 512;
    }

    /* ~5 ms exponential ramp for monitor gain (avoids click on toggle). */
    coeff = (a->sample_rate > 0.0)
                ? (float)(1.0 - exp(-1.0 / (0.005 * a->sample_rate)))
                : 0.05f;
    if (coeff < 0.001f) {
        coeff = 0.001f;
    }
    if (coeff > 0.5f) {
        coeff = 0.5f;
    }

    if (direct_in != NULL) {
        /* Full-duplex: sample-accurate input, no FIFO. */
        int ich = direct_in_ch > 0 ? direct_in_ch : 1;
        for (i = 0; i < frameCount; i++) {
            if (ich == 1) {
                mon_buf[i] = direct_in[i];
            } else {
                mon_buf[i] = 0.5f * (direct_in[i * ich] + direct_in[i * ich + 1]);
            }
        }
        a->mon_env = 1.0f;
    } else {
        underruns = mon_fifo_read(&a->mon, mon_buf, (uint32_t)frameCount);
        if (underruns > 0) {
            a->xruns++;
            /* Fade only on real FIFO underruns, not quiet input. */
            if (underruns > (uint32_t)frameCount / 4) {
                a->mon_env *= 0.75f;
            }
        } else {
            a->mon_env += (1.0f - a->mon_env) * 0.15f;
        }
        if (a->mon_env < 0.05f) {
            a->mon_env = 0.05f;
        }
    }

    smooth = a->mon_gain_smooth;
    for (i = 0; i < frameCount; i++) {
        float g = 0.0f;
        float mixed;
        float mon;

        if (gen_on) {
            g = osc_sample(a, freq, amp);
        }
        gen_buf[i] = g;

        smooth += (mon_gain_target - smooth) * coeff;
        mon = mon_buf[i] * smooth * a->mon_env;

        mixed = soft_clip(g + mon);
        if (ch == 1) {
            out[i] = mixed;
        } else {
            out[i * 2 + 0] = mixed;
            out[i * 2 + 1] = mixed;
        }
    }
    a->mon_gain_smooth = smooth;

    gk_ringbuf_write(&a->ring_gen, gen_buf, (size_t)frameCount);
}

static int out_callback(const void *input, void *output,
                        unsigned long frameCount,
                        const PaStreamCallbackTimeInfo *timeInfo,
                        PaStreamCallbackFlags statusFlags,
                        void *userData)
{
    gk_audio *a = (gk_audio *)userData;
    (void)input;
    (void)timeInfo;

    if (statusFlags & (paOutputUnderflow | paOutputOverflow)) {
        a->xruns++;
    }
    render_out_block(a, (float *)output, frameCount, NULL, 0);
    return paContinue;
}

static int duplex_callback(const void *input, void *output,
                           unsigned long frameCount,
                           const PaStreamCallbackTimeInfo *timeInfo,
                           PaStreamCallbackFlags statusFlags,
                           void *userData)
{
    gk_audio *a = (gk_audio *)userData;
    const float *in = (const float *)input;
    float mono[512];
    unsigned long i;
    int ch = a->in_channels;

    (void)timeInfo;

    if (statusFlags & (paOutputUnderflow | paOutputOverflow | paInputUnderflow | paInputOverflow)) {
        a->xruns++;
    }

    if (frameCount > 512) {
        frameCount = 512;
    }
    if (ch < 1) {
        ch = 1;
    }

    /* Scope ring + DC-blocked mono (duplex still feeds scope from capture). */
    if (in != NULL) {
        for (i = 0; i < frameCount; i++) {
            float x, y;
            if (ch == 1) {
                x = in[i];
            } else {
                x = 0.5f * (in[i * ch] + in[i * ch + 1]);
            }
            /* R = 0.995 @ 48 kHz-ish high-pass */
            y = x - a->mon_dc_x + 0.995f * a->mon_dc_y;
            a->mon_dc_x = x;
            a->mon_dc_y = y;
            mono[i] = y;
        }
        gk_ringbuf_write(&a->ring_in, mono, (size_t)frameCount);
        render_out_block(a, (float *)output, frameCount, mono, 1);
    } else {
        memset(mono, 0, (size_t)frameCount * sizeof(float));
        render_out_block(a, (float *)output, frameCount, mono, 1);
    }
    return paContinue;
}

static int in_callback(const void *input, void *output,
                       unsigned long frameCount,
                       const PaStreamCallbackTimeInfo *timeInfo,
                       PaStreamCallbackFlags statusFlags,
                       void *userData)
{
    gk_audio *a = (gk_audio *)userData;
    const float *in = (const float *)input;
    float mono[512];
    unsigned long i;
    int ch = a->in_channels;
    uint32_t target;
    uint32_t high;

    (void)output;
    (void)timeInfo;

    if (statusFlags & (paInputUnderflow | paInputOverflow)) {
        a->xruns++;
    }
    if (in == NULL) {
        return paContinue;
    }
    if (frameCount > 512) {
        frameCount = 512;
    }
    if (ch < 1) {
        ch = 1;
    }

    memset(mono, 0, sizeof(mono));
    for (i = 0; i < frameCount; i++) {
        float x, y;
        if (ch == 1) {
            x = in[i];
        } else {
            x = 0.5f * (in[i * ch] + in[i * ch + 1]);
        }
        y = x - a->mon_dc_x + 0.995f * a->mon_dc_y;
        a->mon_dc_x = x;
        a->mon_dc_y = y;
        mono[i] = y;
    }
    gk_ringbuf_write(&a->ring_in, mono, (size_t)frameCount);
    mon_fifo_write(&a->mon, mono, (uint32_t)frameCount);

    /* Keep ~2–4 buffers of slack so separate clocks rarely underrun. */
    target = (uint32_t)(a->frames_per_buffer * 2);
    high = (uint32_t)(a->frames_per_buffer * 5);
    if (target < 128) {
        target = 128;
    }
    if (high < target + 64) {
        high = target + 64;
    }
    mon_fifo_trim(&a->mon, target, high);
    return paContinue;
}

gk_audio *gk_audio_create(void)
{
    gk_audio *a = (gk_audio *)calloc(1, sizeof(gk_audio));
    if (a == NULL) {
        return NULL;
    }
    pthread_mutex_init(&a->lock, NULL);
    a->wave = GK_WAVE_SINE;
    a->freq = 440.0f;
    a->amp = 0.2f;
    a->gen_enabled = 1;
    a->monitor = 0;
    a->monitor_gain = 0.35f;
    a->mon_gain_smooth = 0.0f;
    a->mon_env = 1.0f;
    a->noise_state = 0xA341316Cu;
    a->sample_rate = 48000.0;
    a->frames_per_buffer = GK_FRAMES;
    a->in_dev = -1;
    a->out_dev = -1;
    return a;
}

void gk_audio_destroy(gk_audio *a)
{
    if (a == NULL) {
        return;
    }
    gk_audio_stop(a);
    gk_audio_shutdown(a);
    gk_ringbuf_free(&a->ring_gen);
    gk_ringbuf_free(&a->ring_in);
    mon_fifo_free(&a->mon);
    pthread_mutex_destroy(&a->lock);
    free(a);
}

int gk_audio_init(gk_audio *a)
{
    PaError err;
    if (a == NULL) {
        return -1;
    }
    if (a->pa_ready) {
        return 0;
    }
    err = Pa_Initialize();
    if (err != paNoError) {
        set_pa_error(a, "Pa_Initialize", err);
        return -1;
    }
    a->pa_ready = 1;
    set_error(a, NULL);
    return 0;
}

void gk_audio_shutdown(gk_audio *a)
{
    if (a == NULL || !a->pa_ready) {
        return;
    }
    gk_audio_stop(a);
    Pa_Terminate();
    a->pa_ready = 0;
}

int gk_audio_list_devices(gk_audio *a, gk_device_info *out, int max_out)
{
    int count;
    int i;
    int n = 0;
    if (a == NULL || !a->pa_ready) {
        return 0;
    }
    count = Pa_GetDeviceCount();
    if (count < 0) {
        set_pa_error(a, "Pa_GetDeviceCount", count);
        return 0;
    }
    for (i = 0; i < count; i++) {
        const PaDeviceInfo *di = Pa_GetDeviceInfo(i);
        const PaHostApiInfo *hi;
        gk_device_info info;
        if (di == NULL) {
            continue;
        }
        if (di->maxInputChannels <= 0 && di->maxOutputChannels <= 0) {
            continue;
        }
        memset(&info, 0, sizeof(info));
        info.index = i;
        info.max_in = di->maxInputChannels;
        info.max_out = di->maxOutputChannels;
        info.is_input = di->maxInputChannels > 0;
        info.is_output = di->maxOutputChannels > 0;
        info.default_sr = di->defaultSampleRate;
        hi = Pa_GetHostApiInfo(di->hostApi);
        if (hi != NULL && hi->name != NULL) {
            snprintf(info.name, sizeof(info.name), "%s: %s", hi->name, di->name ? di->name : "?");
        } else {
            snprintf(info.name, sizeof(info.name), "%s", di->name ? di->name : "?");
        }
        if (out != NULL && n < max_out) {
            out[n] = info;
        }
        n++;
    }
    (void)max_out;
    return n;
}

int gk_audio_default_input(const gk_audio *a)
{
    (void)a;
    return Pa_GetDefaultInputDevice();
}

int gk_audio_default_output(const gk_audio *a)
{
    (void)a;
    return Pa_GetDefaultOutputDevice();
}

int gk_audio_start(gk_audio *a, int in_dev, int out_dev, double sample_rate)
{
    PaStreamParameters in_params;
    PaStreamParameters out_params;
    PaStreamParameters *in_ptr = NULL;
    PaStreamParameters *out_ptr = NULL;
    PaError err;
    size_t ring_cap;
    double sr = sample_rate;

    if (a == NULL || !a->pa_ready) {
        return -1;
    }
    if (a->running) {
        gk_audio_stop(a);
    }

    if (out_dev < 0 && in_dev < 0) {
        set_error(a, "No input or output device selected");
        return -1;
    }

    if (sr <= 0.0) {
        sr = 48000.0;
        if (out_dev >= 0) {
            const PaDeviceInfo *di = Pa_GetDeviceInfo(out_dev);
            if (di && di->defaultSampleRate > 0) {
                sr = di->defaultSampleRate;
            }
        } else if (in_dev >= 0) {
            const PaDeviceInfo *di = Pa_GetDeviceInfo(in_dev);
            if (di && di->defaultSampleRate > 0) {
                sr = di->defaultSampleRate;
            }
        }
    }

    a->sample_rate = sr;
    a->frames_per_buffer = GK_FRAMES;
    a->phase = 0.0;
    a->xruns = 0;
    a->in_dev = in_dev;
    a->out_dev = out_dev;
    a->mon_gain_smooth = 0.0f;
    a->mon_env = 1.0f;
    a->mon_dc_x = 0.0f;
    a->mon_dc_y = 0.0f;

    ring_cap = (size_t)(sr * GK_RING_SECONDS);
    if (ring_cap < 1024) {
        ring_cap = 1024;
    }
    gk_ringbuf_free(&a->ring_gen);
    gk_ringbuf_free(&a->ring_in);
    mon_fifo_free(&a->mon);
    if (gk_ringbuf_init(&a->ring_gen, ring_cap) != 0 ||
        gk_ringbuf_init(&a->ring_in, ring_cap) != 0 ||
        mon_fifo_init(&a->mon, (uint32_t)(sr * 0.25) /* 250 ms max lag */) != 0) {
        set_error(a, "Ring buffer alloc failed");
        gk_ringbuf_free(&a->ring_gen);
        gk_ringbuf_free(&a->ring_in);
        mon_fifo_free(&a->mon);
        return -1;
    }
    mon_fifo_reset(&a->mon);

    memset(&in_params, 0, sizeof(in_params));
    memset(&out_params, 0, sizeof(out_params));

    if (out_dev >= 0) {
        const PaDeviceInfo *di = Pa_GetDeviceInfo(out_dev);
        if (di == NULL || di->maxOutputChannels < 1) {
            set_error(a, "Invalid output device");
            return -1;
        }
        a->out_channels = di->maxOutputChannels >= 2 ? 2 : 1;
        out_params.device = out_dev;
        out_params.channelCount = a->out_channels;
        out_params.sampleFormat = paFloat32;
        /* Slightly higher latency than "low" reduces separate-stream xruns. */
        out_params.suggestedLatency = di->defaultHighOutputLatency > 0
                                          ? di->defaultHighOutputLatency * 0.5
                                          : di->defaultLowOutputLatency;
        if (out_params.suggestedLatency < di->defaultLowOutputLatency) {
            out_params.suggestedLatency = di->defaultLowOutputLatency;
        }
        out_params.hostApiSpecificStreamInfo = NULL;
        out_ptr = &out_params;
    } else {
        a->out_channels = 0;
    }

    if (in_dev >= 0) {
        const PaDeviceInfo *di = Pa_GetDeviceInfo(in_dev);
        if (di == NULL || di->maxInputChannels < 1) {
            set_error(a, "Invalid input device");
            return -1;
        }
        a->in_channels = di->maxInputChannels >= 2 ? 2 : 1;
        in_params.device = in_dev;
        in_params.channelCount = a->in_channels;
        in_params.sampleFormat = paFloat32;
        in_params.suggestedLatency = di->defaultHighInputLatency > 0
                                         ? di->defaultHighInputLatency * 0.5
                                         : di->defaultLowInputLatency;
        if (in_params.suggestedLatency < di->defaultLowInputLatency) {
            in_params.suggestedLatency = di->defaultLowInputLatency;
        }
        in_params.hostApiSpecificStreamInfo = NULL;
        in_ptr = &in_params;
    } else {
        a->in_channels = 0;
    }

    a->out_stream = NULL;
    a->in_stream = NULL;
    a->duplex_stream = NULL;

    /*
     * Prefer full-duplex when both devices are set: sample-accurate monitor,
     * no FIFO jitter. Fall back to separate streams for mixed devices/hosts.
     */
    if (in_ptr && out_ptr) {
        err = Pa_OpenStream(&a->duplex_stream,
                            in_ptr,
                            out_ptr,
                            sr,
                            (unsigned long)a->frames_per_buffer,
                            paClipOff,
                            duplex_callback,
                            a);
        if (err != paNoError) {
            a->duplex_stream = NULL;
            /* fall through to separate streams */
        }
    }

    if (!a->duplex_stream) {
        if (out_ptr) {
            err = Pa_OpenStream(&a->out_stream,
                                NULL,
                                out_ptr,
                                sr,
                                (unsigned long)a->frames_per_buffer,
                                paClipOff,
                                out_callback,
                                a);
            if (err != paNoError) {
                set_pa_error(a, "Open output", err);
                a->out_stream = NULL;
                return -1;
            }
        }

        if (in_ptr) {
            err = Pa_OpenStream(&a->in_stream,
                                in_ptr,
                                NULL,
                                sr,
                                (unsigned long)a->frames_per_buffer,
                                paClipOff,
                                in_callback,
                                a);
            if (err != paNoError) {
                set_pa_error(a, "Open input", err);
                if (a->out_stream) {
                    Pa_CloseStream(a->out_stream);
                    a->out_stream = NULL;
                }
                a->in_stream = NULL;
                return -1;
            }
        }
    }

    if (a->duplex_stream) {
        err = Pa_StartStream(a->duplex_stream);
        if (err != paNoError) {
            set_pa_error(a, "Start duplex", err);
            gk_audio_stop(a);
            return -1;
        }
    } else {
        /* Start capture first so the monitor FIFO has samples before playback. */
        if (a->in_stream) {
            err = Pa_StartStream(a->in_stream);
            if (err != paNoError) {
                set_pa_error(a, "Start input", err);
                gk_audio_stop(a);
                return -1;
            }
            /* Prime FIFO with a couple of silent buffers worth of headroom via wait */
            Pa_Sleep(8);
        }
        if (a->out_stream) {
            err = Pa_StartStream(a->out_stream);
            if (err != paNoError) {
                set_pa_error(a, "Start output", err);
                gk_audio_stop(a);
                return -1;
            }
        }
    }

    a->running = 1;
    set_error(a, NULL);
    return 0;
}

int gk_audio_stop(gk_audio *a)
{
    if (a == NULL) {
        return -1;
    }
    if (a->duplex_stream) {
        Pa_StopStream(a->duplex_stream);
        Pa_CloseStream(a->duplex_stream);
        a->duplex_stream = NULL;
    }
    if (a->out_stream) {
        Pa_StopStream(a->out_stream);
        Pa_CloseStream(a->out_stream);
        a->out_stream = NULL;
    }
    if (a->in_stream) {
        Pa_StopStream(a->in_stream);
        Pa_CloseStream(a->in_stream);
        a->in_stream = NULL;
    }
    mon_fifo_reset(&a->mon);
    a->running = 0;
    return 0;
}

int gk_audio_is_running(const gk_audio *a)
{
    return a && a->running;
}

void gk_audio_set_wave(gk_audio *a, gk_wave w)
{
    if (a == NULL || w < 0 || w >= GK_WAVE_COUNT) {
        return;
    }
    pthread_mutex_lock(&a->lock);
    a->wave = w;
    pthread_mutex_unlock(&a->lock);
}

void gk_audio_set_freq(gk_audio *a, float hz)
{
    if (a == NULL) {
        return;
    }
    if (hz < 1.0f) {
        hz = 1.0f;
    }
    if (hz > 20000.0f) {
        hz = 20000.0f;
    }
    pthread_mutex_lock(&a->lock);
    a->freq = hz;
    pthread_mutex_unlock(&a->lock);
}

void gk_audio_set_amp(gk_audio *a, float amp)
{
    if (a == NULL) {
        return;
    }
    if (amp < 0.0f) {
        amp = 0.0f;
    }
    if (amp > 1.0f) {
        amp = 1.0f;
    }
    pthread_mutex_lock(&a->lock);
    a->amp = amp;
    pthread_mutex_unlock(&a->lock);
}

void gk_audio_set_gen_enabled(gk_audio *a, int on)
{
    if (a == NULL) {
        return;
    }
    pthread_mutex_lock(&a->lock);
    a->gen_enabled = on ? 1 : 0;
    pthread_mutex_unlock(&a->lock);
}

void gk_audio_set_monitor(gk_audio *a, int on)
{
    if (a == NULL) {
        return;
    }
    pthread_mutex_lock(&a->lock);
    a->monitor = on ? 1 : 0;
    pthread_mutex_unlock(&a->lock);
}

void gk_audio_set_monitor_gain(gk_audio *a, float g)
{
    if (a == NULL) {
        return;
    }
    if (g < 0.0f) {
        g = 0.0f;
    }
    if (g > 1.0f) {
        g = 1.0f;
    }
    pthread_mutex_lock(&a->lock);
    a->monitor_gain = g;
    pthread_mutex_unlock(&a->lock);
}

gk_wave gk_audio_get_wave(const gk_audio *a)
{
    return a ? a->wave : GK_WAVE_SINE;
}

float gk_audio_get_freq(const gk_audio *a)
{
    return a ? a->freq : 440.0f;
}

float gk_audio_get_amp(const gk_audio *a)
{
    return a ? a->amp : 0.2f;
}

int gk_audio_get_gen_enabled(const gk_audio *a)
{
    return a ? a->gen_enabled : 0;
}

int gk_audio_get_monitor(const gk_audio *a)
{
    return a ? a->monitor : 0;
}

double gk_audio_sample_rate(const gk_audio *a)
{
    return a ? a->sample_rate : 0.0;
}

int gk_audio_frames_per_buffer(const gk_audio *a)
{
    return a ? a->frames_per_buffer : 0;
}

unsigned long gk_audio_xruns(const gk_audio *a)
{
    return a ? a->xruns : 0;
}

size_t gk_audio_pull_scope(gk_audio *a, float *out, size_t n, gk_scope_source src)
{
    size_t i;
    if (a == NULL || out == NULL || n == 0) {
        return 0;
    }
    if (src == GK_SRC_GEN) {
        return gk_ringbuf_read_latest(&a->ring_gen, out, n);
    }
    if (src == GK_SRC_INPUT) {
        return gk_ringbuf_read_latest(&a->ring_in, out, n);
    }
    /* Mix */
    {
        float *g = (float *)malloc(n * sizeof(float));
        float *inp = (float *)malloc(n * sizeof(float));
        size_t ng, ni, use;
        if (g == NULL || inp == NULL) {
            free(g);
            free(inp);
            return 0;
        }
        ng = gk_ringbuf_read_latest(&a->ring_gen, g, n);
        ni = gk_ringbuf_read_latest(&a->ring_in, inp, n);
        use = ng > ni ? ng : ni;
        if (use > n) {
            use = n;
        }
        for (i = 0; i < use; i++) {
            float gv = (i < ng) ? g[i] : 0.0f;
            float iv = (i < ni) ? inp[i] : 0.0f;
            out[i] = 0.5f * (gv + iv);
        }
        free(g);
        free(inp);
        return use;
    }
}

const char *gk_audio_last_error(const gk_audio *a)
{
    if (a == NULL || a->last_error[0] == '\0') {
        return NULL;
    }
    return a->last_error;
}
