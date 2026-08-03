#ifndef GK_RINGBUF_H
#define GK_RINGBUF_H

#include <pthread.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Float ring buffer: audio thread writes, UI reads latest window (mutexed). */
typedef struct gk_ringbuf {
    float *data;
    size_t cap;
    size_t wpos; /* next write index, mod cap */
    size_t filled;
    pthread_mutex_t lock;
    int lock_inited;
} gk_ringbuf;

int  gk_ringbuf_init(gk_ringbuf *r, size_t capacity);
void gk_ringbuf_free(gk_ringbuf *r);
void gk_ringbuf_reset(gk_ringbuf *r);
void gk_ringbuf_write(gk_ringbuf *r, const float *samples, size_t n);
/* Copy the most recent n samples into out (oldest→newest). Returns count. */
size_t gk_ringbuf_read_latest(gk_ringbuf *r, float *out, size_t n);

#ifdef __cplusplus
}
#endif

#endif /* GK_RINGBUF_H */
