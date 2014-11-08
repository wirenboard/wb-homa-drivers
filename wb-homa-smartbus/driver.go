package smartbus

import (
	MQTT "git.eclipse.org/gitroot/paho/org.eclipse.paho.mqtt.golang.git"
)

type MQTTMessageHandler func(message MQTT.Message)
