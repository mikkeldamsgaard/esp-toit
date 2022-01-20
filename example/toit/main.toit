// Copyright (C) 2021 Toitware ApS.
// Use of this source code is governed by a Zero-Clause BSD license that can
// be found in the examples/LICENSE file.

import monitor

TYPE_BASE ::= 100
TYPE_QUIT ::= TYPE_BASE
TYPE_STATUS ::= TYPE_QUIT + 1
TYPE_STREAM_START ::= TYPE_STATUS + 1

main:
  handler := Handler

  print "[TOIT] Initiating ping pong"
  res := process_send_ 0 TYPE_STATUS "pasfisk".to_byte_array
  print "[TOIT] Sent off status message, with res $res"
  handler.wait
  print "Done"


class Handler implements SystemMessageHandler_:
  latch_ := monitor.Latch

  constructor:
    set_system_message_handler_ TYPE_QUIT this
    set_system_message_handler_ TYPE_STATUS this
    5.repeat:
      set_system_message_handler_ TYPE_STREAM_START + it this

  on_message type gid pid message -> none:
    assert: type >= TYPE_BASE and type < TYPE_STREAM_START + 5
    print "[TOIT] got message $type from $pid: $(message.to_string)"
    if type == TYPE_QUIT:
        latch_.set null
        return

    res := process_send_ 0 type "pong".to_byte_array
    print "[TOIT] replied with pong, res $res"

  wait:
    latch_.get