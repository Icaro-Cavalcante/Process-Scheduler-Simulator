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

int* gerar_1000_seeds(){
    int arr[1000];
    
    for (int i = 0; i < 1000; i++) {
        arr[i] = gerar_seed();
    }

    return arr;
}