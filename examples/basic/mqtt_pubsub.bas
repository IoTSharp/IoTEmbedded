REM mqtt_pubsub.bas - publish and receive MQTT messages from BASIC
PRINT "BASIC MQTT demo"

LET pub_topic$ = MQTT_BUILD_TOPIC("/v1/devices/up/basic/")
LET sub_topic$ = MQTT_BUILD_TOPIC("/v1/devices/down/basic/")
PRINT "pub topic="; pub_topic$
PRINT "sub topic="; sub_topic$

LET ready = 0
FOR i = 1 TO 20
  LET ready = MQTT_MAINTAIN(15000)
  BASIC_DELAY(500)
NEXT i

IF ready = 0 THEN
  PRINT "MQTT not ready"
  END
ENDIF

IF MQTT_SUBSCRIBE(sub_topic$) THEN
  PRINT "subscribed"
ELSE
  PRINT "subscribe failed"
ENDIF

IF MQTT_PUBLISH(pub_topic$, "hello from BASIC") THEN
  PRINT "published"
ELSE
  PRINT "publish failed"
ENDIF

FOR i = 1 TO 150
  LET pending = MQTT_POLL()
  IF MQTT_RECV() THEN
    PRINT "rx topic="; MQTT_RECV_TOPIC()
    PRINT "rx payload="; MQTT_RECV_PAYLOAD()
  ENDIF
  BASIC_DELAY(200)
NEXT i

PRINT "dropped="; MQTT_OVERFLOW()
PRINT "BASIC MQTT demo done"
END
