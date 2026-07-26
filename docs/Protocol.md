# Protocol

Wire frame: `A5 5A | version:u8 | message_id:u8 | sequence:u16 LE | payload_length:u16 LE | payload | crc16:u16 LE`.

CRC covers version through the final payload byte. Direct responses copy the request sequence. Telemetry uses an independent frame sequence and a `uint32` sample counter.
