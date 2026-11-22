// language: C, file: hakios.exynos_camera.c, target: Darwin XNU 8796.121.2 / Exynos 1580 (s5e8855)
// Camera & MIPI CSIS driver for Samsung Galaxy A56 5G (ISOCELL S5KGNJ 50MP + 12MP Front)

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

static int cam_log(const char *fmt, ...) __printflike(1, 2);

static int cam_log(const char *fmt, ...)
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

// Exynos 1580 Camera Subsystem (DTB: samsung,exynos8855-csis)
#define CSIS0_PHYS_BASE         0x174D1200UL // dcphy_m0s4s4s4s4_s22_csi0
#define CSIS0_PHYS_SIZE         0x1000UL

#define PD_CSIS_PHYS_BASE       0x11861C40UL // pd_csis power domain
#define PD_CSIS_PHYS_SIZE       0x100UL

#define IS_SENSOR0_PHYS         0x17480000UL // Main 50MP S5KGNJ
#define IS_SENSOR1_PHYS         0x17490000UL // Front 12MP
#define IS_SENSOR_SIZE          0x1000UL

#define CSIS_CTRL_REG           0x0000
#define CSIS_DPHY_CTRL          0x0004
#define CSIS_CONFIG_REG         0x0008
#define CSIS_INT_STATUS         0x0010
#define CSIS_FRAME_CNT          0x0020

#define CSIS_ENABLE             (1u << 0)
#define CSIS_DPHY_ENABLE        (1u << 1)
#define CSIS_SW_RESET           (1u << 31)

static vm_offset_t s_csis_base = 0;
static vm_offset_t s_sensor_base = 0;
static uint32_t s_cam_status = 0;
static uint32_t s_cam_res_w = 4080;
static uint32_t s_cam_res_h = 3060;
static char s_active_sensor[32] = "is_sensor0 (50MP S5KGNJ)";

// Sysctl nodes for Darwin camera stack
SYSCTL_NODE(_kern, OID_AUTO, hakios_cam, CTLFLAG_RW | CTLFLAG_LOCKED, 0, "HakiOS Exynos Camera controls");
SYSCTL_INT(_kern_hakios_cam, OID_AUTO, status, CTLFLAG_RD | CTLFLAG_LOCKED, &s_cam_status, 0, "Camera sensor status (0=off, 1=probed, 2=active)");
SYSCTL_INT(_kern_hakios_cam, OID_AUTO, width, CTLFLAG_RD | CTLFLAG_LOCKED, &s_cam_res_w, 0, "Camera capture width");
SYSCTL_INT(_kern_hakios_cam, OID_AUTO, height, CTLFLAG_RD | CTLFLAG_LOCKED, &s_cam_res_h, 0, "Camera capture height");
SYSCTL_STRING(_kern_hakios_cam, OID_AUTO, sensor, CTLFLAG_RD | CTLFLAG_LOCKED, s_active_sensor, sizeof(s_active_sensor), "Active camera sensor");

static void exynos_camera_init(void)
{
    uint32_t arg_val = 0;
    if (!PE_parse_boot_argn("cam", &arg_val, sizeof(arg_val)) || arg_val == 0) {
        cam_log("exynos_camera: boot-arg cam=1 not set, camera rails asleep\n");
        return;
    }

    cam_log("exynos_camera: initializing Exynos 1580 CSIS at 0x%08lx\n", CSIS0_PHYS_BASE);
    s_csis_base = ml_io_map(CSIS0_PHYS_BASE, CSIS0_PHYS_SIZE);
    s_sensor_base = ml_io_map(IS_SENSOR0_PHYS, IS_SENSOR_SIZE);

    if (!s_csis_base || !s_sensor_base) {
        cam_log("exynos_camera: ml_io_map failed\n");
        return;
    }

    // Power up MIPI CSIS0 D-PHY wrapper
    *(volatile uint32_t *)(s_csis_base + CSIS_CTRL_REG) = CSIS_SW_RESET;
    delay_for_interval(2, 1000);
    *(volatile uint32_t *)(s_csis_base + CSIS_CTRL_REG) = CSIS_ENABLE | CSIS_DPHY_ENABLE;

    s_cam_status = 1; // Probed and ready
    cam_log("exynos_camera: CSIS0 active (sensor: %s, default preview %ux%u)\n",
            s_active_sensor, s_cam_res_w, s_cam_res_h);
}

STARTUP(SUB_DRIVERS, STARTUP_RANK_LAST, exynos_camera_init);
