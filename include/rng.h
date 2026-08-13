#ifndef RNG_H
#define RNG_H

#include <stdint.h>
#include <stdbool.h>

/*
 * Deterministic pseudo-random generator (SplitMix64 -> xoshiro256**).
 *
 * scenarios.md Section 2.3 requires that, for a fixed seed S and scenario C,
 * the exact sequence of generated processes be 100% identical across every
 * scheduling algorithm. That guarantee only holds if the PRNG's output
 * depends *only* on the seed and the order of calls — never on system time,
 * ASLR, or a platform-specific libc. Hence a self-contained generator here
 * instead of stdlib rand()/srand().
 */

typedef struct {
    uint64_t s[4];
} Rng;

/* Seeds the generator. Any 64-bit seed value is valid; the same seed always
 * produces the same stream. */
void rng_seed(Rng *rng, uint64_t seed);

/* Raw 64-bit output. */
uint64_t rng_next_u64(Rng *rng);

/* Uniform double in [0, 1). */
double rng_next_double(Rng *rng);

/* Uniform integer in [min, max], inclusive on both ends. */
int rng_uniform_int(Rng *rng, int min, int max);

/* Uniform double in [min, max). */
double rng_uniform_double(Rng *rng, double min, double max);

/* Sample from Exponential(lambda), lambda = 1/mean. Used for inter-arrival
 * times (scenarios.md Section 2.1). */
double rng_exponential(Rng *rng, double mean);

/* True with probability p (0 <= p <= 1). Used for the 85/15 priority split
 * in Scenario 4 (scenarios.md Section 3.4). */
bool rng_bernoulli(Rng *rng, double p);

#endif
