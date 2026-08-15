#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "../include/seed.h"

int rng(int max, int min){
    srand(time(NULL));
    return (rand() % (max - min + 1)) + min;
}

int gerar_seed(){
    return rng(999999999, 100000000);
}
