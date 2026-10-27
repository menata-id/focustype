/*
 * ST7305 reflective LCD driver (Waveshare 400x300), ESP-IDF.
 *
 * The init sequence and address windows come from the Waveshare reference driver as ported in
 * kylehase/ESPHome-ST7305-RLCD (Apache-2.0); the command semantics are from the Sitronix
 * ST7305 datasheet v0.2.
 */

#include <string.h>
#include <stdint.h>

#include "esp_check.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "st7305.h"

static const char *TAG = "st7305";

#define CMD_SLPIN   0x10
#define CMD_SLPOUT  0x11
#define CMD_DISPON  0x29
#define CMD_CASET   0x2A
#define CMD_RASET   0x2B
#define CMD_RAMWR   0x2C
#define CMD_HPM     0x38
#define CMD_LPM     0x39

/* Address window for the full 400x300 panel */
#define COL_START   0x12
#define COL_END     0x2A
#define ROW_START   0x00
#define ROW_END     0xC7

typedef struct {
    uint8_t cmd;
    uint8_t n;
    uint8_t p[10];
    uint16_t delay_ms;
} init_step_t;

static const init_step_t k_init[] = {
    {0xD6, 2, {0x17, 0x02}, 0},                                                  /* NVM load */
    {0xD1, 1, {0x01}, 0},                                                        /* booster on */
    {0xC0, 2, {0x11, 0x04}, 0},                                                  /* gate voltage */
    {0xC1, 4, {0x69, 0x69, 0x69, 0x69}, 0},                                      /* VSHP */
    {0xC2, 4, {0x19, 0x19, 0x19, 0x19}, 0},                                      /* VSLP */
    {0xC4, 4, {0x4B, 0x4B, 0x4B, 0x4B}, 0},                                      /* VSHN */
    {0xC5, 4, {0x19, 0x19, 0x19, 0x19}, 0},                                      /* VSLN */
    {0xD8, 2, {0x80, 0xE9}, 0},                                                  /* oscillator */
    {0xB2, 1, {0x02}, 0},                                                        /* frame rate */
    {0xB3, 10, {0xE5, 0xF6, 0x05, 0x46, 0x77, 0x77, 0x77, 0x77, 0x76, 0x45}, 0}, /* gate EQ, HPM */
    {0xB4, 8, {0x05, 0x46, 0x77, 0x77, 0x77, 0x77, 0x76, 0x45}, 0},              /* gate EQ, LPM */
    {0x62, 3, {0x32, 0x03, 0x1F}, 0},                                            /* gate timing */
    {0xB7, 1, {0x13}, 0},                                                        /* source EQ */
    {0xB0, 1, {0x64}, 0},                                                        /* gate lines (300) */
    {CMD_SLPOUT, 0, {0}, 200},
    {0xC9, 1, {0x00}, 0},                                                        /* source voltage select */
    {0x36, 1, {0x48}, 0},                                                        /* MADCTL: MX=1, DO=1 */
    {0x3A, 1, {0x11}, 0},                                                        /* data format */
    {0xB9, 1, {0x20}, 0},                                                        /* gamma: mono */
    {0xB8, 1, {0x29}, 0},                                                        /* panel setting */
    {0x21, 0, {0}, 0},                                                           /* inversion on */
    {CMD_CASET, 2, {COL_START, COL_END}, 0},
    {CMD_RASET, 2, {ROW_START, ROW_END}, 0},
    {0x35, 1, {0x00}, 0},                                                        /* TE on (pin unused) */
    {0xD0, 1, {0xFF}, 0},                                                        /* auto power down */
    {CMD_HPM, 0, {0}, 0},
    {CMD_DISPON, 0, {0}, 0},
};

static spi_device_handle_t s_dev;
static gpio_num_t s_dc;
static uint8_t *s_panel_buf;

static void IRAM_ATTR pre_cb(spi_transaction_t *t)
{
    gpio_set_level(s_dc, (uint32_t)(uintptr_t)t->user);
}

static esp_err_t tx(const void *data, size_t len, bool is_data, bool keep_cs)
{
    spi_transaction_t t = {
        .length = len * 8,
        .user = (void *)(uintptr_t)(is_data ? 1 : 0),
        .flags = keep_cs ? SPI_TRANS_CS_KEEP_ACTIVE : 0,
    };
    if (len <= 4) {
        memcpy(t.tx_data, data, len);
        t.flags |= SPI_TRANS_USE_TXDATA;
    } else {
        t.tx_buffer = data;
    }
    return spi_device_transmit(s_dev, &t);
}

static esp_err_t send_cmd(uint8_t cmd)
{
    return tx(&cmd, 1, false, false);
}

static esp_err_t send_param(uint8_t v)
{
    return tx(&v, 1, true, false);
}

static esp_err_t send_cmd_params(uint8_t cmd, const uint8_t *p, size_t n)
{
    ESP_RETURN_ON_ERROR(send_cmd(cmd), TAG, "cmd 0x%02X", cmd);
    /* One byte per transaction, as the reference driver does (CS toggles per byte). */
    for (size_t i = 0; i < n; i++) {
        ESP_RETURN_ON_ERROR(send_param(p[i]), TAG, "param %u of cmd 0x%02X", (unsigned)i, cmd);
    }
    return ESP_OK;
}

esp_err_t st7305_init(const st7305_config_t *cfg)
{
    ESP_RETURN_ON_FALSE(cfg, ESP_ERR_INVALID_ARG, TAG, "cfg");
    s_dc = cfg->dc;

    const gpio_config_t out = {
        .pin_bit_mask = (1ULL << cfg->dc) | (1ULL << cfg->rst),
        .mode = GPIO_MODE_OUTPUT,
    };
    ESP_RETURN_ON_ERROR(gpio_config(&out), TAG, "gpio");
    gpio_set_level(cfg->dc, 1);

    const spi_bus_config_t bus = {
        .mosi_io_num = cfg->mosi,
        .miso_io_num = -1,
        .sclk_io_num = cfg->sclk,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = ST7305_PANEL_BUF_SIZE + 8,
    };
    ESP_RETURN_ON_ERROR(spi_bus_initialize(cfg->host, &bus, SPI_DMA_CH_AUTO), TAG, "bus");

    const spi_device_interface_config_t dev = {
        .clock_speed_hz = cfg->clock_hz,
        .mode = 0,
        .spics_io_num = cfg->cs,
        .queue_size = 1,
        .pre_cb = pre_cb,
    };
    ESP_RETURN_ON_ERROR(spi_bus_add_device(cfg->host, &dev, &s_dev), TAG, "device");

    s_panel_buf = heap_caps_malloc(ST7305_PANEL_BUF_SIZE, MALLOC_CAP_DMA | MALLOC_CAP_8BIT);
    ESP_RETURN_ON_FALSE(s_panel_buf, ESP_ERR_NO_MEM, TAG, "panel buffer");

    /* Power-on flow from the datasheet: wait, hardware reset, init, then image. */
    vTaskDelay(pdMS_TO_TICKS(50));
    gpio_set_level(cfg->rst, 1);
    vTaskDelay(pdMS_TO_TICKS(50));
    gpio_set_level(cfg->rst, 0);
    vTaskDelay(pdMS_TO_TICKS(20));
    gpio_set_level(cfg->rst, 1);
    vTaskDelay(pdMS_TO_TICKS(50));

    for (size_t i = 0; i < sizeof(k_init) / sizeof(k_init[0]); i++) {
        const init_step_t *s = &k_init[i];
        ESP_RETURN_ON_ERROR(send_cmd_params(s->cmd, s->p, s->n), TAG, "init step %u", (unsigned)i);
        if (s->delay_ms) {
            vTaskDelay(pdMS_TO_TICKS(s->delay_ms));
        }
    }
    ESP_LOGI(TAG, "initialised (%dx%d)", ST7305_WIDTH, ST7305_HEIGHT);
    return ESP_OK;
}

esp_err_t st7305_flush(const uint8_t *fb)
{
    ESP_RETURN_ON_FALSE(s_panel_buf && fb, ESP_ERR_INVALID_STATE, TAG, "not ready");

    st7305_pack(fb, s_panel_buf);

    ESP_RETURN_ON_ERROR(send_cmd(CMD_HPM), TAG, "hpm");
    ESP_RETURN_ON_ERROR(send_cmd(CMD_DISPON), TAG, "dispon");
    const uint8_t col[2] = {COL_START, COL_END};
    const uint8_t row[2] = {ROW_START, ROW_END};
    ESP_RETURN_ON_ERROR(send_cmd_params(CMD_CASET, col, 2), TAG, "caset");
    ESP_RETURN_ON_ERROR(send_cmd_params(CMD_RASET, row, 2), TAG, "raset");

    /* CS must stay low from the RAMWR command through the whole data burst.
     * The driver only allows CS_KEEP_ACTIVE while the bus is acquired. */
    ESP_RETURN_ON_ERROR(spi_device_acquire_bus(s_dev, portMAX_DELAY), TAG, "acquire");
    const uint8_t ramwr = CMD_RAMWR;
    esp_err_t err = tx(&ramwr, 1, false, true);
    if (err == ESP_OK) {
        err = tx(s_panel_buf, ST7305_PANEL_BUF_SIZE, true, false);
    } else {
        ESP_LOGE(TAG, "ramwr: %s", esp_err_to_name(err));
    }
    spi_device_release_bus(s_dev);
    return err;
}

esp_err_t st7305_sleep(void)
{
    ESP_RETURN_ON_ERROR(send_cmd(CMD_SLPIN), TAG, "slpin");
    vTaskDelay(pdMS_TO_TICKS(100));
    return ESP_OK;
}

esp_err_t st7305_wake(void)
{
    ESP_RETURN_ON_ERROR(send_cmd(CMD_SLPOUT), TAG, "slpout");
    vTaskDelay(pdMS_TO_TICKS(120));
    return ESP_OK;
}

esp_err_t st7305_set_low_power(bool low)
{
    return send_cmd(low ? CMD_LPM : CMD_HPM);
}
