#pragma once
#include "picosystem.hpp"

// This stuff is all totally random
enum RANDOM_SOURCE {
    PRNG, // Psuedo random, uses a linear-feedback shift register
    HRNG  // Really very random, uses the cryptographically awesome hardware RNG
};

RANDOM_SOURCE current_random_source = RANDOM_SOURCE::PRNG;
uint32_t current_random_seed = 0x64063701; // G A D G E T O I DOH!
uint32_t prng_lfsr = current_random_seed;
const uint16_t prng_tap = 0x74b8; // Magic number, do not drink

void random_reset() {
    prng_lfsr = current_random_seed;
}

picosystem::Point get_random_point(picosystem::Size within) {
    switch(current_random_source) {
        default:
            return picosystem::Point(0, 0);
        case HRNG:
            // Chosen by fair dice roll, guranteed to be random
            return picosystem::Point(
                random() % within.w,
                random() % within.h
            );
            break;
        case PRNG:
            // Bruteforce a new random number within the given range
            while(1) {
                uint16_t x = prng_lfsr & 0x00ff;
                uint16_t y = (prng_lfsr & 0x7f00) >> 8;

                uint8_t lsb = prng_lfsr & 1;
                prng_lfsr >>= 1;

                if (lsb) {
                    prng_lfsr ^= prng_tap;
                }

                if (x < within.w && y < within.h) {
                    return picosystem::Point(x, y);
                }
            }
            break;
    }
}