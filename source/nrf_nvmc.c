/* Copyright (c) 2012 Nordic Semiconductor. All Rights Reserved.
*
* The information contained herein is property of Nordic Semiconductor ASA.
* Terms and conditions of usage are described in detail in NORDIC
* SEMICONDUCTOR STANDARD SOFTWARE LICENSE AGREEMENT.
*
* Licensees are granted free, non-transferable use of the information. NO
* WARRANTY of ANY KIND is provided. This heading must NOT be removed from
* the file.
*
* $LastChangedRevision: 17685 $
*/

/**
 *@file
 *@brief NMVC driver implementation
 */

#include "stdbool.h"
#include "nrf.h"
#include "nrf_nvmc.h"
#include "nrf_delay.h"

void nrf_nvmc_wait_for_ready(void)
{
  uint8_t hold_down_timer;
  hold_down_timer = ~0;
  while (NRF_NVMC->READY == NVMC_READY_READY_Busy  && hold_down_timer--)
  {
    nrf_delay_us(5);
  }
}

void nrf_nvmc_page_erase(uint32_t address)
{
  // Enable erase.
  NRF_NVMC->CONFIG = NVMC_CONFIG_WEN_Een;
  nrf_nvmc_wait_for_ready();

  // Erase the page
  NRF_NVMC->ERASEPAGE = address;
  nrf_nvmc_wait_for_ready();

  NRF_NVMC->CONFIG = NVMC_CONFIG_WEN_Ren;
  nrf_nvmc_wait_for_ready();
}

void nrf_nvmc_write_word(uint32_t address, uint32_t value)
{
  // Enable write.
  NRF_NVMC->CONFIG = NVMC_CONFIG_WEN_Wen;
  nrf_nvmc_wait_for_ready();

  *(uint32_t*)address = value;
  nrf_nvmc_wait_for_ready();

  NRF_NVMC->CONFIG = NVMC_CONFIG_WEN_Ren;
  nrf_nvmc_wait_for_ready();
}

void nrf_nvmc_write_words(uint32_t address, const uint32_t * src, uint32_t num_words)
{
  uint32_t i;

  // Enable write.
  NRF_NVMC->CONFIG = NVMC_CONFIG_WEN_Wen;
  nrf_nvmc_wait_for_ready();

  for(i=0;i<num_words;i++)
  {
    ((uint32_t*)address)[i] = src[i];
    nrf_nvmc_wait_for_ready();
  }

  NRF_NVMC->CONFIG = NVMC_CONFIG_WEN_Ren;
  nrf_nvmc_wait_for_ready();
}

