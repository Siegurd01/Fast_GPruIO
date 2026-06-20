#include "fast_gpruio_lib.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static void usage(const char *argv0)
{
    fprintf(stderr,
            "Usage:\n"
            "  %s pulse PIN PULSE_US [--idle 0|1] [--active 0|1] [--firmware PATH]\n"
            "  %s stat\n"
            "  %s calc BYTES BAUD [GUARD_US] [BITS_PER_BYTE]\n"
            "\n"
            "Examples:\n"
            "  %s pulse P2.17 800\n"
            "  %s pulse P1.30 800\n"
            "  %s pulse GPIO65 800\n"
            "  %s pulse GPIO2.1 800\n"
            "  %s calc 8 115200 25 10\n",
            argv0, argv0, argv0, argv0, argv0, argv0, argv0, argv0);
}

static int parse_u32(const char *s, uint32_t *out)
{
    char *end = NULL;
    errno = 0;
    unsigned long v = strtoul(s, &end, 0);
    if (errno != 0 || end == s || *end != '\0' || v > 0xffffffffUL) {
        return -1;
    }
    *out = (uint32_t)v;
    return 0;
}

int main(int argc, char **argv)
{
    if (argc < 2) {
        usage(argv[0]);
        return 2;
    }

    if (strcmp(argv[1], "calc") == 0) {
        if (argc < 4) {
            usage(argv[0]);
            return 2;
        }
        uint32_t bytes = 0;
        uint32_t baud = 0;
        uint32_t guard = 0;
        uint32_t bits = 10;
        if (parse_u32(argv[2], &bytes) != 0 || parse_u32(argv[3], &baud) != 0) {
            usage(argv[0]);
            return 2;
        }
        if (argc > 4 && parse_u32(argv[4], &guard) != 0) {
            usage(argv[0]);
            return 2;
        }
        if (argc > 5 && parse_u32(argv[5], &bits) != 0) {
            usage(argv[0]);
            return 2;
        }
        printf("%u\n", fast_gpruio_calc_pulse_length_us(bytes, bits, baud, guard));
        return 0;
    }

    if (strcmp(argv[1], "stat") == 0) {
        struct fast_gpruio_ctx ctx;
        if (fast_gpruio_init(&ctx, NULL, 2000) != 0) {
            return 1;
        }

        volatile struct fast_gpruio_mailbox *mb = ctx.mb;
        printf("magic=0x%08X version=%u struct_size=%u pru_hz=%u\n",
               mb->magic, mb->version, mb->struct_size, mb->pru_hz);
        printf("timer_mode=%u probe_delta=%u timer_count=%u loop=%u raw_count=%u timer_resets=%u\n",
               mb->timer_mode, mb->timer_probe_delta, mb->last_cycle, mb->loop_counter,
               mb->timer_last_hw_cycle, mb->timer_reset_count);
        printf("cmd write=%u read=%u accepted=%u rejected=%u expired=%u queue_full=%u last_err=%u\n",
               mb->cmd_write_idx, mb->cmd_read_idx, mb->accepted_count,
               mb->rejected_count, mb->expired_count, mb->queue_full_count,
               mb->last_error_status);
        for (uint32_t p = 0; p < FAST_GPRUIO_MAX_PINS; ++p) {
            if (mb->pins[p].enabled != 0u || mb->pins[p].busy != 0u ||
                mb->pins[p].pulse_count != 0u || mb->pins[p].busy_reject_count != 0u) {
                printf("pin%u enabled=%u busy=%u base=0x%08X mask=0x%08X idle=%u deadline=%u last_seq=%u pulses=%u busy_reject=%u\n",
                       p, mb->pins[p].enabled, mb->pins[p].busy,
                       mb->pins[p].gpio_base, mb->pins[p].gpio_mask,
                       mb->pins[p].idle_level, mb->pins[p].deadline_cycle,
                       mb->pins[p].last_seq, mb->pins[p].pulse_count,
                       mb->pins[p].busy_reject_count);
            }
        }
        fast_gpruio_close(&ctx);
        return 0;
    }

    if (strcmp(argv[1], "pulse") != 0 || argc < 4) {
        usage(argv[0]);
        return 2;
    }

    const char *pin_name = argv[2];
    uint32_t pulse_us = 0;
    uint32_t idle = 0;
    uint32_t active = 1;
    const char *firmware = NULL;

    if (parse_u32(argv[3], &pulse_us) != 0) {
        usage(argv[0]);
        return 2;
    }

    int i;
    for (i = 4; i < argc; ++i) {
        if (strcmp(argv[i], "--idle") == 0 && i + 1 < argc) {
            if (parse_u32(argv[++i], &idle) != 0) {
                usage(argv[0]);
                return 2;
            }
            idle = (idle != 0u) ? 1u : 0u;
        } else if (strcmp(argv[i], "--active") == 0 && i + 1 < argc) {
            if (parse_u32(argv[++i], &active) != 0) {
                usage(argv[0]);
                return 2;
            }
            active = (active != 0u) ? 1u : 0u;
        } else if (strcmp(argv[i], "--firmware") == 0 && i + 1 < argc) {
            firmware = argv[++i];
        } else {
            usage(argv[0]);
            return 2;
        }
    }

    struct fast_gpruio_ctx ctx;
    if (fast_gpruio_init(&ctx, firmware, 2000) != 0) {
        return 1;
    }

    uint32_t pin_id = 0;
    if (fast_gpruio_register_pin(&ctx, pin_name, idle, &pin_id) != 0) {
        fast_gpruio_close(&ctx);
        return 1;
    }

    uint32_t seq = 0;
    int rc = fast_gpruio_start_pulse(&ctx, pin_id, pulse_us, active, &seq);
    if (rc != FAST_GPRUIO_STATUS_OK) {
        fprintf(stderr, "start pulse failed: %s (%d)\n",
                fast_gpruio_status_string((uint32_t)rc), rc);
        fast_gpruio_close(&ctx);
        return 1;
    }

    printf("pulse started: pin=%s pin_id=%u seq=%u pulse_us=%u idle=%u active=%u\n",
           pin_name, pin_id, seq, pulse_us, idle, active);
    fast_gpruio_close(&ctx);
    return 0;
}
