//
// Created by joel on 10/10/26.
//

#define _POSIX_C_SOURCE 200112L
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdbool.h>
#include <string.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>
#include <wayland-client.h>
#include "xdg-shell-client-protocol.h"
#include "wlr-layer-shell-unstable-v1-client-protocol.h"

#include <stdio.h>
#include <stdint.h>

/* Shared memory support code */
static void
randname(char *buf)
{
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    long r = ts.tv_nsec;
    for (int i = 0; i < 6; ++i) {
        buf[i] = 'A'+(r&15)+(r&16)*2;
        r >>= 5;
    }
}

static int
create_shm_file(void)
{
    int retries = 100;
    do {
        char name[] = "/wl_shm-XXXXXX";
        randname(name + sizeof(name) - 7);
        --retries;
        int fd = shm_open(name, O_RDWR | O_CREAT | O_EXCL, 0600);
        if (fd >= 0) {
            shm_unlink(name);
            return fd;
        }
    } while (retries > 0 && errno == EEXIST);
    return -1;
}

static int
allocate_shm_file(size_t size)
{
    int fd = create_shm_file();
    if (fd < 0)
        return -1;
    int ret;
    do {
        ret = ftruncate(fd, size);
    } while (ret < 0 && errno == EINTR);
    if (ret < 0) {
        close(fd);
        return -1;
    }
    return fd;
}

/* Wayland code */
struct client_state {
    /* Globals */
    struct wl_display *wl_display;
    struct wl_registry *wl_registry;
    struct wl_shm *wl_shm;
    struct wl_compositor *wl_compositor;
    struct xdg_wm_base *xdg_wm_base;
    /* Objects */
    struct wl_surface *wl_surface;
    struct zwlr_layer_shell_v1 *layer_shell;
    struct zwlr_layer_surface_v1 *layer_surface;
    struct wl_output *wl_output;
    /* State */
    bool running;
    /* Window */
    uint32_t width, height;

};

static void
wl_buffer_release(void *data, struct wl_buffer *wl_buffer)
{
    /* Sent by the compositor when it's no longer using this buffer */
    wl_buffer_destroy(wl_buffer);
}

static const struct wl_buffer_listener wl_buffer_listener = {
    .release = wl_buffer_release,
};

// static void draw_image(char * uri)
// {
//
// }
//
static struct wl_buffer *
draw_frame(struct client_state *state)
{
    FILE *fptr;
    fptr = fopen("images/1344542.ppm", "r");
    char magic_number[4];
    int w, h, m;
    if (fscanf(fptr, "%3s %d %d %d", magic_number, &w, &h, &m) != 4) {
        printf("Error: Encabezado PPM inválido\n");
        fclose(fptr);
        return NULL;
    }
    fgetc(fptr);
    printf("magic_n = %s, width = %d, height = %d, max_colour = %d\n", magic_number, w, h, m);

    const int width = state->width;
    const int height = state->height;
    // para escalar imagen
    const int image_scale = 1;

    int stride = width * 4;
    int size = stride * height;

    int fd = allocate_shm_file(size);
    if (fd == -1) {
        return NULL;
    }

    uint32_t *data = mmap(NULL, size,
            PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (data == MAP_FAILED) {
        close(fd);
        return NULL;
    }

    struct wl_shm_pool *pool = wl_shm_create_pool(state->wl_shm, fd, size);
    struct wl_buffer *buffer = wl_shm_pool_create_buffer(pool,
        0, width, height, stride, WL_SHM_FORMAT_ARGB8888);
    wl_shm_pool_destroy(pool);
    close(fd);

    typedef struct {
        unsigned char r, g, b;
    } pixel;
    pixel currentPix;

    for (int y = 0; y < state->height; ++y) {
        for (int x = 0; x < state->width; ++x) {
            if (fread(&currentPix, 3, 1, fptr) != 1) {
                printf("Error: Se llegó al final del archivo, pixel (%d, %d)\n", x, y);
                break;
            }
            unsigned char r = currentPix.r;
            unsigned char g = currentPix.g;
            unsigned char b = currentPix.b;

            // uint32_t rgb_color = 0xFF000000 |
            //                     ((uint32_t)r << 16) |
            //                     ((uint32_t)g << 8)  |
            //                     (uint32_t)b;
            uint32_t rgb_color = 0xAA005600;
            if (y % image_scale == 0)
            {
                data[y * width / image_scale + x / image_scale] = rgb_color;

            }
        }
    }

    fclose(fptr);
    munmap(data, size);
    wl_buffer_add_listener(buffer, &wl_buffer_listener, NULL);
    return buffer;
}

static void layer_surface_configure(void *data,
        struct zwlr_layer_surface_v1 *surface,
        uint32_t serial, uint32_t w, uint32_t h) {
    struct client_state *state = data;
    zwlr_layer_surface_v1_ack_configure(surface, serial);
    state->width = w;
    state->height = h;
    printf("Configurando capa a tamaño: %d x %d\n", w, h);
    struct wl_buffer *buffer = draw_frame(state);
    if (!buffer) {
        fprintf(stderr, "Error: No se pudo dibujar el frame en layer_surface\n");
        return;
    }
    wl_surface_attach(state->wl_surface, buffer, 0, 0);
    wl_surface_damage_buffer(state->wl_surface, 0, 0, w, h);
    wl_surface_commit(state->wl_surface);
}


static void layer_surface_closed(void *data,
        struct zwlr_layer_surface_v1 *surface) {
    struct client_state *state = data;
    state->running = false;
}

struct zwlr_layer_surface_v1_listener layer_surface_listener = {
    .configure = layer_surface_configure,
    .closed = layer_surface_closed,
};

static void
registry_global(void *data, struct wl_registry *wl_registry,
        uint32_t name, const char *interface, uint32_t version)
{
    struct client_state *state = data;
    if (strcmp(interface, wl_shm_interface.name) == 0) {
        state->wl_shm = wl_registry_bind(
            wl_registry, name, &wl_shm_interface, 1);
    } else if (strcmp(interface, wl_compositor_interface.name) == 0) {
        state->wl_compositor = wl_registry_bind(
            wl_registry, name, &wl_compositor_interface, 4);
    } else if (strcmp(interface, zwlr_layer_shell_v1_interface.name) == 0)
    {
        state->layer_shell = wl_registry_bind(
            wl_registry, name, &zwlr_layer_shell_v1_interface, version < 4 ? version : 4);
    } else if (strcmp(interface, "wl_output") == 0) {
        state->wl_output = wl_registry_bind(wl_registry, name,
            &wl_output_interface, 1);
    }
}

static void
registry_global_remove(void *data,
        struct wl_registry *wl_registry, uint32_t name)
{
    /* This space deliberately left blank */
}

static const struct wl_registry_listener wl_registry_listener = {
    .global = registry_global,
    .global_remove = registry_global_remove,
};

int
main(int argc, char *argv[])
{
    struct client_state state = { 0 };
    state.running = true;
    enum zwlr_layer_shell_v1_layer layer = ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY;
    const char *namespace = "wallpaper-selector";

    state.wl_display = wl_display_connect(NULL);
    if (state.wl_display == NULL) {
        fprintf(stderr, "Failed to create display\n");
        return 1;
    }
    state.wl_registry = wl_display_get_registry(state.wl_display);
    wl_registry_add_listener(state.wl_registry, &wl_registry_listener, &state);
    wl_display_roundtrip(state.wl_display);

    if (state.wl_compositor == NULL) {
        fprintf(stderr, "wl_compositor not available\n");
        return 1;
    }
    if (state.wl_shm == NULL) {
        fprintf(stderr, "wl_shm not available\n");
        return 1;
    }
    if (state.layer_shell == NULL) {
        fprintf(stderr, "layer_shell not available\n");
        return 1;
    }

    state.wl_surface = wl_compositor_create_surface(state.wl_compositor);

    // Ver main() en swaywm/wlroots/examples/layer-shell.c

    state.layer_surface = zwlr_layer_shell_v1_get_layer_surface(
        state.layer_shell, state.wl_surface, state.wl_output, layer, namespace);

    uint32_t anchors = ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP |
                   ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM |
                   ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT |
                   ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT;

    zwlr_layer_surface_v1_set_anchor(state.layer_surface, anchors);

    zwlr_layer_surface_v1_set_exclusive_zone(state.layer_surface, -1);

    zwlr_layer_surface_v1_set_size(state.layer_surface, 0, 0);
    zwlr_layer_surface_v1_add_listener(state.layer_surface,
            &layer_surface_listener, &state);
    wl_surface_commit(state.wl_surface);

    while (state.running && wl_display_dispatch(state.wl_display) != -1) {
        // printf("running\n");
        /* This space deliberately left blank */
    }

    zwlr_layer_surface_v1_destroy(state.layer_surface);
    wl_surface_destroy(state.wl_surface);
    wl_display_disconnect(state.wl_display);

    return 0;
}
