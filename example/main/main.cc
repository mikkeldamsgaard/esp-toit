#include <freertos/FreeRTOS.h>
#include <freertos/portmacro.h>
#include <freertos/task.h>
#include <esp_log.h>
#include "toit_api.h"

const char *MODULE = "MAIN";

class SenderThread: public toit::Thread {
public:
    SenderThread(const char *name, toit_api::Stream *stream): Thread(name), stream(stream) {}

    void entry() override {
        while (true) {
            sleep(1);
            if (!running) return;

            auto *msg = reinterpret_cast<uint8*>(malloc(4));
            memcpy(msg, "ping", 4);
            if (stream->send(msg, 4)) {
                printf("[C] ping message send successfully\n");
            } else {
                printf("[C] message send failed\n");
                free(msg);
            };
        }
    }

    void close() { running = false; printf("Closing sender\n"); }

    bool running = true;
    toit_api::Stream *stream;
};


class Receiver: public toit_api::StreamReceiver {

    bool receive(toit_api::Stream *stream, const uint8 *data, int length) override {
        char txt[length+1]; // This will stack overflow if the data is too big.
        memcpy(txt,data, length);
        txt[length] = 0;
        printf("[C] Received text: %s\n",txt);
        return true;
    }
};

extern "C" {
void app_main() {
    ESP_LOGI(MODULE, "Starting");
    toit_api::ToitApi::set_up();

    toit_api::ToitApi api(reinterpret_cast<toit::Program *>(&toit_image), 1);
    Receiver receiver;
    toit_api::Stream *stream = api.register_stream(0, &receiver);

//    BaseType_t res = xTaskCreate(cmnd_loop_task, "cmnd_loop", 6500, NULL, 2, NULL);
    SenderThread sender("sender_t", stream);

    sender.spawn();
    ESP_LOGI(MODULE, "ToitApi created, running");
    api.run();

    sender.close();
    printf("Joining sender\n");
    sender.join();
    printf("Exit\n");
}
}


