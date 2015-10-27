/*
 * This file is part of the KIWI.KI GmbH Ki firmware.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, you can obtain one at http://mozilla.org/MPL/2.0/
 *
 */

#ifndef _DEBUG_H
#define _DEBUG_H

#ifndef __arm__
#include <stdio.h>
#include <stdbool.h>
#include <sys/time.h>

extern bool steady_state_test; /* test_main.c */

#define _debug_printf(fmt, ...) \
do \
{ \
  if (steady_state_test) \
  { \
    struct timeval lt; \
    gettimeofday(&lt, NULL); \
    printf("%ld.%.06ld%20.20s:%d\t" fmt "\n", lt.tv_sec, lt.tv_usec, \
      __FUNCTION__, __LINE__, ##__VA_ARGS__); \
  } \
} while (0)

#define _debug_break() \
  do { asm("int3"); } while (0)

#else
#ifdef SEMIHOSTED
void _printf(const char *fmt, ...);
unsigned int _clock(void);

#define _debug_printf(fmt, ...) \
do \
{ \
  _printf("%9ld %20.20s:%-4d " fmt "\n", _clock(), __FUNCTION__, __LINE__, \
    ##__VA_ARGS__); \
} while (0)
#else
#define _debug_printf(fmt, ...)
#endif
#define _debug_break()
#endif
#endif
