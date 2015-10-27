# KIWI.KI Alice Firmware

This is the firmware for KIWI.KI Hardware that is based on the nRF51822,
namely, the Ki.

It builds with an arm-none-eabi cross-compiler, and should be able to be
built on Windows, Linux, and OSX. The tests require a native gcc target.

## Licenses

This project is a mix of software developed by KIWI.KI GmbH and others. Parts
developed by KIWI.KI GmbH are released under the MPLv2 license. Please refer
to the top of each file for specific license information. A summary of sources
is available below:

Notifications from code used under license:
* Portions copyright 2012-2015 STMicroelectronics. All rights reserved.
* Portions copyright 2009-2015 Nordic Semiconductor. All Rights Reserved.

Notifications from code used under a 3-Clause BSD License:
* Portions copyright 1986, 1988, 1991, 1993 Regents of the University of California.
  All rights reserved. 
* Portions copyright 2011 Texas Instruments Incorporated. All rights reserved.

## Toolchains

The preferred toolchain is the official ARM GCC toolchain from
<https://launchpad.net/gcc-arm-embedded>, though it should build under
any arm-none-eabi toolchain (including yagarto, sourcery g++, clang, etc.).

## Documentation

The code is full of comments. Please read them.
