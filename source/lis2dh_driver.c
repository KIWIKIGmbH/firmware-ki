/*
 * lis2dh_driver.c
 *
 *  Created on: Oct 22, 2014
 *      Author: tjordan
 */

/*
 * KiwiKi GmBH
 *
 * lis2dh accelerometer functions
 */


#include "lis3dh_driver.h"
#include "lis2dh_driver.h"
#include "spi_master.h"
#include "kiwiki.h"

/*
 * Function to initialize the LIS2DH accelerometer
 * Here we use many functions written for the LIS3DH as the driver for that
 * chip is freely available from ST and is almost identical to the LIS2DH
 */
status_t LIS2DH_init(void)
{
  status_t ret = MEMS_SUCCESS;

  /* Set up low power function */
  if ((ret = LIS3DH_SetMode(LIS3DH_LOW_POWER)) != MEMS_SUCCESS)
  {
    return MEMS_ERROR;
  }
  if ((ret = LIS2DH_SetAct_THS(ACC_THRESHOLD_LOWPWR_G)) != MEMS_SUCCESS)
  {
    return MEMS_ERROR;
  }
  if ((ret = LIS2DH_SetAct_DUR(ACC_THRESHOLD_LOWPWR_DUR)) != MEMS_SUCCESS)
  {
    return MEMS_ERROR;
  }

  /* Set up interrupt on INT1 pin on movement */
  if ((ret = LIS3DH_SetODR(LIS3DH_ODR_100Hz)) != MEMS_SUCCESS)
  {
    return MEMS_ERROR;
  }
  if ((ret = LIS3DH_HPFAOI1Enable(MEMS_ENABLE)) != MEMS_SUCCESS)
  {
    return MEMS_ERROR;
  }
  if ((ret = LIS3DH_SetFilterDataSel(MEMS_SET)) != MEMS_SUCCESS)
  {
    return MEMS_ERROR;
  }
  if ((ret = LIS3DH_SetInt1Pin(LIS3DH_I1_INT1_ON_PIN_INT1_ENABLE)) != MEMS_SUCCESS)
  {
    return MEMS_ERROR;
  }
  if ((ret = LIS3DH_SetInt1Threshold(ACC_THRESHOLD_MOVEMENT)) != MEMS_SUCCESS)
  {
    return MEMS_ERROR;
  }
  if ((ret = LIS3DH_SetInt1Duration(ACC_THRESHOLD_DURATION)) != MEMS_SUCCESS)
  {
    return MEMS_ERROR;
  }
  if ((ret = LIS3DH_SetIntConfiguration(
      LIS3DH_INT1_ZHIE_ENABLE |
      LIS3DH_INT1_YHIE_ENABLE |
      LIS3DH_INT1_XHIE_ENABLE)) != MEMS_SUCCESS)
  {
    return MEMS_ERROR;
  }
  if ((ret = LIS3DH_ResetInt1Latch()) != MEMS_SUCCESS)
  {
    return MEMS_ERROR;
  }
  /* Disable latch INT1 on movement */
  if ((ret = LIS3DH_Int1LatchEnable(MEMS_DISABLE)) != MEMS_SUCCESS)
  {
    return MEMS_ERROR;
  }

  /* Setup double-tap feature */
  if ((ret = LIS3DH_HPFClickEnable(MEMS_ENABLE)) != MEMS_SUCCESS)
  {
    return MEMS_ERROR;
  }
  if ((ret = LIS3DH_SetInt2Pin(LIS3DH_CLICK_ON_PIN_INT2_ENABLE)) != MEMS_SUCCESS)
  {
    return MEMS_ERROR;
  }
  if ((ret = LIS3DH_SetClickCFG(
      LIS3DH_ZD_ENABLE |
      LIS3DH_YD_DISABLE |
      LIS3DH_XD_DISABLE )) != MEMS_SUCCESS)
  {
    return MEMS_ERROR;
  }
  if ((ret = LIS3DH_SetClickTHS(ACC_DOUBLE_TAP_THRESHOLD)) != MEMS_SUCCESS)
  {
    return MEMS_ERROR;
  }
  if ((ret = LIS3DH_SetClickLIMIT(ACC_DOUBLE_TAP_LIMIT)) != MEMS_SUCCESS)
  {
    return MEMS_ERROR;
  }
  if ((ret = LIS3DH_SetClickLATENCY(ACC_DOUBLE_TAP_LATENCY)) != MEMS_SUCCESS)
  {
    return MEMS_ERROR;
  }
  if ((ret = LIS3DH_SetClickWINDOW(ACC_DOUBLE_TAP_WINDOW)) != MEMS_SUCCESS)
  {
    return MEMS_ERROR;
  }
  /*
   * this doesnt work, the latch doesn't latch
   * but the interrupt stays high long enough for us to read it anyway
   * so we don't actually need to latch/read/clear.
   * Left here as a comment for future "why didn't we do it like this?"...
   */
  if ((ret = LIS2DH_ResetInt2Latch()) != MEMS_SUCCESS)
  {
    return MEMS_ERROR;
  }
  /* Latch INT2 on double tap */
  if ((ret = LIS2DH_Int2LatchEnable(MEMS_ENABLE)) != MEMS_SUCCESS)
  {
    return MEMS_ERROR;
  }

/*
 DEBUGGING: Read all accelerometer registers

  uint8_t volatile i;
  uint8_t temp[0x40] = {0};
  for (i=0;i<=0x3f;i++)
  {
    LIS3DH_ReadReg(i, &temp[i]);
  }
*/
  return ret;
}

/******************************************************************************
 * The following functions are not in the driver supplied by ST in their LIS3DH
 * driver files since they are specific to the LIS2DH
 *****************************************************************************/

/*
 * Sleep to wake, return to Sleep activation threshold
 */
status_t LIS2DH_SetAct_THS(u8_t val)
{
  if (val > 127)
    return MEMS_ERROR;

  if( !LIS3DH_WriteReg(LIS2DH_Act_THS, val) )
    return MEMS_ERROR;

  return MEMS_SUCCESS;
}

/*
 * Sleep to Wake, Return to Sleep duration
 */
status_t LIS2DH_SetAct_DUR(u8_t val)
{
  if( !LIS3DH_WriteReg(LIS2DH_Act_DUR, val) )
    return MEMS_ERROR;

  return MEMS_SUCCESS;
}

/*******************************************************************************
* Function Name  : LIS2DH_Int1LatchEnable
* Description    : Enable Interrupt 2 Latching function
* Input          : ENABLE/DISABLE
* Output         : None
* Return         : Status [MEMS_ERROR, MEMS_SUCCESS]
*******************************************************************************/
status_t LIS2DH_Int2LatchEnable(State_t latch) {
  u8_t value;

  if( !LIS3DH_ReadReg(LIS3DH_CTRL_REG5, &value) )
    return MEMS_ERROR;

  value &= 0xFD;
  value |= latch << LIS2DH_LIR_INT2;

  if( !LIS3DH_WriteReg(LIS3DH_CTRL_REG5, value) )
    return MEMS_ERROR;

  return MEMS_SUCCESS;
}

/*******************************************************************************
* Function Name  : LIS2DH_ResetInt1Latch
* Description    : Reset Interrupt 1 Latching function
* Input          : None
* Output         : None
* Return         : Status [MEMS_ERROR, MEMS_SUCCESS]
*******************************************************************************/
status_t LIS2DH_ResetInt2Latch(void) {
  u8_t value;

  if( !LIS3DH_ReadReg(LIS2DH_INT2_SRC, &value) )
    return MEMS_ERROR;

  return MEMS_SUCCESS;
}



