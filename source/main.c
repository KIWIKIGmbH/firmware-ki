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
