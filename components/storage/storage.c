#include <stdbool.h>
#include <stdio.h>

#include "driver/sdmmc_host.h"
#include "esp_log.h"
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include "wear_levelling.h"

#include "board.h"
#include "storage.h"

static const char *TAG = "storage";
static bool s_on_sd;
static wl_handle_t s_wl = WL_INVALID_HANDLE;

static esp_err_t mount_sd(void)
{
    if (FT_SD_PIN_CLK < 0 || FT_SD_PIN_CMD < 0 || FT_SD_PIN_D0 < 0) {
        return ESP_ERR_NOT_SUPPORTED;
    }

    const esp_vfs_fat_mount_config_t mount_cfg = {
        .format_if_mount_failed = false,
        .max_files = 5,
        .allocation_unit_size = 16 * 1024,
    };
    sdmmc_host_t host = SDMMC_HOST_DEFAULT();
    host.flags = SDMMC_HOST_FLAG_1BIT;
    sdmmc_slot_config_t slot = SDMMC_SLOT_CONFIG_DEFAULT();
    slot.width = 1;
    slot.clk = FT_SD_PIN_CLK;
    slot.cmd = FT_SD_PIN_CMD;
    slot.d0 = FT_SD_PIN_D0;
    slot.flags |= SDMMC_SLOT_FLAG_INTERNAL_PULLUP;

    sdmmc_card_t *card = NULL;
    const esp_err_t err = esp_vfs_fat_sdmmc_mount(STORAGE_MOUNT_POINT, &host, &slot, &mount_cfg, &card);
    if (err == ESP_OK) {
        sdmmc_card_print_info(stdout, card);
    }
    return err;
}

static esp_err_t mount_flash(void)
{
    const esp_vfs_fat_mount_config_t mount_cfg = {
        .format_if_mount_failed = true,
        .max_files = 5,
        .allocation_unit_size = 4096,
    };
    return esp_vfs_fat_spiflash_mount_rw_wl(STORAGE_MOUNT_POINT, "storage", &mount_cfg, &s_wl);
}

esp_err_t storage_mount(void)
{
    esp_err_t err = mount_sd();
    if (err == ESP_OK) {
        s_on_sd = true;
        ESP_LOGI(TAG, "microSD mounted at %s", STORAGE_MOUNT_POINT);
        return ESP_OK;
    }
    if (err != ESP_ERR_NOT_SUPPORTED) {
        ESP_LOGW(TAG, "microSD mount failed: %s; using internal flash", esp_err_to_name(err));
    } else {
        ESP_LOGW(TAG, "SD pins not set in board.h; using internal flash");
    }

    err = mount_flash();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "flash mount failed: %s", esp_err_to_name(err));
        return err;
    }
    ESP_LOGI(TAG, "internal flash mounted at %s", STORAGE_MOUNT_POINT);
    return ESP_OK;
}

bool storage_on_sd(void)
{
    return s_on_sd;
}
