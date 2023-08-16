#pragma once

#include "scheduler.h"

namespace toit_api {
using namespace toit;

class Stream;

class StreamReceiver {
public:
    virtual bool receive(Stream *stream, const uint8 *data, int length) = 0;

    friend toit_api::Stream;
};

class ToitApi;

class Stream {
public:
    Stream(uint16 id, StreamReceiver *receiver) : _id(id), _receiver(receiver) {}

    // Send the byte buffer data to the toit VM.
    //
    // On success this transfers ownership, as the VM needs to be able to
    // manage the memory after the buffer has been delivered.
    //
    // In case of error the ownership of the buffer is returned to the caller.
    bool send(uint8 *buf, int length);

    uint16 id() const { return _id; }
    StreamReceiver *receiver() { return _receiver; }

private:
    const uint16 _id;
    StreamReceiver *_receiver;
};

class ToitApiInternals;
class ToitApiMessageHandler;

class ToitApi {
public:
    ToitApi(uint8 num_streams);

    ~ToitApi();

    // Allocate system resources
    static void set_up();

    // This call blocks until the toit program exists.
    Scheduler::ExitState run(uint8 priority=Process::PRIORITY_NORMAL);

    // Registers a stream for communicating with the toit vm
    // stream_id between 0 and num_streams - 1.
    Stream *register_stream(uint16 stream_id, StreamReceiver *receiver);

    // Requests that the message loop terminates on the toit side
    void request_stop();

    // Tries to allocate memory up to 3 times, and if malloc fails, runs a gc.
    void *safe_malloc(int size, int caps);

    static ToitApi *instance() { return _instance; }


private:
    void dispatch_message_from_vm(uint16 stream_id, uint8* data, int length);

    ToitApiInternals *_internals;
    static ToitApi *_instance;

    friend Stream;
    friend ToitApiMessageHandler;
};
}