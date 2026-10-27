#include "hub.h"

#include <inttypes.h>
#include <stdio.h>

struct channel {
    const char *name;
    int64_t gain;
    int64_t release;
    int64_t trigger;
    bool alarm;
};

int main(void) {
    struct channel channels[] = {
        {"bearing", 10, 600, 800, false},
        {"coolant", 5, 300, 400, false},
        {"ambient", 1, 60, 80, false},
    };
    const int16_t batches[][6] = {
        {50, 80, 90, 70, 60, 85},
        {75, 65, 60, 55, 40, 30},
        {-10, 20, 30, 40, 50, 60},
    };
    for (size_t batch = 0; batch < 3; ++batch) {
        uint8_t packet[17] = {70, 69, 1, 6, 0};
        for (size_t i = 0; i < 6; ++i) {
            uint16_t bits = (uint16_t)batches[batch][i];
            packet[5 + i * 2] = (uint8_t)bits;
            packet[6 + i * 2] = (uint8_t)(bits >> 8);
        }
        for (size_t i = 0; i < sizeof(channels) / sizeof(channels[0]); ++i) {
            struct channel *channel = &channels[i];
            int64_t calibrated[6];
            int64_t report[9];
            fermi_process_batch_result result =
                fermi_process_batch(packet, sizeof(packet), calibrated, 6, report, 9, channel->gain,
                                    1, 0, channel->release, channel->trigger, channel->alarm);
            if (result.status != FERMI_OK) {
                fprintf(stderr, "%s: status=%u code=%u\n", channel->name, result.status,
                        result.code);
                return 1;
            }
            // Commit channel state only after a successful batch.
            channel->alarm = report[8] != 0;
            printf("batch=%zu channel=%-7s min=%" PRId64 " max=%" PRId64 " mean=%" PRId64
                   " rises=%" PRId64 " falls=%" PRId64 " alarm=%s\n",
                   batch + 1, channel->name, report[1], report[2], report[3], report[6], report[7],
                   channel->alarm ? "on" : "off");
        }
    }
    return 0;
}
