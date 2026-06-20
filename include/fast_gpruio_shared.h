#ifndef FAST_GPRUIO_SHARED_H
#define FAST_GPRUIO_SHARED_H

#include <stdint.h>

#define FAST_GPRUIO_MAGIC              0x4750494Fu /* 'GPIO' */
#define FAST_GPRUIO_VERSION            1u
#define FAST_GPRUIO_PRU_HZ             200000000u
#define FAST_GPRUIO_CYCLES_PER_US      200u

#define FAST_GPRUIO_MAX_PINS           16u
#define FAST_GPRUIO_CMD_RING_SIZE      16u

#define FAST_GPRUIO_MAILBOX_PHYS       0x4A303000u
#define FAST_GPRUIO_MAILBOX_MAP_SIZE   0x1000u

#define FAST_GPRUIO_DEFAULT_FW_NAME    "fast_gpruio_pru1_fw.out"

#define FAST_GPRUIO_STATUS_OK          0u
#define FAST_GPRUIO_STATUS_BAD_PIN     1u
#define FAST_GPRUIO_STATUS_PIN_BUSY    2u
#define FAST_GPRUIO_STATUS_BAD_DURATION 3u
#define FAST_GPRUIO_STATUS_QUEUE_FULL  4u

#define FAST_GPRUIO_TIMER_HW_CYCLE     1u
#define FAST_GPRUIO_TIMER_SOFT_CYCLE   2u
#define FAST_GPRUIO_TIMER_HW_DELTA     3u
#define FAST_GPRUIO_TIMER_IEP          4u

#define FAST_GPRUIO_GPIO0_BASE         0x44E07000u
#define FAST_GPRUIO_GPIO1_BASE         0x4804C000u
#define FAST_GPRUIO_GPIO2_BASE         0x481AC000u
#define FAST_GPRUIO_GPIO3_BASE         0x481AE000u

struct fast_gpruio_pin_slot {
    volatile uint32_t enabled;
    volatile uint32_t gpio_base;
    volatile uint32_t gpio_mask;
    volatile uint32_t idle_level;
    volatile uint32_t busy;
    volatile uint32_t deadline_cycle;
    volatile uint32_t last_seq;
    volatile uint32_t pulse_count;
    volatile uint32_t busy_reject_count;
    volatile uint32_t reserved[7];
};

struct fast_gpruio_cmd_slot {
    volatile uint32_t command_seq;
    volatile uint32_t pin_id;
    volatile uint32_t duration_cycles;
    volatile uint32_t active_level;
    volatile uint32_t result_seq;
    volatile uint32_t result_status;
    volatile uint32_t reserved[2];
};

struct fast_gpruio_mailbox {
    volatile uint32_t magic;
    volatile uint32_t version;
    volatile uint32_t struct_size;
    volatile uint32_t pru_hz;

    volatile uint32_t cmd_write_idx;
    volatile uint32_t cmd_read_idx;
    volatile uint32_t next_seq_seen;
    volatile uint32_t last_error_status;

    volatile uint32_t loop_counter;
    volatile uint32_t last_cycle;
    volatile uint32_t accepted_count;
    volatile uint32_t rejected_count;
    volatile uint32_t expired_count;
    volatile uint32_t queue_full_count;
    volatile uint32_t timer_mode;
    volatile uint32_t timer_probe_delta;
    volatile uint32_t timer_reset_count;
    volatile uint32_t timer_last_hw_cycle;

    struct fast_gpruio_pin_slot pins[FAST_GPRUIO_MAX_PINS];
    struct fast_gpruio_cmd_slot cmds[FAST_GPRUIO_CMD_RING_SIZE];
};

#endif
