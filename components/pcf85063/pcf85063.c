#include <string.h>
#include <sys/time.h>

#include "driver/i2c_master.h"
#include "esp_log.h"

#include "board.h"
#include "pcf85063.h"
#include "pcf_regs.h"

#define RTC_ADDR      0x51
#define RTC_REG_CTRL1 0x00
#define RTC_REG_SEC   0x04
#define RTC_TIMEOUT   100  /* ms */

static const char *TAG = "rtc";
static i2c_master_bus_handle_t s_bus;
static i2c_master_dev_handle_t s_dev;

static esp_err_t read_regs(uint8_t reg, uint8_t *buf, size_t len)
{
    return i2c_master_transmit_receive(s_dev, &reg, 1, buf, len, RTC_TIMEOUT);
}

static esp_err_t write_regs(uint8_t reg, const uint8_t *buf, size_t len)
{
    uint8_t out[1 + PCF_REG_COUNT];
    if (len > PCF_REG_COUNT) {
        return ESP_ERR_INVALID_ARG;
    }
    out[0] = reg;
    memcpy(out + 1, buf, len);
    return i2c_master_transmit(s_dev, out, 1 + len, RTC_TIMEOUT);
}

static esp_err_t open_bus(void)
{
    if (s_dev) {
        return ESP_OK;
    }
    const i2c_master_bus_config_t bus_cfg = {
        .i2c_port = I2C_NUM_0,
        .sda_io_num = FT_PIN_I2C_SDA,
        .scl_io_num = FT_PIN_I2C_SCL,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    esp_err_t err = i2c_new_master_bus(&bus_cfg, &s_bus);
    if (err != ESP_OK) {
        return err;
    }
    const i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = RTC_ADDR,
        .scl_speed_hz = 100000,
    };
    return i2c_master_bus_add_device(s_bus, &dev_cfg, &s_dev);
}

static void set_system_time(time_t t)
{
    const struct timeval tv = {.tv_sec = t, .tv_usec = 0};
    settimeofday(&tv, NULL);
}

esp_err_t pcf_init(void)
{
    esp_err_t err = open_bus();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "i2c setup failed: %s", esp_err_to_name(err));
        return err;
    }
    uint8_t regs[PCF_REG_COUNT];
    err = read_regs(RTC_REG_SEC, regs, sizeof(regs));
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "PCF85063 did not answer: %s", esp_err_to_name(err));
        return err;
    }
    time_t t;
    if (!pcf_regs_decode(regs, &t)) {
        ESP_LOGW(TAG, "clock lost its time; it needs to be set");
        return ESP_ERR_INVALID_STATE;
    }
    set_system_time(t);
    ESP_LOGI(TAG, "system time set from RTC: %lld", (long long)t);
    return ESP_OK;
}

esp_err_t pcf_set(time_t t)
{
    esp_err_t err = open_bus();
    if (err == ESP_OK) {
        /* 24 h mode, running: clear 12_24 (bit 1) and STOP (bit 5), keep the other bits. */
        uint8_t ctrl1;
        err = read_regs(RTC_REG_CTRL1, &ctrl1, 1);
        if (err == ESP_OK) {
            ctrl1 &= (uint8_t) ~((1 << 1) | (1 << 5));
            err = write_regs(RTC_REG_CTRL1, &ctrl1, 1);
        }
    }
    if (err == ESP_OK) {
        uint8_t regs[PCF_REG_COUNT];
        pcf_regs_encode(t, regs);
        err = write_regs(RTC_REG_SEC, regs, sizeof(regs));
    }
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "could not write the RTC: %s", esp_err_to_name(err));
    }
    set_system_time(t);  /* the running session uses the new time even if the chip failed */
    return err;
}
