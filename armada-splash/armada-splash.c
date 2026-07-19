// Boot splash: centers an image with status text below it, from a status file.
// fbdev backend for early boot (never touches the GPU); x11 backend for the
// gamescope phase (gamescope shows it via the fallback-appid patch, yields to
// Steam). Auto-selects x11 when an X display is present, else fbdev.
//
// Image = "ASP1" container: "ASP1" | u32 width LE | u32 height LE | W*H*4 BGRA.
// Status file: one line per row centered below the image; leading '!' = red.

#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <linux/fb.h>
#include <linux/kd.h>
#include <poll.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/timerfd.h>
#include <unistd.h>

#include "font8x8_basic.h"
#include "stb_truetype.h"   // declarations only; implementation in stb_impl.c

static volatile sig_atomic_t running = 1;
static void on_signal(int s) { (void)s; running = 0; }

// For fbdev 90/270 rotation, SW/SH are the panel dimensions swapped.
static int SW, SH;
static uint32_t *shadow;      // SW*SH ARGB8888
static uint32_t bg = 0xFF000000;

static uint32_t *img;
static int img_w, img_h;

static int text_scale = 2;

static char g_cur[512];
static char g_shown[512];

static void put_shadow(int x, int y, uint32_t c) {
    if (x >= 0 && x < SW && y >= 0 && y < SH) shadow[y * SW + x] = c;
}

static void draw_glyph(int ch, int px, int py, int s, uint32_t color) {
    if (ch < 0 || ch > 127) ch = '?';
    const char *g = font8x8_basic[ch];
    for (int row = 0; row < 8; row++)
        for (int col = 0; col < 8; col++)
            if (g[row] & (1 << col))
                for (int dy = 0; dy < s; dy++)
                    for (int dx = 0; dx < s; dx++)
                        put_shadow(px + col * s + dx, py + row * s + dy, color);
}

static int text_px_width(const char *s) { return (int)strlen(s) * 8 * text_scale; }

static void draw_text_centered(const char *s, int y, int scale, uint32_t color) {
    int x = (SW - text_px_width(s)) / 2;
    if (x < 0) x = 0;
    for (const char *p = s; *p && *p != '\n'; p++) {
        draw_glyph((unsigned char)*p, x, y, scale, color);
        x += 8 * scale;
    }
}

static stbtt_fontinfo g_ttf;
static unsigned char *g_ttf_buf;
static int g_ttf_ok = 0;
static float g_ttf_scale;
static int g_ttf_ascent, g_ttf_descent, g_ttf_linegap;
static int g_text_px = 32;
static int g_text_px_req = 0; // --text-height (0 = auto from width)
static int g_gap = -1;   // px between image and text; <0 = auto (half text height)
static int g_layout = 0; // 0 = centered group, 1 = logo centered + text bottom
static uint32_t g_appid = 0x41524D41u; // STEAM_GAME appid; match GAMESCOPE_FALLBACK_APPID

static inline void blend_shadow(int x, int y, uint32_t color, int cov) {
    if (cov <= 0 || x < 0 || x >= SW || y < 0 || y >= SH) return;
    if (cov > 255) cov = 255;
    uint32_t d = shadow[y * SW + x];
    int sr = (color >> 16) & 255, sg = (color >> 8) & 255, sb = color & 255;
    int dr = (d >> 16) & 255, dg = (d >> 8) & 255, db = d & 255;
    shadow[y * SW + x] = 0xFF000000 |
        (((sr * cov + dr * (255 - cov)) / 255) << 16) |
        (((sg * cov + dg * (255 - cov)) / 255) << 8) |
        ((sb * cov + db * (255 - cov)) / 255);
}

static int load_font(const char *path, int px) {
    if (!path || !*path) return 0;
    int fd = open(path, O_RDONLY);
    if (fd < 0) return 0;
    struct stat st;
    if (fstat(fd, &st) != 0 || st.st_size <= 0) { close(fd); return 0; }
    g_ttf_buf = malloc(st.st_size);
    size_t off = 0; ssize_t r;
    while (off < (size_t)st.st_size && (r = read(fd, g_ttf_buf + off, st.st_size - off)) > 0) off += r;
    close(fd);
    if (off != (size_t)st.st_size ||
        !stbtt_InitFont(&g_ttf, g_ttf_buf, stbtt_GetFontOffsetForIndex(g_ttf_buf, 0))) {
        free(g_ttf_buf); g_ttf_buf = NULL; return 0;
    }
    g_ttf_scale = stbtt_ScaleForPixelHeight(&g_ttf, (float)px);
    stbtt_GetFontVMetrics(&g_ttf, &g_ttf_ascent, &g_ttf_descent, &g_ttf_linegap);
    g_text_px = px;
    return (g_ttf_ok = 1);
}

static int tt_text_w(const char *s) {
    float w = 0;
    for (const char *p = s; *p; p++) {
        int adv, lsb;
        stbtt_GetCodepointHMetrics(&g_ttf, (unsigned char)*p, &adv, &lsb);
        w += adv * g_ttf_scale;
        if (p[1]) w += stbtt_GetCodepointKernAdvance(&g_ttf, (unsigned char)*p, (unsigned char)p[1]) * g_ttf_scale;
    }
    return (int)(w + 0.5f);
}

static void tt_blit(const char *s, float x, int baseline, uint32_t color, int num, int den) {
    for (const char *p = s; *p; p++) {
        int c = (unsigned char)*p, ix0, iy0, ix1, iy1;
        stbtt_GetCodepointBitmapBox(&g_ttf, c, g_ttf_scale, g_ttf_scale, &ix0, &iy0, &ix1, &iy1);
        int gw = ix1 - ix0, gh = iy1 - iy0;
        if (gw > 0 && gh > 0) {
            unsigned char *bmp = malloc((size_t)gw * gh);
            if (bmp) {
                stbtt_MakeCodepointBitmap(&g_ttf, bmp, gw, gh, gw, g_ttf_scale, g_ttf_scale, c);
                int gx = (int)(x + 0.5f) + ix0, gy = baseline + iy0;
                for (int j = 0; j < gh; j++)
                    for (int i = 0; i < gw; i++)
                        blend_shadow(gx + i, gy + j, color, bmp[j * gw + i] * num / den);
                free(bmp);
            }
        }
        int adv, lsb;
        stbtt_GetCodepointHMetrics(&g_ttf, c, &adv, &lsb);
        x += adv * g_ttf_scale;
        if (p[1]) x += stbtt_GetCodepointKernAdvance(&g_ttf, c, (unsigned char)p[1]) * g_ttf_scale;
    }
}

static void tt_draw_centered(const char *s, int baseline, uint32_t color) {
    int x = (SW - tt_text_w(s)) / 2; if (x < 0) x = 0;
    int sh = g_text_px / 20; if (sh < 1) sh = 1;
    tt_blit(s, x + sh, baseline + sh, 0xFF000000, 150, 255);   // soft drop shadow
    tt_blit(s, x, baseline, color, 255, 255);                  // text
}

static void compose(const char *status) {
    for (int i = 0; i < SW * SH; i++) shadow[i] = bg;

    char buf[512];
    snprintf(buf, sizeof buf, "%s", status ? status : "");
    char *lines[8]; int n = 0;
    for (char *p = buf; *p && n < 8; ) {
        lines[n++] = p;
        char *nl = strchr(p, '\n');
        if (!nl) break;
        *nl = 0; p = nl + 1;
    }
    if (n && lines[n - 1][0] == 0) n--;

    int line_h, ascent_px = 0;
    if (g_ttf_ok) {
        line_h = (int)((g_ttf_ascent - g_ttf_descent + g_ttf_linegap) * g_ttf_scale + 0.5f);
        ascent_px = (int)(g_ttf_ascent * g_ttf_scale + 0.5f);
    } else {
        line_h = 8 * text_scale + text_scale;
    }
    int text_block_h = n * line_h;
    int logo_x = (SW - img_w) / 2, logo_y, text_top;

    if (g_layout == 1) {
        // split layout: --gap is the bottom margin.
        logo_y = (SH - img_h) / 2;
        int margin = g_gap >= 0 ? g_gap : g_text_px;
        text_top = SH - margin - text_block_h;
    } else {
        int gap = img_h ? (g_gap >= 0 ? g_gap : (g_ttf_ok ? g_text_px / 2 : text_scale * 4)) : 0;
        int top = (SH - (img_h + gap + text_block_h)) / 2;
        logo_y = top;
        text_top = top + img_h + gap;
    }
    if (logo_y < 0) logo_y = 0;
    if (text_top < 0) text_top = 0;

    if (img_w && img_h)
        for (int y = 0; y < img_h; y++)
            for (int x = 0; x < img_w; x++)
                put_shadow(logo_x + x, logo_y + y, img[y * img_w + x]);

    int ty = text_top;
    for (int i = 0; i < n; i++) {
        const char *ln = lines[i];
        uint32_t col = 0xFFFFFFFF;
        if (*ln == '!') { col = 0xFFFF4040; ln++; }
        if (g_ttf_ok) tt_draw_centered(ln, ty + ascent_px, col);
        else draw_text_centered(ln, ty, text_scale, col);
        ty += line_h;
    }
}

static int load_image(const char *path) {
    if (!path || !*path) return 0;
    int fd = open(path, O_RDONLY);
    if (fd < 0) { fprintf(stderr, "armada-splash: cannot open %s\n", path); return 0; }
    uint8_t hdr[12];
    if (read(fd, hdr, 12) != 12 || memcmp(hdr, "ASP1", 4) != 0) {
        fprintf(stderr, "armada-splash: %s is not an ASP1 image\n", path);
        close(fd); return 0;
    }
    img_w = hdr[4] | hdr[5] << 8 | hdr[6] << 16 | (uint32_t)hdr[7] << 24;
    img_h = hdr[8] | hdr[9] << 8 | hdr[10] << 16 | (uint32_t)hdr[11] << 24;
    if (img_w <= 0 || img_h <= 0 || img_w > 8192 || img_h > 8192) {
        fprintf(stderr, "armada-splash: bad image dims %dx%d\n", img_w, img_h);
        close(fd); return 0;
    }
    size_t sz = (size_t)img_w * img_h * 4;
    img = malloc(sz);
    size_t off = 0; ssize_t r;
    while (off < sz && (r = read(fd, (uint8_t *)img + off, sz - off)) > 0) off += r;
    close(fd);
    if (off != sz) { fprintf(stderr, "armada-splash: short image\n"); free(img); img = NULL; return 0; }
    return 1;
}

// Poll the whole file each call (not mtime) so same-second updates are caught.
static void read_status(const char *path, char *out, size_t n) {
    out[0] = 0;
    if (!path) return;
    int fd = open(path, O_RDONLY);
    if (fd < 0) return;
    ssize_t r = read(fd, out, n - 1);
    close(fd);
    out[r < 0 ? 0 : r] = 0;
}

// ================= fbdev backend =================

struct fbdev {
    int fd, ttyfd, prev_kd;
    uint8_t *map; size_t maplen; long dataoff;
    int fbw, fbh, bpp, stride, angle;
    int roff, rlen, goff, glen, boff, blen;
};
static struct fbdev fbd;
static int keep_vt = 0;   // hand off KD_GRAPHICS to the next instance instead of restoring

static void fb_restore(void) {
    if (fbd.ttyfd < 0) return;
    // On handoff, leave KD_GRAPHICS so fbcon does not repaint before the next owner.
    if (!keep_vt) ioctl(fbd.ttyfd, KDSETMODE, fbd.prev_kd);
    close(fbd.ttyfd); fbd.ttyfd = -1;
}

static int fb_open(const char *dev, int angle) {
    fbd.ttyfd = -1;
    fbd.fd = open(dev, O_RDWR);
    if (fbd.fd < 0) { fprintf(stderr, "armada-splash: open %s: %s\n", dev, strerror(errno)); return 0; }
    struct fb_var_screeninfo v; struct fb_fix_screeninfo f;
    if (ioctl(fbd.fd, FBIOGET_VSCREENINFO, &v) || ioctl(fbd.fd, FBIOGET_FSCREENINFO, &f)) {
        fprintf(stderr, "armada-splash: FBIOGET_*SCREENINFO failed\n"); return 0;
    }
    fbd.fbw = v.xres; fbd.fbh = v.yres; fbd.bpp = v.bits_per_pixel;
    fbd.stride = f.line_length;              // NEVER xres*bpp/8
    fbd.roff = v.red.offset;   fbd.rlen = v.red.length;
    fbd.goff = v.green.offset; fbd.glen = v.green.length;
    fbd.boff = v.blue.offset;  fbd.blen = v.blue.length;
    fbd.angle = angle;
    if (fbd.rlen == 0) { fbd.roff = 16; fbd.rlen = 8; fbd.goff = 8; fbd.glen = 8; fbd.boff = 0; fbd.blen = 8; }

    if (angle != 0 && angle != 90 && angle != 180 && angle != 270) {
        fprintf(stderr, "armada-splash: invalid --rotate %d\n", angle); return 0;
    }
    if (fbd.bpp < 16 || fbd.bpp > 32 || fbd.bpp % 8 != 0) {
        fprintf(stderr, "armada-splash: unsupported bpp %d (need byte-aligned 16..32)\n", fbd.bpp);
        return 0;
    }
    if (fbd.stride < fbd.fbw * (fbd.bpp / 8) ||
        (size_t)fbd.stride * fbd.fbh > (size_t)f.smem_len) {
        fprintf(stderr, "armada-splash: fb geometry exceeds smem_len\n"); return 0;
    }

    long pgoff = (long)f.smem_start % sysconf(_SC_PAGESIZE);
    fbd.maplen = (size_t)f.smem_len + pgoff;
    fbd.map = mmap(NULL, fbd.maplen, PROT_READ | PROT_WRITE, MAP_SHARED, fbd.fd, 0);
    if (fbd.map == MAP_FAILED) { fprintf(stderr, "armada-splash: mmap fb: %s\n", strerror(errno)); return 0; }
    fbd.dataoff = pgoff;

    // After a compositor exits the fbdev may be FB_BLANK_POWERDOWN; unblank or
    // writes land in memory that is not scanned out.
    ioctl(fbd.fd, FBIOBLANK, FB_BLANK_UNBLANK);

    // Arm the KD_TEXT restore only if both ioctls succeed, else prev_kd is bogus.
    fbd.ttyfd = open("/dev/tty0", O_RDWR);
    if (fbd.ttyfd >= 0) {
        if (ioctl(fbd.ttyfd, KDGETMODE, &fbd.prev_kd) == 0 &&
            ioctl(fbd.ttyfd, KDSETMODE, KD_GRAPHICS) == 0) {
            atexit(fb_restore);
        } else {
            close(fbd.ttyfd); fbd.ttyfd = -1;
        }
    }

    if (angle == 90 || angle == 270) { SW = fbd.fbh; SH = fbd.fbw; }
    else { SW = fbd.fbw; SH = fbd.fbh; }
    return 1;
}

// Shift left for len > 8; `c >> (8 - len)` would be UB there.
static inline uint32_t fb_chan(uint32_t c8, int len, int off) {
    if (len <= 0) return 0;
    uint32_t v = len >= 8 ? (c8 << (len - 8)) : (c8 >> (8 - len));
    return v << off;
}
static inline uint32_t fb_pack(uint32_t argb) {
    return fb_chan((argb >> 16) & 0xff, fbd.rlen, fbd.roff) |
           fb_chan((argb >> 8) & 0xff, fbd.glen, fbd.goff) |
           fb_chan(argb & 0xff, fbd.blen, fbd.boff);
}

static void fb_present(void) {
    int Bpp = fbd.bpp >> 3;
    for (int ly = 0; ly < SH; ly++) {
        for (int lx = 0; lx < SW; lx++) {
            int px, py;
            switch (fbd.angle) {
                case 90:  px = fbd.fbw - 1 - ly; py = lx; break;
                case 180: px = fbd.fbw - 1 - lx; py = fbd.fbh - 1 - ly; break;
                case 270: px = ly; py = fbd.fbh - 1 - lx; break;
                default:  px = lx; py = ly; break;
            }
            if (px < 0 || px >= fbd.fbw || py < 0 || py >= fbd.fbh) continue;
            uint32_t v = fb_pack(shadow[ly * SW + lx]);
            uint8_t *dst = fbd.map + fbd.dataoff + (size_t)py * fbd.stride + (size_t)px * Bpp;
            for (int k = 0; k < Bpp; k++) dst[k] = (v >> (k * 8)) & 0xff;
        }
    }
}

// ================= x11 backend (Xwayland under gamescope) =================
// gamescope's steam mode shows only X11 windows via its appid focus machinery.
// STEAM_GAME = g_appid opts this window into the fallback-appid patch; set
// GAMESCOPE_FALLBACK_APPID to the same value.
#ifdef HAVE_X11
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>

static Display *x_dpy;
static Window x_win;
static GC x_gc;
static XImage *x_img;

static int x11_init(void) {
    if (!x_dpy) x_dpy = XOpenDisplay(NULL);   // may already be open (sized in main)
    if (!x_dpy) { fprintf(stderr, "armada-splash: cannot open X display\n"); return 0; }
    int scr = DefaultScreen(x_dpy);
    XSetWindowAttributes a = { 0 };
    a.background_pixel = BlackPixel(x_dpy, scr);
    x_win = XCreateWindow(x_dpy, RootWindow(x_dpy, scr), 0, 0, SW, SH, 0,
                          DefaultDepth(x_dpy, scr), InputOutput,
                          DefaultVisual(x_dpy, scr), CWBackPixel, &a);
    XChangeProperty(x_dpy, x_win, XInternAtom(x_dpy, "STEAM_GAME", False),
                    XA_CARDINAL, 32, PropModeReplace, (unsigned char *)&g_appid, 1);
    XStoreName(x_dpy, x_win, "armada-splash");
    XSelectInput(x_dpy, x_win, ExposureMask | StructureNotifyMask);
    XMapWindow(x_dpy, x_win);
    x_gc = XCreateGC(x_dpy, x_win, 0, NULL);
    // shadow is ARGB8888, i.e. X 24/32-bit TrueColor LSBFirst; use it directly.
    x_img = XCreateImage(x_dpy, DefaultVisual(x_dpy, scr), DefaultDepth(x_dpy, scr),
                         ZPixmap, 0, (char *)shadow, SW, SH, 32, SW * 4);
    if (!x_img) { fprintf(stderr, "armada-splash: XCreateImage failed\n"); return 0; }
    x_img->byte_order = LSBFirst;
    XFlush(x_dpy);
    return 1;
}

static void x11_present(void) {
    if (!x_img) return;
    XPutImage(x_dpy, x_win, x_gc, x_img, 0, 0, 0, 0, SW, SH);
    XFlush(x_dpy);
}

// Adapt to gamescope resizing the window (per-panel resolution/orientation).
static void x11_resize(int w, int h) {
    if (w <= 0 || h <= 0 || (w == SW && h == SH)) return;
    uint32_t *ns = realloc(shadow, (size_t)w * h * 4);
    if (!ns) return;
    shadow = ns; SW = w; SH = h;
    text_scale = SW / 540; if (text_scale < 2) text_scale = 2;
    g_text_px = g_text_px_req > 0 ? g_text_px_req : SW / 26;
    if (g_text_px < 14) g_text_px = 14;
    if (g_ttf_ok) g_ttf_scale = stbtt_ScaleForPixelHeight(&g_ttf, (float)g_text_px);
    if (x_img) { x_img->data = NULL; XDestroyImage(x_img); }   // don't free shadow (aliased)
    int scr = DefaultScreen(x_dpy);
    x_img = XCreateImage(x_dpy, DefaultVisual(x_dpy, scr), DefaultDepth(x_dpy, scr),
                         ZPixmap, 0, (char *)shadow, SW, SH, 32, SW * 4);
    if (x_img) x_img->byte_order = LSBFirst;
    compose(g_cur);
    x11_present();
}
#endif // HAVE_X11

// ================= ppm debug backend =================
// Debug backend: write one composed frame as a PPM (no fb/compositor needed).
static void write_ppm(const char *path) {
    FILE *f = fopen(path, "wb");
    if (!f) { fprintf(stderr, "armada-splash: cannot write %s\n", path); return; }
    fprintf(f, "P6\n%d %d\n255\n", SW, SH);
    for (int i = 0; i < SW * SH; i++) {
        uint32_t p = shadow[i];
        uint8_t rgb[3] = { (p >> 16) & 0xff, (p >> 8) & 0xff, p & 0xff };
        fwrite(rgb, 1, 3, f);
    }
    fclose(f);
    fprintf(stderr, "armada-splash: wrote %s (%dx%d)\n", path, SW, SH);
}

// ================= main =================

static const char *arg(int argc, char **argv, const char *k, const char *def) {
    for (int i = 1; i < argc - 1; i++) if (!strcmp(argv[i], k)) return argv[i + 1];
    return def;
}

int main(int argc, char **argv) {
    const char *image = arg(argc, argv, "--image", NULL);
    const char *status = arg(argc, argv, "--status", NULL);
    const char *backend = arg(argc, argv, "--backend", "auto");
    const char *fbdev = arg(argc, argv, "--fbdev", "/dev/fb0");
    int angle = atoi(arg(argc, argv, "--rotate", "0"));
    for (int i = 1; i < argc; i++) if (!strcmp(argv[i], "--keep-vt")) keep_vt = 1;
    const char *bgs = arg(argc, argv, "--bg", NULL);
    if (bgs) bg = 0xFF000000 | (strtoul(bgs, NULL, 0) & 0xFFFFFF);
    int req_w = atoi(arg(argc, argv, "--width", "0"));
    int req_h = atoi(arg(argc, argv, "--height", "0"));

    signal(SIGINT, on_signal);
    signal(SIGTERM, on_signal);

    // auto: x11 when an X display is present, else fbdev.
    int use_x11 = !strcmp(backend, "x11");
    if (!strcmp(backend, "auto")) use_x11 = getenv("DISPLAY") != NULL;

#ifndef HAVE_X11
    if (use_x11) { fprintf(stderr, "armada-splash: built without x11\n"); return 1; }
#endif

    int use_ppm = !strcmp(backend, "ppm");
#ifdef HAVE_X11
    if (use_x11) {
        x_dpy = XOpenDisplay(NULL);
        if (!x_dpy) { fprintf(stderr, "armada-splash: cannot open X display\n"); return 1; }
        int scr = DefaultScreen(x_dpy);
        SW = req_w > 0 ? req_w : DisplayWidth(x_dpy, scr);   // gamescope's logical output
        SH = req_h > 0 ? req_h : DisplayHeight(x_dpy, scr);
    } else
#endif
    if (use_ppm) {
        SW = req_w > 0 ? req_w : 1080;
        SH = req_h > 0 ? req_h : 1920;
    } else {
        if (!fb_open(fbdev, angle)) return 1;
    }

    shadow = malloc((size_t)SW * SH * 4);
    if (!shadow) { fprintf(stderr, "armada-splash: OOM\n"); return 1; }
    g_text_px_req = atoi(arg(argc, argv, "--text-height", "0"));
    text_scale = SW / 540; if (text_scale < 2) text_scale = 2;
    g_text_px = g_text_px_req > 0 ? g_text_px_req : SW / 26;
    if (g_text_px < 14) g_text_px = 14;
    g_gap = atoi(arg(argc, argv, "--gap", "-1"));
    if (!strcmp(arg(argc, argv, "--layout", "group"), "split")) g_layout = 1;
    g_appid = (uint32_t)strtoul(arg(argc, argv, "--appid", "0x41524d41"), NULL, 0);
    load_font(arg(argc, argv, "--font", "/usr/share/armada/splash/font.ttf"), g_text_px);
    load_image(image);

    read_status(status, g_cur, sizeof g_cur);
    compose(g_cur);
    strcpy(g_shown, g_cur);

    if (use_ppm) { write_ppm(arg(argc, argv, "--out", "/tmp/armada-splash.ppm")); return 0; }

#ifdef HAVE_X11
    if (use_x11) {
        if (!x11_init()) return 1;
        x11_present();
        int tfd = timerfd_create(CLOCK_MONOTONIC, TFD_CLOEXEC);
        struct itimerspec its = { {0, 250000000}, {0, 250000000} };
        timerfd_settime(tfd, 0, &its, NULL);
        int xfd = ConnectionNumber(x_dpy);
        while (running) {
            struct pollfd fds[2] = { { xfd, POLLIN, 0 }, { tfd, POLLIN, 0 } };
            if (poll(fds, 2, -1) < 0 && errno == EINTR) continue;
            if (fds[0].revents & POLLIN)
                while (XPending(x_dpy)) {
                    XEvent ev; XNextEvent(x_dpy, &ev);
                    if (ev.type == Expose) x11_present();
                    else if (ev.type == ConfigureNotify) x11_resize(ev.xconfigure.width, ev.xconfigure.height);
                }
            if (fds[1].revents & POLLIN) {
                uint64_t x; ssize_t rr = read(tfd, &x, sizeof x); (void)rr;
                read_status(status, g_cur, sizeof g_cur);
                if (strcmp(g_cur, g_shown)) { compose(g_cur); strcpy(g_shown, g_cur); x11_present(); }
            }
        }
        return 0;
    }
#endif

    fb_present();
    int tfd = timerfd_create(CLOCK_MONOTONIC, TFD_CLOEXEC);
    struct itimerspec its = { {0, 250000000}, {0, 250000000} };
    timerfd_settime(tfd, 0, &its, NULL);
    while (running) {
        struct pollfd pfd = { tfd, POLLIN, 0 };
        if (poll(&pfd, 1, -1) < 0 && errno == EINTR) continue;
        uint64_t x; ssize_t rr = read(tfd, &x, sizeof x); (void)rr;
        read_status(status, g_cur, sizeof g_cur);
        if (strcmp(g_cur, g_shown)) { compose(g_cur); strcpy(g_shown, g_cur); fb_present(); }
    }
    fb_restore();
    return 0;
}
