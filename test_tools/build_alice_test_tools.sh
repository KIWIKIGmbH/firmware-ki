#!/bin/bash
gcc -g alice_test_beacon.c -o alice_test_beacon -I ../include/gcc -I ../source -I ../include -lrt -pthread 
gcc -g alice_test_rand.c -o alice_test_rand -I ../include/gcc -I ../source -I ../include -lrt -pthread 
gcc -g alice_test_stop.c -o alice_test_stop -I ../include/gcc -I ../source -I ../include -lrt -pthread 
gcc -g alice_test_autosend.c -o alice_test_autosend -I ../include/gcc -I ../source -I ../include -lrt -pthread 

