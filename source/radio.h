#include <stdint.h>

#ifndef _radio_h
#define _radio_h

#define MAX_PACKET_SIZE 32
#define MAX_ADDRESS_LENGTH 5
#define ADDRESS_LENGTH  4

enum
{
    NRF_OUTPUT_POWER_P4_DBM =  0x04,
    NRF_OUTPUT_POWER_P0_DBM =  0x00,
    NRF_OUTPUT_POWER_N4_DBM =  0xFC,
    NRF_OUTPUT_POWER_N8_DBM =  0xF8,
    NRF_OUTPUT_POWER_N12_DBM = 0xF4,
    NRF_OUTPUT_POWER_N16_DBM = 0xF0,
    NRF_OUTPUT_POWER_N20_DBM = 0xEC,
    NRF_OUTPUT_POWER_N30_DBM = 0xD8
};

enum
{
    NRF_DATARATE_1000_KBPS =  0,
    NRF_DATARATE_2000_KBPS =  1,
    NRF_DATARATE_0250_KBPS =  2,
    NRF_DATARATE_BTLE_1MBIT = 3
};

enum
{
    NRF_CRC_0_BYTE = 0,
    NRF_CRC_1_BYTE = 1,
    NRF_CRC_2_BYTE = 2,
    NRF_CRC_3_BYTE = 3
};

typedef struct
{
  uint8_t payload[MAX_PACKET_SIZE];
  uint32_t payloadLength;
  uint32_t pipe;
  int32_t rssi;
} radio_packet_t;

enum {
  RADIO_PIPE_KIWI = 0,
  RADIO_PIPE_RAND = 1,
  RADIO_PIPE_MM_UUID_REQ = 2,
  RADIO_PIPE_MM_SECRETS = 3,
  RADIO_PIPE_NONE = 127,
};

#define LISTEN_QUANTA 100

void radio_init(void);
void radio_send_packet(volatile radio_packet_t * data, char * address, uint8_t count, uint8_t us_wait_after);
void radio_end_listen(void);
void radio_start_listen(volatile radio_packet_t * data, uint8_t is_manufacturing_mode);
uint16_t radio_middle_listen(volatile radio_packet_t * data, uint16_t us_listen_duration);
uint16_t radio_listen(volatile radio_packet_t * data, uint16_t us_listen_duration, uint8_t is_manufacturing);
void radio_shutdown(uint8_t power_off);

/* These are exposed for testing only. Don't use them */
uint8_t radio_convert_byte(const char byte);
uint32_t radio_convert_bytes(const char * bytes);

#endif
