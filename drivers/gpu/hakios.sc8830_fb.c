// language: C, file: drivers/gpu/hakios.sc8830_fb.c, target: Darwin/XNU ARMv7 Spreadtrum SC8830
// *Spreadtrum SC8830/SharkL DISPC (Display Controller) framebuffer scanout for Darwin boot console*

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#define SC8830_DISPC_BASE       0x20800000u

#define DISPC_CTRL              0x0000u
#define DISPC_SIZE_XY           0x0004u
#define DISPC_PITCH             0x0008u
#define DISPC_DISP_MODE         0x000Cu
#define DISPC_LAYER0_BASE       0x0020u
#define DISPC_LAYER0_SIZE       0x0024u
#define DISPC_LAYER0_PITCH      0x0028u
#define DISPC_LAYER0_CTRL       0x002Cu

#define J105M_FB_WIDTH          480u
#define J105M_FB_HEIGHT         800u
#define J105M_FB_BPP            32u
#define J105M_FB_PITCH          (J105M_FB_WIDTH * (J105M_FB_BPP / 8u)) // 1920 bytes
#define J105M_FB_PHYS_BASE      0x8E000000u                           // Allocated at top of RAM

static volatile uint32_t * const g_dispc = (volatile uint32_t *)SC8830_DISPC_BASE;
static uint32_t * const g_fb_ptr = (uint32_t *)J105M_FB_PHYS_BASE;

struct darwin_video_boot_args {
    uint32_t v_baseAddr;
    uint32_t v_display;
    uint32_t v_rowBytes;
    uint32_t v_width;
    uint32_t v_height;
    uint32_t v_depth;
};

void hakios_sc8830_fb_init(struct darwin_video_boot_args *out_video) {
    // DISPC Layer 0 configured for direct scanout
    *(volatile uint32_t *)(SC8830_DISPC_BASE + DISPC_LAYER0_BASE) = J105M_FB_PHYS_BASE;
    *(volatile uint32_t *)(SC8830_DISPC_BASE + DISPC_LAYER0_PITCH) = J105M_FB_PITCH;
    *(volatile uint32_t *)(SC8830_DISPC_BASE + DISPC_LAYER0_SIZE) = (J105M_FB_WIDTH << 16) | J105M_FB_HEIGHT;
    *(volatile uint32_t *)(SC8830_DISPC_BASE + DISPC_LAYER0_CTRL) = 0x00000001u; // Enable Layer 0, ARGB8888

    // Clear screen to pure black
    for (uint32_t i = 0; i < (J105M_FB_WIDTH * J105M_FB_HEIGHT); i++) {
        g_fb_ptr[i] = 0x00000000u;
    }

    if (out_video) {
        out_video->v_baseAddr = J105M_FB_PHYS_BASE;
        out_video->v_display = 1;
        out_video->v_rowBytes = J105M_FB_PITCH;
        out_video->v_width = J105M_FB_WIDTH;
        out_video->v_height = J105M_FB_HEIGHT;
        out_video->v_depth = J105M_FB_BPP;
    }
}

void hakios_sc8830_draw_pixel(uint32_t x, uint32_t y, uint32_t color) {
    if (x < J105M_FB_WIDTH && y < J105M_FB_HEIGHT) {
        g_fb_ptr[y * J105M_FB_WIDTH + x] = color;
    }
}
