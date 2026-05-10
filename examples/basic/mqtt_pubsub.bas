REM mqtt_pubsub.bas - BASIC owns MQTT connect, subscribe, publish, receive, reconnect
PRINT "BASIC MQTT loop demo"

LET pub_topic$ = MQTT_BUILD_TOPIC("/v1/devices/up/basic/")
LET sub_topic$ = MQTT_BUILD_TOPIC("/v1/devices/down/basic/")
PRINT "pub topic="; pub_topic$
PRINT "sub topic="; sub_topic$

LET subscribed_ok = 0
IF MQTT_CONNECT() = 0 THEN
  PRINT "connect failed"
  END
ENDIF

IF MQTT_SUBSCRIBE(sub_topic$) = 0 THEN
  PRINT "subscribe failed"
  MQTT_DISCONNECT()
  END
ENDIF
LET subscribed_ok = 1

LET seq = 0
DO
  LET seq = seq + 1

  IF MQTT_MAINTAIN(15000) = 0 THEN
    LET subscribed_ok = 0
    PRINT "mqtt reconnecting"
    BASIC_DELAY(1000)
  ELSE
    IF subscribed_ok = 0 THEN
      IF MQTT_SUBSCRIBE(sub_topic$) THEN
        LET subscribed_ok = 1
        PRINT "resubscribed"
      ENDIF
    ENDIF
  ENDIF

  IF MQTT_PUBLISH(pub_topic$, "hello from BASIC") THEN
    PRINT "published "; seq
  ELSE
    PRINT "publish failed "; seq
  ENDIF

  MQTT_POLL()
  WHILE MQTT_RECV()
    PRINT "rx topic="; MQTT_RECV_TOPIC()
    PRINT "rx payload="; MQTT_RECV_PAYLOAD()

    IF MQTT_RECV_PAYLOAD() = "disconnect" THEN
      PRINT "disconnect requested"
      MQTT_DISCONNECT()
      LET subscribed_ok = 0
      BASIC_DELAY(1000)
      IF MQTT_CONNECT() THEN
        IF MQTT_SUBSCRIBE(sub_topic$) THEN
          LET subscribed_ok = 1
        ENDIF
      ENDIF
    ENDIF
  WEND

  BASIC_DELAY(1000)
UNTIL 0

MQTT_DISCONNECT()
END
