REM app02.bas - backup slot and import smoke test
PRINT "APP02: loaded"

DEF app02_value()
  RETURN 202
ENDDEF

DEF app02_name$()
  RETURN "app02.bas"
ENDDEF

DEF app02_health()
  LET ok = 1
  RETURN ok
ENDDEF

PRINT "APP02: ready"
