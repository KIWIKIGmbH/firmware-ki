#include "test.h"
#include "crypto.h"

const uint8_t aes_key[16] = {
  0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF,
  0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF,
};

const uint8_t aes_data[16] = {
  0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF,
  0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF,
};

/* Precalcuated for her pleasure */
const uint8_t aes_output[16] = {
  0xA1, 0xEE, 0x56, 0x08, 0xB3, 0x3a, 0xF0, 0x54,
  0x70, 0x85, 0x86, 0x08, 0xd1, 0xde, 0x08, 0x0f,
};

TEST(crypto_generate_bytes, 0, 0)
{
  const uint8_t byte_test_o[8] = { 0x0 };
  uint8_t byte_test[8] = { 0x0 };

  /* Make sure this doesn't do anything like segfault */
  crypto_gen_random_bytes(0, 5);

  /* Make sure this doesn't do anything */
  crypto_gen_random_bytes(byte_test, 0);
  TEST_MEM_EQ(byte_test, byte_test_o, 8);

  /* Make sure this generates only one byte */
  crypto_gen_random_bytes(byte_test, 1);

  /* risky, 1 in 255 chance this will false positive */
  TEST_NE(byte_test[0], 0x0);
  TEST_EQ(byte_test[1], 0x0);

  /* Make sure this generates only seven bytes */
  crypto_gen_random_bytes(byte_test, 7);
  TEST_EQ(byte_test[7], 0x0);
}

TEST(crypto_aes_encryption, 0, 0)
{
  aes_state_t state;

  memcpy(state.key, aes_key, AES_BLOCK_SIZE);
  memcpy(state.in, aes_data, AES_BLOCK_SIZE);
  memcpy(state.out, aes_data, AES_BLOCK_SIZE);

  /*
   * Test with a null key, null data, and null state
   *
   * Make sure nothing changes
   */
  crypto_aes_encrypt(0, aes_data, &state);
  TEST_MEM_EQ(state.key, aes_key, AES_BLOCK_SIZE);
  TEST_MEM_EQ(state.in, aes_data, AES_BLOCK_SIZE);
  TEST_MEM_EQ(state.out, aes_data, AES_BLOCK_SIZE);

  crypto_aes_encrypt(aes_key, 0, &state);
  TEST_MEM_EQ(state.key, aes_key, AES_BLOCK_SIZE);
  TEST_MEM_EQ(state.in, aes_data, AES_BLOCK_SIZE);
  TEST_MEM_EQ(state.out, aes_data, AES_BLOCK_SIZE);

  crypto_aes_encrypt(aes_key, aes_data, 0);
  TEST_MEM_EQ(state.key, aes_key, AES_BLOCK_SIZE);
  TEST_MEM_EQ(state.in, aes_data, AES_BLOCK_SIZE);
  TEST_MEM_EQ(state.out, aes_data, AES_BLOCK_SIZE);

  /*
   * Now, give it all three pieces, and make sure the output is as expected
   */
  crypto_aes_encrypt(aes_key, aes_data, &state);
  TEST_MEM_EQ(state.key, aes_key, AES_BLOCK_SIZE);
  TEST_MEM_EQ(state.in, aes_data, AES_BLOCK_SIZE);
  TEST_MEM_EQ(state.out, aes_output, AES_BLOCK_SIZE);

}

TEST(crypto_aes_decryption, 0, 0)
{
  aes_state_t state;

  memcpy(state.key, aes_key, AES_BLOCK_SIZE);
  memcpy(state.in, aes_output, AES_BLOCK_SIZE);
  memcpy(state.out, aes_output, AES_BLOCK_SIZE);

  /*
   * Test with a null key, null data, and null state
   *
   * Make sure nothing changes
   */
  crypto_aes_decrypt(0, aes_data, &state);
  TEST_MEM_EQ(state.key, aes_key, AES_BLOCK_SIZE);
  TEST_MEM_EQ(state.in, aes_output, AES_BLOCK_SIZE);
  TEST_MEM_EQ(state.out, aes_output, AES_BLOCK_SIZE);

  crypto_aes_decrypt(aes_key, 0, &state);
  TEST_MEM_EQ(state.key, aes_key, AES_BLOCK_SIZE);
  TEST_MEM_EQ(state.in, aes_output, AES_BLOCK_SIZE);
  TEST_MEM_EQ(state.out, aes_output, AES_BLOCK_SIZE);

  crypto_aes_decrypt(aes_key, aes_data, 0);
  TEST_MEM_EQ(state.key, aes_key, AES_BLOCK_SIZE);
  TEST_MEM_EQ(state.in, aes_output, AES_BLOCK_SIZE);
  TEST_MEM_EQ(state.out, aes_output, AES_BLOCK_SIZE);

  /*
   * Now, give it all three pieces, and make sure the output is as expected
   */
  crypto_aes_decrypt(aes_key, aes_output, &state);
  TEST_MEM_EQ(state.key, aes_key, AES_BLOCK_SIZE);
  TEST_MEM_EQ(state.in, aes_output, AES_BLOCK_SIZE);
  TEST_MEM_EQ(state.out, aes_data, AES_BLOCK_SIZE);

}
