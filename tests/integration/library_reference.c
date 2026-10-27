/* Independent catalog workload oracle. Circulation is covered by native tests. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct {
    unsigned active, id, total, available, lost;
} Book;
typedef struct {
    unsigned sequence, operation, first, second, reason;
} Entry;

static uint32_t random_state = UINT32_C(0x98f23b71);

static unsigned next_number(void) {
    random_state ^= random_state << 13;
    random_state ^= random_state >> 17;
    random_state ^= random_state << 5;
    return random_state;
}

static FILE *create(const char *directory, unsigned index, const char *suffix) {
    char path[4096];
    int length = snprintf(path, sizeof(path), "%s/catalog-%u.%s", directory, index, suffix);
    if (length < 0 || (size_t)length >= sizeof(path)) {
        exit(1);
    }
    FILE *file = fopen(path, "wx");
    if (file == NULL) {
        perror(path);
        exit(1);
    }
    return file;
}

int main(int argc, char **argv) {
    if (argc != 2) {
        return 2;
    }
    for (unsigned trial = 0; trial < 4; trial++) {
        Book books[32] = {{0}};
        Entry audit[64] = {{0}};
        unsigned accepted = 0, rejected = 0;
        FILE *input = create(argv[1], trial, "journal");
        FILE *expected = create(argv[1], trial, "expected");
        for (unsigned sequence = 1; sequence <= 4096; sequence++) {
            unsigned choice = next_number() % 4;
            unsigned operation = choice == 0 ? 1 : choice == 1 ? 8 : choice == 2 ? 9 : 16;
            unsigned id = next_number() % 40 + 1;
            unsigned amount = next_number() % 12;
            unsigned reason = 0;
            int found = -1, free_slot = -1;
            for (unsigned slot = 0; slot < 32; slot++) {
                if (books[slot].active && books[slot].id == id) {
                    found = (int)slot;
                }
                if (!books[slot].active && free_slot < 0) {
                    free_slot = (int)slot;
                }
            }
            if (operation == 1) {
                fprintf(input, "BOOK %u %u\n", id, amount);
                if (amount == 0) {
                    reason = 1;
                } else if (found >= 0) {
                    reason = 2;
                } else if (free_slot < 0) {
                    reason = 5;
                } else {
                    books[free_slot] = (Book){1, id, amount, amount, 0};
                }
            } else {
                if (operation == 16) {
                    fprintf(input, "RETIRE %u\n", id);
                    amount = 0;
                } else {
                    fprintf(input, "%s %u %u\n", operation == 8 ? "RESTOCK" : "REMOVE", id, amount);
                }
                if (found < 0) {
                    reason = 3;
                } else if (operation == 16) {
                    books[found].active = 0;
                } else if (amount == 0 || (operation == 8 && amount > 1000 - books[found].total)) {
                    reason = 1;
                } else if (operation == 9 && amount > books[found].available) {
                    reason = 15;
                } else if (operation == 8) {
                    books[found].total += amount;
                    books[found].available += amount;
                } else {
                    books[found].total -= amount;
                    books[found].available -= amount;
                }
            }
            accepted += reason == 0;
            rejected += reason != 0;
            audit[(sequence - 1) % 64] = (Entry){sequence, operation, id, amount, reason};
        }
        fprintf(expected, "STATE 0 4096 0 %u %u 0 0 0 0 0 0 0 0 0\n", accepted, rejected);
        for (unsigned slot = 0; slot < 32; slot++) {
            Book book = books[slot];
            if (book.active) {
                fprintf(expected, "BOOK %u %u %u %u %u\n", slot, book.id, book.total,
                        book.available, book.lost);
            }
        }
        for (unsigned sequence = 4033; sequence <= 4096; sequence++) {
            Entry entry = audit[(sequence - 1) % 64];
            fprintf(expected, "AUDIT %u 0 %u %u %u 0 %u\n", entry.sequence, entry.operation,
                    entry.first, entry.second, entry.reason);
        }
        if (fclose(input) != 0 || fclose(expected) != 0) {
            return 1;
        }
    }
    return 0;
}
