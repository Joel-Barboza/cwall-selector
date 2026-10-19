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
#include <stdlib.h>
#include <dirent.h>

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
    struct wl_seat *wl_seat;
    struct wl_keyboard *wl_keyboard;
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
    /* Buffer */
    struct wl_buffer *buffer;
    uint32_t *data;
    /* Images Dir */
    DIR *dir;
    char **image_uris;
    int image_uris_count;

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

static void
create_wl_shm(struct client_state *state)
{
    int stride = state->width * 4;
    int size = stride * state->height;

    int fd = allocate_shm_file(size);
    if (fd == -1) {
        return;
    }

    state->data = mmap(NULL, size,
            PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (state->data == MAP_FAILED) {
        close(fd);
        return;
    }

    struct wl_shm_pool *pool = wl_shm_create_pool(state->wl_shm, fd, size);
    state->buffer = wl_shm_pool_create_buffer(pool,
        0, state->width, state->height, stride, WL_SHM_FORMAT_ARGB8888);
    wl_buffer_add_listener(state->buffer, &wl_buffer_listener, NULL);
    wl_shm_pool_destroy(pool);
    close(fd);
    //return state->buffer;
}

static void
draw_image(struct client_state *state)
{
    // char *uri = "images/1344542.ppm";
    int scale = 8;

    FILE *fptr = fopen(state->image_uris[0], "r");
    if (!fptr) {
        printf("Error: No se pudo abrir el archivo PPM\n");
        return;
    }

    char magic_number[4];
    int w, h, m;
    if (fscanf(fptr, "%3s %d %d %d", magic_number, &w, &h, &m) != 4) {
        printf("Error: Encabezado PPM inválido\n");
        fclose(fptr);
        return;
    }
    fgetc(fptr); // consume newline after header

    int offset_x = state->width / 2 - (w / scale) * 0.5;
    int offset_y = state->height - w / scale - 10;
    typedef struct {
        unsigned char r, g, b;
    } pixel;
    pixel currentPix;

    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            if (fread(&currentPix, 3, 1, fptr) != 1) {
                break;
            }

            if (y % scale == 0 && x % scale == 0) {
                // Apply scale and add the starting offsets
                int target_y = (y / scale) + offset_y;
                int target_x = (x / scale) + offset_x;

                // Ensure we stay within screen boundaries
                if (target_y >= 0 && target_y < state->height &&
                    target_x >= 0 && target_x < state->width) {

                    uint32_t rgb_color = 0xFF000000 |
                                         ((uint32_t)currentPix.r << 16) |
                                         ((uint32_t)currentPix.g << 8)  |
                                         (uint32_t)currentPix.b;

                    state->data[target_y * state->width + target_x] = rgb_color;
                    }
            }
        }
    }

    fclose(fptr);
}


static void
draw_frame(struct client_state *state)
{
    int stride = state->width * 4;
    int size = stride * state->height;

    uint32_t rgb_color = 0xAA000000;
    for (int y = 0; y < state->height; ++y) {
        for (int x = 0; x < state->width; ++x) {
            state->data[y * state->width + x] = rgb_color;
        }
    }
}

static void layer_surface_configure(void *data,
        struct zwlr_layer_surface_v1 *surface,
        uint32_t serial, uint32_t w, uint32_t h) {
    struct client_state *state = data;
    zwlr_layer_surface_v1_ack_configure(surface, serial);
    state->width = w;
    state->height = h;

    create_wl_shm(state);
    printf("Configurando capa a tamaño: %d x %d\n", w, h);
    draw_frame(state);
    draw_image(state);
    if (!state->buffer) {
        fprintf(stderr, "Error: No se pudo dibujar el frame en layer_surface\n");
        return;
    }
    wl_surface_attach(state->wl_surface, state->buffer, 0, 0);
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

static void wl_keyboard_keymap(void *data, struct wl_keyboard *wl_keyboard,
        uint32_t format, int32_t fd, uint32_t size) {
    // Who cares
}

static void wl_keyboard_enter(void *data, struct wl_keyboard *wl_keyboard,
        uint32_t serial, struct wl_surface *surface, struct wl_array *keys) {
    //wlr_log(WLR_DEBUG, "Keyboard enter");
    printf("keyboard enter\n");
}

static void wl_keyboard_leave(void *data, struct wl_keyboard *wl_keyboard,
        uint32_t serial, struct wl_surface *surface) {
    //wlr_log(WLR_DEBUG, "Keyboard leave");
    printf("keyboard leave\n");
}

static void wl_keyboard_key(void *data, struct wl_keyboard *wl_keyboard,
        uint32_t serial, uint32_t time, uint32_t key, uint32_t state) {
    //wlr_log(WLR_DEBUG, "Key event: %d %d", key, state);
    // printf("Key event: %d %d", key, state);
    struct client_state *client_state = data;
    // 28 is enter
    if (key == 28)
    {
        client_state->running = false;
    }
}

static void wl_keyboard_modifiers(void *data, struct wl_keyboard *wl_keyboard,
        uint32_t serial, uint32_t mods_depressed, uint32_t mods_latched,
        uint32_t mods_locked, uint32_t group) {
    // Who cares
}

static void wl_keyboard_repeat_info(void *data, struct wl_keyboard *wl_keyboard,
        int32_t rate, int32_t delay) {
    // Who cares
}

static struct wl_keyboard_listener keyboard_listener = {
    .keymap = wl_keyboard_keymap,
    .enter = wl_keyboard_enter,
    .leave = wl_keyboard_leave,
    .key = wl_keyboard_key,
    .modifiers = wl_keyboard_modifiers,
    .repeat_info = wl_keyboard_repeat_info,
};

static void seat_handle_capabilities(void *data, struct wl_seat *wl_seat,
        enum wl_seat_capability caps) {
    struct client_state *state = data;
    if ((caps & WL_SEAT_CAPABILITY_KEYBOARD)) {
        state->wl_keyboard = wl_seat_get_keyboard(wl_seat);
        wl_keyboard_add_listener(state->wl_keyboard, &keyboard_listener, state);
    }
}

static void seat_handle_name(void *data, struct wl_seat *wl_seat,
        const char *name) {
    // Who cares
}

static struct wl_seat_listener seat_listener = {
    .capabilities = seat_handle_capabilities,
    .name = seat_handle_name,
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
    } else if (strcmp(interface, wl_seat_interface.name) == 0) {
        state->wl_seat = wl_registry_bind(wl_registry, name,
                &wl_seat_interface, 1);
        wl_seat_add_listener(state->wl_seat, &seat_listener, state);
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
    state.image_uris = NULL;
    state.image_uris_count = 0;
    state.dir = opendir("./images");
    struct dirent *d;
    if (state.dir) {
        while ((d = readdir(state.dir)) != NULL) {
            char *ext = strrchr(d->d_name, '.');
            if (ext && strcmp(ext, ".ppm") == 0)
            {
                int nuevo_tamano = state.image_uris_count + 1;
                char **temporal = realloc(state.image_uris, nuevo_tamano * sizeof(char *));

                if (temporal == NULL) {
                    perror("Error al realocar memoria para el arreglo");
                    break;
                }
                state.image_uris = temporal;

                size_t largo_total = strlen("./images/") + strlen(d->d_name) + 1;
                state.image_uris[state.image_uris_count] = (char *)malloc(largo_total);

                if (state.image_uris[state.image_uris_count] != NULL) {
                    snprintf(state.image_uris[state.image_uris_count], largo_total, "./images/%s", d->d_name);

                    state.image_uris_count++;
                }
            }
        }
        closedir(state.dir);
    }
    printf("Se encontraron %d imagenes:\n", state.image_uris_count);
    for (int i = 0; i < state.image_uris_count; i++) {
        printf("Imagen [%d]: %s\n", i, state.image_uris[i]);
    }

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
    zwlr_layer_surface_v1_set_keyboard_interactivity(state.layer_surface, 2);

    zwlr_layer_surface_v1_set_size(state.layer_surface, 0, 0);
    zwlr_layer_surface_v1_add_listener(state.layer_surface,
            &layer_surface_listener, &state);
    wl_surface_commit(state.wl_surface);

    while (state.running && wl_display_dispatch(state.wl_display) != -1) {
        // printf("running\n");
        /* This space deliberately left blank */
    }

    int size = state.width * 4 * state.height;
    if (state.data) {
        munmap(state.data, size);
    }

    for (int i = 0; i < state.image_uris_count; i++) {
        free(state.image_uris[i]);
    }
    free(state.image_uris);

    zwlr_layer_surface_v1_destroy(state.layer_surface);
    wl_surface_destroy(state.wl_surface);
    wl_display_disconnect(state.wl_display);

    return 0;
}
