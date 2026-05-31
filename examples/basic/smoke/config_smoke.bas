REM CONFIG smoke - reads and mutates non-secret runtime config keys
PRINT "CONFIG: start"

LET original$ = CONFIG_GET("network_mode", "unknown")
PRINT "CONFIG: mode="; original$
PRINT "CONFIG: set_probe="; CONFIG_SET("probe_interval_ms", 30000)
PRINT "CONFIG: probe="; CONFIG_GET("probe_interval_ms", "missing")
PRINT "CONFIG: apply="; CONFIG_APPLY()
PRINT "CONFIG: network_mode="; NETWORK_MODE()
PRINT "CONFIG: network_link="; NETWORK_LINK()
PRINT "CONFIG: network_ready="; NETWORK_READY()
PRINT "CONFIG: use_original="; NETWORK_USE(original$, 0)
PRINT "CONFIG: done"
END
