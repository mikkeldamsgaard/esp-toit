
#include <esp_ota_ops.h>

namespace toit_api {
class ToitApiMessageSender {
public:
    virtual bool send_message_to_vm(int type, uint8 *data, int length) = 0;
};

class OtaService {
public:
    OtaService(ToitApiMessageSender &sender, int type): _sender(sender), _handle(0), _running(false), _type(type) {}
    void handle_message(void *data, int length);

private:
    void start_ota();
    void write_ota_data(uint8 *data, int length);
    void complete_ota(int delay);
    void abort_ota();
    void send_error(uint8 error, const uint8 *data, int length);
    void send_error(uint8 error);
    void send_success(uint8 cmd);


    const esp_partition_t* _update_partition;
    ToitApiMessageSender &_sender;
    esp_ota_handle_t _handle;
    bool _running;
    int _type;
};
}
