#ifndef FAST_GPRUIO_LIB_H
#define FAST_GPRUIO_LIB_H

#include <stddef.h>
#include <stdint.h>

#include "../include/fast_gpruio_shared.h"

#ifdef __cplusplus
extern "C" {
#endif

struct fast_gpruio_ctx {
    int mem_fd;
    volatile struct fast_gpruio_mailbox *mb;
    uint32_t next_seq;
};

int fast_gpruio_init(struct fast_gpruio_ctx *ctx,
                     const char *firmware_out_path,
                     uint32_t ready_timeout_ms);

void fast_gpruio_close(struct fast_gpruio_ctx *ctx);

int fast_gpruio_load_pru1_firmware(const char *firmware_out_path,
                                   const char *firmware_name);

int fast_gpruio_open_mailbox(struct fast_gpruio_ctx *ctx);
int fast_gpruio_wait_ready(struct fast_gpruio_ctx *ctx, uint32_t timeout_ms);

int fast_gpruio_register_pin(struct fast_gpruio_ctx *ctx,
                             const char *pin_name,
                             uint32_t idle_level,
                             uint32_t *out_pin_id);

int fast_gpruio_register_gpio_raw(struct fast_gpruio_ctx *ctx,
                                  uint32_t pin_id,
                                  uint32_t gpio_bank,
                                  uint32_t gpio_bit,
                                  uint32_t idle_level);

int fast_gpruio_start_pulse_cycles(struct fast_gpruio_ctx *ctx,
                                   uint32_t pin_id,
                                   uint32_t pulse_cycles,
                                   uint32_t active_level,
                                   uint32_t *out_seq);

int fast_gpruio_start_pulse(struct fast_gpruio_ctx *ctx,
                            uint32_t pin_id,
                            uint32_t pulse_us,
                            uint32_t active_level,
                            uint32_t *out_seq);

int fast_gpruio_is_pin_busy(struct fast_gpruio_ctx *ctx, uint32_t pin_id);

uint32_t fast_gpruio_calc_pulse_length_us(size_t len,
                                          uint32_t bits_per_byte_on_wire,
                                          uint32_t baud,
                                          uint32_t guard_us);

const char *fast_gpruio_status_string(uint32_t status);

#ifdef __cplusplus
}
#endif

#endif
