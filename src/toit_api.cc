#include "toit_api.h"
#include "os.h"
#include "vm.h"
#include "top.h"
#include "flash_registry.h"
#include "scheduler.h"
#include "rtc_memory_esp32.h"

namespace toit_api {
using namespace toit;

extern "C" uword toit_image;
extern "C" uword toit_image_size;

static uint8 uuid[] = {0x4d, 0xb6, 0xd0, 0x03, 0xd4, 0xc0, 0x44, 0x05, 0xb6, 0x64, 0xdc, 0xb3, 0xf3, 0x80, 0xe8, 0x12};

class ReceiverThread : public Thread {
public:
    ReceiverThread(Peer *peer, Mutex *mutex, ConditionVariable *conditionVariable, ToitApi *api) :
            Thread("api_receiver"), _peer(peer), _conditionVariable(conditionVariable), _mutex(mutex), _api(api) {
        _peer = peer;
        _conditionVariable = conditionVariable;
        _mutex = mutex;
    };

    void stop() {
        Locker locker(_mutex);
        _running = false;
        OS::signal_all(_conditionVariable);
    }
protected:
    void entry() override {
        auto event_source = InterProcessMessageEventSource::instance();
        while (true) {
            {
                Locker locker(_mutex);
                OS::wait(_conditionVariable, 10000);
            }
            if (!_running) return;

            while (event_source->has_frame(_peer)) {
                int stream_id = event_source->read_stream_id(_peer);
                int length = event_source->read_length(_peer);
                uint8 *data =event_source->read_bytes(_peer);

                _api->dispatch_message_from_vm(stream_id, data, length);
                InterProcessMessageEventSource::instance()->skip_frame(_peer);
            }
        }
    }

private:
    Peer *_peer;
    ConditionVariable *_conditionVariable;
    Mutex *_mutex;
    bool _running = true;
    ToitApi *_api;
};

class ResourceGroupForInterop : public ResourceGroup {
public:
    ResourceGroupForInterop(Process *process, Mutex *mutex, ConditionVariable *conditionVariable) :
            ResourceGroup(process, InterProcessMessageEventSource::instance()),
            _conditionVariable(conditionVariable),
            _mutex(mutex) {}

protected:
    uint32_t on_event(Resource *resource, word data, uint32_t state) override {
        OS::signal_all(_conditionVariable);
        return 0;
    }

private:
    ConditionVariable *_conditionVariable;
    Mutex *_mutex;
};

void ToitApi::set_up() {
    RtcMemory::set_up();
    OS::set_up();
    FlashRegistry::set_up();
}

class ToitApiInternals {
    ToitApiInternals(Program *program, uint8 num_streams, uint32 max_receive_wait_time_ms):
    program(program), num_streams(num_streams),max_receive_wait_time_ms(max_receive_wait_time_ms),streams(_new Stream*[num_streams]) {
        memset(streams, 0, sizeof(void *)*num_streams);
        vm.load_platform_event_sources();

        Block *initial_block = vm.heap_memory()->allocate_initial_block();
        int interop_group_id = vm.scheduler()->next_group_id();
        interop_process_group = ProcessGroup::create(interop_group_id);
        interop_process= _new Process(program, interop_process_group, null, initial_block);

        InterProcessMessageEventSource *event_source = InterProcessMessageEventSource::instance();

        mutex = event_source->mutex();
        condition = OS::allocate_condition_variable(mutex);

        // Setup interop process, to enable RPC communication into the VM
        interop_resource_group = _new ResourceGroupForInterop(interop_process, mutex, condition);

        peer = _new Peer(interop_resource_group);

        channel = Channel::create(uuid);
        event_source->attach(peer, channel);
        event_source->add_pending_channel(channel);
    }

    ~ToitApiInternals() {
        for (int i = 0; i < num_streams; i++) {
            if (streams[i] != null) delete streams[i];
        }
        delete streams;

        delete channel;
        interop_resource_group->tear_down();

        free(condition);
        delete interop_process;
        delete interop_process_group;
    }

    Scheduler::ExitState run(ToitApi *api) {
        ReceiverThread receiverThread(peer, mutex, condition, api);
        receiverThread.spawn();

        int boot_group_id = vm.scheduler()->next_group_id();
        auto result = vm.scheduler()->run_boot_program(const_cast<Program *>(program), null, boot_group_id);
        receiverThread.stop();
        receiverThread.join();
        return result;
    }

    toit_api::Stream *register_stream(int stream_id, StreamReceiver *receiver, ToitApi *toit_api) {
        ASSERT(streams[stream_id] == null && stream_id < num_streams)
        streams[stream_id] = _new toit_api::Stream(stream_id, receiver, toit_api);
        return streams[stream_id];
    }

    void dispatch_message_from_vm(int stream_id, uint8 *data, int length) {
        if (stream_id < 0 || stream_id > num_streams) return; // ignore invalid stream ids

        Stream *stream = streams[stream_id];

        if (stream == null) return; // ignore unregistered streams

        stream->receiver()->receive(stream, data, length);
    }

    // A terrible amount of objects is needed to trick the VM into talking with us.
    // Hidden away to keep users of the library sane
    VM vm;
    Program *program;
    const int num_streams;
    const uint32 max_receive_wait_time_ms;
    Stream **streams;
    Peer *peer;

    ProcessGroup *interop_process_group;
    Process *interop_process;
    Channel *channel;
    ResourceGroupForInterop *interop_resource_group;
    Mutex* mutex;
    ConditionVariable *condition;

    friend ToitApi;
    friend Stream;
};

ToitApi::ToitApi(Program *program, uint8 num_streams, uint32 max_receive_wait_time_ms) {
    internals = _new ToitApiInternals(program, num_streams, max_receive_wait_time_ms);
}

ToitApi::~ToitApi() noexcept { delete internals; }

Scheduler::ExitState ToitApi::run() {
    return internals->run(this);
}

toit_api::Stream *ToitApi::register_stream(int stream_id, StreamReceiver *receiver) {
    return internals->register_stream(stream_id, receiver, this);
}

void ToitApi::dispatch_message_from_vm(int stream_id, uint8 *data, int length) {
    return internals->dispatch_message_from_vm(stream_id, data, length);
}

bool Stream::send(uint8 *buf, int length) {
    return InterProcessMessageEventSource::instance()->send(_toit_api->internals->peer, id(), 8, length, buf);
}




}