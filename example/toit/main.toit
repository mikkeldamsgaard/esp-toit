// Copyright (C) 2021 Toitware ApS.
// Use of this source code is governed by a Zero-Clause BSD license that can
// be found in the examples/LICENSE file.

import uuid
import rpc_transport show Channel_

main:
  id ::= uuid.parse "4db6d003-d4c0-4405-b664-dcb3f380e812"
  native_channel ::= Channel_.open id

  print "[TOIT] Initiating ping pong"
  while true:
    f ::= native_channel.receive
    if f != null:
       print "[TOIT] Received non null frame on stream $(f.stream_id), content=$(f.bytes.to_string)"
       native_channel.send 0 8 "pong".to_byte_array
    
  print "Done"