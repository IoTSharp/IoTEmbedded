REM MQTT smoke - requires local broker and configured network link
PRINT "MQTT: start"

LET broker$ = CONFIG_GET("mqtt_ip", "127.0.0.1")
LET mqtt = MQTT_CONNECT(broker$, 1883, "basic-smoke", "", "", 30)
PRINT "MQTT: handle="; mqtt
IF mqtt = 0 THEN
  PRINT "MQTT: error="; MQTT_LAST_ERROR()
  END
ENDIF

PRINT "MQTT: connected="; MQTT_CONNECTED(mqtt)
PRINT "MQTT: subscribe="; MQTT_SUBSCRIBE(mqtt, "iotembedded/basic/smoke", 0)
PRINT "MQTT: publish="; MQTT_PUBLISH(mqtt, "iotembedded/basic/smoke", "hello", 0, 0)
LET msg = MQTT_RECEIVE(mqtt, 1000)
IF msg = NIL THEN
  PRINT "MQTT: receive=nil"
ELSE
  PRINT "MQTT: receive_topic="; GET(msg, "topic")
  PRINT "MQTT: receive_payload="; GET(msg, "payload")
ENDIF
PRINT "MQTT: unsubscribe="; MQTT_UNSUBSCRIBE(mqtt, "iotembedded/basic/smoke")
PRINT "MQTT: disconnect="; MQTT_DISCONNECT(mqtt)
PRINT "MQTT: done"
END
