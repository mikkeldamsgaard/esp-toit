#pragma once

#include "scheduler.h"
#include "event_sources/rpc_transport.h"

namespace toit_api {
using namespace toit;

class Stream;

class StreamReceiver {
public:
    virtual bool receive(Stream *stream, uint8 *data, int length) = 0;

    friend toit_api::Stream;
};

class ToitApi;

class Stream {
public:
    Stream(int id, StreamReceiver *receiver, ToitApi *toit_api) : _id(id), _receiver(receiver), _toit_api(toit_api) {}

    // Send the byte buffer data to the toit VM.
    //
    // On success this transfers ownership, as the VM needs to be able to
    // manage the memory after the buffer has been delivered.
    //
    // In case of error the ownership of the buffer is returned to the caller.
    bool send(uint8 *buf, int length);

    int id() { return _id; }
    StreamReceiver *receiver() { return _receiver; }

private:
    const int _id;
    StreamReceiver *_receiver;
    ToitApi *_toit_api;
};

class ToitApiInternals;
class ReceiverThread;

class ToitApi {
public:
    ToitApi(Program *program, uint8 num_streams, uint32 max_receive_wait_time_ms = 1000);

    ~ToitApi();

    // Allocate system resources
    static void set_up();

    // This call blocks until the toit program exists.
    Scheduler::ExitState run();

    // Registers a stream for communicating with the toit vm
    // stream_id between 0 and num_streams - 1.
    Stream *register_stream(int stream_id, StreamReceiver *receiver);

private:
    void dispatch_message_from_vm(int stream_id, uint8* data, int length);

    ToitApiInternals *internals;
    friend Stream;
    friend ReceiverThread;
};
}