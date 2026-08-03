#include "ringbuf.h"

#include <stdlib.h>
#include <string.h>

int gk_ringbuf_init(gk_ringbuf *r, size_t capacity)
{
    if (r == NULL || capacity == 0) {
        return -1;
    }
    memset(r, 0, sizeof(*r));
    r->data = (float *)calloc(capacity, sizeof(float));
    if (r->data == NULL) {
        return -1;
    }
    r->cap = capacity;
    if (pthread_mutex_init(&r->lock, NULL) != 0) {
        free(r->data);
        r->data = NULL;
        return -1;
    }
    r->lock_inited = 1;
    return 0;
}

void gk_ringbuf_free(gk_ringbuf *r)
{
    if (r == NULL) {
        return;
    }
    if (r->lock_inited) {
        pthread_mutex_destroy(&r->lock);
        r->lock_inited = 0;
    }
    free(r->data);
    memset(r, 0, sizeof(*r));
}

void gk_ringbuf_reset(gk_ringbuf *r)
{
    if (r == NULL || r->data == NULL) {
        return;
    }
    if (r->lock_inited) {
        pthread_mutex_lock(&r->lock);
    }
    memset(r->data, 0, r->cap * sizeof(float));
    r->wpos = 0;
    r->filled = 0;
    if (r->lock_inited) {
        pthread_mutex_unlock(&r->lock);
    }
}

void gk_ringbuf_write(gk_ringbuf *r, const float *samples, size_t n)
{
    size_t i;
    if (r == NULL || r->data == NULL || samples == NULL || n == 0) {
        return;
    }
    if (r->lock_inited) {
        pthread_mutex_lock(&r->lock);
    }
    for (i = 0; i < n; i++) {
        r->data[r->wpos] = samples[i];
        r->wpos++;
        if (r->wpos >= r->cap) {
            r->wpos = 0;
        }
        if (r->filled < r->cap) {
            r->filled++;
        }
    }
    if (r->lock_inited) {
        pthread_mutex_unlock(&r->lock);
    }
}

size_t gk_ringbuf_read_latest(gk_ringbuf *r, float *out, size_t n)
{
    size_t available;
    size_t start;
    size_t i;
    if (r == NULL || r->data == NULL || out == NULL || n == 0) {
        return 0;
    }
    if (r->lock_inited) {
        pthread_mutex_lock(&r->lock);
    }
    available = r->filled;
    if (available == 0) {
        if (r->lock_inited) {
            pthread_mutex_unlock(&r->lock);
        }
        return 0;
    }
    if (n > available) {
        n = available;
    }
    if (r->wpos >= n) {
        start = r->wpos - n;
    } else {
        start = r->cap - (n - r->wpos);
    }
    for (i = 0; i < n; i++) {
        size_t idx = start + i;
        if (idx >= r->cap) {
            idx -= r->cap;
        }
        out[i] = r->data[idx];
    }
    if (r->lock_inited) {
        pthread_mutex_unlock(&r->lock);
    }
    return n;
}
