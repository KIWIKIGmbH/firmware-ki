# KIWI.KI Alice Firmware

This is the firmware for KIWI.KI Hardware that is based on the nRF51822,
namely, the Ki.

It builds with an arm-none-eabi cross-compiler, and should be able to be
built on Windows, Linux, and OSX. The tests require a native gcc target.

## Licenses

This project is a mix of software developed by KIWI.KI GmbH and others. Parts
developed by KIWI.KI GmbH are released under the MPLv2 license. Please refer
to the top of each file for specific license information.

Nordic Semiconductor was generous enough to grant us permission to re-release
portions of the nRF51 SDK under a 3-Clause BSD License. This grant is included
as "Nordic Semiconductor ASA License Grant.pdf", and the text of the license is
available in "Nordic License.txt". The files as identified by their MD5 hashes
have not been altered (including the previous licensing terms) in any way.

A summary of sources and licenses follows:

Notifications from code used under implicit license grant:
* Portions copyright 2012-2015 STMicroelectronics. All rights reserved.
  Retrieved from the ST Website without registration, under the name
  "STSW-MEMS006".

Notifications from code used under a 3-Clause BSD License:
* Portions copyright 2015 Nordic Semiconductor. All rights reserved.
* Portions copyright 2009 - 2013 ARM LIMITED. All rights reserved.
* Portions copyright 2011 Texas Instruments Incorporated. All rights reserved.
* Portions copyright 1986, 1988, 1991, 1993 Regents of the University of
  California. All rights reserved.

## Toolchains

The preferred toolchain is the official ARM GCC toolchain from
<https://launchpad.net/gcc-arm-embedded>, though it should build under
any arm-none-eabi toolchain (including yagarto, sourcery g++, clang, etc.).

## Documentation

The code is full of comments. Please read them.

## Contact
KIWI.KI GmbH is responsible for this firmware. Our website is http://www.kiwi.ki -- we can be reached via email at info -at- kiwi.ki
