
#include <esp_ota_ops.h>

namespace toit_api {
class ToitApiMessageSender {
public:
    virtual bool send_message_to_vm(int target, int type, uint8 *data, int length) = 0;
};
}
