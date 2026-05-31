REM MODBUS RTU smoke - requires RS485 loopback test slave or mocked Modbus responder
PRINT "MODBUS: start"

LET port = SERIAL_OPEN(1, "RS485")
DIM regs(4)
DIM coils(4)
PRINT "MODBUS: read_hold="; MODBUS_READ_HOLD_REGS(port, 1, 0, 2, regs, 200)
PRINT "MODBUS: reg0_hi="; regs(0)
PRINT "MODBUS: reg0_lo="; regs(1)
PRINT "MODBUS: read_coils="; MODBUS_READ_COILS(port, 1, 0, 4, coils, 200)
PRINT "MODBUS: write_reg="; MODBUS_WRITE_REG(port, 1, 0, 123, 200)
PRINT "MODBUS: write_coil="; MODBUS_WRITE_COIL(port, 1, 0, 1, 200)
PRINT "MODBUS: last_error="; MODBUS_LAST_ERROR()
PRINT "MODBUS: done"
END
