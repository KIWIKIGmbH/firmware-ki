/*
 * This file is part of the KIWI.KI GmbH Ki firmware.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, you can obtain one at http://mozilla.org/MPL/2.0/
 *
 */

#include <stdint.h>
#include <stddef.h>

#ifndef _hw_h
#define _hw_h

#ifdef __arm__
#define WFE() __WFE()
#define WFI() __WFI()
#define SEV() __SEV()
#else
#define WFE()
#define WFI()
#define SEV()
#endif

/* LFCLK frequency in Hertz, constant. */
#define LFCLK_FREQUENCY       (32768UL)

/* Required RTC working clock RTC_FREQUENCY Hertz. Changable. */
#define RTC_FREQUENCY         (1000UL)  /* 1k gives approx resolution of 1mS */

/* f = LFCLK/(prescaler + 1) */
#define COUNTER_PRESCALER     ((LFCLK_FREQUENCY/RTC_FREQUENCY) - 1)

/* GPIO defines */
#define ACC_INT1 9  /* this is the motion detection input  */
#define ACC_INT2 8  /* this is the double tap input */

#define PIN_DETECTED 1
#define PIN_DEFAULT  0

volatile int8_t movement_pin_status;
volatile int8_t double_tap_pin_status;

void wait_for_val_ne(volatile uint32_t *value);
void hw_init(void);
void hw_enable_movement_detect(void);
void hw_disable_movement_detect(void);
void hw_enable_double_tap(void);
void hw_rtc_wakeup(uint32_t ms);
uint32_t hw_rtc_value(void);
void hw_rtc_start(void);
void hw_rtc_clear(void);
void hw_rtc_stop(void);
void hw_sleep_power_off(void);
void hw_sleep_power_on(void);
void hw_clear_port_event();
void hw_switch_to_lfclock(void);
void hw_switch_to_hfclock(void);
void hw_stop_LF_clk(void);
void hw_start_LF_clk(void);
void hw_read_reset_reason(uint32_t *resetreas);
uint32_t hw_ficr_deviceid(size_t index);

#endif
