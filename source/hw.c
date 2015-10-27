/*
 * This file is part of the KIWI.KI GmbH Ki firmware.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, you can obtain one at http://mozilla.org/MPL/2.0/
 *
 */

#include "hw.h"
#include "nrf.h"
#include "nrf51.h"
#include "debug.h"
#include "nrf_gpio.h"
#include "nrf_gpiote.h"
#include "nrf_delay.h"

/* Some struct defines so that debuggers know how to read our memory */
volatile NVIC_Type *Interrupts = NVIC;
volatile SCB_Type *SystemControlBlock = SCB;

/* Function to eliminate blocking */
void wait_for_val_ne(volatile uint32_t *value)
{
  uint8_t hold_down = 0xFF;
  while (*value == 0 && hold_down--)
  {
    nrf_delay_us(1);
  }
  if (hold_down)
  {
    return;
  }
  else
  {
    /* Reboot */
    NVIC_SystemReset();
  }
}

void hw_init()
{
  /* Start the HF clock (XTAL) */
  hw_switch_to_hfclock();

  /* Start up the 32.768kHz LF clock (RC) for RTC functions */
  hw_start_LF_clk();

  /* Configure the RAM retention parameters */
  NRF_POWER->RAMON = POWER_RAMON_ONRAM0_RAM0On   << POWER_RAMON_ONRAM0_Pos
                   | POWER_RAMON_ONRAM1_RAM1Off  << POWER_RAMON_ONRAM1_Pos
                   | POWER_RAMON_OFFRAM0_RAM0Off << POWER_RAMON_OFFRAM0_Pos
                   | POWER_RAMON_OFFRAM1_RAM1Off << POWER_RAMON_OFFRAM1_Pos;

  /* Enable Send-Event-on-Send feature (enables WFE's to wake up) */
  SCB->SCR |= SCB_SCR_SEVONPEND_Msk;

  /*
   * Set up accelerometer interrupt pins to generate detect signal
   * this signal wakes the device up from sleep and also gives us
   * a low power way to detect movement
   */
  hw_enable_movement_detect();
  hw_enable_double_tap();

  /* Enable sense interrupt */
  NVIC_EnableIRQ(GPIOTE_IRQn);
  NRF_GPIOTE->INTENSET  = GPIOTE_INTENCLR_PORT_Enabled << GPIOTE_INTENCLR_PORT_Pos;
}

void hw_enable_double_tap()
{
   nrf_gpio_cfg_sense_input(
     ACC_INT2,
     NRF_GPIO_PIN_PULLDOWN,
     NRF_GPIO_PIN_SENSE_HIGH
   );
}

void hw_enable_movement_detect()
{
  nrf_gpio_cfg_sense_input(
    ACC_INT1,
    NRF_GPIO_PIN_PULLDOWN,
    NRF_GPIO_PIN_SENSE_HIGH
  );
}

void hw_disable_movement_detect()
{
  nrf_gpio_cfg_sense_input(
    ACC_INT1,
    NRF_GPIO_PIN_PULLDOWN,
    NRF_GPIO_PIN_NOSENSE
  );
}

void hw_clear_port_event()
{
  NRF_GPIOTE->EVENTS_PORT = 0;
}

/* Handle a sense interrupt */
void GPIOTE_IRQHandler(void)
{
  /* double tap? */
  if (nrf_gpio_pin_read(ACC_INT2))
  {
    double_tap_pin_status = PIN_DETECTED;
  }
  /* movement? */
  if (nrf_gpio_pin_read(ACC_INT1))
  {
    movement_pin_status = PIN_DETECTED;
  }
  NRF_GPIOTE->EVENTS_PORT = 0;
  NVIC_ClearPendingIRQ(GPIOTE_IRQn);
}

/*
 * Function to switch CPU clock source to internal LFCLK RC oscillator.
 */
void hw_switch_to_lfclock(void)
{
  /* Stop the HF xtal (falls back to the internal RC) */
  NRF_CLOCK->TASKS_HFCLKSTOP = 1;
}

/*
 * Function to start up the HF clk.
 * The CPU switches its clk src automatically .
 */
void hw_switch_to_hfclock(void)
{
  /* Mark the HF clock as not started */
  NRF_CLOCK->EVENTS_HFCLKSTARTED = 0;

  /* Try to start the HF clock */
  NRF_CLOCK->TASKS_HFCLKSTART = 1;

  /* Start clock */
  wait_for_val_ne(&NRF_CLOCK->EVENTS_HFCLKSTARTED);
}

/*
 * Function for stopping the internal LFCLK RC oscillator.
 */
void hw_stop_LF_clk(void)
{
  /* Stop the LF clock */
  NRF_CLOCK->TASKS_LFCLKSTOP = 1;
}

/*
 * Function for starting the internal LFCLK with RC oscillator as its source.
 */
void hw_start_LF_clk(void)
{
  /* Select the internal 32kHz RC */
  NRF_CLOCK->LFCLKSRC = (CLOCK_LFCLKSRC_SRC_RC << CLOCK_LFCLKSRC_SRC_Pos);

  /* Mark the LF Clock as not started */
  NRF_CLOCK->EVENTS_LFCLKSTARTED = 0;

  /* Try to start the LF clock */
  NRF_CLOCK->TASKS_LFCLKSTART = 1;

  /* Start clock */
  wait_for_val_ne(&NRF_CLOCK->EVENTS_LFCLKSTARTED);
}

void hw_read_reset_reason(uint32_t *resetreas)
{
  /* Read the reason why we reset into state. We like to store it into state so
   * that we can test how our firmware reacts in different simulated reset
   * scenarios. It also makes reading the reason from the debugger easier. */
  *resetreas = NRF_POWER->RESETREAS;

  /* Set the reset reason to 0, because it is a latched register and we have
   * read it and want it to be valid the next time we read it (not contained
   * old latched values from last time we read it). */
  NRF_POWER->RESETREAS = 0;

#if DEBUG_RESET_REASON
  /* Print some messages about why we reset. */

  _debug_printf("I reset because: 0x%08x", *resetreas);

  if (*resetreas & POWER_RESETREAS_DIF_Msk)
  {
    _debug_printf("-Reset due to wake-up from OFF mode when wakeup is"
                  " triggered from entering into debug interface mode");
  }
  if (*resetreas & POWER_RESETREAS_LPCOMP_Msk)
  {
    _debug_printf("-Reset due to wake-up from OFF mode when wakeup is"
                  " triggered from ANADETECT signal from LPCOMP");
  }
  if (*resetreas & POWER_RESETREAS_OFF_Msk)
  {
    _debug_printf("-Reset due to wake-up from OFF mode when wakeup is"
                  " triggered from the DETECT signal from GPIO");
  }
  if (*resetreas & POWER_RESETREAS_LOCKUP_Msk)
  {
    _debug_printf("-Reset from CPU lock-up detected");
  }
  if (*resetreas & POWER_RESETREAS_SREQ_Msk)
  {
    _debug_printf("-Reset from AIRCR.SYSRESETREQ detected");
  }
  if (*resetreas & POWER_RESETREAS_DOG_Msk)
  {
    _debug_printf("-Reset from watchdog detected");
  }
  if (*resetreas & POWER_RESETREAS_RESETPIN_Msk)
  {
    _debug_printf("-Reset from pin detected");
  }
#endif
}

uint32_t hw_ficr_deviceid(size_t index)
{
  return NRF_FICR->DEVICEID[index];
}

/*
 * Function for configuring the RTC to generate COMPARE0 event after t mS.
 */
void hw_rtc_wakeup(uint32_t ms)
{
  /* Set prescaler to a TICK of RTC_FREQUENCY. */
  NRF_RTC1->PRESCALER = COUNTER_PRESCALER;

  /* Compare0 after approximately COMPARE_COUNTERTIME mS */
  NRF_RTC1->CC[0] = ms;// * RTC_FREQUENCY;

  NRF_RTC1->EVTENSET = RTC_EVTEN_COMPARE0_Msk;
  NRF_RTC1->INTENSET = RTC_INTENSET_COMPARE0_Msk;

  NRF_RTC1->EVENTS_COMPARE[0] = 0;

  /* Clear the counter */
  NRF_RTC1->TASKS_CLEAR = 1;

  /* Clear and then enable the RTC IRQ */
  NVIC_ClearPendingIRQ(RTC1_IRQn);
  NVIC_EnableIRQ(RTC1_IRQn);
}

uint32_t hw_rtc_value(void)
{
  /* Return the RTC1 value */
  return NRF_RTC1->COUNTER;
}

void hw_rtc_start(void)
{
  /* Start the RTC */
  NRF_RTC1->TASKS_START = 1;
}

void hw_rtc_clear(void)
{
  /* Clear the RTC */
  NRF_RTC1->TASKS_CLEAR = 1;
  NVIC_ClearPendingIRQ(RTC1_IRQn);
}

void hw_rtc_stop(void)
{
  /* Stop and clear the RTC timer */
  NRF_RTC1->TASKS_STOP = 1;
  NRF_RTC1->TASKS_CLEAR = 1;
  NVIC_ClearPendingIRQ(RTC1_IRQn);
}

void hw_sleep_power_on(void)
{
  /* Set the power mode to power on sleeping, retain some RAM */
  NRF_POWER->RAMON = POWER_RAMON_ONRAM0_Msk | POWER_RAMON_ONRAM1_Msk;

  /* Signal an event and wait for it */
  SEV(); WFE();

  /* Go to sleep, baby */
  WFE();
}

void hw_sleep_power_off(void)
{
  /* Set the power mode to power off sleeping, no RAM retention */
  NRF_POWER->RAMON = POWER_RAMON_OFFRAM0_RAM0Off | POWER_RAMON_OFFRAM1_RAM1Off;

  /* Clear the reset reason, so that it'll be valid when we wake up. */
  NRF_POWER->RESETREAS = 0;

  /* Enter system OFF. After wakeup the chip will be reset */
  NRF_POWER->SYSTEMOFF = 1;
  while(1);
}

void RTC1_IRQHandler(void)
{
  /* This handler will be run after wakeup from system ON (RTC wakeup) */
  if(NRF_RTC1->EVENTS_COMPARE[0])
  {
    NRF_RTC1->EVENTS_COMPARE[0] = 0;
    NRF_RTC1->TASKS_CLEAR = 1;
  }
}
