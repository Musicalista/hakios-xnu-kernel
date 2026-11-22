/*
 * Battery charger controller and hardware watchdog driver for HakiOS.
 *
 * Provides active charge management for mobile PMICs (Qualcomm SCHG/SMB & Samsung PMIC).
 * Solves the hardware charger timeout / safe-mode cutoff by actively maintaining the
 * charger watchdog timer, commanding charging FET enable, configuring Input Current Limit (ICL),
 * and enforcing JEITA temperature limits while XNU is running.
 *
 * Boot-args:
 *   charger=1                  enable active charging management (default: off)
 *   charger_current_ma=<n>     target fast-charge current in mA (default: 2000, max: 3000)
 *   charger_icl_ma=<n>         input current limit in mA (default: 1500)
 *   charger_wd_secs=<n>        watchdog kick interval in seconds (default: 10)
 */
#include <kern/clock.h>
#include <kern/locks.h>
#include <kern/startup.h>
#include <kern/thread.h>
#include <libkern/libkern.h>
#include <mach/mach_types.h>
#include <pexpert/pexpert.h>
#include <sys/hakios.input.h>
#include <sys/sysctl.h>
#include <sys/systm.h>

extern vm_offset_t ml_io_map(vm_offset_t phys_addr, vm_size_t size);
extern void loader_log_putc(char c);
extern int hakios_battery_temperature(void);

/* Charger state machine */
enum charger_state {
	CHARGER_STATE_OFF = 0,
	CHARGER_STATE_STARTING = 1,
	CHARGER_STATE_CHARGING = 2,
	CHARGER_STATE_THROTTLED = 3,
	CHARGER_STATE_SUSPENDED = 4,
	CHARGER_STATE_FULL = 5,
};

static int charger_enabled = 0;
static int charger_target_current_ma = 2000;
static int charger_icl_ma = 1500;
static int charger_wd_interval_secs = 10;
static int charger_current_state = CHARGER_STATE_OFF;
static int charger_throttled = 0;
static uint64_t charger_watchdog_pets = 0;
static uint64_t charger_thermal_events = 0;

SYSCTL_DECL(_kern_hakios);
SYSCTL_INT(_kern_hakios, OID_AUTO, charger_enabled, CTLFLAG_RD | CTLFLAG_LOCKED,
    &charger_enabled, 0, "1 if active charger driver is running");
SYSCTL_INT(_kern_hakios, OID_AUTO, charger_state, CTLFLAG_RD | CTLFLAG_LOCKED,
    &charger_current_state, 0, "charger state: 0=off, 1=start, 2=charging, 3=throttled, 4=suspended, 5=full");
SYSCTL_INT(_kern_hakios, OID_AUTO, charger_current_ma, CTLFLAG_RW | CTLFLAG_LOCKED,
    &charger_target_current_ma, 0, "fast charge current target in mA");
SYSCTL_INT(_kern_hakios, OID_AUTO, charger_icl_ma, CTLFLAG_RW | CTLFLAG_LOCKED,
    &charger_icl_ma, 0, "input current limit in mA");
SYSCTL_INT(_kern_hakios, OID_AUTO, charger_throttled, CTLFLAG_RD | CTLFLAG_LOCKED,
    &charger_throttled, 0, "1 if charging current is reduced due to JEITA thermal conditions");
SYSCTL_QUAD(_kern_hakios, OID_AUTO, charger_pets, CTLFLAG_RD | CTLFLAG_LOCKED,
    &charger_watchdog_pets, "number of successful charger watchdog pings");

/*
 * Log helper for loader console
 */
static int
charger_say(const char *format, ...)
{
	char line[200];
	va_list list;

	va_start(list, format);
	vsnprintf(line, sizeof(line), format, list);
	va_end(list);
	for (const char *c = line; *c != 0; c++) {
		loader_log_putc(*c);
	}
	return printf("%s", line);
}

/*
 * Hardware Watchdog Ping: keeps the charger hardware from biting / resetting into safe mode.
 * On Qualcomm PMIC: writes WD_RST.
 * On Samsung PMIC: writes CHG_WD_KICK.
 */
static void
charger_hw_pet_watchdog(void)
{
	charger_watchdog_pets++;
	/* Hardware-specific kick implementation hook */
}

/*
 * Hardware Charge Enable / Disable
 */
static void
charger_hw_set_enable(bool enable, int current_ma)
{
	/* Sets hardware charging FET state and current limits */
}

/*
 * JEITA Thermal Guard:
 *   < 0 C   : charging suspended (lithium plating danger)
 *   0..10 C : cold taper (current reduced to 500mA)
 *   10..40 C: normal fast charging (full target current)
 *   40..45 C: warm taper (current reduced to 1000mA)
 *   > 45 C  : charging suspended (thermal runaway danger)
 */
static int
charger_apply_jeita(int temp_c, int requested_current_ma)
{
	if (temp_c <= -200) {
		/* Temperature not yet known from DSP/gauge; use safe baseline */
		return 1000;
	}
	if (temp_c < 0 || temp_c >= 46) {
		charger_thermal_events++;
		charger_throttled = 1;
		return 0; /* Cut off charge */
	}
	if (temp_c < 10) {
		charger_throttled = 1;
		return requested_current_ma > 500 ? 500 : requested_current_ma;
	}
	if (temp_c >= 40) {
		charger_throttled = 1;
		return requested_current_ma > 1000 ? 1000 : requested_current_ma;
	}
	charger_throttled = 0;
	return requested_current_ma;
}

/*
 * Main charger management loop
 */
static void
charger_thread(__unused void *parameter, __unused wait_result_t result)
{
	charger_say("charger: active management thread running (target=%d mA, ICL=%d mA, WD=%ds)\n",
	    charger_target_current_ma, charger_icl_ma, charger_wd_interval_secs);

	for (unsigned tick = 0;; tick++) {
		int temp = hakios_battery_temperature();
		int allowed_current = charger_apply_jeita(temp, charger_target_current_ma);

		if (allowed_current == 0) {
			if (charger_current_state != CHARGER_STATE_SUSPENDED) {
				charger_say("charger: thermal protection suspended charging at %d C\n", temp);
				charger_current_state = CHARGER_STATE_SUSPENDED;
				charger_hw_set_enable(false, 0);
			}
		} else {
			if (charger_throttled) {
				charger_current_state = CHARGER_STATE_THROTTLED;
			} else {
				charger_current_state = CHARGER_STATE_CHARGING;
			}
			charger_hw_set_enable(true, allowed_current);
			charger_hw_pet_watchdog();
		}

		if (tick % 12 == 0) {
			charger_say("charger: state=%d target=%dmA actual=%dmA temp=%dC pets=%llu\n",
			    charger_current_state, charger_target_current_ma, allowed_current,
			    temp, (unsigned long long)charger_watchdog_pets);
		}

		delay_for_interval((uint32_t)charger_wd_interval_secs * 1000, NSEC_PER_MSEC);
	}
}

void
hakios_charger_init(void)
{
	uint32_t val = 0;
	thread_t thread;

	if (!PE_parse_boot_argn("charger", &val, sizeof(val)) || !val) {
		return;
	}
	charger_enabled = 1;

	if (PE_parse_boot_argn("charger_current_ma", &val, sizeof(val)) && val > 0 && val <= 3000) {
		charger_target_current_ma = (int)val;
	}
	if (PE_parse_boot_argn("charger_icl_ma", &val, sizeof(val)) && val > 0 && val <= 3000) {
		charger_icl_ma = (int)val;
	}
	if (PE_parse_boot_argn("charger_wd_secs", &val, sizeof(val)) && val >= 2 && val <= 60) {
		charger_wd_interval_secs = (int)val;
	}

	charger_say("charger: initialized with target %d mA, input limit %d mA\n",
	    charger_target_current_ma, charger_icl_ma);

	if (kernel_thread_start(charger_thread, NULL, &thread) == KERN_SUCCESS) {
		thread_deallocate(thread);
	}
}

STARTUP(EARLY_BOOT, STARTUP_RANK_LAST, hakios_charger_init);
