#pragma once

#include <stdbool.h>
#include "esp_err.h"

#define STORAGE_MOUNT_POINT "/data"

/* Mounts the filesystem the notes live on at STORAGE_MOUNT_POINT.
 *
 * Tries the microSD card first (SDMMC, 1-bit). While the SD pins in board.h are unset, or if the
 * card cannot be mounted, it falls back to the internal "storage" flash partition so the app can
 * still run. storage_on_sd() tells which one is in use. */
esp_err_t storage_mount(void);
bool storage_on_sd(void);
