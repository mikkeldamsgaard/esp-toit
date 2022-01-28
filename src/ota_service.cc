#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "toit_api.h"
#include "toit_internal.h"

namespace toit_api {

enum {
    OTA_START,
    OTA_WRITE,
    OTA_COMPLETE,
    OTA_ABORT,
    OTA_SUCCESS,

    OTA_ERROR = 0xFF
};

enum {
    OTA_ERR_UNKNOWN_MESSAGE_TYPE,
    OTA_ERR_ALREADY_RUNNING,
    OTA_NO_OTA_PARTITION_FOUND,
    OTA_ESP32_API_CALL_FAILED,
    OTA_ERR_OTA_NOT_STARTED
};

void OtaService::handle_message(void *data, int length) {
    auto *u8data = reinterpret_cast<uint8 *>(data);
    switch (*u8data) {
        case OTA_START:
            start_ota();
            break;
        case OTA_WRITE:
            write_ota_data(u8data, length);
            break;
        case OTA_COMPLETE:
            complete_ota(u8data[1]);
            break;
        case OTA_ABORT:
            abort_ota();
            break;
        default:
            send_error(OTA_ERR_UNKNOWN_MESSAGE_TYPE, u8data, 1);
    }
}

void OtaService::start_ota() {
    if (_running) {
        send_error(OTA_ERR_ALREADY_RUNNING);
        return;
    }

    _update_partition = esp_ota_get_next_update_partition(NULL);
    if (!_update_partition) {
        send_error(OTA_NO_OTA_PARTITION_FOUND);
        return;
    }

    esp_err_t res = esp_ota_begin(_update_partition, 0, &_handle);
    if (res != ESP_OK) {
        send_error(OTA_ESP32_API_CALL_FAILED, (uint8 *)&res, sizeof (int));
        return;
    }

    _running = true;
    send_success(OTA_START);
}

void OtaService::write_ota_data(uint8 *data, int length) {
    if (!_running) {
        send_error(OTA_ERR_OTA_NOT_STARTED);
        return;
    }

    esp_err_t res = esp_ota_write(_handle, data, length);
    if (res != ESP_OK) {
        send_error(OTA_ESP32_API_CALL_FAILED, (uint8 *)&res, sizeof (int));
        return;
    }

    send_success(OTA_WRITE);
}

void OtaService::complete_ota(int delay) {
    if (!_running) {
        send_error(OTA_ERR_OTA_NOT_STARTED);
        return;
    }

    esp_err_t res = esp_ota_end(_handle);
    if (res != ESP_OK) {
        send_error(OTA_ESP32_API_CALL_FAILED, (uint8 *)&res, sizeof (int));
        return;
    }

    res = esp_ota_set_boot_partition(_update_partition);
    if (res != ESP_OK) {
        send_error(OTA_ESP32_API_CALL_FAILED, (uint8 *)&res, sizeof (int));
        return;
    }

    vTaskDelay(delay*1000/ portTICK_PERIOD_MS);
    esp_restart();
    //_running = false;
}


void OtaService::abort_ota() {
    if (!_running) {
        send_error(OTA_ERR_OTA_NOT_STARTED);
        return;
    }

    esp_err_t res = esp_ota_abort(_handle);
    if (res != ESP_OK) {
        send_error(OTA_ESP32_API_CALL_FAILED, (uint8 *)&res, sizeof (int));
        return;
    }

    _running = false;
    send_success(OTA_ABORT);
}

void OtaService::send_error(uint8 error) {
    send_error(error, nullptr, 0);
}

void OtaService::send_error(uint8 error, const uint8 *data, int length) {
    auto buf = static_cast<uint8 *>(malloc(2+length));
    buf[0] = OTA_ERROR;
    buf[1] = error;
    if (length) memcpy(buf+2, data,length);
    if (!_sender.send_message_to_vm(_type, buf,2+length)) {
        ESP_LOGE("OtaService", "Send error failed!");
        free(buf);
    }
}

void OtaService::send_success(uint8 cmd) {
    auto buf = static_cast<uint8 *>(malloc(2));
    buf[0] = OTA_SUCCESS;
    buf[1] = cmd;
    if (!_sender.send_message_to_vm(_type, buf,2)) {
        ESP_LOGE("OtaService", "Send success failed!");
        free(buf);
    }
}

}