#include "audio.h"
#include "ringbuf.h"

#include "../third_party/portaudio.h"

#include <math.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define GK_RING_SECONDS 0.12
#define GK_FRAMES       256
#define GK_MAX_CH       2

struct gk_audio {
    int pa_ready;
    int running;

    PaStream *out_stream;
    PaStream *in_stream;

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

    unsigned long xruns;
    char last_error[256];
};

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

static int out_callback(const void *input, void *output,
                        unsigned long frameCount,
                        const PaStreamCallbackTimeInfo *timeInfo,
                        PaStreamCallbackFlags statusFlags,
                        void *userData)
{
    gk_audio *a = (gk_audio *)userData;
    float *out = (float *)output;
    unsigned long i;
    float gen_buf[512];
    float mon_buf[512];
    float freq, amp, mon_gain;
    int gen_on, mon_on, ch;
    size_t mon_n = 0;

    (void)input;
    (void)timeInfo;

    if (statusFlags & (paOutputUnderflow | paOutputOverflow | paInputUnderflow | paInputOverflow)) {
        a->xruns++;
    }

    pthread_mutex_lock(&a->lock);
    freq = a->freq;
    amp = a->amp;
    gen_on = a->gen_enabled;
    mon_on = a->monitor;
    mon_gain = a->monitor_gain;
    pthread_mutex_unlock(&a->lock);

    ch = a->out_channels;
    if (ch < 1) {
        ch = 1;
    }
    if (ch > GK_MAX_CH) {
        ch = GK_MAX_CH;
    }

    if (frameCount > 512) {
        frameCount = 512; /* safety; PA should honor framesPerBuffer */
    }

    /* Latest input window for monitor (aligned as well as separate streams allow). */
    if (mon_on) {
        mon_n = gk_ringbuf_read_latest(&a->ring_in, mon_buf, (size_t)frameCount);
    }

    for (i = 0; i < frameCount; i++) {
        float g = 0.0f;
        float mixed;
        if (gen_on) {
            g = osc_sample(a, freq, amp);
        }
        gen_buf[i] = g;
        mixed = g;
        if (mon_on && mon_n > 0) {
            size_t mi = (i < mon_n) ? i : (mon_n - 1);
            mixed += mon_buf[mi] * mon_gain;
        }
        if (mixed > 1.0f) {
            mixed = 1.0f;
        } else if (mixed < -1.0f) {
            mixed = -1.0f;
        }
        if (ch == 1) {
            out[i] = mixed;
        } else {
            out[i * 2 + 0] = mixed;
            out[i * 2 + 1] = mixed;
        }
    }

    gk_ringbuf_write(&a->ring_gen, gen_buf, (size_t)frameCount);
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

    for (i = 0; i < frameCount; i++) {
        if (ch == 1) {
            mono[i] = in[i];
        } else {
            mono[i] = 0.5f * (in[i * ch] + in[i * ch + 1]);
        }
    }
    gk_ringbuf_write(&a->ring_in, mono, (size_t)frameCount);
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

    ring_cap = (size_t)(sr * GK_RING_SECONDS);
    if (ring_cap < 1024) {
        ring_cap = 1024;
    }
    gk_ringbuf_free(&a->ring_gen);
    gk_ringbuf_free(&a->ring_in);
    if (gk_ringbuf_init(&a->ring_gen, ring_cap) != 0 ||
        gk_ringbuf_init(&a->ring_in, ring_cap) != 0) {
        set_error(a, "Ring buffer alloc failed");
        return -1;
    }

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
        out_params.suggestedLatency = di->defaultLowOutputLatency;
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
        in_params.suggestedLatency = di->defaultLowInputLatency;
        in_params.hostApiSpecificStreamInfo = NULL;
        in_ptr = &in_params;
    } else {
        a->in_channels = 0;
    }

    /* Prefer separate streams so input/output can be different devices. */
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

    if (a->out_stream) {
        err = Pa_StartStream(a->out_stream);
        if (err != paNoError) {
            set_pa_error(a, "Start output", err);
            gk_audio_stop(a);
            return -1;
        }
    }
    if (a->in_stream) {
        err = Pa_StartStream(a->in_stream);
        if (err != paNoError) {
            set_pa_error(a, "Start input", err);
            gk_audio_stop(a);
            return -1;
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
