/*
 * lis2dh_driver.h
 *
 *  Created on: Oct 22, 2014
 *      Author: tjordan
 */

#ifndef LIS2DH_DRIVER_H_
#define LIS2DH_DRIVER_H_

#include "lis3dh_driver.h"

//CONTROL REGISTER 5
#define LIS2DH_LIR_INT2                                BIT(1)
#define LIS2DH_D4D_INT2                                BIT(0)

//INTERRUPT 2 SOURCE REGISTER
#define LIS2DH_INT2_SRC       0x35

//Sleep values
#define LIS2DH_Act_THS             0x3E
#define LIS2DH_Act_DUR             0x3F

/*
 * KiwiKi functions
 */
status_t LIS2DH_init(void);
status_t LIS2DH_SetAct_THS(u8_t val);
status_t LIS2DH_SetAct_DUR(u8_t val);
status_t LIS2DH_Int2LatchEnable(State_t latch);
status_t LIS2DH_ResetInt2Latch(void);

#endif /* LIS2DH_DRIVER_H_ */
