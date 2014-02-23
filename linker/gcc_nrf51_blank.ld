/* Linker script to configure memory regions. */
MEMORY
{
  /* We're not using a soft device, so we have full reign over the device */
  FLASH (rx) : ORIGIN = 0x0, LENGTH = 0x3EFFF
  STORAGE_REGION (r) : ORIGIN = 0x3F000, LENGTH = 0x400
  RAM (rwx) : ORIGIN = 0x20000000, LENGTH = 0x2000
}
INCLUDE "gcc_nrf51_common.ld"
