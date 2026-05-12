REM common.bas - import smoke test, usually stored in the backup EEPROM slot
PRINT "COMMON: loaded"

DEF common_value()
  RETURN 202
ENDDEF

DEF common_name$()
  RETURN "common.bas"
ENDDEF

DEF common_health()
  LET ok = 1
  RETURN ok
ENDDEF

PRINT "COMMON: ready"
