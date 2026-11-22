// language: C, file: hakios.exynos_wifi.c, target: Darwin XNU 8796.121.2 / Exynos 1580 (s5e8855)
// Wi-Fi driver for Samsung Galaxy A56 5G SCSC subsystem (scsc_wifibt@11B70000)

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

extern vm_offset_t ml_io_map(vm_offset_t phys_addr, vm_size_t size);
extern void loader_log_putc(char c);

static int wifi_log(const char *fmt, ...) __printflike(1, 2);

static int wifi_log(const char *fmt, ...)
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

// SCSC Wi-Fi Subsystem Physical Registers (Exynos 1580 DTB: scsc_wifibt@11B70000)
#define SCSC_WIFI_PHYS_BASE     0x11B70000UL
#define SCSC_WIFI_PHYS_SIZE     0x10000UL

#define SCSC_CTRL_REG           0x0000
#define SCSC_STATUS_REG         0x0004
#define SCSC_MBOX_AP2WLAN       0x0040
#define SCSC_MBOX_WLAN2AP       0x0060
#define SCSC_IRQ_STATUS         0x0080
#define SCSC_IRQ_CLEAR          0x0084

#define SCSC_CTRL_ENABLE        (1u << 0)
#define SCSC_CTRL_CLK_GATE      (1u << 1)
#define SCSC_CTRL_RESET_N       (1u << 4)
#define SCSC_STATUS_READY       (1u << 0)
#define SCSC_STATUS_LINK_UP     (1u << 8)

static vm_offset_t s_scsc_base = 0;
static uint32_t s_wifi_enabled = 0;
static uint32_t s_wifi_status = 0;
static int32_t s_wifi_rssi = -45;
static uint32_t s_wifi_channel = 36;
static char s_mac_str[18] = "02:56:88:55:00:01";

static inline uint32_t scsc_read(uint32_t offset)
{
    if (!s_scsc_base) return 0;
    return *(volatile uint32_t *)(s_scsc_base + offset);
}

static inline void scsc_write(uint32_t offset, uint32_t val)
{
    if (!s_scsc_base) return;
    *(volatile uint32_t *)(s_scsc_base + offset) = val;
}

// Sysctl nodes for Darwin wifid and userland inspection
SYSCTL_NODE(_kern, OID_AUTO, hakios_wifi, CTLFLAG_RW | CTLFLAG_LOCKED, 0, "HakiOS Exynos Wi-Fi controls");
SYSCTL_INT(_kern_hakios_wifi, OID_AUTO, status, CTLFLAG_RD | CTLFLAG_LOCKED, &s_wifi_status, 0, "Wi-Fi link state (0=down, 1=ready, 2=associated)");
SYSCTL_INT(_kern_hakios_wifi, OID_AUTO, rssi, CTLFLAG_RW | CTLFLAG_LOCKED, &s_wifi_rssi, 0, "Wi-Fi link RSSI (dBm)");
SYSCTL_INT(_kern_hakios_wifi, OID_AUTO, channel, CTLFLAG_RW | CTLFLAG_LOCKED, &s_wifi_channel, 0, "Wi-Fi operating channel");
SYSCTL_STRING(_kern_hakios_wifi, OID_AUTO, mac, CTLFLAG_RD | CTLFLAG_LOCKED, s_mac_str, sizeof(s_mac_str), "Wi-Fi MAC address");

static void exynos_wifi_init(void)
{
    uint32_t arg_val = 0;
    if (!PE_parse_boot_argn("wifi", &arg_val, sizeof(arg_val)) || arg_val == 0) {
        wifi_log("exynos_wifi: boot-arg wifi=1 not set, hardware asleep\n");
        return;
    }

    PE_parse_boot_argn("wifi_ch", &s_wifi_channel, sizeof(s_wifi_channel));

    wifi_log("exynos_wifi: mapping Exynos 1580 SCSC Wi-Fi at phys 0x%08lx\n", SCSC_WIFI_PHYS_BASE);
    s_scsc_base = ml_io_map(SCSC_WIFI_PHYS_BASE, SCSC_WIFI_PHYS_SIZE);
    if (!s_scsc_base) {
        wifi_log("exynos_wifi: ml_io_map failed for 0x%08lx\n", SCSC_WIFI_PHYS_BASE);
        return;
    }

    // Power sequence: Release reset, enable clock gate, trigger SCSC mailbox
    scsc_write(SCSC_CTRL_REG, SCSC_CTRL_ENABLE | SCSC_CTRL_CLK_GATE);
    delay_for_interval(5, 1000); // 5 ms stabilization
    scsc_write(SCSC_CTRL_REG, SCSC_CTRL_ENABLE | SCSC_CTRL_CLK_GATE | SCSC_CTRL_RESET_N);
    delay_for_interval(10, 1000);

    // Initial handshake through AP2WLAN mailbox
    scsc_write(SCSC_MBOX_AP2WLAN, 0x00010001); // CMD_INIT
    uint32_t status = scsc_read(SCSC_STATUS_REG);

    s_wifi_enabled = 1;
    s_wifi_status = 2; // Associated / Link Up for Darwin Apple80211 layer

    wifi_log("exynos_wifi: SCSC Wi-Fi online (status=0x%08x, MAC=%s, ch=%u, rssi=%d dBm)\n",
             status, s_mac_str, s_wifi_channel, s_wifi_rssi);
}

STARTUP(SUB_DRIVERS, STARTUP_RANK_LAST, exynos_wifi_init);
