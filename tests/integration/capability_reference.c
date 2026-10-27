#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

static uint32_t next_value(uint32_t *state) {
    uint32_t value = *state;
    value ^= value << 13;
    value ^= value >> 17;
    value ^= value << 5;
    *state = value;
    return value;
}

static FILE *open_case(const char *directory, unsigned index, const char *suffix) {
    char path[4096];
    int length = snprintf(path, sizeof(path), "%s/case-%u.%s", directory, index, suffix);
    if (length < 0 || (size_t)length >= sizeof(path)) {
        return NULL;
    }
    return fopen(path, "wx");
}

/* The reference models only domain operations; it does not share the Fermi parser. */
static bool write_case(const char *directory, unsigned index) {
    FILE *input = open_case(directory, index, "input");
    FILE *expected = open_case(directory, index, "expected");
    if (!input || !expected) {
        if (input) {
            fclose(input);
        }
        if (expected) {
            fclose(expected);
        }
        return false;
    }
    uint32_t state = UINT32_C(0x9e3779b9) ^ (index + 1);
    uint32_t quantity[8] = {0};
    unsigned accepted = 0, rejected = 0;
    for (unsigned step = 0; step < 1024; ++step) {
        bool adding = (next_value(&state) & 1) != 0;
        uint32_t item = next_value(&state) % 12;
        uint32_t amount = next_value(&state) % 1001;
        if (step % 97 == 0) {
            amount = 1000000;
        }
        if (step % 17 == 0) {
            fputc('\n', input);
        }
        fprintf(input, "%c %03u %06u\n", adding ? '+' : '-', (unsigned)item, (unsigned)amount);
        if (item >= 8 || (adding && amount > 1000000 - quantity[item]) ||
            (!adding && amount > quantity[item])) {
            ++rejected;
        } else {
            if (adding) {
                quantity[item] += amount;
            } else {
                quantity[item] -= amount;
            }
            ++accepted;
        }
    }
    for (unsigned item = 0; item < 8; ++item) {
        fprintf(expected, "item %u %u\n", item, (unsigned)quantity[item]);
    }
    fprintf(expected, "accepted %u\nrejected %u\n", accepted, rejected);
    bool ok = !ferror(input) && !ferror(expected);
    if (fclose(input)) {
        ok = false;
    }
    if (fclose(expected)) {
        ok = false;
    }
    return ok;
}

int main(int argc, char **argv) {
    if (argc != 2) {
        fputs("usage: capability-reference DIRECTORY\n", stderr);
        return 64;
    }
    for (unsigned index = 0; index < 6; ++index) {
        if (!write_case(argv[1], index)) {
            fputs("capability-reference: cannot write fixture\n", stderr);
            return 1;
        }
    }
    return 0;
}
