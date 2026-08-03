#include "config.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

void gk_config_defaults(gk_config *c)
{
    if (c == NULL) {
        return;
    }
    memset(c, 0, sizeof(*c));
    c->win_w = 900;
    c->win_h = 560;
    c->wave = 0; /* sine */
    c->freq = 440.0f;
    c->amp = 0.2f;
    c->gen_enabled = 1;
    c->monitor = 0;
    c->scope_source = 0;
    c->timebase_ms = 20.0f;
    c->scope_gain = 1.0f;
}

static void config_path(char *buf, size_t bufsz)
{
    const char *xdg = getenv("XDG_CONFIG_HOME");
    if (xdg != NULL && xdg[0] != '\0') {
        snprintf(buf, bufsz, "%s/gk-oscillator/config", xdg);
    } else {
        const char *home = getenv("HOME");
        if (home == NULL || home[0] == '\0') {
            home = ".";
        }
        snprintf(buf, bufsz, "%s/.config/gk-oscillator/config", home);
    }
}

static int mkdir_p_parent(const char *path)
{
    char tmp[512];
    char *slash;
    size_t n = strlen(path);
    if (n >= sizeof(tmp)) {
        return -1;
    }
    memcpy(tmp, path, n + 1);
    slash = strrchr(tmp, '/');
    if (slash == NULL) {
        return 0;
    }
    *slash = '\0';
    if (tmp[0] == '\0') {
        return 0;
    }
    if (mkdir(tmp, 0700) == 0 || errno == EEXIST) {
        return 0;
    }
    slash = strrchr(tmp, '/');
    if (slash != NULL) {
        *slash = '\0';
        if (mkdir(tmp, 0700) == 0 || errno == EEXIST) {
            *slash = '/';
            return (mkdir(tmp, 0700) == 0 || errno == EEXIST) ? 0 : -1;
        }
    }
    return -1;
}

void gk_config_load(gk_config *c)
{
    char path[512];
    FILE *f;
    char line[512];

    gk_config_defaults(c);
    if (c == NULL) {
        return;
    }
    config_path(path, sizeof(path));
    f = fopen(path, "r");
    if (f == NULL) {
        return;
    }
    while (fgets(line, sizeof(line), f)) {
        char *eq, *key, *val, *nl;
        nl = strchr(line, '\n');
        if (nl) {
            *nl = '\0';
        }
        if (line[0] == '#' || line[0] == '\0') {
            continue;
        }
        eq = strchr(line, '=');
        if (eq == NULL) {
            continue;
        }
        *eq = '\0';
        key = line;
        val = eq + 1;
        if (strcmp(key, "win_x") == 0) {
            c->win_x = atoi(val);
            c->has_geometry = 1;
        } else if (strcmp(key, "win_y") == 0) {
            c->win_y = atoi(val);
            c->has_geometry = 1;
        } else if (strcmp(key, "win_w") == 0) {
            c->win_w = atoi(val);
            c->has_geometry = 1;
        } else if (strcmp(key, "win_h") == 0) {
            c->win_h = atoi(val);
            c->has_geometry = 1;
        } else if (strcmp(key, "input_name") == 0) {
            snprintf(c->input_name, sizeof(c->input_name), "%s", val);
        } else if (strcmp(key, "output_name") == 0) {
            snprintf(c->output_name, sizeof(c->output_name), "%s", val);
        } else if (strcmp(key, "wave") == 0) {
            c->wave = atoi(val);
        } else if (strcmp(key, "freq") == 0) {
            c->freq = (float)atof(val);
        } else if (strcmp(key, "amp") == 0) {
            c->amp = (float)atof(val);
        } else if (strcmp(key, "gen_enabled") == 0) {
            c->gen_enabled = atoi(val);
        } else if (strcmp(key, "monitor") == 0) {
            c->monitor = atoi(val);
        } else if (strcmp(key, "scope_source") == 0) {
            c->scope_source = atoi(val);
        } else if (strcmp(key, "timebase_ms") == 0) {
            c->timebase_ms = (float)atof(val);
        } else if (strcmp(key, "scope_gain") == 0) {
            c->scope_gain = (float)atof(val);
        }
    }
    fclose(f);
}

void gk_config_save(const gk_config *c)
{
    char path[512];
    FILE *f;
    if (c == NULL) {
        return;
    }
    config_path(path, sizeof(path));
    if (mkdir_p_parent(path) != 0) {
        return;
    }
    f = fopen(path, "w");
    if (f == NULL) {
        return;
    }
    fprintf(f, "win_x=%d\n", c->win_x);
    fprintf(f, "win_y=%d\n", c->win_y);
    fprintf(f, "win_w=%d\n", c->win_w);
    fprintf(f, "win_h=%d\n", c->win_h);
    fprintf(f, "input_name=%s\n", c->input_name);
    fprintf(f, "output_name=%s\n", c->output_name);
    fprintf(f, "wave=%d\n", c->wave);
    fprintf(f, "freq=%.3f\n", (double)c->freq);
    fprintf(f, "amp=%.4f\n", (double)c->amp);
    fprintf(f, "gen_enabled=%d\n", c->gen_enabled);
    fprintf(f, "monitor=%d\n", c->monitor);
    fprintf(f, "scope_source=%d\n", c->scope_source);
    fprintf(f, "timebase_ms=%.2f\n", (double)c->timebase_ms);
    fprintf(f, "scope_gain=%.3f\n", (double)c->scope_gain);
    fclose(f);
}
