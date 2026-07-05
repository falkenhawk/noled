#include <stdint.h>

#include <taihen.h>
#include <psp2common/kernel/modulemgr.h>
#include <psp2kern/kernel/suspend.h>
#include <psp2kern/kernel/syscon.h>
#include <psp2kern/kernel/threadmgr.h>
#include <psp2kern/lowio/gpio.h>
#include <psp2kern/power.h>

#include "led_policy.h"
#include "power_led.h"

#define SCE_LOWIO_KSCE_GPIO_PORT_SET_NID 0xD454A584

#define NOLED_POLICY_THREAD_PRIORITY 0x10000100
#define NOLED_POLICY_THREAD_STACK 0x4000
#define NOLED_POLICY_DELAY_US 500000
#define NOLED_POLICY_RESUME_SETTLE_US 500000
#define NOLED_POLICY_HEARTBEAT_LOOPS 20

void _start() __attribute__ ((weak, alias ("module_start")));

/* taihenModuleUtils export for runtime NID resolution */
extern int module_get_export_func(SceUID pid,
	const char *modname,
	uint32_t libnid,
	uint32_t funcnid,
	uintptr_t *func);

static SceUID hook_gpio_port_set_uid = -1;
static tai_hook_ref_t hook_gpio_port_set_ref;
static volatile int power_led_policy_running = 0;
static volatile int power_led_policy_resumed = 0;

static SceUID noled_gpio_port_set(int bus, int port)
{
	/* keep the OS from re-lighting the home button and game card LEDs
	 * after resume or notifications; model independent */
	if (noled_should_block_gpio_set(bus, port)) {
		return 0;
	}
	return TAI_CONTINUE(SceUID, hook_gpio_port_set_ref, bus, port);
}

static int power_led_policy_apply(int release)
{
	static uintptr_t blink_type2_func = 0;
	int (*blink_type2)(int, int, int, int);

	if (blink_type2_func == 0) {
		module_get_export_func(KERNEL_PID,
			NOLED_SYSCON_MODULE_NAME,
			NOLED_SYSCON_FOR_DRIVER_LIB_NID,
			NOLED_SYSCON_CTRL_LED_BLINK_TYPE2_NID,
			&blink_type2_func);
		if (blink_type2_func == 0) {
			return -1;
		}
	}

	blink_type2 = (int (*)(int, int, int, int))blink_type2_func;
	if (release) {
		return blink_type2(NOLED_POWER_LED_RELEASE_DEV,
			NOLED_POWER_LED_RELEASE_UNK,
			NOLED_POWER_LED_RELEASE_ON_TIME,
			NOLED_POWER_LED_RELEASE_OFF_TIME);
	}
	return blink_type2(NOLED_POWER_LED_OFF_DEV,
		NOLED_POWER_LED_OFF_UNK,
		NOLED_POWER_LED_OFF_ON_TIME,
		NOLED_POWER_LED_OFF_OFF_TIME);
}

static int power_led_off_sysevent_handler(int resume, int eventid, void *args, void *opt)
{
	(void)eventid;
	(void)args;
	(void)opt;

	if (resume) {
		power_led_policy_resumed = 1;
	}
	return 0;
}

static int power_led_policy_thread(SceSize argc, void *args)
{
	int applied_state = -1;
	int loops_since_apply = 0;
	int stable_charging;
	int last_sample;

	(void)argc;
	(void)args;

	ksceSysconWaitInitialized();
	if (!noled_power_led_indicator_supported(
		(unsigned int)ksceSysconGetHardwareInfo())) {
		/* PCH-1000 era board: classic GPIO LED blocking only */
		return 0;
	}

	ksceKernelRegisterSysEventHandler("znoled_led_policy",
		power_led_off_sysevent_handler,
		0);

	stable_charging = kscePowerIsBatteryCharging() > 0;
	last_sample = stable_charging;

	while (power_led_policy_running) {
		int sample = kscePowerIsBatteryCharging() > 0;

		/* two equal consecutive samples before switching, so brief
		 * charge-state flaps do not toggle the LED */
		if (sample == last_sample) {
			stable_charging = sample;
		}
		last_sample = sample;

		if (power_led_policy_resumed) {
			/* let the resume path finish restoring the stock LED state
			 * before overriding it again */
			ksceKernelDelayThread(NOLED_POLICY_RESUME_SETTLE_US);
			power_led_policy_resumed = 0;
			applied_state = -1;
		}

		if (applied_state != stable_charging ||
			loops_since_apply >= NOLED_POLICY_HEARTBEAT_LOOPS) {
			/* charging -> release to syscon policy (orange); idle -> dark;
			 * the heartbeat re-apply is an idempotent safety net */
			power_led_policy_apply(stable_charging);
			applied_state = stable_charging;
			loops_since_apply = 0;
		}

		ksceKernelDelayThread(NOLED_POLICY_DELAY_US);
		loops_since_apply++;
	}

	return 0;
}

int module_start(SceSize argc, const void *args)
{
	SceUID thread_uid;

	(void)argc;
	(void)args;

	ksceGpioPortClear(NOLED_GPIO_BUS, NOLED_GPIO_HOME_BUTTON_PORT);
	ksceGpioPortClear(NOLED_GPIO_BUS, NOLED_GPIO_GAME_CARD_PORT);
	hook_gpio_port_set_uid = taiHookFunctionExportForKernel(KERNEL_PID,
		&hook_gpio_port_set_ref,
		"SceLowio",
		TAI_ANY_LIBRARY,
		SCE_LOWIO_KSCE_GPIO_PORT_SET_NID,
		noled_gpio_port_set);

	/* the power LED policy thread gates itself on the model (and waits for
	 * syscon), so module_start never blocks the boot chain */
	power_led_policy_running = 1;
	thread_uid = ksceKernelCreateThread("noled_led_policy",
		power_led_policy_thread,
		NOLED_POLICY_THREAD_PRIORITY,
		NOLED_POLICY_THREAD_STACK,
		0,
		0,
		0);
	if (thread_uid >= 0) {
		if (ksceKernelStartThread(thread_uid, 0, 0) < 0) {
			ksceKernelDeleteThread(thread_uid);
			power_led_policy_running = 0;
		}
	} else {
		power_led_policy_running = 0;
	}

	return SCE_KERNEL_START_SUCCESS;
}

int module_stop(SceSize argc, const void *args)
{
	(void)argc;
	(void)args;

	/* stay resident: the GPIO hook and policy thread are not torn down at
	 * runtime; removal happens via taiHEN config plus reboot, which
	 * restores everything stock */
	power_led_policy_running = 0;
	return SCE_KERNEL_STOP_FAIL;
}
