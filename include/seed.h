#ifndef MAP_H
#define MAP_H

#include <stdio.h>
#define SEED_CONST 345678910

int rng(int max, int min);

int gerar_seed();

int* gerar_1000_seeds();
#endif