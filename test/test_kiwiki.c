#include "test.h"
#include "kiwiki.h"
#include "hw.h"
#include "radio.h"
#include "nrf51.h"

ki_secrets_t secrets_default =
{
  .key_id = { [0 ... SIZE_KI_ID - 1] = 0xff },
  .private_key = { [0 ... AES_BLOCK_SIZE-1] = 0xff }
};

ki_state_t state_default =
{
  .ki = (ki_secrets_t *)&secrets_default,
  .challenge = {{0},{0}},
  .sensor_id = {0},
  .ki_random = {0},
  .fsm_state = KI_STATE_SLEEP,
  .door_prox_state = MAYBE_IN_FRONT_OF_DOOR,
  .is_installer_ki = 0xff,
  .is_tracked_ki = 0xff,
  .seen_stopwatch = 0,
  .packet_stat = 0,
  .motionless_time = MOTIONLESS_TIME,
  .has_been_manufactured = false,
  .is_hw_good = true,
};

uint8_t fake_radio_memory[sizeof(NRF_RADIO_Type)];
extern NRF_RADIO_Type *RadioPtr;

/* RTC, RADIO, and Clock switching need to be mocked */

TEST(kiwiki_test_step, 0, 0)
{
  /* Test each state: */
  /* Listen and get nothing */
  /* Listen and get rand */
  /* Listen and get beacon */
  /* Listen rand and get nothing */
  /* Listen rand and get beacon */
  /* Listen rand and get rand for wrong sensor */
  /* Listen rand and get rand */
  /* Sleep with recently seen low */
  /* Sleep with recently seen medium */
  /* Sleep with recently seen high */
}

TEST(kiwiki_test_setup_state, 0, 0)
{
  /* Make sure that the default state is sane */
  ki_state_t state;
  kiwiki_setup_state(&state);

  TEST_MEM_EQ(&state.challenge, &state_default.challenge, sizeof(state.challenge));
  TEST_MEM_EQ(&state.sensor_id, &state_default.sensor_id, sizeof(state.sensor_id));
  TEST_MEM_EQ(&state.ki_random, &state_default.ki_random, sizeof(state.ki_random));
  TEST_EQ(state.fsm_state, state_default.fsm_state);
  TEST_EQ(state.door_prox_state, state_default.door_prox_state);
  TEST_EQ(state.is_installer_ki, state_default.is_installer_ki);
  TEST_EQ(state.is_tracked_ki, state_default.is_tracked_ki);
  TEST_EQ(state.seen_stopwatch, state_default.seen_stopwatch);
  TEST_EQ(state.packet_stat, state_default.packet_stat);
  TEST_EQ(state.motionless_time, state_default.motionless_time);
  TEST_EQ(state.has_been_manufactured, state_default.has_been_manufactured);
  TEST_EQ(state.is_hw_good, state_default.is_hw_good);
}

TEST(kiwiki_test_receive_beacon, 0, 0)
{
  /* Make the Radio memory use fake memory instead */
  RadioPtr = (NRF_RADIO_Type *)fake_radio_memory;

  /* Get a valid beacon, ensure response is correct and state updated */
  ki_state_t state;
  kiwiki_setup_state(&state);
  radio_packet_t packet;
  packet.payload[0] = 0x12;
  packet.payload[1] = 0x34;
  packet.payload[2] = 0x56;
  packet.payload[3] = 0x78;

  kiwiki_receive_beacon(&state, &packet);

  /* Make sure that the sensor ID is copied correclty */
  TEST_MEM_EQ(&state.sensor_id, &packet.payload, SIZE_SENSOR_ID);

  /* Make sure that a random packet is generated */
  TEST_MEM_EQ(RadioPtr->PACKETPTR, &state.ki_random, SIZE_RANDOM);
  TEST_MEM_EQ(RadioPtr->PACKETPTR + SIZE_RANDOM, &packet.payload, SIZE_SENSOR_ID);

  /* Make sure that the random packet is sent to the correct pipe */
  TEST_EQ(RadioPtr->PREFIX0, radio_convert_byte('R'));

  /* Make sure that the state is set correctly */
  TEST_EQ(state.fsm_state, KI_STATE_LISTEN_RAND);

}

TEST(kiwiki_test_receive_random, 0, 0)
{
  /* Make the Radio memory use fake memory instead */
  RadioPtr = (NRF_RADIO_Type *)fake_radio_memory;

  /* Get a valid random, ensure challenge is correct and state updated */
  ki_state_t state;
  kiwiki_setup_state(&state);
  radio_packet_t packet;
  packet.payload[0]  = 0x12;
  packet.payload[1]  = 0x34;
  packet.payload[2]  = 0x56;
  packet.payload[3]  = 0x78;
  packet.payload[4]  = 0x12;
  packet.payload[5]  = 0x34;
  packet.payload[6]  = 0x56;
  packet.payload[7]  = 0x78; // -- random
  packet.payload[8]  = 0x12;
  packet.payload[9]  = 0x34;
  packet.payload[10] = 0x56;
  packet.payload[11] = 0x78; // -- sensor id

  /* Not tracked, please */
  state.is_tracked_ki = 0;

  kiwiki_receive_random(&state, &packet);

  /* Make sure that a challenge packet is generated */
  TEST_MEM_EQ(RadioPtr->PACKETPTR + SIZE_CHALLENGE, &packet.payload + SIZE_RANDOM, SIZE_SENSOR_ID);

  /* Make sure that the challenge packet is sent to the correct pipe */
  TEST_EQ(RadioPtr->PREFIX0, radio_convert_byte('C'));

  /* Test Tracked ki */
  state.is_tracked_ki = 1;
  kiwiki_receive_random(&state, &packet);

  /* Make sure that the challenge packet is sent to the correct pipe */
  TEST_EQ(RadioPtr->PREFIX0, radio_convert_byte('K'));
  state.is_tracked_ki = 0;

  /* Test Double-tap */
  state.double_tap_challenges = SEND_DOUBLE_TAP_CHALLENGES;

  kiwiki_receive_random(&state, &packet);
  TEST_EQ(RadioPtr->PREFIX0, radio_convert_byte('D'));

}

TEST(kiwiki_test_calculate_combikey, 0, 0)
{
  /* Make sure the combikey is correct in the general case */
  /* Make sure the combikey is correct in the installer case */
}

TEST(kiwiki_test_calculate_challenge, 0, 0)
{
  /* Make sure the challenge is correct */
}

TEST(kiwiki_test_has_been_manufactured, 0, 0)
{
  /* Given a state, determine if a Ki has been manufactured. */
  ki_state_t state;
  kiwiki_setup_state(&state);

  /* It should not be, by default */
  TEST_EQ(state.has_been_manufactured, false);

  /* If the Ki ID is different from 0xff, then it should be */
  state.ki->key_id[0] = 0x0;
  TEST_EQ(has_been_manufactured(&state), true);

  /* and back... */
  state.ki->key_id[0] = 0xFF;
  TEST_EQ(has_been_manufactured(&state), false);

  /* If the private key is different from 0xff, then it should be */
  state.ki->private_key[0] = 0x0;
  TEST_EQ(has_been_manufactured(&state), true);

}

TEST(kiwiki_test_update_doubletap_timer, 0, 0)
{
  ki_state_t state;
  kiwiki_setup_state(&state);

  /* Test that with no double tap, nothing happens */
  double_tap_pin_status = PIN_DEFAULT;
  kiwiki_update_doubletap_timer(&state, 0);
  TEST_EQ(state.double_tap_challenges, SEND_NORMAL_CHALLENGES);

  /* Test what with a double tap, it changes the challenges */
  double_tap_pin_status = PIN_DETECTED;
  kiwiki_update_doubletap_timer(&state, 0);
  TEST_EQ(state.double_tap_challenges, SEND_DOUBLE_TAP_CHALLENGES);

  /* Make sure it goes back to normal */
  double_tap_pin_status = PIN_DEFAULT;
  kiwiki_update_doubletap_timer(&state, DOUBLE_TAP_TIME);
  TEST_EQ(state.double_tap_challenges, SEND_DOUBLE_TAP_CHALLENGES);
  kiwiki_update_doubletap_timer(&state, 0);
  TEST_EQ(state.double_tap_challenges, SEND_NORMAL_CHALLENGES);

}
