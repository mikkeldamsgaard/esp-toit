#include <multi_heap.h>
#include <esp_heap_caps.h>
#include "toit_api.h"
#include "os.h"
#include "vm.h"
#include "top.h"
#include "flash_registry.h"
#include "scheduler.h"
#include "rtc_memory_esp32.h"
#include "toit_internal.h"
namespace toit_api {
using namespace toit;

const int TYPE_BASE = 100;
const int TYPE_STREAM_START = 200;

enum {
    TYPE_QUIT = TYPE_BASE,
    TYPE_STATUS,
};

class ToitApiMessageHandler : public ExternalSystemMessageHandler {
public:
    explicit ToitApiMessageHandler(VM *vm) : ExternalSystemMessageHandler(vm) {
    }
    virtual ~ToitApiMessageHandler() = default;;

    void on_message(int sender, int type, void *data, int length) override;
};


class ToitApiInternals : ToitApiMessageSender {
    ToitApiInternals(Program *program, uint8 num_streams) :
            _program(program), _num_streams(num_streams),
            _streams(_new Stream *[num_streams]), _sender(-1) {
        memset(_streams, 0, sizeof(void *) * num_streams);
        _vm.load_platform_event_sources();
        _boot_group_id = _vm.scheduler()->next_group_id();

        _message_handler = _new ToitApiMessageHandler(&_vm);
    }

    virtual ~ToitApiInternals() {
        for (int i = 0; i < _num_streams; i++) {
            if (_streams[i] != null) delete _streams[i];
        }
        delete _message_handler;
        delete _streams;
    }

    Scheduler::ExitState run() {
        _message_handler->start();
        auto result = _vm.scheduler()->run_boot_program(const_cast<Program *>(_program), null, _boot_group_id);
        return result;
    }

    toit_api::Stream *register_stream(int stream_id, StreamReceiver *receiver, ToitApi *api) {
        ASSERT(_streams[stream_id] == null && stream_id < _num_streams)
        _streams[stream_id] = _new toit_api::Stream(stream_id, receiver);
        return _streams[stream_id];
    }

    void dispatch_message_from_vm(int sender, int type, void *data, int length) {
        _sender = sender;

        switch (type) {
            case TYPE_QUIT: break;
            case TYPE_STATUS: break; // TODO(mikkel) Implement some status exchange
            default:
                int stream_id = type - TYPE_STREAM_START;

                if (stream_id < 0 || stream_id >= _num_streams) return; // invalid type/stream_id

                Stream *stream = _streams[stream_id];

                if (stream == null) return; // ignore unregistered streams

                stream->receiver()->receive(stream, static_cast<const uint8 *>(data), length);
        }
    }

    bool send_message_to_vm(int type, uint8 *data, int length) {
        if (_sender == -1) {
            printf("[toit_api] No sender registered, not sending message\n");
            return false;
        }
        return _message_handler->send(_sender, type, data, length);
    }

    void request_stop() {
        int length = 1;
        uint8* dummy = unvoid_cast<uint8*>(malloc(length));
        if (!_message_handler->send(_sender, TYPE_QUIT, dummy, 0)) {
            free(dummy);
        }
    }

    VM _vm;
    Program *_program;
    const int _num_streams;
    Stream **_streams;
    ToitApiMessageHandler *_message_handler;
    int _sender;
    int _boot_group_id;

    friend ToitApi;
    friend Stream;
    friend ToitApiMessageHandler;

};


void ToitApiMessageHandler::on_message(int sender, int type, void *data, int length) {
    // printf("[toit_api] Received message with type %d, from sender %d and length %d\n", type, sender, length);
    ToitApi::instance()->_internals->dispatch_message_from_vm(sender, type, data, length);
}

ToitApi *ToitApi::_instance = null;

void ToitApi::set_up() {
    RtcMemory::set_up();
    OS::set_up();
    FlashRegistry::set_up();
}

ToitApi::ToitApi(Program *program, uint8 num_streams) {
    ASSERT(_instance == null)
    _instance = this;
  _internals = _new ToitApiInternals(program, num_streams);
}

ToitApi::~ToitApi() noexcept {
    ASSERT(_instance != null)
    delete _internals;
    _instance = null;
}

Scheduler::ExitState ToitApi::run() {
    return _internals->run();
}

Stream *ToitApi::register_stream(uint16 stream_id, StreamReceiver *receiver) {
    return _internals->register_stream(stream_id, receiver, this);
}

void ToitApi::request_stop() {
    _internals->request_stop();
}

bool Stream::send(uint8 *buf, int length) {
    return ToitApi::instance()->_internals->send_message_to_vm(TYPE_STREAM_START+id(), buf, length);
}


}