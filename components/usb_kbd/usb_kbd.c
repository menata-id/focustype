/*
 * USB host keyboard on the ESP32-S3 native USB port, built on the ESP-IDF usb_host component
 * and the espressif/usb_host_hid class driver (same flow as that component's hid_host example).
 *
 * The keyboard needs 5 V on VBUS. Whether the board's USB-C port can supply it is not
 * established yet; see README.md.
 */

#include <stdatomic.h>
#include <string.h>

#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "usb/usb_host.h"
#include "usb/hid_host.h"

#include "usb_kbd.h"

static const char *TAG = "usb_kbd";

#define REPEAT_DELAY_US   400000
#define REPEAT_PERIOD_US  35000

static QueueHandle_t s_queue;
static kbd_parser_t s_parser;
static atomic_int s_kbd_count;
static esp_timer_handle_t s_repeat_timer;
static int64_t s_repeat_next_us;

static void push(const kbd_event_t *ev)
{
    if (xQueueSend(s_queue, ev, 0) != pdTRUE) {
        ESP_LOGW(TAG, "event queue full, dropping key");
    }
}

static void push_simple(kbd_key_t key)
{
    kbd_event_t ev = {.key = key};
    push(&ev);
}

static void repeat_cb(void *arg)
{
    (void)arg;
    if (s_parser.held_usage == 0) {
        return;
    }
    const int64_t now = esp_timer_get_time();
    if (now < s_repeat_next_us) {
        return;
    }
    kbd_event_t ev;
    if (kbd_hid_translate(s_parser.held_usage, s_parser.held_mods, &ev)) {
        push(&ev);
    }
    s_repeat_next_us = now + REPEAT_PERIOD_US;
}

static void handle_report(const uint8_t *data, size_t len)
{
    kbd_event_t evs[6];
    const uint8_t before = s_parser.held_usage;
    const size_t n = kbd_hid_parse(&s_parser, data, len, evs, 6);
    for (size_t i = 0; i < n; i++) {
        push(&evs[i]);
    }
    /* A new key restarts the repeat delay; releasing everything stops repeating. */
    if (s_parser.held_usage != 0 && s_parser.held_usage != before) {
        s_repeat_next_us = esp_timer_get_time() + REPEAT_DELAY_US;
    }
}

static void hid_interface_cb(hid_host_device_handle_t dev, const hid_host_interface_event_t event,
                             void *arg)
{
    (void)arg;
    switch (event) {
    case HID_HOST_INTERFACE_EVENT_INPUT_REPORT: {
        uint8_t data[64];
        size_t len = 0;
        if (hid_host_device_get_raw_input_report_data(dev, data, sizeof(data), &len) == ESP_OK) {
            handle_report(data, len);
        }
        break;
    }
    case HID_HOST_INTERFACE_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "keyboard disconnected");
        hid_host_device_close(dev);
        memset(&s_parser, 0, sizeof(s_parser));
        if (atomic_fetch_sub(&s_kbd_count, 1) == 1) {
            push_simple(KBD_KEY_DISCONNECTED);
        }
        break;
    case HID_HOST_INTERFACE_EVENT_TRANSFER_ERROR:
        ESP_LOGW(TAG, "HID transfer error");
        break;
    default:
        break;
    }
}

static void hid_device_cb(hid_host_device_handle_t dev, const hid_host_driver_event_t event,
                          void *arg)
{
    (void)arg;
    if (event != HID_HOST_DRIVER_EVENT_CONNECTED) {
        return;
    }

    hid_host_dev_params_t params;
    if (hid_host_device_get_params(dev, &params) != ESP_OK) {
        return;
    }
    if (params.proto != HID_PROTOCOL_KEYBOARD) {
        ESP_LOGI(TAG, "ignoring HID interface (proto %d)", (int)params.proto);
        return;
    }

    const hid_host_device_config_t cfg = {
        .callback = hid_interface_cb,
        .callback_arg = NULL,
    };
    if (hid_host_device_open(dev, &cfg) != ESP_OK) {
        ESP_LOGE(TAG, "hid_host_device_open failed");
        return;
    }
    if (params.sub_class == HID_SUBCLASS_BOOT_INTERFACE) {
        hid_class_request_set_protocol(dev, HID_REPORT_PROTOCOL_BOOT);
        hid_class_request_set_idle(dev, 0, 0);
    }
    if (hid_host_device_start(dev) != ESP_OK) {
        ESP_LOGE(TAG, "hid_host_device_start failed");
        hid_host_device_close(dev);
        return;
    }

    ESP_LOGI(TAG, "keyboard connected");
    atomic_fetch_add(&s_kbd_count, 1);
    push_simple(KBD_KEY_CONNECTED);
}

static void usb_lib_task(void *arg)
{
    (void)arg;
    for (;;) {
        uint32_t flags = 0;
        usb_host_lib_handle_events(portMAX_DELAY, &flags);
        if (flags & USB_HOST_LIB_EVENT_FLAGS_NO_CLIENTS) {
            usb_host_device_free_all();
        }
    }
}

esp_err_t usb_kbd_start(void)
{
    s_queue = xQueueCreate(32, sizeof(kbd_event_t));
    if (!s_queue) {
        return ESP_ERR_NO_MEM;
    }

    const usb_host_config_t host_cfg = {
        .skip_phy_setup = false,
        .intr_flags = ESP_INTR_FLAG_LEVEL1,
    };
    esp_err_t err = usb_host_install(&host_cfg);
    if (err != ESP_OK) {
        return err;
    }
    xTaskCreate(usb_lib_task, "usb_lib", 4096, NULL, 2, NULL);

    const hid_host_driver_config_t hid_cfg = {
        .create_background_task = true,
        .task_priority = 5,
        .stack_size = 4096,
        .core_id = 0,
        .callback = hid_device_cb,
        .callback_arg = NULL,
    };
    err = hid_host_install(&hid_cfg);
    if (err != ESP_OK) {
        return err;
    }

    const esp_timer_create_args_t t = {.callback = repeat_cb, .name = "kbd_repeat"};
    err = esp_timer_create(&t, &s_repeat_timer);
    if (err != ESP_OK) {
        return err;
    }
    return esp_timer_start_periodic(s_repeat_timer, 10000);
}

bool usb_kbd_next(kbd_event_t *ev, TickType_t timeout)
{
    if (!s_queue) {
        /* USB host not started: behave like a timeout so callers can poll without spinning. */
        vTaskDelay(timeout);
        return false;
    }
    return xQueueReceive(s_queue, ev, timeout) == pdTRUE;
}
