REM app01.bas - primary EEPROM BASIC smoke test
PRINT "APP01: boot"

IMPORT "app02.bas"

LET total = 0
FOR i = 1 TO 5
  LET total = total + i
NEXT i
PRINT "APP01: sum(1..5)="; total

IF total = 15 THEN
  PRINT "APP01: for-loop ok"
ELSE
  PRINT "APP01: for-loop failed"
ENDIF

LET text$ = "IoTEmBASIC"
PRINT "APP01: text="; text$
PRINT "APP01: len="; LEN(text$)
PRINT "APP01: left3="; LEFT(text$, 3)
PRINT "APP01: mid="; MID(text$, 3, 2)
PRINT "APP01: right5="; RIGHT(text$, 5)

DIM meter(3)
LET meter(0) = 11
LET meter(1) = 22
LET meter(2) = 33
PRINT "APP01: array="; meter(0); ","; meter(1); ","; meter(2)

DEF checksum(a, b, c)
  RETURN a + b + c
ENDDEF
PRINT "APP01: checksum="; checksum(meter(0), meter(1), meter(2))

LET backup_value = app02_value()
PRINT "APP01: imported value="; backup_value
PRINT "APP01: imported name="; app02_name$()

IF backup_value = 202 THEN
  PRINT "APP01: import ok"
ELSE
  PRINT "APP01: import failed"
ENDIF

PRINT "APP01: done"
END
