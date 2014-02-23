#include "stdint.h"
#include <stdbool.h>
#include "crypto.h"
#include "radio.h"

#ifndef KIWI_KI_H
#define KIWI_KI_H

#define likely(x)       __builtin_expect((x),1)
#define unlikely(x)     __builtin_expect((x),0)

#define KIWI_CHANNEL 81

#define SEND_DOUBLE_TAP_CHALLENGES 1
#define SEND_NORMAL_CHALLENGES 0

/* State machine states */
typedef enum
{
  KI_STATE_SLEEP = 0,
  KI_STATE_LISTEN_BEACON,
  KI_STATE_LISTEN_RAND,
  KI_STATE_PROCESSING_BEACON,
  KI_STATE_PROCESSING_RANDOM,
  KI_STATE_LISTEN_MANUFACTURE,
} fsm_state_t;

/* Listen times, in microseconds */
enum
{
  LISTEN_TIME_BEACON = 1500,
  LISTEN_TIME_RANDOM = 10000,
  LISTEN_TIME_MANUFACTURING = 1000,
};

/* Piece size constants */
enum
{
  SIZE_SENSOR_ID = 4,
  SIZE_KI_ID = 4,
  SIZE_RANDOM = 8,
  SIZE_CHALLENGE = 16,
};

/* Packet malarky */
enum
{
  WAIT_BEFORE_RANDOM = 50,  /* uS delay to allow sensor to start listening */
  SEND_COUNT_RANDOM = 1,
  SEND_COUNT_CHALLENGE = 1,
  SEND_COUNT_MANUFACTURING = 50,
  SEND_SPACING_RANDOM = 1,
  SEND_SPACING_CHALLENGE = 50,
  SEND_SPACING_MANUFACTURING = 50,
  PACKET_STAT_THRESH = 4,  /* Threshold for door proximity status transitioning */
  PACKET_STAT_MAX = 5,     /* Packet stat window size */
};

/* Door proximity state machine states */
typedef enum
{
  NOT_IN_FRONT_OF_DOOR,
  MAYBE_IN_FRONT_OF_DOOR,
  DEFINITELY_IN_FRONT_OF_DOOR,
  IN_FRONT_OF_DOOR_LONG_TIME,
} door_prox_state_t;

/*
 * Timing constants (mS)
 * IFOD == In Front Of Door
 */
enum
{
  MAYBE_IFOD_THRESHOLD = 1500,     /* When we decide we're not ifod */
  LONGTIME_IFOD_THRESHOLD = 13000, /* When we decide we're ifod a long time */
  DOOR_SEEN_SW_MAX = 20000,        /* Stop watch maximum */
  POLL_INTERVAL_STANDARD = 950,    /* Assuming everything is normal */
  POLL_INTERVAL_SHORT = 650,       /* Walked up to a door recently */
  POLL_INTERVAL_SHORTEST = 150,    /* Just walked up to a door */
  POLL_INTERVAL_LONG = 1950,       /* Standing IFOD for a long time */
  MOTIONLESS_TIME = 5000,         /* Time before going to deep sleep */
  DOUBLE_TAP_TIME = 5000,          /* How long to send double tap challenges */
};

/* Accelerometer constants */
enum
{
  ACC_THRESHOLD_LOWPWR_G = 0x04,    /* Sleep to wake, return to Sleep activation threshold */
  ACC_THRESHOLD_LOWPWR_DUR = 0x20,  /* Sleep to Wake, Return to Sleep duration */
  ACC_THRESHOLD_MOVEMENT = 8,
  ACC_THRESHOLD_DURATION = 0,
  /* Double Tap Sensitivity Parameters */
  ACC_DOUBLE_TAP_THRESHOLD = 0x6E,  /* Value 0-127d */
  /* Double Tap Time Parameters */
  ACC_DOUBLE_TAP_LIMIT = 0x20,      /* Value 0-127d */
  ACC_DOUBLE_TAP_LATENCY = 0x10,    /* Value 0-255d */
  ACC_DOUBLE_TAP_WINDOW = 0x30,     /* Value 0-255d */
};

/* Ki secrets */
typedef struct __attribute__((__packed__))
{
  uint8_t key_id[SIZE_KI_ID];
  uint8_t private_key[AES_BLOCK_SIZE];
} ki_secrets_t;

/* Sensor info */
typedef struct __attribute__((__packed__))
{
  uint8_t challenge_key[AES_BLOCK_SIZE];
  uint8_t sensor_id[SIZE_SENSOR_ID];
} sensor_data_t;

/* Contents of a beacon */
typedef struct __attribute__((__packed__))
{
  uint8_t sensor_id[SIZE_SENSOR_ID];
} beacon_packet_t;

/* Contents of a random packet */
typedef struct __attribute__((__packed__))
{
  uint8_t random[SIZE_RANDOM];
  uint8_t sensor_id[SIZE_SENSOR_ID];
} random_packet_t;

/* Contents of a challenge packet */
typedef struct __attribute__((__packed__))
{
  uint8_t challenge[SIZE_CHALLENGE];
  uint8_t sensor_id[SIZE_SENSOR_ID];
} challenge_packet_t;

/* Contents of a HWID packet for manufacturing */
typedef struct __attribute__((__packed__))
{
  uint32_t hw_id[2];
  uint8_t firmware_version[7];
  uint8_t sensor_id[SIZE_SENSOR_ID];
} manufacturing_uuid_packet_t;

/*
 * Contents of the secret packet from the manufacturing machine
 * in response to a manufacturing request.
 */
typedef struct __attribute__((__packed__))
{
  uint32_t ki_id;
  uint8_t secret[AES_BLOCK_SIZE];
  uint32_t hw_id[2];
  uint8_t installer_ki;
  uint8_t tracked_ki;
  uint8_t sensor_id_half[2];
} manufacturing_secrets_packet_t;

typedef struct
{
  ki_secrets_t * ki;                      /* This Ki's secrets */
  sensor_data_t challenge;                /* The cached combikey for the challenge */
  uint8_t sensor_id[SIZE_SENSOR_ID];      /* The sensor that sent the beacon */
  uint8_t ki_random[SIZE_RANDOM];         /* Our current random number */
  fsm_state_t fsm_state;                  /* Current state of the KIWI FSM */
  door_prox_state_t door_prox_state;      /* Current state of door proximity */
  uint8_t is_installer_ki;                /* Is this Ki an installer Ki? */
  uint8_t is_tracked_ki;                  /* Is this Ki a tracked Ki? */
  uint16_t seen_stopwatch;                /* Used for gauging timing for near-sensor */
  uint8_t packet_stat;                    /* Packet statistic */
  int16_t motionless_time;                /* Time device has been in motion */
  int16_t double_tap_time;                /* Time since device was double tapped */
  bool double_tap_challenges;             /* Send double tap challenges switch */
  bool double_tap_flipflop;               /* Used for sending challenges on alternate pipes */
  bool has_been_manufactured;             /* Once secrets/KiID are assigned, this gets set */
  bool is_hw_good;                        /* Do all hardware tests pass? */
  uint32_t resetreas;                     /* The most recently read value from
                                             RESETREAS */
} ki_state_t;

/* Forward Declarations */
void kiwiki_setup_state(ki_state_t *);
void kiwiki_receive_beacon(ki_state_t *, volatile radio_packet_t *);
void kiwiki_receive_random(ki_state_t *, volatile radio_packet_t *);
void kiwiki_calculate_combikey(ki_state_t *, random_packet_t *);
void kiwiki_calculate_challenge(ki_state_t *, random_packet_t *, challenge_packet_t *);
void kiwiki_step(ki_state_t *);
void kiwiki_process_mm_uuid_req(ki_state_t * state, volatile radio_packet_t * packet);
void kiwiki_process_mm_secrets(ki_state_t * state, volatile radio_packet_t * packet);
bool has_been_manufactured(ki_state_t * state);
void kiwiki_update_doubletap_timer(ki_state_t * state, int16_t update_time);

#endif
