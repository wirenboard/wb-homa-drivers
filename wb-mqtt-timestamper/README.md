wb-mqtt-timestamper 
====================

Служба, которая фиксирует время изменения контрола в MQTT,  
запускается скриптом `/etc/init.d/wb-mqtt-timestamper start`. К каждому контролу публикуется время его изменения 
в топик `/devices/+/controls/CONTROL_NAME/meta/ts` в формате Unix. Параметры запуска службы можно изменить в файле 
`/etc/defaults/wb-mqtt-timestamper` или непосредственно в командной строке.
Описания параметров доступны в help(```wb-mqtt-timestamper -h```).
