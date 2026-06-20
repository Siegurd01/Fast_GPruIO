/*
 * Fast_GPruIO PRU1 firmware for PocketBeagle / AM335x.
 *
 * PRU1 schedules non-blocking GPIO pulses through AM335x GPIO registers.
 * Time comes from the PRU-ICSS IEP timer at 200 MHz.
 * It uses PRU1 local DRAM mailbox at 0x00001000 (ARM phys 0x4A303000).
 */

typedef unsigned int u32;
typedef int s32;

#include "../include/fast_gpruio_shared.h"

#define PRU_CFG_SYSCFG_ADDR          0x00026004u
#define PRU_CFG_SYSCFG               (*(volatile u32 *)PRU_CFG_SYSCFG_ADDR)
#define PRU_CFG_SYSCFG_STANDBY_INIT  (1u << 4)

#define PRU_CFG_IEPCLK_ADDR          0x00026030u
#define PRU_CFG_IEPCLK               (*(volatile u32 *)PRU_CFG_IEPCLK_ADDR)
#define PRU_CFG_IEPCLK_OCP_EN        1u

#define PRU_IEP_BASE                 0x0002E000u
#define IEP_TMR_GLB_CFG              (*(volatile u32 *)(PRU_IEP_BASE + 0x00u))
#define IEP_TMR_GLB_STS              (*(volatile u32 *)(PRU_IEP_BASE + 0x04u))
#define IEP_TMR_COMPEN               (*(volatile u32 *)(PRU_IEP_BASE + 0x08u))
#define IEP_TMR_CNT                  (*(volatile u32 *)(PRU_IEP_BASE + 0x0Cu))
#define IEP_TMR_CMP_CFG              (*(volatile u32 *)(PRU_IEP_BASE + 0x40u))
#define IEP_TMR_CMP_STS              (*(volatile u32 *)(PRU_IEP_BASE + 0x44u))
#define IEP_CNT_EN                   0x1u
#define IEP_DEFAULT_INC_1            0x10u

#define GPIO_SETDATAOUT              0x194u
#define GPIO_CLEARDATAOUT            0x190u
#define GPIO_REG32(base, off)        (*(volatile u32 *)((base) + (off)))

#define MAILBOX_ADDR                 0x00001000u
#define MB                           ((volatile struct fast_gpruio_mailbox *)MAILBOX_ADDR)

struct my_resource_table {
    u32 ver;
    u32 num;
    u32 reserved[2];
};

#pragma DATA_SECTION(resourceTable, ".resource_table")
#pragma RETAIN(resourceTable)
struct my_resource_table resourceTable = {
    1u,
    0u,
    {0u, 0u}
};

static void gpio_write(u32 base, u32 mask, u32 level)
{
    if (level != 0u) {
        GPIO_REG32(base, GPIO_SETDATAOUT) = mask;
    } else {
        GPIO_REG32(base, GPIO_CLEARDATAOUT) = mask;
    }
}

static void zero_mailbox(void)
{
    volatile u32 *p = (volatile u32 *)MB;
    u32 i;

    for (i = 0u; i < (u32)(sizeof(struct fast_gpruio_mailbox) / sizeof(u32)); ++i) {
        p[i] = 0u;
    }
}

static void init_iep_timer(void)
{
    PRU_CFG_IEPCLK |= PRU_CFG_IEPCLK_OCP_EN;

    IEP_TMR_GLB_CFG = 0u;
    IEP_TMR_CNT = 0u;
    IEP_TMR_GLB_STS = 0x1u;
    IEP_TMR_CMP_STS = 0xffu;
    IEP_TMR_COMPEN = 0u;
    IEP_TMR_CMP_CFG = 0u;
    IEP_TMR_GLB_CFG = IEP_CNT_EN | IEP_DEFAULT_INC_1;
}

static u32 read_timer(void)
{
    return IEP_TMR_CNT;
}

static void expire_pulses(u32 now)
{
    u32 i;

    for (i = 0u; i < FAST_GPRUIO_MAX_PINS; ++i) {
        volatile struct fast_gpruio_pin_slot *pin = &MB->pins[i];
        if ((pin->enabled != 0u) && (pin->busy != 0u)) {
            if ((s32)(now - pin->deadline_cycle) >= 0) {
                gpio_write(pin->gpio_base, pin->gpio_mask, pin->idle_level);
                pin->busy = 0u;
                MB->expired_count++;
            }
        }
    }
}

static u32 validate_pin(volatile struct fast_gpruio_pin_slot *pin)
{
    if (pin->enabled == 0u) {
        return FAST_GPRUIO_STATUS_BAD_PIN;
    }
    if (pin->gpio_mask == 0u) {
        return FAST_GPRUIO_STATUS_BAD_PIN;
    }
    if ((pin->gpio_base != FAST_GPRUIO_GPIO0_BASE) &&
        (pin->gpio_base != FAST_GPRUIO_GPIO1_BASE) &&
        (pin->gpio_base != FAST_GPRUIO_GPIO2_BASE) &&
        (pin->gpio_base != FAST_GPRUIO_GPIO3_BASE)) {
        return FAST_GPRUIO_STATUS_BAD_PIN;
    }
    return FAST_GPRUIO_STATUS_OK;
}

static void process_commands(void)
{
    while (MB->cmd_read_idx != MB->cmd_write_idx) {
        u32 idx = MB->cmd_read_idx & (FAST_GPRUIO_CMD_RING_SIZE - 1u);
        volatile struct fast_gpruio_cmd_slot *cmd = &MB->cmds[idx];
        u32 seq = cmd->command_seq;
        u32 status = FAST_GPRUIO_STATUS_OK;

        if (seq == 0u) {
            break;
        }

        if (cmd->pin_id >= FAST_GPRUIO_MAX_PINS) {
            status = FAST_GPRUIO_STATUS_BAD_PIN;
        } else if (cmd->duration_cycles == 0u || cmd->duration_cycles >= 0x80000000u) {
            status = FAST_GPRUIO_STATUS_BAD_DURATION;
        } else {
            volatile struct fast_gpruio_pin_slot *pin = &MB->pins[cmd->pin_id];
            status = validate_pin(pin);
            if (status == FAST_GPRUIO_STATUS_OK) {
                if (pin->busy != 0u) {
                    pin->busy_reject_count++;
                    status = FAST_GPRUIO_STATUS_PIN_BUSY;
                } else {
                    pin->busy = 1u;
                    pin->last_seq = seq;
                    gpio_write(pin->gpio_base, pin->gpio_mask, cmd->active_level);
                    pin->deadline_cycle = read_timer() + cmd->duration_cycles;
                    pin->pulse_count++;
                    MB->accepted_count++;
                }
            }
        }

        if (status != FAST_GPRUIO_STATUS_OK) {
            MB->rejected_count++;
            MB->last_error_status = status;
        }

        cmd->result_status = status;
        cmd->result_seq = seq;
        MB->next_seq_seen = seq;
        MB->cmd_read_idx = (MB->cmd_read_idx + 1u) & (FAST_GPRUIO_CMD_RING_SIZE - 1u);
    }
}

void main(void)
{
    PRU_CFG_SYSCFG &= ~PRU_CFG_SYSCFG_STANDBY_INIT;
    init_iep_timer();

    zero_mailbox();
    MB->version = FAST_GPRUIO_VERSION;
    MB->struct_size = (u32)sizeof(struct fast_gpruio_mailbox);
    MB->pru_hz = FAST_GPRUIO_PRU_HZ;
    MB->timer_probe_delta = 0u;
    MB->timer_mode = FAST_GPRUIO_TIMER_IEP;
    MB->magic = FAST_GPRUIO_MAGIC;

    while (1) {
        u32 now = read_timer();
        MB->loop_counter++;
        MB->last_cycle = now;
        MB->timer_last_hw_cycle = now;

        expire_pulses(now);
        process_commands();
    }
}
