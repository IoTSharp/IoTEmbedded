REM SERIAL smoke - requires loopback or mocked serial port 2
PRINT "SERIAL: start"

LET port = SERIAL_OPEN(2, "SERIAL")
PRINT "SERIAL: open="; TYPE(port)
PRINT "SERIAL: baud="; SERIAL_BAUD(port)
PRINT "SERIAL: set_baud="; SERIAL_SET_BAUD(port, 9600)
PRINT "SERIAL: flush="; SERIAL_FLUSH(port)
PRINT "SERIAL: write="; SERIAL_WRITE(port, "PING", 20)
PRINT "SERIAL: read_text="; SERIAL_READ(port, 4, 20)

DIM tx(4)
LET tx(0) = 1
LET tx(1) = 2
LET tx(2) = 3
LET tx(3) = 4
DIM rx(4)
PRINT "SERIAL: write_bytes="; SERIAL_WRITE_BYTES(port, tx, 4, 20)
PRINT "SERIAL: read_bytes="; SERIAL_READ_BYTES(port, rx, 4, 20)
PRINT "SERIAL: done"
END
