/*
 * This file is part of the KIWI.KI GmbH Ki firmware.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, you can obtain one at http://mozilla.org/MPL/2.0/
 *
 */

#include "crypto.h"
#include "kiwiki.h"
#include "radio.h"
#include "hw.h"
#include "spi_master.h"
#include "lis2dh_driver.h"

/*****************************************************************************/
/** Main **/
/*****************************************************************************/
int main(void)
{
  /* KI state stored here */
  ki_state_t state;

  /* Set up the state machine */
  kiwiki_setup_state(&state);

  /* Set up the core */
  hw_init();

  /* Set up the accelerometer */
  if(!LIS2DH_init())
  {
    state.is_hw_good = false;
  }

  /* FSM */
  for (;;)
  {
    kiwiki_step(&state);
  }
}
