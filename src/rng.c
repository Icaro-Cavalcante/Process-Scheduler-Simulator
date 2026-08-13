#include "../include/rng.h"
#include <math.h>

/* ---- SplitMix64, used only to expand a single 64-bit seed into the four
 * 64-bit words xoshiro256** needs as initial state. ---- */
static uint64_t splitmix64_next(uint64_t *state) {
    uint64_t z = (*state += 0x9E3779B97F4A7C15ULL);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
    return z ^ (z >> 31);
}

static uint64_t rotl(const uint64_t x, int k) {
    return (x << k) | (x >> (64 - k));
}

void rng_seed(Rng *rng, uint64_t seed) {
    uint64_t sm_state = seed;
    for (int i = 0; i < 4; i++) {
        rng->s[i] = splitmix64_next(&sm_state);
    }
}

uint64_t rng_next_u64(Rng *rng) {
    uint64_t *s = rng->s;
    const uint64_t result = rotl(s[1] * 5, 7) * 9;
    const uint64_t t = s[1] << 17;

    s[2] ^= s[0];
    s[3] ^= s[1];
    s[1] ^= s[2];
    s[0] ^= s[3];
    s[2] ^= t;
    s[3] = rotl(s[3], 45);

    return result;
}

double rng_next_double(Rng *rng) {
    /* Take the top 53 bits for a uniform double in [0, 1) with full mantissa precision. */
    const uint64_t v = rng_next_u64(rng) >> 11;
    return (double)v * (1.0 / 9007199254740992.0); /* 2^53 */
}

int rng_uniform_int(Rng *rng, int min, int max) {
    if (min > max) { int tmp = min; min = max; max = tmp; }
    uint64_t range = (uint64_t)(max - min) + 1;
    return min + (int)(rng_next_u64(rng) % range);
}

double rng_uniform_double(Rng *rng, double min, double max) {
    return min + rng_next_double(rng) * (max - min);
}

double rng_exponential(Rng *rng, double mean) {
    /* Inverse-CDF sampling: X = -mean * ln(U), U ~ Uniform(0,1).
     * Guard against U = 0 (log undefined) by resampling — astronomically
     * rare with a 53-bit mantissa but cheap to guard against. */
    double u;
    do {
        u = rng_next_double(rng);
    } while (u <= 0.0);
    return -mean * log(u);
}

bool rng_bernoulli(Rng *rng, double p) {
    return rng_next_double(rng) < p;
}
