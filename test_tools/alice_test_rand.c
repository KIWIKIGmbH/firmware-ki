#include "radio.h"
#include "nrf51.h"
#include <unistd.h>
#include <sys/mman.h>
#include <stdio.h>
#include <sys/fcntl.h>
#include <stdbool.h>

struct kiwi_test_shm {
  bool autosend;
  bool send_beacon;
  bool send_rand;
  NRF_RADIO_Type fake_radio_memory;
  bool should_quit;
};

struct kiwi_test_shm *sptr;
int fd;

int main(int argc, char *argv[])
{
    fd = shm_open("kiwiki_test_shm", O_CREAT | O_RDWR, S_IRUSR | S_IWUSR);
    if (fd == -1) return -1;
    if (ftruncate(fd, sizeof(struct kiwi_test_shm)) == -1) return -1;
    sptr = mmap(NULL, sizeof(struct kiwi_test_shm), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (sptr == MAP_FAILED) return -1;
    sptr->autosend = false;
    sptr->send_beacon = false;
    sptr->send_rand = true;
}
