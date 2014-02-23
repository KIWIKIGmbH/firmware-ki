#include "radio.h"
#include "nrf.h"
#include "nrf51.h"
#include "nrf_delay.h"
#include "nrf51_bitfields.h"
#include "debug.h"
#include "string.h"
#include "nrf_nvmc.h"
#include "hw.h"

/* Mock struct so that we can see it when debugging */
NRF_RADIO_Type *RadioPtr = NRF_RADIO;

/* Perform initial setup of the RADIO hardware */
void radio_init()
{
  RadioPtr->POWER = 1;
  RadioPtr->TXPOWER = NRF_OUTPUT_POWER_P0_DBM;
  RadioPtr->MODE = NRF_DATARATE_2000_KBPS;
  RadioPtr->PCNF0 = 0x000000UL;
  RadioPtr->PCNF1 = 0x1030400UL;
  RadioPtr->RXADDRESSES = 0x03;
  RadioPtr->CRCCNF = NRF_CRC_1_BYTE;
  RadioPtr->CRCPOLY = 0x107;
  RadioPtr->CRCINIT = 0xFF;
  RadioPtr->FREQUENCY = 0x51;
  RadioPtr->SHORTS = 0b00000000;

  /* Short delay to allow the radio to spin up */
  nrf_delay_ms(3);
}

/* Lookup Table for reversing bits */
static const uint8_t radio_bit_lookup[16] =
{
   0x0, 0x8, 0x4, 0xC,
   0x2, 0xA, 0x6, 0xE,
   0x1, 0x9, 0x5, 0xD,
   0x3, 0xB, 0x7, 0xF
};

/* Flip the bit order */
uint8_t radio_convert_byte(const char byte)
{
  return (radio_bit_lookup[byte % 16] << 4) | radio_bit_lookup[byte / 16];
}

/* Flip the bit orders for up to four bytes (but not the first byte) */
uint32_t radio_convert_bytes(const char * bytes)
{
  uint8_t i = 1;
  uint32_t ret = 0;

  if (bytes == 0)
  {
    return 0;
  }

  uint8_t addrlen = strlen(bytes);

  /* Shift reversed bytes in until we're at the address size */
  do
  {
    ret = ret << 8 | radio_convert_byte(bytes[i]);
    i++;
  } while (i < ADDRESS_LENGTH && i < addrlen);

  /* Keep shifting until the first byte is in the leftmost position */
  while (i++ < MAX_ADDRESS_LENGTH)
  {
    ret = ret << 8;
  }

  return ret;
}

void radio_start_listen(volatile radio_packet_t * data, uint8_t is_manufacturing_mode)
{

  if (!data)
  {
    _debug_printf("NULL data pointer%s", "");
    return;
  }

  /* Set up the radio */
  radio_init();

  /* Set the pipe to some value that could never happen */
  data->pipe = RADIO_PIPE_NONE;

  /* Turn off the RADIO Task */
  radio_shutdown(0);

  /* Set the packet source pointer */
  RadioPtr->PACKETPTR = (uint32_t)data->payload;

  /* Set up the RADIO parameters */
  RadioPtr->PCNF1 =
            /* Disable data whitening agent */
            (RADIO_PCNF1_WHITEEN_Disabled << RADIO_PCNF1_WHITEEN_Pos) |
            /* Set endianness to BIG */
            (RADIO_PCNF1_ENDIAN_Big << RADIO_PCNF1_ENDIAN_Pos) |
            /* Set 'base' address length */
            ((ADDRESS_LENGTH - 1) << RADIO_PCNF1_BALEN_Pos) |

            /* Set expected packet length  */
            (data->payloadLength << RADIO_PCNF1_STATLEN_Pos) |

            /* Set the maximum length to the actual max packet size */
            (MAX_PACKET_SIZE << RADIO_PCNF1_MAXLEN_Pos);

  /* Set the listen addresses */
  if (is_manufacturing_mode)
  {
    RadioPtr->PREFIX0 = radio_convert_byte('S') << 24 |  /* 4. Manufacture secrets */
                        radio_convert_byte('M') << 16;   /* 3. Manufacture uuid? */
    RadioPtr->RXADDRESSES = 0x0C;
  }
  else
  {
    RadioPtr->PREFIX0 = radio_convert_byte('R') << 8 |
                        radio_convert_byte('K');
    RadioPtr->RXADDRESSES = 0x03;
  }

  /* Set address bases */
  RadioPtr->BASE0 = radio_convert_bytes("KIWI");  /* Beacon pipe */
  RadioPtr->BASE1 = radio_convert_bytes("RNKI");  /* Random pipe */

  /* Clear the event ready task flag */
  RadioPtr->EVENTS_READY = 0U;

  /* Tell the radio that the task is now receiving */
  RadioPtr->TASKS_RXEN = 1U;

  /* Wait for radio to ramp up */
  wait_for_val_ne(&RadioPtr->EVENTS_READY);

  _debug_printf("XCVR Turned on in RX mode%s", "");
}

uint16_t radio_middle_listen(volatile radio_packet_t * data, uint16_t us_listen_duration)
{
  if (!data || !us_listen_duration)
  {
    return 0;
  }

  /* Clear packet RX'd */
  RadioPtr->EVENTS_END = 0U;   /* clr END (packet received) flag */

  /* Break listen time into LISTEN_QUANTA quanta */
  uint32_t wait_time = us_listen_duration / LISTEN_QUANTA;

  /* Start Listening */
  RadioPtr->TASKS_START = 1U;
  while (wait_time-- && (RadioPtr->EVENTS_END == 0U))
  {
    nrf_delay_us(LISTEN_QUANTA);
  }

  /* If no packet was received the whole duration */
  if (RadioPtr->EVENTS_END == 0U)
  {
    _debug_printf("No packet received.%s", "");
    return 0;
  }

  /*
   * By here, we know we got a packet
   */
  data->pipe = RadioPtr->RXMATCH;
  data->rssi = RadioPtr->RSSISAMPLE * -1;

  /* Return the amount of time left to listen */
  return wait_time * LISTEN_QUANTA;
}

void radio_end_listen(void)
{
  /* Stop the radio task */
  radio_shutdown(0);

  _debug_printf("XCVR should shutdown%s", "");
}

/*
 * Turn on the radio to listen for a packet for a specified number
 * of microseconds.
 *
 * Returns zero if no packet, number of us left to listen otherwise
 */
uint16_t radio_listen(volatile radio_packet_t * data, uint16_t us_listen_duration, uint8_t is_manufacturing)
{

  radio_start_listen(data, is_manufacturing);
  uint16_t result = (radio_middle_listen(data, us_listen_duration));
  radio_end_listen();
  return result;
}

/*
 * Transmit a packet
 * * to the specified address
 * * the specified number of times
 * * and wait a certain amount of microseconds between each packet.
 */
void radio_send_packet(volatile radio_packet_t * data, char * address, uint8_t count, uint8_t us_wait_after)
{

  if (!count || !data || !address)
  {
    return;
  }

  /* Turn off the RADIO Task */
  RadioPtr->TASKS_DISABLE = 1U;

  /* Block until the radio is off */
  wait_for_val_ne(&RadioPtr->EVENTS_DISABLED);

  /* Set the packet source pointer */
  RadioPtr->PACKETPTR = (uint32_t)data->payload;
  /* Set up the RADIO parameters */
  RadioPtr->PCNF1 =
            /* Disable data whitening agent */
            (RADIO_PCNF1_WHITEEN_Disabled << RADIO_PCNF1_WHITEEN_Pos) |
            /* Set endianness to BIG */
            (RADIO_PCNF1_ENDIAN_Big << RADIO_PCNF1_ENDIAN_Pos) |
            /* Set 'base' address length */
            ((ADDRESS_LENGTH - 1) << RADIO_PCNF1_BALEN_Pos) |
            /* Set packet length */
            ((data->payloadLength) << RADIO_PCNF1_STATLEN_Pos) |
            /* Set the maximum length to zero (ignore) */
            (MAX_PACKET_SIZE << RADIO_PCNF1_MAXLEN_Pos);

  /* Clear the SHORTS register */
  RadioPtr->SHORTS = 0b00000000;

  /* Calculate the proper addresses */
  RadioPtr->PREFIX0 = (uint32_t)radio_convert_byte(address[0]);
  RadioPtr->BASE0 = radio_convert_bytes(address);

  _debug_printf("Sending packet to address %s 0x%02x%08x",
      address,
      RadioPtr->PREFIX0,
      RadioPtr->BASE0);

  /* Tell the RADIO to use the above address */
  RadioPtr->TXADDRESS = 0;

  /* Clear event ready */
  RadioPtr->EVENTS_READY = 0U;

  /* Enable the TX Task */
  RadioPtr->TASKS_TXEN = 1U;

  /* Block until Radio is powered up */
  wait_for_val_ne(&RadioPtr->EVENTS_READY);

  while (count--)
  {
    /* Clear event flag */
    RadioPtr->EVENTS_END = 0U;

    /* Start the task */
    RadioPtr->TASKS_START = 1U;

    /* Reset the hold-down timer */
    uint8_t hold_down_timer = 255;

    /* Block until sent */
    while (!RadioPtr->EVENTS_END && hold_down_timer--);

    /* Wait the packet spacing */
    nrf_delay_us(us_wait_after);
  }

  /* Stop Radio Task */
  radio_shutdown(0);
}

void radio_shutdown(uint8_t power_off)
{
  /* Turn off the radio event generator */
  RadioPtr->EVENTS_DISABLED = 0U;

  /* Shutdown the transmitter */
  RadioPtr->TASKS_DISABLE = 1U;

  /* Block until the radio is off */
  wait_for_val_ne(&RadioPtr->EVENTS_DISABLED);

  if (power_off)
  {
    /* Turn power off */
    RadioPtr->POWER = 0;
  }
}

void RADIO_IRQHandler(void)
{
  NVIC_ClearPendingIRQ(RADIO_IRQn);
}
