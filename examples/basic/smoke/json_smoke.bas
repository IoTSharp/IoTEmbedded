REM JSON smoke - pure interpreter check
PRINT "JSON: start"

PRINT "JSON: valid="; JSON_VALID("{\"device\":{\"id\":\"d1\"},\"values\":[1,2]}")
LET root = JSON_OBJECT()
PRINT "JSON: set_id="; JSON_SET_STRING(root, "device.id", "d1")
PRINT "JSON: set_temp="; JSON_SET_NUMBER(root, "telemetry.temperature", 23.5)
PRINT "JSON: set_online="; JSON_SET_BOOL(root, "state.online", 1)
LET arr = JSON_ARRAY()
PRINT "JSON: append1="; JSON_APPEND_NUMBER(arr, "", 10)
PRINT "JSON: append2="; JSON_APPEND_NUMBER(arr, "", 20)
PRINT "JSON: set_arr="; JSON_SET_JSON(root, "samples", arr)
PRINT "JSON: id="; JSON_GET_STRING(root, "device.id", "missing")
PRINT "JSON: temp="; JSON_GET_NUMBER(root, "telemetry.temperature", 0)
PRINT "JSON: online="; JSON_GET_BOOL(root, "state.online", 0)
PRINT "JSON: count="; JSON_COUNT(root, "samples")
PRINT "JSON: sample0="; JSON_AT_INT(root, "samples", 0, -1)
PRINT "JSON: text="; JSON_STRINGIFY(root)
PRINT "JSON: done"
END
