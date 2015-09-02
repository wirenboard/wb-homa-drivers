wb-mqtt-db
====================

Конфигурация демона сохранения

В примере демон сохраняет данные от устройств “kvadro-1wire_69" и "wb-w1" (драйвера встроенных портов 1-wire). 
Данные от устройства “kvadro-1wire_42” из конфига выше не сохраняются.

```
root@wirenboard:~# cat /etc/wb-mqtt-db.conf
```

```
{
	"groups": {
    	"w1": {
        	"channels" : ["/devices/kvadro-1wire_69/controls/+", "/devices/wb-w1/controls/+"],
        	"values" : 10000,
        	"values_total" : 100000,
        	"min_interval" : 20,
        	"min_unchanged_interval" : 600
    	}
	},
	"database" : "/var/lib/wirenboard/db/data.db"
}
```



300 - сохранять не чаще, чем раз в пять минут, 3600 - сохранять опорные значения раз в час.

