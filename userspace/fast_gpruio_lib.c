#define _GNU_SOURCE

#include "fast_gpruio_lib.h"

#include <errno.h>
#include <ctype.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

struct board_pin_info {
    char config_pin_name[8];
    uint32_t gpio_bank;
    uint32_t gpio_bit;
    uint32_t linux_gpio;
    uint32_t has_linux_gpio;
    uint32_t from_static_table;
    char current_mode[32];
    char default_function[32];
    char non_gpio_reason[64];
};

static int parse_u32_decimal(const char *s, const char **end_out, uint32_t *out);

struct static_pin_info {
    const char *name;
    uint32_t gpio_capable;
    uint32_t gpio_bank;
    uint32_t gpio_bit;
    uint32_t linux_gpio;
    const char *default_function;
    const char *captured_mode;
    const char *non_gpio_reason;
};

static const struct static_pin_info g_static_pins[] = {
    { "P1.01", 0u, 0u, 0u, 0u, "", "", "VIN-AC" },
    { "P1.02", 1u, 2u, 23u, 87u, "gpio_input", "default", "" },
    { "P1.03", 0u, 0u, 0u, 0u, "", "", "usb1_vbus_out" },
    { "P1.04", 1u, 2u, 25u, 89u, "gpio", "default", "" },
    { "P1.05", 0u, 0u, 0u, 0u, "", "", "usb1_vbus_in" },
    { "P1.06", 1u, 0u, 5u, 5u, "spi_cs", "default", "" },
    { "P1.07", 0u, 0u, 0u, 0u, "", "", "VIN-USB" },
    { "P1.08", 1u, 0u, 2u, 2u, "spi_sclk", "uart", "" },
    { "P1.09", 0u, 0u, 0u, 0u, "", "", "USB1-DN" },
    { "P1.10", 1u, 0u, 3u, 3u, "spi", "uart", "" },
    { "P1.11", 0u, 0u, 0u, 0u, "", "", "USB1-DP" },
    { "P1.12", 1u, 0u, 4u, 4u, "spi", "default", "" },
    { "P1.13", 0u, 0u, 0u, 0u, "", "", "USB1-ID" },
    { "P1.14", 0u, 0u, 0u, 0u, "", "", "VOUT-3.3V" },
    { "P1.15", 0u, 0u, 0u, 0u, "", "", "GND" },
    { "P1.16", 0u, 0u, 0u, 0u, "", "", "GND" },
    { "P1.17", 0u, 0u, 0u, 0u, "", "", "VREFN" },
    { "P1.18", 0u, 0u, 0u, 0u, "", "", "VREFP" },
    { "P1.19", 0u, 0u, 0u, 0u, "", "", "AIN0" },
    { "P1.20", 1u, 0u, 20u, 20u, "gpio", "default", "" },
    { "P1.21", 0u, 0u, 0u, 0u, "", "", "AIN1" },
    { "P1.22", 0u, 0u, 0u, 0u, "", "", "GND" },
    { "P1.23", 0u, 0u, 0u, 0u, "", "", "AIN2" },
    { "P1.24", 0u, 0u, 0u, 0u, "", "", "VOUT-5V" },
    { "P1.25", 0u, 0u, 0u, 0u, "", "", "AIN3" },
    { "P1.26", 1u, 0u, 12u, 12u, "i2c", "default", "" },
    { "P1.27", 0u, 0u, 0u, 0u, "", "", "AIN4" },
    { "P1.28", 1u, 0u, 13u, 13u, "i2c", "default", "" },
    { "P1.29", 1u, 3u, 21u, 117u, "pruin", "default", "" },
    { "P1.30", 1u, 1u, 11u, 43u, "uart", "uart", "" },
    { "P1.31", 1u, 3u, 18u, 114u, "pruin", "default", "" },
    { "P1.32", 1u, 1u, 10u, 42u, "uart", "uart", "" },
    { "P1.33", 1u, 3u, 15u, 111u, "pruin", "default", "" },
    { "P1.34", 1u, 0u, 26u, 26u, "gpio", "default", "" },
    { "P1.35", 1u, 2u, 24u, 88u, "pruin", "default", "" },
    { "P1.36", 1u, 3u, 14u, 110u, "pwm", "pruin", "" },
    { "P2.01", 1u, 1u, 18u, 50u, "pwm", "default", "" },
    { "P2.02", 1u, 1u, 27u, 59u, "gpio", "default", "" },
    { "P2.03", 1u, 0u, 23u, 23u, "gpio", "default", "" },
    { "P2.04", 1u, 1u, 26u, 58u, "gpio", "default", "" },
    { "P2.05", 1u, 0u, 30u, 30u, "uart", "uart", "" },
    { "P2.06", 1u, 1u, 25u, 57u, "gpio", "default", "" },
    { "P2.07", 1u, 0u, 31u, 31u, "uart", "uart", "" },
    { "P2.08", 1u, 1u, 28u, 60u, "gpio", "default", "" },
    { "P2.09", 1u, 0u, 15u, 15u, "i2c", "default", "" },
    { "P2.10", 1u, 1u, 20u, 52u, "gpio", "default", "" },
    { "P2.11", 1u, 0u, 14u, 14u, "i2c", "default", "" },
    { "P2.12", 0u, 0u, 0u, 0u, "", "", "POWER_BUTTON" },
    { "P2.13", 0u, 0u, 0u, 0u, "", "", "VOUT-5V" },
    { "P2.14", 0u, 0u, 0u, 0u, "", "", "BAT-VIN" },
    { "P2.15", 0u, 0u, 0u, 0u, "", "", "GND" },
    { "P2.16", 0u, 0u, 0u, 0u, "", "", "BAT-TEMP" },
    { "P2.17", 1u, 2u, 1u, 65u, "gpio", "gpio", "" },
    { "P2.18", 1u, 1u, 15u, 47u, "gpio", "default", "" },
    { "P2.19", 1u, 0u, 27u, 27u, "gpio", "gpio", "" },
    { "P2.20", 1u, 2u, 0u, 64u, "gpio", "default", "" },
    { "P2.21", 0u, 0u, 0u, 0u, "", "", "GND" },
    { "P2.22", 1u, 1u, 14u, 46u, "gpio", "default", "" },
    { "P2.23", 0u, 0u, 0u, 0u, "", "", "VOUT-3.3V" },
    { "P2.24", 1u, 1u, 12u, 44u, "gpio", "default", "" },
    { "P2.25", 1u, 1u, 9u, 41u, "spi", "default", "" },
    { "P2.26", 0u, 0u, 0u, 0u, "", "", "RESET#" },
    { "P2.27", 1u, 1u, 8u, 40u, "spi", "default", "" },
    { "P2.28", 1u, 3u, 20u, 116u, "pruin", "default", "" },
    { "P2.29", 1u, 0u, 7u, 7u, "spi_sclk", "default", "" },
    { "P2.30", 1u, 3u, 17u, 113u, "pruin", "default", "" },
    { "P2.31", 1u, 0u, 19u, 19u, "spi_cs", "default", "" },
    { "P2.32", 1u, 3u, 16u, 112u, "pruin", "default", "" },
    { "P2.33", 1u, 1u, 13u, 45u, "gpio", "default", "" },
    { "P2.34", 1u, 3u, 19u, 115u, "pruin", "default", "" },
    { "P2.35", 1u, 2u, 22u, 86u, "gpio_input", "default", "" },
    { "P2.36", 0u, 0u, 0u, 0u, "", "", "AIN7" },
};

static uint64_t now_ns_raw(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}

static uint32_t gpio_base_for_bank(uint32_t bank)
{
    switch (bank) {
    case 0u: return FAST_GPRUIO_GPIO0_BASE;
    case 1u: return FAST_GPRUIO_GPIO1_BASE;
    case 2u: return FAST_GPRUIO_GPIO2_BASE;
    case 3u: return FAST_GPRUIO_GPIO3_BASE;
    default: return 0u;
    }
}

static int write_text_file(const char *path, const char *text)
{
    int fd = open(path, O_WRONLY | O_CLOEXEC);
    if (fd < 0) {
        perror(path);
        return -1;
    }

    size_t len = strlen(text);
    ssize_t n = write(fd, text, len);
    close(fd);

    if (n != (ssize_t)len) {
        perror("write sysfs");
        return -1;
    }

    return 0;
}

static int read_text_file(const char *path, char *buf, size_t buf_len)
{
    int fd = open(path, O_RDONLY | O_CLOEXEC);
    if (fd < 0) {
        return -1;
    }

    ssize_t n = read(fd, buf, buf_len - 1u);
    close(fd);

    if (n < 0) {
        return -1;
    }

    buf[n] = '\0';
    return 0;
}

static int copy_file(const char *src, const char *dst)
{
    int in = open(src, O_RDONLY | O_CLOEXEC);
    if (in < 0) {
        perror(src);
        return -1;
    }

    int out = open(dst, O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0644);
    if (out < 0) {
        perror(dst);
        close(in);
        return -1;
    }

    char buf[8192];
    for (;;) {
        ssize_t r = read(in, buf, sizeof(buf));
        if (r == 0) {
            break;
        }
        if (r < 0) {
            perror("read firmware");
            close(in);
            close(out);
            return -1;
        }

        char *p = buf;
        ssize_t left = r;
        while (left > 0) {
            ssize_t w = write(out, p, (size_t)left);
            if (w < 0) {
                perror("write firmware");
                close(in);
                close(out);
                return -1;
            }
            p += w;
            left -= w;
        }
    }

    close(in);
    close(out);
    return 0;
}

static int find_pru1_remoteproc(char *out, size_t out_len)
{
    int i;
    for (i = 0; i < 16; i++) {
        char rp[128];
        char link[512];
        snprintf(rp, sizeof(rp), "/sys/class/remoteproc/remoteproc%d", i);

        ssize_t n = readlink(rp, link, sizeof(link) - 1u);
        if (n < 0) {
            continue;
        }
        link[n] = '\0';

        if (strstr(link, "4a338000.pru") != NULL) {
            snprintf(out, out_len, "%s", rp);
            return 0;
        }
    }

    if (access("/sys/class/remoteproc/remoteproc2", F_OK) == 0) {
        snprintf(out, out_len, "/sys/class/remoteproc/remoteproc2");
        return 0;
    }

    fprintf(stderr, "PRU1 remoteproc not found\n");
    return -1;
}

static int run_config_pin_gpio(const char *pin_name)
{
    char cmd[128];
    int n = snprintf(cmd, sizeof(cmd), "config-pin %s gpio", pin_name);
    if (n < 0 || (size_t)n >= sizeof(cmd)) {
        return -1;
    }
    return system(cmd);
}

static int config_pin_available(void)
{
    return (system("command -v config-pin >/dev/null 2>&1") == 0) ? 1 : 0;
}

static int capture_command(const char *cmd, char *buf, size_t buf_len)
{
    FILE *fp;
    size_t used = 0u;

    if (buf == NULL || buf_len == 0u) {
        return -1;
    }

    buf[0] = '\0';
    fp = popen(cmd, "r");
    if (fp == NULL) {
        return -1;
    }

    while (used + 1u < buf_len) {
        size_t n = fread(buf + used, 1u, buf_len - used - 1u, fp);
        used += n;
        if (n == 0u) {
            break;
        }
    }
    buf[used] = '\0';

    return pclose(fp);
}

static int configure_linux_gpio(uint32_t linux_gpio, uint32_t idle_level)
{
    char path[128];
    char value[2];

    snprintf(path, sizeof(path), "/sys/class/gpio/gpio%u", linux_gpio);
    if (access(path, F_OK) != 0) {
        int fd = open("/sys/class/gpio/export", O_WRONLY | O_CLOEXEC);
        if (fd >= 0) {
            char num[16];
            int n = snprintf(num, sizeof(num), "%u", linux_gpio);
            if (n > 0) {
                (void)write(fd, num, (size_t)n);
            }
            close(fd);
        }
    }

    snprintf(path, sizeof(path), "/sys/class/gpio/gpio%u/direction", linux_gpio);
    (void)write_text_file(path, "out");

    value[0] = (idle_level != 0u) ? '1' : '0';
    value[1] = '\0';
    snprintf(path, sizeof(path), "/sys/class/gpio/gpio%u/value", linux_gpio);
    return write_text_file(path, value);
}

static int parse_board_pin_name(const char *pin_name, char *out, size_t out_len)
{
    const char *p = pin_name;
    uint32_t header = 0u;
    uint32_t index = 0u;

    if (p == NULL || out == NULL || out_len < 7u) {
        return -1;
    }
    if (toupper((unsigned char)p[0]) != 'P' || (p[1] != '1' && p[1] != '2')) {
        return -1;
    }

    header = (uint32_t)(p[1] - '0');
    p += 2;
    if (*p != '.' && *p != '_') {
        return -1;
    }
    ++p;

    if (parse_u32_decimal(p, &p, &index) != 0 || *p != '\0') {
        return -1;
    }
    if (index < 1u || index > 36u) {
        return -1;
    }

    snprintf(out, out_len, "P%u.%02u", header, index);
    return 0;
}

static int parse_gpio_bank_bit_from_text(const char *text, uint32_t *bank, uint32_t *bit)
{
    const char *p = text;

    while (p != NULL && *p != '\0') {
        p = strstr(p, "gpio");
        if (p == NULL) {
            break;
        }
        p += 4;
        if (isdigit((unsigned char)p[0]) && p[1] == '_' &&
            isdigit((unsigned char)p[2])) {
            const char *q = p;
            uint32_t b = 0u;
            uint32_t v = 0u;

            if (parse_u32_decimal(q, &q, &b) == 0 && *q == '_') {
                ++q;
                if (parse_u32_decimal(q, &q, &v) == 0 && b <= 3u && v <= 31u) {
                    *bank = b;
                    *bit = v;
                    return 0;
                }
            }
        }
    }

    return -1;
}

static int text_line_has_gpio_mode(const char *text)
{
    const char *line = strstr(text, "Function if cape loaded:");
    const char *p;

    if (line == NULL) {
        return 0;
    }

    for (p = line; *p != '\0' && *p != '\n' && *p != '\r'; ++p) {
        if (strncmp(p, "gpio", 4) == 0) {
            return 1;
        }
    }

    return 0;
}

static int parse_kernel_gpio_id(const char *text, uint32_t *linux_gpio)
{
    const char *p = strstr(text, "Kernel GPIO id:");

    if (p == NULL) {
        return -1;
    }
    p += strlen("Kernel GPIO id:");
    while (*p == ' ' || *p == '\t') {
        ++p;
    }
    return parse_u32_decimal(p, NULL, linux_gpio);
}

static void describe_non_gpio_pin(const char *text, const char *config_pin_name,
                                  char *out, size_t out_len)
{
    const char *p;
    const char *end;

    if (out == NULL || out_len == 0u) {
        return;
    }
    out[0] = '\0';

    p = strstr(text, "Pin is not modifiable:");
    if (p != NULL) {
        p += strlen("Pin is not modifiable:");
        while (*p == ' ' || *p == '\t') {
            ++p;
        }
        if (config_pin_name != NULL && strncmp(p, config_pin_name, strlen(config_pin_name)) == 0) {
            p += strlen(config_pin_name);
            while (*p == ' ' || *p == '\t') {
                ++p;
            }
        }
        end = p;
        while (*end != '\0' && *end != '\r' && *end != '\n') {
            ++end;
        }
        if (end > p) {
            snprintf(out, out_len, "not modifiable: %.*s", (int)(end - p), p);
            return;
        }
    }

    p = strstr(text, "Function if no cape loaded:");
    if (p != NULL) {
        p += strlen("Function if no cape loaded:");
        while (*p == ' ' || *p == '\t') {
            ++p;
        }
        end = p;
        while (*end != '\0' && *end != '\r' && *end != '\n') {
            ++end;
        }
        if (end > p) {
            snprintf(out, out_len, "default function: %.*s", (int)(end - p), p);
            return;
        }
    }

    snprintf(out, out_len, "config-pin did not report a GPIO-capable function");
}

static void read_current_pin_mode(const char *config_pin_name, char *out, size_t out_len)
{
    char cmd[64];
    char text[256];
    char *mode;
    char *end;

    if (out == NULL || out_len == 0u) {
        return;
    }
    out[0] = '\0';

    snprintf(cmd, sizeof(cmd), "config-pin -q %s 2>/dev/null", config_pin_name);
    if (capture_command(cmd, text, sizeof(text)) < 0) {
        return;
    }

    mode = strstr(text, "Mode:");
    if (mode == NULL) {
        return;
    }
    mode += strlen("Mode:");
    while (*mode == ' ' || *mode == '\t') {
        ++mode;
    }
    end = mode;
    while (*end != '\0' && *end != ' ' && *end != '\t' &&
           *end != '\r' && *end != '\n') {
        ++end;
    }

    snprintf(out, out_len, "%.*s", (int)(end - mode), mode);
}

static int current_mode_is_gpio(const char *mode)
{
    return (mode != NULL && strncmp(mode, "gpio", 4) == 0) ? 1 : 0;
}

static const struct static_pin_info *find_static_pin(const char *pin_name)
{
    size_t i;

    for (i = 0u; i < sizeof(g_static_pins) / sizeof(g_static_pins[0]); ++i) {
        if (strcmp(pin_name, g_static_pins[i].name) == 0) {
            return &g_static_pins[i];
        }
    }

    return NULL;
}

static int query_board_pin_info_static(const char *pin_name,
                                       struct board_pin_info *info,
                                       int print_errors)
{
    const struct static_pin_info *pin = find_static_pin(pin_name);

    if (pin == NULL || info == NULL) {
        return -1;
    }

    if (pin->gpio_capable == 0u) {
        if (print_errors) {
            fprintf(stderr, "Fast_GPruIO pin %s cannot be used as GPIO (not modifiable: %s)\n",
                    pin_name,
                    (pin->non_gpio_reason != NULL && pin->non_gpio_reason[0] != '\0') ?
                    pin->non_gpio_reason : "not GPIO-capable");
        }
        return -2;
    }

    info->gpio_bank = pin->gpio_bank;
    info->gpio_bit = pin->gpio_bit;
    info->linux_gpio = pin->linux_gpio;
    info->has_linux_gpio = 1u;
    info->from_static_table = 1u;
    snprintf(info->current_mode, sizeof(info->current_mode), "%s",
             pin->captured_mode != NULL ? pin->captured_mode : "");
    snprintf(info->default_function, sizeof(info->default_function), "%s",
             pin->default_function != NULL ? pin->default_function : "");
    snprintf(info->non_gpio_reason, sizeof(info->non_gpio_reason), "%s",
             pin->non_gpio_reason != NULL ? pin->non_gpio_reason : "");
    return 0;
}

static int query_board_pin_info(const char *pin_name, struct board_pin_info *info)
{
    char cmd[64];
    char text[2048];

    if (info == NULL) {
        return -1;
    }
    memset(info, 0, sizeof(*info));

    if (parse_board_pin_name(pin_name, info->config_pin_name,
                             sizeof(info->config_pin_name)) != 0) {
        return -1;
    }

    if (!config_pin_available()) {
        fprintf(stderr,
                "Fast_GPruIO warning: config-pin not found; using static PocketBeagle pin table\n");
        return query_board_pin_info_static(info->config_pin_name, info, 1);
    }

    snprintf(cmd, sizeof(cmd), "config-pin -i %s 2>&1", info->config_pin_name);
    if (capture_command(cmd, text, sizeof(text)) < 0) {
        fprintf(stderr,
                "Fast_GPruIO warning: config-pin -i failed for %s; using static PocketBeagle pin table\n",
                info->config_pin_name);
        return query_board_pin_info_static(info->config_pin_name, info, 1);
    }

    if (!text_line_has_gpio_mode(text)) {
        char reason[128];
        describe_non_gpio_pin(text, info->config_pin_name, reason, sizeof(reason));
        fprintf(stderr, "Fast_GPruIO pin %s cannot be used as GPIO (%s)\n",
                info->config_pin_name, reason);
        return -2;
    }
    if (parse_gpio_bank_bit_from_text(text, &info->gpio_bank, &info->gpio_bit) != 0) {
        fprintf(stderr, "Fast_GPruIO cannot parse GPIO bank/bit for %s\n",
                info->config_pin_name);
        return -1;
    }
    if (parse_kernel_gpio_id(text, &info->linux_gpio) == 0) {
        info->has_linux_gpio = 1u;
    }

    read_current_pin_mode(info->config_pin_name, info->current_mode,
                          sizeof(info->current_mode));
    snprintf(info->default_function, sizeof(info->default_function), "%s", "");
    return 0;
}

static int parse_u32_decimal(const char *s, const char **end_out, uint32_t *out)
{
    const char *p = s;
    uint32_t v = 0u;

    if (p == NULL || !isdigit((unsigned char)*p)) {
        return -1;
    }

    while (isdigit((unsigned char)*p)) {
        uint32_t digit = (uint32_t)(*p - '0');
        if (v > (0xffffffffu - digit) / 10u) {
            return -1;
        }
        v = v * 10u + digit;
        ++p;
    }

    if (end_out != NULL) {
        *end_out = p;
    }
    *out = v;
    return 0;
}

static int parse_raw_gpio_name(const char *pin_name, uint32_t *bank, uint32_t *bit)
{
    const char *p = pin_name;
    uint32_t first = 0u;
    uint32_t second = 0u;

    if (p == NULL || strncasecmp(p, "gpio", 4) != 0) {
        return -1;
    }
    p += 4;

    if (parse_u32_decimal(p, &p, &first) != 0) {
        return -1;
    }

    if (*p == '.' || *p == '_') {
        ++p;
        if (parse_u32_decimal(p, &p, &second) != 0 || *p != '\0') {
            return -1;
        }
        if (first > 3u || second > 31u) {
            return -1;
        }
        *bank = first;
        *bit = second;
        return 0;
    }

    if (*p == '\0') {
        if (first > 127u) {
            return -1;
        }
        *bank = first / 32u;
        *bit = first % 32u;
        return 0;
    }

    return -1;
}

static int find_free_pin_slot(struct fast_gpruio_ctx *ctx, uint32_t *out_pin_id)
{
    uint32_t i;

    if (ctx == NULL || ctx->mb == NULL || out_pin_id == NULL) {
        return -1;
    }

    for (i = 0u; i < FAST_GPRUIO_MAX_PINS; ++i) {
        if (ctx->mb->pins[i].enabled == 0u && ctx->mb->pins[i].busy == 0u) {
            *out_pin_id = i;
            return 0;
        }
    }

    return -1;
}

static int find_registered_pin_slot(struct fast_gpruio_ctx *ctx,
                                    uint32_t gpio_bank,
                                    uint32_t gpio_bit,
                                    uint32_t *out_pin_id)
{
    uint32_t i;
    uint32_t base;
    uint32_t mask;

    if (ctx == NULL || ctx->mb == NULL || out_pin_id == NULL ||
        gpio_bank > 3u || gpio_bit > 31u) {
        return -1;
    }

    base = gpio_base_for_bank(gpio_bank);
    mask = (uint32_t)(1u << gpio_bit);
    for (i = 0u; i < FAST_GPRUIO_MAX_PINS; ++i) {
        volatile struct fast_gpruio_pin_slot *pin = &ctx->mb->pins[i];
        if (pin->enabled != 0u &&
            pin->gpio_base == base && pin->gpio_mask == mask) {
            *out_pin_id = i;
            return 0;
        }
    }

    return -1;
}

static int find_registered_or_free_pin_slot(struct fast_gpruio_ctx *ctx,
                                             uint32_t gpio_bank,
                                             uint32_t gpio_bit,
                                             uint32_t *out_pin_id)
{
    if (find_registered_pin_slot(ctx, gpio_bank, gpio_bit, out_pin_id) == 0) {
        return 0;
    }
    return find_free_pin_slot(ctx, out_pin_id);
}

int fast_gpruio_load_pru1_firmware(const char *firmware_out_path,
                                   const char *firmware_name)
{
    if (firmware_name == NULL) {
        firmware_name = FAST_GPRUIO_DEFAULT_FW_NAME;
    }

    char rp[256];
    if (find_pru1_remoteproc(rp, sizeof(rp)) != 0) {
        return -1;
    }

    if (firmware_out_path != NULL) {
        char dst[512];
        snprintf(dst, sizeof(dst), "/lib/firmware/%s", firmware_name);
        if (copy_file(firmware_out_path, dst) != 0) {
            return -1;
        }
    }

    char state_path[512];
    char firmware_path[512];
    snprintf(state_path, sizeof(state_path), "%s/state", rp);
    snprintf(firmware_path, sizeof(firmware_path), "%s/firmware", rp);

    char state[64];
    if (read_text_file(state_path, state, sizeof(state)) == 0) {
        if (strstr(state, "running") != NULL) {
            if (write_text_file(state_path, "stop") != 0) {
                return -1;
            }
            usleep(50000);
        }
    }

    if (write_text_file(firmware_path, firmware_name) != 0) {
        return -1;
    }

    if (write_text_file(state_path, "start") != 0) {
        return -1;
    }

    usleep(200000);
    return 0;
}

int fast_gpruio_open_mailbox(struct fast_gpruio_ctx *ctx)
{
    if (ctx == NULL) {
        return -1;
    }

    memset(ctx, 0, sizeof(*ctx));
    ctx->mem_fd = -1;

    int fd = open("/dev/mem", O_RDWR | O_SYNC | O_CLOEXEC);
    if (fd < 0) {
        perror("open /dev/mem");
        return -1;
    }

    void *map = mmap(NULL,
                     FAST_GPRUIO_MAILBOX_MAP_SIZE,
                     PROT_READ | PROT_WRITE,
                     MAP_SHARED,
                     fd,
                     FAST_GPRUIO_MAILBOX_PHYS);
    if (map == MAP_FAILED) {
        perror("mmap Fast_GPruIO mailbox");
        close(fd);
        return -1;
    }

    ctx->mem_fd = fd;
    ctx->mb = (volatile struct fast_gpruio_mailbox *)map;
    ctx->next_seq = 1u;
    return 0;
}

int fast_gpruio_wait_ready(struct fast_gpruio_ctx *ctx, uint32_t timeout_ms)
{
    if (ctx == NULL || ctx->mb == NULL) {
        fprintf(stderr, "Fast_GPruIO mailbox is not mapped\n");
        return -1;
    }

    uint64_t deadline = now_ns_raw() + (uint64_t)timeout_ms * 1000000ULL;

    while (now_ns_raw() < deadline) {
        if (ctx->mb->magic == FAST_GPRUIO_MAGIC &&
            ctx->mb->version == FAST_GPRUIO_VERSION) {
            return 0;
        }
        usleep(1000);
    }

    fprintf(stderr, "Fast_GPruIO firmware not ready: magic=0x%08X version=%u\n",
            ctx->mb->magic, ctx->mb->version);
    return -1;
}

int fast_gpruio_init(struct fast_gpruio_ctx *ctx,
                     const char *firmware_out_path,
                     uint32_t ready_timeout_ms)
{
    if (ctx == NULL) {
        return -1;
    }

    memset(ctx, 0, sizeof(*ctx));
    ctx->mem_fd = -1;

    if (firmware_out_path != NULL) {
        if (fast_gpruio_load_pru1_firmware(firmware_out_path,
                                           FAST_GPRUIO_DEFAULT_FW_NAME) != 0) {
            return -1;
        }
    }

    if (fast_gpruio_open_mailbox(ctx) != 0) {
        return -1;
    }

    if (fast_gpruio_wait_ready(ctx, ready_timeout_ms) != 0) {
        fast_gpruio_close(ctx);
        return -1;
    }

    return 0;
}

void fast_gpruio_close(struct fast_gpruio_ctx *ctx)
{
    if (ctx == NULL) {
        return;
    }

    if (ctx->mb != NULL) {
        munmap((void *)ctx->mb, FAST_GPRUIO_MAILBOX_MAP_SIZE);
    }

    if (ctx->mem_fd >= 0) {
        close(ctx->mem_fd);
    }

    ctx->mb = NULL;
    ctx->mem_fd = -1;
    ctx->next_seq = 0;
}

int fast_gpruio_register_gpio_raw(struct fast_gpruio_ctx *ctx,
                                  uint32_t pin_id,
                                  uint32_t gpio_bank,
                                  uint32_t gpio_bit,
                                  uint32_t idle_level)
{
    if (ctx == NULL || ctx->mb == NULL) {
        return -1;
    }
    if (pin_id >= FAST_GPRUIO_MAX_PINS || gpio_bank > 3u || gpio_bit > 31u) {
        return -1;
    }

    uint32_t base = gpio_base_for_bank(gpio_bank);
    if (base == 0u) {
        return -1;
    }

    volatile struct fast_gpruio_pin_slot *pin = &ctx->mb->pins[pin_id];
    if (pin->busy != 0u) {
        fprintf(stderr, "Fast_GPruIO pin %u is busy\n", pin_id);
        return -2;
    }
    if (pin->enabled != 0u &&
        (pin->gpio_base != base || pin->gpio_mask != (uint32_t)(1u << gpio_bit))) {
        fprintf(stderr, "Fast_GPruIO pin slot %u is already used\n", pin_id);
        return -1;
    }

    pin->enabled = 0u;
    __sync_synchronize();
    pin->gpio_base = base;
    pin->gpio_mask = (uint32_t)(1u << gpio_bit);
    pin->idle_level = (idle_level != 0u) ? 1u : 0u;
    pin->busy = 0u;
    pin->deadline_cycle = 0u;
    pin->last_seq = 0u;
    __sync_synchronize();
    pin->enabled = 1u;
    __sync_synchronize();
    return 0;
}

int fast_gpruio_register_pin(struct fast_gpruio_ctx *ctx,
                             const char *pin_name,
                             uint32_t idle_level,
                             uint32_t *out_pin_id)
{
    struct board_pin_info board_pin;
    char board_name[8];

    if (pin_name == NULL) {
        return -1;
    }

    if (query_board_pin_info(pin_name, &board_pin) != 0) {
        uint32_t bank = 0u;
        uint32_t bit = 0u;
        uint32_t pin_id = 0u;

        if (parse_board_pin_name(pin_name, board_name, sizeof(board_name)) == 0) {
            return -1;
        }

        if (parse_raw_gpio_name(pin_name, &bank, &bit) != 0) {
            fprintf(stderr, "Fast_GPruIO unsupported pin: %s\n", pin_name);
            return -1;
        }

        if (find_registered_or_free_pin_slot(ctx, bank, bit, &pin_id) != 0) {
            fprintf(stderr, "Fast_GPruIO has no free pin slots\n");
            return -1;
        }

        fprintf(stderr,
                "Fast_GPruIO raw GPIO %s -> GPIO%u.%u, pin_id %u; configure pinmux before use\n",
                pin_name, bank, bit, pin_id);

        int rc = fast_gpruio_register_gpio_raw(ctx, pin_id, bank, bit, idle_level);
        if (rc == 0 && out_pin_id != NULL) {
            *out_pin_id = pin_id;
        }
        return rc;
    }

    if (board_pin.from_static_table != 0u) {
        fprintf(stderr,
                "Fast_GPruIO warning: %s resolved from static table as GPIO%u.%u; pinmux was not checked or switched\n",
                board_pin.config_pin_name, board_pin.gpio_bank, board_pin.gpio_bit);
        if (board_pin.current_mode[0] != '\0' &&
            !current_mode_is_gpio(board_pin.current_mode) &&
            strcmp(board_pin.current_mode, "default") != 0) {
            fprintf(stderr,
                    "Fast_GPruIO warning: captured mode for %s was '%s'\n",
                    board_pin.config_pin_name, board_pin.current_mode);
        }
    } else if (board_pin.current_mode[0] != '\0' &&
        !current_mode_is_gpio(board_pin.current_mode)) {
        fprintf(stderr,
                "Fast_GPruIO warning: %s current mode is '%s'; switching to gpio\n",
                board_pin.config_pin_name, board_pin.current_mode);
    }

    if (board_pin.from_static_table == 0u &&
        run_config_pin_gpio(board_pin.config_pin_name) != 0) {
        fprintf(stderr, "config-pin failed for %s\n", board_pin.config_pin_name);
        return -1;
    }

    if (board_pin.has_linux_gpio != 0u) {
        if (configure_linux_gpio(board_pin.linux_gpio, idle_level) != 0) {
            fprintf(stderr, "GPIO%u setup failed for %s\n",
                    board_pin.linux_gpio, board_pin.config_pin_name);
            return -1;
        }
    }

    uint32_t pin_id = 0u;
    if (find_registered_or_free_pin_slot(ctx,
                                         board_pin.gpio_bank,
                                         board_pin.gpio_bit,
                                         &pin_id) != 0) {
        fprintf(stderr, "Fast_GPruIO has no free pin slots\n");
        return -1;
    }

    int rc = fast_gpruio_register_gpio_raw(ctx,
                                           pin_id,
                                           board_pin.gpio_bank,
                                           board_pin.gpio_bit,
                                           idle_level);
    if (rc == 0 && out_pin_id != NULL) {
        *out_pin_id = pin_id;
    }
    return rc;
}

int fast_gpruio_is_pin_busy(struct fast_gpruio_ctx *ctx, uint32_t pin_id)
{
    if (ctx == NULL || ctx->mb == NULL || pin_id >= FAST_GPRUIO_MAX_PINS) {
        return -1;
    }
    return (ctx->mb->pins[pin_id].busy != 0u) ? 1 : 0;
}

int fast_gpruio_start_pulse_cycles(struct fast_gpruio_ctx *ctx,
                                   uint32_t pin_id,
                                   uint32_t pulse_cycles,
                                   uint32_t active_level,
                                   uint32_t *out_seq)
{
    if (ctx == NULL || ctx->mb == NULL) {
        return -1;
    }
    if (ctx->mb->magic != FAST_GPRUIO_MAGIC) {
        fprintf(stderr, "Fast_GPruIO firmware not ready: bad magic 0x%08X\n",
                ctx->mb->magic);
        return -1;
    }
    if (pin_id >= FAST_GPRUIO_MAX_PINS) {
        return FAST_GPRUIO_STATUS_BAD_PIN;
    }
    if (pulse_cycles == 0u || pulse_cycles >= 0x80000000u) {
        return FAST_GPRUIO_STATUS_BAD_DURATION;
    }
    if (ctx->mb->pins[pin_id].busy != 0u) {
        fprintf(stderr, "Fast_GPruIO pin %u is busy\n", pin_id);
        return FAST_GPRUIO_STATUS_PIN_BUSY;
    }

    uint32_t write_idx = ctx->mb->cmd_write_idx & (FAST_GPRUIO_CMD_RING_SIZE - 1u);
    uint32_t next_idx = (write_idx + 1u) & (FAST_GPRUIO_CMD_RING_SIZE - 1u);
    if (next_idx == (ctx->mb->cmd_read_idx & (FAST_GPRUIO_CMD_RING_SIZE - 1u))) {
        ctx->mb->queue_full_count++;
        return FAST_GPRUIO_STATUS_QUEUE_FULL;
    }

    uint32_t seq = ctx->next_seq++;
    if (seq == 0u) {
        seq = ctx->next_seq++;
    }

    volatile struct fast_gpruio_cmd_slot *cmd = &ctx->mb->cmds[write_idx];
    cmd->pin_id = pin_id;
    cmd->duration_cycles = pulse_cycles;
    cmd->active_level = (active_level != 0u) ? 1u : 0u;
    cmd->result_status = 0xffffffffu;
    cmd->result_seq = 0u;
    __sync_synchronize();
    cmd->command_seq = seq;
    __sync_synchronize();
    ctx->mb->cmd_write_idx = next_idx;
    __sync_synchronize();

    uint64_t deadline = now_ns_raw() + 10000000ULL;
    while (now_ns_raw() < deadline) {
        if (cmd->result_seq == seq) {
            if (out_seq != NULL) {
                *out_seq = seq;
            }
            if (cmd->result_status == FAST_GPRUIO_STATUS_PIN_BUSY) {
                fprintf(stderr, "Fast_GPruIO pin %u is busy\n", pin_id);
            }
            return (int)cmd->result_status;
        }
    }

    fprintf(stderr, "Fast_GPruIO timeout waiting command ack: seq=%u\n", seq);
    return -1;
}

int fast_gpruio_start_pulse(struct fast_gpruio_ctx *ctx,
                            uint32_t pin_id,
                            uint32_t pulse_us,
                            uint32_t active_level,
                            uint32_t *out_seq)
{
    uint64_t cycles = (uint64_t)pulse_us * FAST_GPRUIO_CYCLES_PER_US;
    if (cycles == 0u || cycles >= 0x80000000ULL) {
        return FAST_GPRUIO_STATUS_BAD_DURATION;
    }
    return fast_gpruio_start_pulse_cycles(ctx,
                                          pin_id,
                                          (uint32_t)cycles,
                                          active_level,
                                          out_seq);
}

uint32_t fast_gpruio_calc_pulse_length_us(size_t len,
                                          uint32_t bits_per_byte_on_wire,
                                          uint32_t baud,
                                          uint32_t guard_us)
{
    if (bits_per_byte_on_wire == 0u || baud == 0u) {
        return guard_us;
    }

    uint64_t bits = (uint64_t)len * (uint64_t)bits_per_byte_on_wire;
    uint64_t us = (bits * 1000000ULL + (uint64_t)baud - 1ULL) / (uint64_t)baud;
    us += guard_us;
    if (us > 0xffffffffULL) {
        return 0xffffffffu;
    }
    return (uint32_t)us;
}

const char *fast_gpruio_status_string(uint32_t status)
{
    switch (status) {
    case FAST_GPRUIO_STATUS_OK: return "ok";
    case FAST_GPRUIO_STATUS_BAD_PIN: return "bad pin";
    case FAST_GPRUIO_STATUS_PIN_BUSY: return "pin busy";
    case FAST_GPRUIO_STATUS_BAD_DURATION: return "bad duration";
    case FAST_GPRUIO_STATUS_QUEUE_FULL: return "queue full";
    default: return "unknown";
    }
}
