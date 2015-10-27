/*
 * This file is part of the KIWI.KI GmbH Ki firmware.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, you can obtain one at http://mozilla.org/MPL/2.0/
 *
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
 * LIS2DH functions
 */
status_t LIS2DH_init(void);
status_t LIS2DH_SetAct_THS(u8_t val);
status_t LIS2DH_SetAct_DUR(u8_t val);
status_t LIS2DH_Int2LatchEnable(State_t latch);
status_t LIS2DH_ResetInt2Latch(void);

#endif /* LIS2DH_DRIVER_H_ */
