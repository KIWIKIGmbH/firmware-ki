#include "stdint.h"

#ifndef _crypto_h
#define _crypto_h

/* Use the hardware accelerated features,
 * if available */
#ifdef __arm__
#ifdef BOARD_KI_ALICE
#define USE_HARDWARE_RANDOM
#define USE_HARDWARE_AES
#endif
#endif

#define AES_BLOCK_SIZE 16

/* State struct for AES operations */
typedef struct __attribute__((__packed__)) {
  uint8_t key[AES_BLOCK_SIZE];
  uint8_t in[AES_BLOCK_SIZE];
  uint8_t out[AES_BLOCK_SIZE];
} aes_state_t;

/* Forward declarations */
void crypto_aes_encrypt(const uint8_t * key, const uint8_t * data, aes_state_t * state);
void crypto_aes_decrypt(const uint8_t * key, const uint8_t * data, aes_state_t * state);
void crypto_gen_random_bytes(uint8_t * bytes, uint8_t count);

#endif
