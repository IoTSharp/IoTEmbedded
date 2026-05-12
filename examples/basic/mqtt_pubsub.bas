REM mqtt_pubsub.bas - standard MQTT handle model
PRINT "BASIC MQTT handle demo"

IF MQTT_SETUP_CH395("192.168.137.110", 1883, "d0001", "auto", "192.168.137.201", "192.168.137.11", "255.255.255.0") = 0 THEN
  PRINT "ch395 setup failed"
  END
ENDIF

LET pub_topic$ = "/v1/devices/up/basic/d0001"
LET sub_topic$ = "/v1/devices/down/basic/d0001"
PRINT "pub topic="; pub_topic$
PRINT "sub topic="; sub_topic$

LET mqtt = MQTT_CONNECT("192.168.137.110", 1883, "basic-d0001", "d0001", "auto", 60)
IF mqtt = 0 THEN
  PRINT "connect failed "; MQTT_LAST_ERROR()
  END
ENDIF

IF MQTT_SUBSCRIBE(mqtt, sub_topic$, 0) = 0 THEN
  PRINT "subscribe failed "; MQTT_LAST_ERROR(mqtt)
  MQTT_DISCONNECT(mqtt)
  END
ENDIF

LET seq = 0
DO
  LET seq = seq + 1

  IF MQTT_CONNECTED(mqtt) = 0 THEN
    PRINT "reconnect"
    MQTT_DISCONNECT(mqtt)
    DELAY(1000)
    LET mqtt = MQTT_CONNECT("192.168.137.110", 1883, "basic-d0001", "d0001", "auto", 60)
    IF mqtt <> 0 THEN
      MQTT_SUBSCRIBE(mqtt, sub_topic$, 0)
    ENDIF
  ENDIF

  IF mqtt <> 0 THEN
    IF MQTT_PUBLISH(mqtt, pub_topic$, "hello from BASIC", 0, 0) THEN
      PRINT "published "; seq
    ELSE
      PRINT "publish failed "; MQTT_LAST_ERROR(mqtt)
    ENDIF

    LET msg = MQTT_RECEIVE(mqtt, 100)
    WHILE msg <> NIL
      PRINT "rx topic="; GET(msg, "topic")
      PRINT "rx payload="; GET(msg, "payload")

      IF GET(msg, "payload") = "disconnect" THEN
        PRINT "disconnect requested"
        MQTT_DISCONNECT(mqtt)
        LET mqtt = 0
      ENDIF

      IF mqtt <> 0 THEN
        LET msg = MQTT_RECEIVE(mqtt, 0)
      ELSE
        LET msg = NIL
      ENDIF
    WEND
  ENDIF

  DELAY(1000)
UNTIL 0

IF mqtt <> 0 THEN
  MQTT_DISCONNECT(mqtt)
ENDIF
END
