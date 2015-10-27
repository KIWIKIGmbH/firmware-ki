/*
 * This file is part of the KIWI.KI GmbH Ki firmware.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, you can obtain one at http://mozilla.org/MPL/2.0/
 *
 */

#include "string.h"
#include "kiwiki.h"
#include "hw.h"
#include "debug.h"
#include "nrf_delay.h"
#include "nrf_nvmc.h"

/*
 * This will allocate space in the binary
 * to hold the secrets. We will use this
 * during manufacturing
 */
static ki_secrets_t flash_secret_data
__attribute__((section(".storage_section"))) =
{
  .key_id = { [0 ... SIZE_KI_ID - 1] = 0xff },
  .private_key = { [0 ... AES_BLOCK_SIZE-1] = 0xff }
};

static uint8_t flash_is_installer_ki
__attribute__((section(".storage_section"))) = 0xff;

static uint8_t flash_is_tracked_ki
__attribute__((section(".storage_section"))) = 0xff;

/* We'll use this as a packet buffer */
volatile radio_packet_t packet = {
  .payload = {},
  .payloadLength = 0,
  .pipe = RADIO_PIPE_NONE,
  .rssi = 0
};

/*
 * Check to see if we have been manufactured yet
 */
bool has_been_manufactured(ki_state_t * state)
{
  uint8_t i;
  for (i = 0; i < SIZE_KI_ID; i++)
  {
    if (state->ki->key_id[i] != 0xff)
    {
      return true;
    }
  }
  for(i = 0; i < AES_BLOCK_SIZE; i++)
  {
    if(state->ki->private_key[i] != 0xff)
    {
      return true;
    }
  }
  return false;
}

/*
 * Initialize a ki_state_t, ready to be used
 */
void kiwiki_setup_state(ki_state_t * state)
{
  ki_state_t work_state =
  {
    .ki = (ki_secrets_t *)&flash_secret_data,
    .challenge = {{0},{0}},
    .sensor_id = {0},
    .ki_random = {0},
    .fsm_state = KI_STATE_SLEEP,
    .door_prox_state = MAYBE_IN_FRONT_OF_DOOR,
    .is_installer_ki = (uint8_t)flash_is_installer_ki,
    .is_tracked_ki = (uint8_t)flash_is_tracked_ki,
    .seen_stopwatch = 0,
    .packet_stat = 0,
    .motionless_time = MOTIONLESS_TIME,
    .has_been_manufactured = false,
    .is_hw_good = true,
  };

  /* Check to see if we have been manufactured yet */
  work_state.has_been_manufactured = has_been_manufactured(&work_state);

  /* Check to see why we reset. */
  hw_read_reset_reason(&work_state.resetreas);

  memcpy(state, &work_state, sizeof(ki_state_t));
}

void kiwiki_set_state(ki_state_t * state, fsm_state_t newstate)
{
  _debug_printf("State Change. Old: %d, New: %d", state->fsm_state, newstate);
  state->fsm_state = newstate;
}

/* Update our "packet rate" counter */
void kiwiki_update_pckt_rate(ki_state_t * state)
{
  if (packet.pipe != RADIO_PIPE_NONE)
  {
    if(state->packet_stat < PACKET_STAT_MAX)
    {
      state->packet_stat++;
    }
  }
  else
  {
    if(state->packet_stat > 0)
    {
      state->packet_stat--;
    }
  }
}

/*
 * Update the double-tap counter
 */
void kiwiki_update_doubletap_timer(ki_state_t * state, int16_t update_time)
{
  if (double_tap_pin_status == PIN_DETECTED)
  {
    /* If double tap detected */
    state->double_tap_time = DOUBLE_TAP_TIME;
    state->double_tap_challenges = SEND_DOUBLE_TAP_CHALLENGES;

    double_tap_pin_status = PIN_DEFAULT;
  }
  else
  {
    /* subtract awake time from double tap timer */
    if (state->double_tap_time > 0)
    {
      state->double_tap_time = state->double_tap_time - update_time;
    }
    else
    {
      /* If double tap timer negative, stop sending double tap challenges
       * over DHAL pipe */
      state->double_tap_challenges = SEND_NORMAL_CHALLENGES;
    }
  }
}

/*
 * THIS IS OUR MAIN STATE MACHINE
 */
void kiwiki_step(ki_state_t * state)
{
  /* How long is left to listen */
  uint16_t listen_time_left = 0;

  /* How long to sleep */
  uint16_t sleep_time;

  /* How long we were awake */
  int16_t awake_time;

  switch (state->fsm_state)
  {
    case KI_STATE_LISTEN_BEACON:
      /* Set expected packet size */
      if(likely(state->has_been_manufactured))
      {
        packet.payloadLength = sizeof(beacon_packet_t);
      }
      else
      {
        packet.payloadLength = sizeof(manufacturing_secrets_packet_t);
      }

      /* Turn on the XCVR for RX */
      radio_start_listen(&packet, !state->has_been_manufactured);

      if(likely(state->has_been_manufactured))
      {
        /* Listen for a beacon.
         * Keep listening while we have time and have not yet received a beacon,
         * even if we get something else */
        listen_time_left = LISTEN_TIME_BEACON;
        while (listen_time_left && packet.pipe != RADIO_PIPE_KIWI)
        {
          listen_time_left = radio_middle_listen(&packet, listen_time_left);
        }
      }
      else
      {
        /* Listen for a manufacturing machine beacon.
         * Keep listening while we have time and have not yet received a beacon,
         * even if we get something else */
        listen_time_left = LISTEN_TIME_MANUFACTURING;
        while (listen_time_left && (
                (packet.pipe != RADIO_PIPE_MM_UUID_REQ) &&
                (packet.pipe != RADIO_PIPE_MM_SECRETS)
               ))
        {
          listen_time_left = radio_middle_listen(&packet, listen_time_left);
        }
      }

      /* Turn off the XCVR */
      radio_end_listen();

      /* Didn't get a packet during timeout, just go to sleep */
      if (listen_time_left == 0)
      {
        kiwiki_set_state(state, KI_STATE_SLEEP);
        break;
      }

      /* Update our "packet rate" counter */
      kiwiki_update_pckt_rate(state);

      _debug_printf("Packet on pipe %d", packet.pipe);

      /* If we have been manufactured, listen only for beacons */
      if (state->has_been_manufactured)
      {
        /* Did we get a beacon? */
        if (packet.pipe == RADIO_PIPE_KIWI)
        {
          /* We received a beacon, process it */
          kiwiki_receive_beacon(state, &packet);
        }
        /* Packet recieved on wrong pipe. Go back to sleep */
        else
        {
          kiwiki_set_state(state, KI_STATE_SLEEP);
        }
      }
      /* If we have not been manufactured, listen only for uuid requests (or secrets) */
      else
      {
        /* Did we get a request from the manufacturing machine for our uuid? */
        if (packet.pipe == RADIO_PIPE_MM_UUID_REQ)
        {
          if (state->is_hw_good)
          {
            kiwiki_process_mm_uuid_req(state, &packet);
          }
        }
        /* Did we get secrets from a the manufacturing machine? */
        else if (packet.pipe == RADIO_PIPE_MM_SECRETS)
        {
          kiwiki_process_mm_secrets(state, &packet);
        }
        /* Packet received on wrong pipe. Back to sleep */
        else
        {
          kiwiki_set_state(state, KI_STATE_SLEEP);
        }
      }
    break;

    case KI_STATE_LISTEN_RAND:

      /* Set expected packet size */
      packet.payloadLength = sizeof(random_packet_t);

      /* Turn on the XCVR for RX */
      radio_start_listen(&packet, !state->has_been_manufactured);

      /* Listen for a random for some amount of time */
      listen_time_left = LISTEN_TIME_RANDOM;

      random_packet_t * sensor_rand_pckt = (random_packet_t *)packet.payload;

      /*
       * Keep listening while we have time and have not yet received a random,
       * even if we get something else
       *
       * check also that the RAND is from the same sensor as the BEACON
       */
      while(listen_time_left)
      {
        listen_time_left = radio_middle_listen(&packet, listen_time_left);
        if (packet.pipe == RADIO_PIPE_RAND &&
          !memcmp(sensor_rand_pckt->sensor_id, state->sensor_id, SIZE_SENSOR_ID)
           )
        {
          break;
        }
      }

      /* Turn off the XCVR */
      radio_end_listen();

      /* Did we get a random? */
      if (listen_time_left)
      {
        /* yes, process it and send a bunch of challenges */
        kiwiki_receive_random(state, &packet);
        kiwiki_set_state(state, KI_STATE_SLEEP);
      }
      else
      {
        /* No random after sending ours? Listen for another beacon. */
        kiwiki_set_state(state, KI_STATE_LISTEN_BEACON);
      }
      break;

    default:
    case KI_STATE_SLEEP:
      _debug_printf("Going to sleep%s", "");

      /* Cut Radio Power */
      radio_shutdown(1);

      awake_time = (int16_t) hw_rtc_value();

      /* Add awake time to our door seen stop-watch */
      if (state->seen_stopwatch < DOOR_SEEN_SW_MAX)
      {
        state->seen_stopwatch += awake_time;
      }

      kiwiki_update_doubletap_timer(state, awake_time);
      if (state->double_tap_challenges == SEND_DOUBLE_TAP_CHALLENGES)
      {
        state->door_prox_state = NOT_IN_FRONT_OF_DOOR;
      }

      /* Wait an amount of time based on when we last saw a door */
      switch (state->door_prox_state)
      {
        case MAYBE_IN_FRONT_OF_DOOR:
          if (state->seen_stopwatch > MAYBE_IFOD_THRESHOLD)
          {
            _debug_printf("Moved away from door...%s", "");
            state->door_prox_state = NOT_IN_FRONT_OF_DOOR;
            sleep_time = POLL_INTERVAL_STANDARD;
          }
          else if (state->packet_stat >= PACKET_STAT_THRESH)
          {
            _debug_printf("Door found for sure...%s", "");
            state->seen_stopwatch = 0;
            state->door_prox_state = DEFINITELY_IN_FRONT_OF_DOOR;
            sleep_time = POLL_INTERVAL_SHORT;
          }
          else
          {
            _debug_printf("Maybe in front of door...%s", "");
            sleep_time = POLL_INTERVAL_SHORTEST;
          }
          break;

        case NOT_IN_FRONT_OF_DOOR:
          if (state->packet_stat > 0)
          {
            _debug_printf("Maybe a door there...%s", "");
            state->seen_stopwatch = 0;
            state->door_prox_state = MAYBE_IN_FRONT_OF_DOOR;
            sleep_time = POLL_INTERVAL_SHORTEST;
          }
          else
          {
            /*
             * Normal ~1.5 second poll time with some random jitter
             * Here we subtract 16 and add a random of between 0 and 32
             * which gives us a +- 16uS jitter.
             */
            _debug_printf("Ki inactive...%s", "");


            /* Jitter index: this is just a barrel rolling index into the
             * random number array used for pseudo random jitter */
            static int8_t jindx;

            /* Move the jitter index */
            if(--jindx < 0)
            {
              jindx = SIZE_RANDOM - 1;
            }
            sleep_time = POLL_INTERVAL_STANDARD - 16 +
                (state->ki_random[jindx] & 0x1F);
          }
          break;

        case DEFINITELY_IN_FRONT_OF_DOOR:
          if (state->packet_stat < PACKET_STAT_THRESH)
          {
            _debug_printf("Possibly walked away from the door...%s", "");
            state->seen_stopwatch = 0;
            state->door_prox_state = MAYBE_IN_FRONT_OF_DOOR;
            sleep_time = POLL_INTERVAL_SHORTEST;
          }
          else if (state->seen_stopwatch > LONGTIME_IFOD_THRESHOLD)
          {
            _debug_printf("Been in front of door for a while now...%s", "");
            state->door_prox_state = IN_FRONT_OF_DOOR_LONG_TIME;
            sleep_time = POLL_INTERVAL_LONG;
          }
          else
          {
            _debug_printf("Definitely in front of door...%s", "");
            sleep_time = POLL_INTERVAL_SHORT;
          }
          break;

        case IN_FRONT_OF_DOOR_LONG_TIME:
          if (state->packet_stat < PACKET_STAT_THRESH)
          {
            _debug_printf("Possibly walked away from the door...%s", "");
            state->seen_stopwatch = 0;
            state->door_prox_state = MAYBE_IN_FRONT_OF_DOOR;
            sleep_time = POLL_INTERVAL_SHORTEST;
          }
          else
          {
            _debug_printf("Ki sitting in front of door...%s", "");
            sleep_time = POLL_INTERVAL_LONG;
          }
          break;

        /* Should never get here */
        default:
          state->door_prox_state = NOT_IN_FRONT_OF_DOOR;
          sleep_time = POLL_INTERVAL_STANDARD;
          break;
      }

      if (movement_pin_status == PIN_DETECTED)
      {
        /* If motion detected by accelerometer, reset counter */
        state->motionless_time = MOTIONLESS_TIME;

        /* Disable the movement sense momentarily so that we can distinguish
         * a double tap sense if it occurs.
         * This gets re-enabled when we wake from a light sleep next */
        hw_disable_movement_detect();

        /* Reset the pin state */
        movement_pin_status = PIN_DEFAULT;
      }
      else
      {
        /*
         * else no motion was detected
         *  - subtract awake time from the motionless timer
         * */
        if ((state->motionless_time -= awake_time) < 0)
        {
          /*
           * We have not detected motion for over 30 seconds: go to deep sleep
           * The next interrupt from the accelerometer will cause a CPU reset
           */
          hw_clear_port_event();
          hw_enable_movement_detect();
          hw_sleep_power_off();
          /*  * * * * * * * * * * * * * * *
           * We are DEEPLY sleeping here  *
           * Reset will occur on next int *
           *       from accelerometer     *
           *  * * * * * * * * * * * * * * */
        }
      }

      /*
       * Set the alarm clock time.
       * this also clears the rtc so that we will know how long we slept.
       */
      hw_rtc_wakeup(sleep_time);

      /* Turn on the alarm clock */
      hw_rtc_start();

      /* Turn off HF clock */
      hw_switch_to_lfclock();

      /* Go to sleep */
      hw_sleep_power_on();

      /*  * * * * * * * * * * * * * * *
       * We are LIGHTLY sleeping here *
       *  * * * * * * * * * * * * * * */

      /* Reset the rtc so that we will know the time spent awake */
      hw_rtc_clear();

      /* Andale, andale! */
      hw_switch_to_hfclock();

      /* subtract however long we were asleep for from our motionless timer */
      state->motionless_time -=  sleep_time;
      hw_enable_movement_detect();

      /* Add sleep time to our door seen stop-watch */
      if (state->seen_stopwatch < DOOR_SEEN_SW_MAX)
      {
        state->seen_stopwatch += sleep_time;
      }

      /* Add sleep time to our double tap timer */
      kiwiki_update_doubletap_timer(state, (int16_t) sleep_time);

      kiwiki_set_state(state, KI_STATE_LISTEN_BEACON);
      break;
  }
}

/*
 * We received a beacon
 *
 * Send our random number, and then listen for a random number
 */
void kiwiki_receive_beacon(ki_state_t * state, volatile radio_packet_t * packet)
{
  beacon_packet_t * beacon = (beacon_packet_t *)packet->payload;

  _debug_printf("Received beacon for sensor id 0x%02x%02x%02x%02x",
    beacon->sensor_id[0],
    beacon->sensor_id[1],
    beacon->sensor_id[2],
    beacon->sensor_id[3]);

  /*
   * Copy the sensor's ID out of the beacon
   */
  memcpy(state->sensor_id, beacon->sensor_id, SIZE_SENSOR_ID);

  /*
   * Generate a ki->sensor random packet
   */
  random_packet_t ki_random;
  memcpy(ki_random.random, state->ki_random, sizeof(ki_random.random));
  memcpy(ki_random.sensor_id, state->sensor_id, sizeof(ki_random.sensor_id));

  radio_packet_t ki_random_pckt;
  memcpy(ki_random_pckt.payload, &ki_random, sizeof(random_packet_t));
  ki_random_pckt.payloadLength = sizeof(random_packet_t);

  /* wait for the sensor to be listening */
  nrf_delay_us(WAIT_BEFORE_RANDOM);
  /*
   * Send ki->sensor random packet
   */
  radio_send_packet(&ki_random_pckt, "RAND", SEND_COUNT_RANDOM, SEND_SPACING_RANDOM);

   /*
   * Now listen for a response from the sensor
   */
  kiwiki_set_state(state, KI_STATE_LISTEN_RAND);
}

/*
 * Transmit our HWID (UUID)
 */
void kiwiki_process_mm_uuid_req(ki_state_t * state, volatile radio_packet_t * packet)
{
  beacon_packet_t * beacon = (beacon_packet_t *)packet->payload;

  /*
   * Prepare a uuid packet for the manufacturing machine
   */
  manufacturing_uuid_packet_t uuid_pckt;

  /*
   * Copy the sensor's ID out of the beacon
   */
  memcpy(state->sensor_id, beacon->sensor_id, SIZE_SENSOR_ID);

  /* Copy UUID into packet */
  uuid_pckt.hw_id[0] = hw_ficr_deviceid(0);
  uuid_pckt.hw_id[1] = hw_ficr_deviceid(1);

  /* Copy the git revision number in */
  memcpy(uuid_pckt.firmware_version, PROGRAM_VERSION,
      sizeof(uuid_pckt.firmware_version));

  /* Copy sensor id in */
  memcpy(uuid_pckt.sensor_id, state->sensor_id,
      sizeof(uuid_pckt.sensor_id));

  radio_packet_t ki_manufacture_pckt;
  memcpy(ki_manufacture_pckt.payload, &uuid_pckt,
      sizeof(uuid_pckt));
  ki_manufacture_pckt.payloadLength = sizeof(uuid_pckt);

  /*
   * Send UUID packet
   */
  radio_send_packet(&ki_manufacture_pckt, "MHAL", SEND_COUNT_MANUFACTURING,
      SEND_SPACING_MANUFACTURING);
}

/*
 * Write the received secrets into flash
 */
void kiwiki_process_mm_secrets(ki_state_t * state, volatile radio_packet_t * packet)
{
  manufacturing_secrets_packet_t* secrets =
      (manufacturing_secrets_packet_t *) packet->payload;
  /*
   * Check that the packet is from the manufacturing machine
   * and that the secret and ki_id are for this hw id
   */
    if (
      memcmp(secrets->sensor_id_half, state->sensor_id, 2) != 0 ||
      secrets->hw_id[0] != hw_ficr_deviceid(0) ||
      secrets->hw_id[1] != hw_ficr_deviceid(1)
      )
    {
      return;
    }

  /*
   * Now we know we have received manufacturing secrets for this device,
   * write them to flash and reboot.
   */

  /* erase the flash page */
  nrf_nvmc_page_erase((uint32_t)&flash_secret_data);

  /*
   * Write Ki ID
   */
  uint32_t word;
  word = ((uint32_t)((secrets->ki_id&0x000000FF) >> 0) << 24) |
         ((uint32_t)((secrets->ki_id&0x0000FF00) >> 8) <<16) |
         ((uint32_t)((secrets->ki_id&0x00FF0000) >> 16)<<8)  |
          (uint32_t)((secrets->ki_id&0xFF000000) >> 24);
  nrf_nvmc_write_word((uint32_t)&flash_secret_data.key_id[0], word);


  word = ((uint32_t)secrets->secret[0]<<24) |
         ((uint32_t)secrets->secret[1]<<16) |
         ((uint32_t)secrets->secret[2]<<8)  |
          (uint32_t)secrets->secret[3];
  nrf_nvmc_write_word((uint32_t)&flash_secret_data.private_key[12], word);

  word = ((uint32_t)secrets->secret[4]<<24) |
         ((uint32_t)secrets->secret[5]<<16) |
         ((uint32_t)secrets->secret[6]<<8)  |
          (uint32_t)secrets->secret[7];
  nrf_nvmc_write_word((uint32_t)&flash_secret_data.private_key[8], word);

  word = ((uint32_t)secrets->secret[8]<<24) |
         ((uint32_t)secrets->secret[9]<<16) |
         ((uint32_t)secrets->secret[10]<<8)  |
          (uint32_t)secrets->secret[11];
  nrf_nvmc_write_word((uint32_t)&flash_secret_data.private_key[4], word);

  word = ((uint32_t)secrets->secret[12]<<24) |
         ((uint32_t)secrets->secret[13]<<16) |
         ((uint32_t)secrets->secret[14]<<8)  |
          (uint32_t)secrets->secret[15];
  nrf_nvmc_write_word((uint32_t)&flash_secret_data.private_key[0], word);

  /* write installer ki and trackable ki switches */
  word = ((uint32_t)secrets->installer_ki) |
         ((uint32_t)secrets->tracked_ki<<8);
  nrf_nvmc_write_word((uint32_t)&flash_secret_data.private_key[16], word);

  /* write read back protection bit */
  nrf_nvmc_write_word((uint32_t)&NRF_UICR->RBPCONF,0);

  /* Reboot */
  NVIC_SystemReset();
}

/*
 * We received a random, generate a challenge, and send it.
 */
void kiwiki_receive_random(ki_state_t * state, volatile radio_packet_t * packet)
{
  /* Set the state to 'processing random' */
  kiwiki_set_state(state, KI_STATE_PROCESSING_RANDOM);

  random_packet_t sensor_rand_pckt;
  memcpy(&sensor_rand_pckt, (void *)packet->payload, sizeof(random_packet_t));

  _debug_printf("Received random for sensor id 0x%02x%02x%02x%02x",
      sensor_rand_pckt.sensor_id[0],
      sensor_rand_pckt.sensor_id[1],
      sensor_rand_pckt.sensor_id[2],
      sensor_rand_pckt.sensor_id[3]);

  /* Maybe calculate a new combikey */
  kiwiki_calculate_combikey(state, &sensor_rand_pckt);

  /* Generate a ki->sensor challenge packet */
  challenge_packet_t challenge;
  kiwiki_calculate_challenge(state, &sensor_rand_pckt, &challenge);

  radio_packet_t challenge_packet;
  memcpy(challenge_packet.payload, &challenge, sizeof(challenge_packet_t));
  challenge_packet.payloadLength = sizeof(challenge_packet_t);

  /*
   * Send ki->sensor challenge packet
   */
  if (!state->is_tracked_ki)
  {
    if (state->double_tap_challenges == SEND_NORMAL_CHALLENGES)
    {
      radio_send_packet(&challenge_packet, "CHAL", SEND_COUNT_CHALLENGE, SEND_SPACING_CHALLENGE);
    }
    else /* Send double tap challenges */
    {
      state->double_tap_flipflop = !state->double_tap_flipflop;
      if (state->double_tap_flipflop)
      {
        radio_send_packet(&challenge_packet, "DHAL", SEND_COUNT_CHALLENGE, SEND_SPACING_CHALLENGE);
      }
      else
      {
        radio_send_packet(&challenge_packet, "CHAL", SEND_COUNT_CHALLENGE, SEND_SPACING_CHALLENGE);
      }
    }
  }
  else
  {
    /* Tracked Ki go to a different pipe altogether */
    radio_send_packet(&challenge_packet, "KHAL", SEND_COUNT_CHALLENGE, SEND_SPACING_CHALLENGE);
  }

  /*
   * Now is a pretty good time to re-generate the random number since
   * we're not in any timing-dependent part anymore.
   */
  crypto_gen_random_bytes(state->ki_random, SIZE_RANDOM);
}

void kiwiki_calculate_combikey(ki_state_t * state, random_packet_t * response)
{
  uint8_t hash[AES_BLOCK_SIZE] = {0};
  uint8_t * p = hash;

  /* If we already calculated the combikey for this sensor in the past,
   * don't do it again */
  if(!memcmp(state->challenge.sensor_id, response->sensor_id, SIZE_SENSOR_ID))
  {
    _debug_printf("Combikey already calculated for sensor 0x%02x%02x%02x%02x",
      response->sensor_id[0],
      response->sensor_id[1],
      response->sensor_id[2],
      response->sensor_id[3]);
    return;
  }

  _debug_printf("Calculating Combikey for sensor 0x%02x%02x%02x%02x",
      response->sensor_id[0],
      response->sensor_id[1],
      response->sensor_id[2],
      response->sensor_id[3]);

  /*
   * Add the Ki ID and the Sensor ID in the specified order
   *
   * If we're an installer Ki, then it's all 1s
   */
  if (state->is_installer_ki)
  {
    *p++ = 0xFF;
    *p++ = 0xFF;
    *p++ = 0xFF;
    *p++ = 0xFF;
    *p++ = 0xFF;
    *p++ = 0xFF;
    *p++ = 0xFF;
    *p++ = 0xFF;
  }
  else
  {
    *p++ = state->ki->key_id[3];
    *p++ = state->ki->key_id[2];
    *p++ = state->ki->key_id[1];
    *p++ = state->ki->key_id[0];

    *p++ = response->sensor_id[0];
    *p++ = response->sensor_id[1];
    *p++ = response->sensor_id[2];
    *p++ = response->sensor_id[3];
  }

  /* AES the two IDs with Ki secret key */
  aes_state_t combikey;
  crypto_aes_encrypt(state->ki->private_key, hash, &combikey);

  /* Copy the challenge key to the state */
  memcpy(state->challenge.challenge_key, combikey.out, AES_BLOCK_SIZE);

  /* Update the cached door ID */
  memcpy(state->challenge.sensor_id, response->sensor_id, SIZE_SENSOR_ID);

}

void kiwiki_calculate_challenge(ki_state_t * state, random_packet_t * sensor_rand_pckt, challenge_packet_t * ki_challenge)
{
  uint8_t * p;

  /* Clear the response struct */
  memset(ki_challenge, 0, sizeof(challenge_packet_t));

  p = ki_challenge->challenge;

  /* Add the Ki Random */
  *p++ = state->ki_random[7];
  *p++ = state->ki_random[6];
  *p++ = state->ki_random[5];
  *p++ = state->ki_random[4];
  *p++ = state->ki_random[3];
  *p++ = state->ki_random[2];
  *p++ = state->ki_random[1];
  *p++ = state->ki_random[0];

  /* Add the Sensor random */
  *p++ = sensor_rand_pckt->random[7];
  *p++ = sensor_rand_pckt->random[6];
  *p++ = sensor_rand_pckt->random[5];
  *p++ = sensor_rand_pckt->random[4];
  *p++ = sensor_rand_pckt->random[3];
  *p++ = sensor_rand_pckt->random[2];
  *p++ = sensor_rand_pckt->random[1];
  *p++ = sensor_rand_pckt->random[0];

  /* Encrypt the random numbers with the challenge key */
  aes_state_t challenge;
  crypto_aes_encrypt(state->challenge.challenge_key, ki_challenge->challenge, &challenge);

  /* Copy the encrypted random numbers back to our challenge buffer */
  memcpy(ki_challenge->challenge, challenge.out, sizeof(ki_challenge->challenge));

  /* Set the intended target */
  memcpy(ki_challenge->sensor_id, state->challenge.sensor_id, sizeof(ki_challenge->sensor_id));

}
