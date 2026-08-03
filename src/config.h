#ifndef GK_CONFIG_H
#define GK_CONFIG_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct gk_config {
    int win_x, win_y, win_w, win_h;
    int has_geometry;

    char input_name[256];
    char output_name[256];

    int wave;           /* gk_wave */
    float freq;
    float amp;
    int gen_enabled;
    int monitor;

    int scope_source;   /* gk_scope_source */
    float timebase_ms;  /* display window in ms */
    float scope_gain;
} gk_config;

void gk_config_defaults(gk_config *c);
void gk_config_load(gk_config *c);
void gk_config_save(const gk_config *c);

#ifdef __cplusplus
}
#endif

#endif /* GK_CONFIG_H */
