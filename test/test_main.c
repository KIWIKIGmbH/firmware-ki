#include "test.h"
#include "radio.h"
#include "kiwiki.h"
#include "hw.h"
#include "spi_master.h"
#include "lis2dh_driver.h"
#include "nrf51.h"
#include "debug.h"
#include <unistd.h>
#include <sys/mman.h>
#include <stdio.h>
#include <sys/time.h>
#include <sys/fcntl.h>
#include <pthread.h>

struct kiwi_test_shm {
  bool autosend;
  bool send_beacon;
  bool send_rand;
  NRF_RADIO_Type fake_radio_memory;
  bool should_quit;
  bool accl_double_tap_int;
  bool accl_movement_int;
};

struct kiwi_test_shm *sptr;
pthread_t listen_thread;
pthread_t send_thread;
pthread_t beacon_thread;
pthread_t rand_thread;
pthread_t shutdown_thread;
pthread_t accelerometer_thread;

int fd;
bool sending = false;
bool rx_en = false;
bool tx_en = false;
volatile bool has_packet;
radio_packet_t packet;
ki_state_t * mState;
bool steady_state_test = false;
extern NRF_RADIO_Type *RadioPtr;


#define MOVEMENT_INTERVAL 45
#define DOUBLETAP_INTERVAL 6
void *gen_fake_accl_ints(void * a)
{
  int movement_timer = 0;
  int doubletap_timer = 0;
  for(;;)
  {
    if(movement_timer-- <= 0)
    {
      movement_timer = MOVEMENT_INTERVAL;
      sptr->accl_movement_int = true;
      usleep(2000);
      sptr->accl_movement_int = false;
    }
    if(doubletap_timer-- <= 0)
    {
      doubletap_timer = DOUBLETAP_INTERVAL;
      sptr->accl_double_tap_int = true;
      usleep(2000);
      sptr->accl_double_tap_int = false;
    }
    sleep(1);
  }
  pthread_exit(NULL);
}

void *sendrand(void * a)
{
  for(;;)
  {
    if(!has_packet && sptr->send_rand)
    {
      has_packet = true;
      /* copy something into packet */
      packet.pipe = 1;
      packet.rssi = 0;

      random_packet_t rand = {
        .random = {0},
        .sensor_id = { 0x01, 0x02, 0x03, 0x04 }
      };
      packet.payloadLength = sizeof(random_packet_t);

      memcpy(packet.payload, &rand, sizeof(random_packet_t));

    }
    usleep(10);
  }
  pthread_exit(NULL);
}


void *beacon(void * a)
{
  for(;;)
  {
    if(!has_packet && sptr->send_beacon)
    {
      has_packet = true;
      /* copy something into packet */
      packet.pipe = 0;
      packet.rssi = 0;
      packet.payloadLength = 4;

      uint8_t door_id[4] = { 0x01, 0x02, 0x03, 0x04 };
      memcpy(packet.payload, door_id, 4);

    }
    usleep(10);
  }
  pthread_exit(NULL);
}

void *shutdown(void * a)
{
  volatile NRF_RADIO_Type *rptr = &(sptr->fake_radio_memory);
  for(;;)
  {
    /* Spin up the receiver */
    if (rptr->TASKS_RXEN && rptr->EVENTS_READY==0 && !rx_en)
    {
      rptr->TASKS_DISABLE = 0U;
      _debug_printf("RX: TURNING ON XCVR%s", "");
      usleep(130);
      _debug_printf("RX: XCVR READY%s", "");
      rptr->EVENTS_READY = 1;
      rx_en = true;
    }

    /* Spin up the transmitter */
    if (rptr->TASKS_TXEN && rptr->EVENTS_READY==0 && !tx_en)
    {
      rptr->TASKS_DISABLE = 0U;
      _debug_printf("TX: TURNING ON XCVR%s", "");
      usleep(130);
      _debug_printf("TX: XCVR READY%s", "");
      rptr->EVENTS_READY = 1;
      tx_en = true;
    }

/*    if (rptr->TASKS_DISABLE && rx_en)
    {
      _debug_printf("RX: TURNING XCVR OFF%s", "");
      rptr->TASKS_RXEN = 0;
      rptr->TASKS_START = 0;
      rx_en = false;
    }
    else if (rptr->TASKS_DISABLE && tx_en)
    {
      _debug_printf("TX: TURNING XCVR OFF%s", "");
      rptr->TASKS_TXEN = 0;
      rptr->TASKS_START = 0;
      tx_en = false;
    }
    else
    */
    if (rptr->TASKS_DISABLE)
    {
//      _debug_printf("XX: DISABLING RADIO%s", "");
      rptr->TASKS_TXEN = 0;
      rptr->TASKS_RXEN = 0;
      rptr->TASKS_START = 0;
      rx_en = false;
      tx_en = false;
      rptr->TASKS_DISABLE = 0;
    }
  }
}

void *listen(void * a)
{
  volatile NRF_RADIO_Type *rptr = &(sptr->fake_radio_memory);
  for(;;)
  {
    if(rptr->TASKS_START && rx_en)
    {
        if(has_packet)
        {
          /* copy our fake packet to the shared memory */
          memcpy((void *)rptr->PACKETPTR, &packet, sizeof(radio_packet_t));
          has_packet = false;

          /* Mark the packet as recieved */
          rptr->EVENTS_END = 1;
         }
    }
    usleep(1);
  }
  pthread_exit(NULL);
}

void *send(void * a)
{
  volatile NRF_RADIO_Type *rptr = &(sptr->fake_radio_memory);
  for(;;)
  {
    if (rptr->TASKS_START && tx_en)
    {
      sending = true;

      /* Switch between sending beacons and randoms */
      if(rptr->PREFIX0 == (uint32_t)radio_convert_byte('R'))
      {
        _debug_printf("TX: SENDING RAND FROM KI%s", "");
        if(sptr->autosend)
        {
          sptr->send_beacon = false;
          sptr->send_rand = true;
        }
      }
      else if(rptr->PREFIX0 == (uint32_t)radio_convert_byte('K'))
      {
        if(!mState->is_installer_ki)
        {
          /* We should not send tracked chals if we're not installer */
          _debug_break();
        }
        if(sptr->autosend)
        {
          sptr->send_beacon = true;
          sptr->send_rand = false;
        }

        _debug_printf("TX: SENDING TRACKED CHAL FROM KI%s", "");
      }
      else if(rptr->PREFIX0 == (uint32_t)radio_convert_byte('C'))
      {
        if(mState->is_installer_ki)
        {
          /* We should not send untracked chals if we're installer */
          _debug_break();
        }
        if(sptr->autosend)
        {
          sptr->send_beacon = true;
          sptr->send_rand = false;
        }
        _debug_printf("TX: SENDING CHAL FROM KI%s", "");
      }
      else
      {
        _debug_printf("TX: Sending to odd/unknown address?%s", "");
        _debug_break();
      }
      /* now fakepacket exists. we can test it for something */

      /* wait how long? */
      usleep(150);

      /* Mark the packet as sent */
      rptr->EVENTS_END = 1;
      rptr->TASKS_START = 0;
      sending = false;
      _debug_printf("TX: PACKET SENT%s", "");
    }
    usleep(1);
  }
  pthread_exit(NULL);
}


static void handle_command_line(
  int argc, char *argv[], char **junit_xml_output_filepath)
{
  for (int i = 1; i < argc; ++i)
  {
    if (argv[i][0] == '-')
    {
      switch (argv[i][1])
      {
        case 'v':
          test_verbose = true;
          break;
        case 'f':
          if (argc > i + 1)
          {
            *junit_xml_output_filepath = argv[i + 1];
            with_color = false;
            ++i; /* Consume the file name token. */
          }
        case 's':
          steady_state_test = true;
          break;
      }
    }
  }
}

/* If you provide "-v" on the command line, the test output will be more
 * verbose. If you provide a "-f" followed by a file name on the command line,
 * the test runner will output JUint-style XML to that file. */
int main(int argc, char *argv[])
{
  char *junit_xml_output_filepath = NULL;
  handle_command_line(argc, argv, &junit_xml_output_filepath);

  /* This should more or less mirror the firmware main,
   * with the addition of timing information and the SHM */
  if (steady_state_test)
  {
    /* shm-map our faked out memory so other applications can
     * interact with us */
    fd = shm_open("kiwiki_test_shm", O_CREAT | O_RDWR, S_IRUSR | S_IWUSR);
    if (fd == -1) return -1;
    if (ftruncate(fd, sizeof(struct kiwi_test_shm)) == -1) return -1;
    sptr = mmap(NULL, sizeof(struct kiwi_test_shm), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (sptr == MAP_FAILED) return -1;
    sptr->should_quit = false;

    /* Spin up the autosender */
    if(sptr->autosend)
    {
      sptr->send_beacon = true;
      sptr->send_rand = false;
    }

    /* Redirect the radio to the mmap'd struct */
    RadioPtr = &(sptr->fake_radio_memory);

    pthread_create(&listen_thread, NULL, listen, NULL);

    pthread_create(&send_thread, NULL, send, NULL);

    pthread_create(&beacon_thread, NULL, beacon, NULL);

    pthread_create(&rand_thread, NULL, sendrand, NULL);

    pthread_create(&shutdown_thread, NULL, shutdown, NULL);

    RadioPtr->TASKS_RXEN = 0;
    RadioPtr->TASKS_TXEN = 0;

    pthread_create(&accelerometer_thread, NULL, gen_fake_accl_ints, NULL);

    /* ------ main starts here ------- */

    /* KI state stored here */
    ki_state_t state;
    mState = &state;

    /* Set up the state macheen */
    kiwiki_setup_state(&state);

    /* Set up the chip */
    hw_init();

    /* Set up the radio */
    radio_init();

    /* Set up the accelerometer */
    LIS2DH_init();

    /* FSM */
    while (!sptr->should_quit)
    {
      kiwiki_step(&state);

      /* Print the current timestamp and the state
       * We can use this to reconstruct power measurements
       * and behavior */
      _debug_printf("State after step: %d", state.fsm_state);
    }
  }

  TEST_INIT(junit_xml_output_filepath);

  RUN_TESTS(
    crypto,
    crypto_aes_encryption,
    crypto_aes_decryption,
    crypto_generate_bytes
  );

  RUN_TESTS(
    radio,
    radio_test_convert_byte,
    radio_test_convert_bytes,
    radio_test_init
  );

  RUN_TESTS(
    kiwiki,
    kiwiki_test_step,
    kiwiki_test_setup_state,
    kiwiki_test_receive_beacon,
    kiwiki_test_receive_random,
    kiwiki_test_calculate_combikey,
    kiwiki_test_calculate_challenge,
    kiwiki_test_has_been_manufactured,
    kiwiki_test_update_doubletap_timer
  );

  TEST_FINALIZE();


}
