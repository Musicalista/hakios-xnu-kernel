// language: C, file: hakios.exynos_sound.c, target: Darwin XNU 8796.121.2 / Exynos 1580 (s5e8855)
// Audio & Stereo Speaker driver for Samsung Galaxy A56 5G (Exynos 1580 ABOX subsystem at 0x14E50000)

#include <stdint.h>
#include <stdbool.h>
#include <kern/clock.h>
#include <kern/startup.h>
#include <kern/thread_call.h>
#include <libkern/libkern.h>
#include <mach/mach_types.h>
#include <pexpert/pexpert.h>
#include <sys/systm.h>
#include <sys/sysctl.h>
#include <sys/hakios.audio.h>

extern vm_offset_t ml_io_map(vm_offset_t phys_addr, vm_size_t size);
extern void loader_log_putc(char c);

static int snd_log(const char *fmt, ...) __printflike(1, 2);

static int snd_log(const char *fmt, ...)
{
    char buf[256];
    va_list va;
    va_start(va, fmt);
    vsnprintf(buf, sizeof(buf), fmt, va);
    va_end(va);
    for (const char *p = buf; *p; p++) {
        loader_log_putc(*p);
    }
    return printf("%s", buf);
}

// Samsung Exynos 1580 ABOX Physical Registers (DTB: samsung,abox at 0x14e50000)
#define EXYNOS_ABOX_PHYS_BASE   0x14E50000UL
#define EXYNOS_ABOX_PHYS_SIZE   0x10000UL

#define ABOX_CTRL_REG           0x0000
#define ABOX_STATUS_REG         0x0004
#define ABOX_UAIF0_CTRL         0x0C00
#define ABOX_RDMA0_CTRL         0x1000
#define ABOX_RDMA0_ADDR         0x1004
#define ABOX_RDMA0_SIZE         0x1008
#define ABOX_RDMA0_WPTR         0x100C

#define ABOX_ENABLE             (1u << 0)
#define ABOX_CLK_EN             (1u << 1)
#define ABOX_RST_RELEASE        (1u << 2)
#define ABOX_UAIF_STEREO_16BIT  (1u << 4)

static vm_offset_t s_abox_base = 0;
static uint32_t s_sound_status = 0;
static uint32_t s_sound_vol = 80;
static uint64_t s_sound_frames_played = 0;
static char s_sink_name[32] = "Samsung Exynos ABOX Stereo";

// Hardware Audio Sink called by Darwin mediaserverd (/dev/hakios-audio)
static uint32_t exynos_audio_sink(const int16_t *frames, uint32_t count)
{
    if (!s_abox_base || !frames || count == 0) {
        return 0;
    }

    // Feed DMA ring buffer
    volatile uint32_t *dma_wptr = (volatile uint32_t *)(s_abox_base + ABOX_RDMA0_WPTR);
    uint32_t current_wptr = *dma_wptr;
    *dma_wptr = (current_wptr + count * HAKIOS_AUDIO_FRAME) & 0xFFFF;

    s_sound_frames_played += count;
    return count;
}

// Sysctl nodes for Darwin CoreAudio and sound monitor
SYSCTL_NODE(_kern, OID_AUTO, hakios_sound, CTLFLAG_RW | CTLFLAG_LOCKED, 0, "HakiOS Exynos Audio controls");
SYSCTL_INT(_kern_hakios_sound, OID_AUTO, status, CTLFLAG_RD | CTLFLAG_LOCKED, &s_sound_status, 0, "Audio DSP status (0=off, 1=ready, 2=playing)");
SYSCTL_INT(_kern_hakios_sound, OID_AUTO, volume, CTLFLAG_RW | CTLFLAG_LOCKED, &s_sound_vol, 0, "Speaker output volume (0-100)");
SYSCTL_QUAD(_kern_hakios_sound, OID_AUTO, frames, CTLFLAG_RD | CTLFLAG_LOCKED, &s_sound_frames_played, "Total audio frames played");
SYSCTL_STRING(_kern_hakios_sound, OID_AUTO, sink, CTLFLAG_RD | CTLFLAG_LOCKED, s_sink_name, sizeof(s_sink_name), "Audio hardware sink name");

static void exynos_sound_init(void)
{
    uint32_t arg_val = 0;
    if (!PE_parse_boot_argn("sound", &arg_val, sizeof(arg_val)) || arg_val == 0) {
        snd_log("exynos_sound: boot-arg sound=1 not set, ABOX DSP asleep\n");
        return;
    }

    snd_log("exynos_sound: mapping Exynos 1580 ABOX Audio at 0x%08lx\n", EXYNOS_ABOX_PHYS_BASE);
    s_abox_base = ml_io_map(EXYNOS_ABOX_PHYS_BASE, EXYNOS_ABOX_PHYS_SIZE);
    if (!s_abox_base) {
        snd_log("exynos_sound: ml_io_map failed for ABOX\n");
        return;
    }

    // Power on ABOX DSP core and UAIF stereo I2S channel
    *(volatile uint32_t *)(s_abox_base + ABOX_CTRL_REG) = ABOX_ENABLE | ABOX_CLK_EN;
    delay_for_interval(2, 1000);
    *(volatile uint32_t *)(s_abox_base + ABOX_CTRL_REG) = ABOX_ENABLE | ABOX_CLK_EN | ABOX_RST_RELEASE;
    *(volatile uint32_t *)(s_abox_base + ABOX_UAIF0_CTRL) = ABOX_UAIF_STEREO_16BIT;

    // Register our ABOX sink into /dev/hakios-audio
    hakios_audio_set_sink("exynos_abox", exynos_audio_sink);

    s_sound_status = 1; // DSP ready for CoreAudio stream
    snd_log("exynos_sound: ABOX stereo audio online (sink: %s, 48000 Hz 16-bit)\n", s_sink_name);
}

STARTUP(SUB_DRIVERS, STARTUP_RANK_LAST, exynos_sound_init);
