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

#include <stdio.h>

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
    struct xdg_surface *xdg_surface;
    struct xdg_toplevel *xdg_toplevel;
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
    const int width = w, height = h;
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
    //struct wl_shm_pool *pool = wl_shm_create_pool(image, fd, size);
    struct wl_buffer *buffer = wl_shm_pool_create_buffer(pool,
        0, width, height, stride, WL_SHM_FORMAT_ARGB8888);
    wl_shm_pool_destroy(pool);
    close(fd);

    typedef struct {
        unsigned char r, g, b;
    } pixel;
    pixel currentPix;

    int n = 8;
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            if (fread(&currentPix, 3, 1, fptr) != 1) {
                printf("Error: Se llegó al final del archivo, pixel (%d, %d)\n", x, y);
                break;
            }
            unsigned char r = currentPix.r;
            unsigned char g = currentPix.g;
            unsigned char b = currentPix.b;

            uint32_t rgb_color = 0xFF000000 |
                                ((uint32_t)r << 16) |
                                ((uint32_t)g << 8)  |
                                (uint32_t)b;
            if (y % n == 0)
            {
                data[y * width/n + x/n] = rgb_color;

            }
        }
    }

    fclose(fptr);
    munmap(data, size);
    wl_buffer_add_listener(buffer, &wl_buffer_listener, NULL);
    return buffer;
}

static void
xdg_toplevel_configure(void *data, struct xdg_toplevel *xdg_toplevel,
        int32_t width, int32_t height, struct wl_array *states)
{
    /* This space deliberately left blank */
}

static void
xdg_toplevel_close(void *data, struct xdg_toplevel *xdg_toplevel)
{
    // Al recibir el evento de cerrar, rompemos el bucle cambiando una condición
    bool *running = data;
    *running = false;
}

static const struct xdg_toplevel_listener xdg_toplevel_listener = {
    .configure = xdg_toplevel_configure,
    .close = xdg_toplevel_close,
};

static void
xdg_surface_configure(void *data,
        struct xdg_surface *xdg_surface, uint32_t serial)
{
    struct client_state *state = data;
    xdg_surface_ack_configure(xdg_surface, serial);

    struct wl_buffer *buffer = draw_frame(state);
    wl_surface_attach(state->wl_surface, buffer, 0, 0);
    wl_surface_commit(state->wl_surface);
}

static const struct xdg_surface_listener xdg_surface_listener = {
    .configure = xdg_surface_configure,
};

static void
xdg_wm_base_ping(void *data, struct xdg_wm_base *xdg_wm_base, uint32_t serial)
{
    xdg_wm_base_pong(xdg_wm_base, serial);
}

static const struct xdg_wm_base_listener xdg_wm_base_listener = {
    .ping = xdg_wm_base_ping,
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
    } else if (strcmp(interface, xdg_wm_base_interface.name) == 0) {
        state->xdg_wm_base = wl_registry_bind(
                wl_registry, name, &xdg_wm_base_interface, 1);
        xdg_wm_base_add_listener(state->xdg_wm_base,
                &xdg_wm_base_listener, state);
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
    bool running = true;

    state.wl_display = wl_display_connect(NULL);
    state.wl_registry = wl_display_get_registry(state.wl_display);
    wl_registry_add_listener(state.wl_registry, &wl_registry_listener, &state);
    wl_display_roundtrip(state.wl_display);

    state.wl_surface = wl_compositor_create_surface(state.wl_compositor);
    state.xdg_surface = xdg_wm_base_get_xdg_surface(
            state.xdg_wm_base, state.wl_surface);
    xdg_surface_add_listener(state.xdg_surface, &xdg_surface_listener, &state);
    state.xdg_toplevel = xdg_surface_get_toplevel(state.xdg_surface);
    xdg_toplevel_add_listener(state.xdg_toplevel, &xdg_toplevel_listener, &running);
    xdg_toplevel_set_title(state.xdg_toplevel, "Example client");
    xdg_toplevel_set_max_size(state.xdg_toplevel, 440, 250);
    xdg_toplevel_set_min_size(state.xdg_toplevel, 440, 250);
    //xdg_surface_set_window_geometry(state.xdg_surface, 600, 500, 400, 250);
    //xdg_toplevel_resize(state.xdg_toplevel);
    //xdg_toplevel_set_fullscreen(state.xdg_toplevel);
    wl_surface_commit(state.wl_surface);

    while (running && wl_display_dispatch(state.wl_display) != -1) {
        /* This space deliberately left blank */
    }

    xdg_toplevel_destroy(state.xdg_toplevel);
    xdg_surface_destroy(state.xdg_surface);
    wl_surface_destroy(state.wl_surface);
    wl_display_disconnect(state.wl_display);

    return 0;
}
    //fprintf(out, "P6 256 256 255\n");
    //printf(out);
    // fgets(type, sizeof(type), fptr);
    // printf("%s", type);

    // char dim[11];
    // fgets(dim, sizeof(dim), fptr);
    // printf("%s", dim);
    // char maxVal[3];
    // fgets(maxVal, sizeof(maxVal), fptr);
    // printf("%s", maxVal);
    // printf("\n");
    // FILE *fptr;
    // fptr = fopen("images/1344542.ppm", "r");
    // char type[10];
    // char magic_number[1];
    // int w, h, m;
    // int red = 0;
    // int green = 0;
    // int blue = 0;
    // fscanf(fptr, "%s %d %d %d", &magic_number, &w, &h, &m);
    // printf("magic_n = %s, width = %d, height = %d, max_colour = %d\n", magic_number, w, h, m);
    // typedef struct {
    //     unsigned char r, g, b;
    // } pixel;
    // pixel currentPix;
    // int i = 0;
    // while (fread(&currentPix, 3, 1, fptr) && i < 10)
    // {
    //
    //     printf("%d %d %d\n", currentPix.r, currentPix.g, currentPix.b);
    //     i++;
    //
    // }
    // printf("\n");
    // fclose(fptr);
    // return 0;
