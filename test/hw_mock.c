#include "hw.h"
#include <pthread.h>
#include <unistd.h>
#include <stdbool.h>
#include "debug.h"

bool using_lfclock = false;
bool using_hfclock = false;
uint32_t ms_to_sleep = 0;
bool event = false;
pthread_t rtc_thread;
extern bool steady_state_test; /* test_main.c */

void wait_for_val_ne(volatile uint32_t *value)
{
  uint8_t hold_down = 0xFF;
  while(*value == 0 && hold_down--)
  {
    if (steady_state_test)
    {
      usleep(1000);
    }
  }
}

void *timerthread(void * arg)
{
  if (steady_state_test)
  {
    /* Sleep for the amount of time set */
    usleep(ms_to_sleep * 1000);
  }

  /* Set the event flag */
  event = true;

  pthread_exit(NULL);
}

void hw_init()
{
  /* Goggles */
}

void hw_switch_to_lfclock(void)
{
  using_hfclock = false;
  using_lfclock = true;
}

void hw_switch_to_hfclock(void)
{
  using_lfclock = false;
  using_hfclock = true;
}

void hw_enable_movement_detect(void)
{
  ;
}

void hw_disable_movement_detect(void)
{
  ;
}

/*
 * Function for configuring the RTC to generate COMPARE0 event after t mS.
 */
void hw_rtc_wakeup(uint32_t ms)
{
  _debug_printf("Setting wakeup to %dms", ms);
  ms_to_sleep = ms;
}

void hw_rtc_start(void)
{
  /* Start the RTC */
  pthread_create(&rtc_thread, NULL, timerthread, NULL);
}

void hw_rtc_stop(void)
{
  ms_to_sleep = 0;
  event = false;
}

void hw_sleep_power_on(void)
{
  while(!event)
  {
    if (steady_state_test) {
      usleep(1000);
    }
  }
}

/*****************************/
uint8_t hw_acc_int_detect(void)
{
  return 0;
}
uint8_t hw_acc_read_dt_int(void)
{
  return 0;
}
uint32_t hw_rtc_value(void)
{
  /* Return the RTC1 value */
  return 0;
}
void hw_sleep_power_off(void)
{
  /* Set the power mode to power off sleeping, no RAM retention */
}
void hw_rtc_clear(void)
{
  /* Clear the RTC */
}
void hw_clear_port_event(void)
{
  /* Clear the port change event */
}

void hw_read_reset_reason(uint32_t *resetreas)
{
  /* We don't know what reset reason we should pretend to read, so we don't
   * do anything. */
}

uint32_t hw_ficr_deviceid(size_t index)
{
  /* Return a fake device ID. */
  switch (index)
  {
    case 0:
    {
      return 0xBEEFC007;
    }
    case 1:
    {
      return 0xC0DEBABE;
    }
    default:
    {
      return 0x00000000;
    }
  }
}
