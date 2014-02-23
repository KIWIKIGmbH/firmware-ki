#include "hw.h"
#include "crypto.h"
#include "string.h"
#include "stdlib.h"

#ifdef BOARD_KI_ALICE
#include "nrf.h"
#include "nrf51.h"
#endif

/*
 * Forward Declaration for AES Encryption/Decryption
 *
 * The software implementation is from TI, and has not been analyzed for
 * security properties (optimized for size, not speed)
 */
void aes_enc_dec(unsigned char *state, unsigned char *key, unsigned char dir);

void crypto_aes_decrypt(const uint8_t * key, const uint8_t * data, aes_state_t * state)
{
  if (!key)
  {
    return;
  }

  if (!data)
  {
    return;
  }

  if (!state)
  {
    return;
  }

  /* Clear the buffer */
  memset(state, 0, sizeof(aes_state_t));

  /* Copy the key in to the buffer */
  memcpy(state->key, key, AES_BLOCK_SIZE);

  /* Copy the data into the buffer */
  memcpy(state->in, data, AES_BLOCK_SIZE);
  memcpy(state->out, data, AES_BLOCK_SIZE);

  /* Use the TI library to decrypt */
  aes_enc_dec(state->out, state->key, 1);

  /* Fix the key */
  memcpy(state->key, key, AES_BLOCK_SIZE);
}

void crypto_aes_encrypt(const uint8_t * key, const uint8_t * data, aes_state_t * state)
{
  if (!key)
  {
    return;
  }

  if (!data)
  {
    return;
  }

  if (!state)
  {
    return;
  }

  /* Clear the buffer */
  memset(state, 0, sizeof(aes_state_t));

  /* Copy the key in to the buffer */
  memcpy(state->key, key, AES_BLOCK_SIZE);

  /* Copy the data into the buffer */
  memcpy(state->in, data, AES_BLOCK_SIZE);

#ifdef USE_HARDWARE_AES
#ifdef BOARD_KI_ALICE
  /* Set the data pointer for the AES hardware */
  NRF_ECB->ECBDATAPTR = (uint32_t)state;

  /* Clear the AES hardware status flag */
  NRF_ECB->EVENTS_ENDECB = 0;

  /* Turn on the hardware AES engine */
  NRF_ECB->TASKS_STARTECB = 1;

  /*
   * Block until AES is done
   */
  wait_for_val_ne(&NRF_ECB->EVENTS_ENDECB);

  /* Clear result flag */
  NRF_ECB->EVENTS_ENDECB = 0;

#else
#error "Asked to use hard AES acceleration, but no implementation available"
#endif
#else
/* Fallback software implementation */

  /* Copy the data into the buffer */
  memcpy(state->out, data, AES_BLOCK_SIZE);

  /* Use the TI library to encrypt */
  aes_enc_dec(state->out, state->key, 0);

  /* Fix the key */
  memcpy(state->key, key, AES_BLOCK_SIZE);
#endif
}


void crypto_gen_random_bytes(uint8_t * bytes, uint8_t count)
{
  if (!bytes)
  {
    return;
  }

  if (!count)
  {
    return;
  }

#ifdef USE_HARDWARE_RANDOM
#ifdef BOARD_KI_ALICE
  /* Tell the RNG we want some random numbers. */
  NRF_RNG->INTENSET = RNG_INTENSET_VALRDY_Set;

  /* Start the RNG task */
  NRF_RNG->TASKS_START = 1U;

  /*
   * Pull one byte of random at a time
   * from the hardware RNG until we don't
   * need anymore...
   */

  while (count--)
  {
    /* Clear the VALRDY event flag */
    NRF_RNG->EVENTS_VALRDY = 0;

    /* Get a random byte (reboot on fail) */
    wait_for_val_ne(&NRF_RNG->EVENTS_VALRDY);

    /* Read the random byte into our array */
    bytes[count] = (uint8_t) NRF_RNG->VALUE;
  };

  /* Stop the RNG task */
  NRF_RNG->TASKS_STOP = 1U;
#else
#error "Asked to use hard random generation, but no implementation available!"
#endif
#else
  /* Fallback software implementation */
  /*
   * Use rand() to generate a random
   * byte
   */
  while(count--)
  {
    bytes[count] = (uint8_t) rand();
  }

#endif
}
