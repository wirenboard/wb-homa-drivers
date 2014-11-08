package smartbus

import (
	"testing"
	MQTT "git.eclipse.org/gitroot/paho/org.eclipse.paho.mqtt.golang.git"
)

func TestFakeMQTT(t *testing.T) {
	broker := NewFakeMQTTBroker(t)

	c1 := broker.MakeClient("c1", func (message MQTT.Message) {
		broker.Rec("message for c1: %s", FormatMQTTMessage(message))
	})
	c1.Subscribe("/some/topic")

	c2 := broker.MakeClient("c2", func (message MQTT.Message) {
		broker.Rec("message for c2: %s", FormatMQTTMessage(message))
	})
	c2.Subscribe("/some/topic")
	c2.Subscribe("/another/topic")

	m := MQTT.NewMessage([]byte("somemsg"))
	m.SetQoS(MQTT.QOS_ONE)
	m.SetRetainedFlag(true)
	c1.Publish("/some/topic", m)

	m = MQTT.NewMessage([]byte("anothermsg"))
	m.SetQoS(MQTT.QOS_ZERO)
	c1.Publish("/another/topic", m)

	c2.Unsubscribe("/another/topic")
	c1.Publish("/another/topic", m)
	c1.Publish("/some/topic", m)

	c1.Unsubscribe("/some/topic")
	c1.Publish("/some/topic", m)

	broker.Verify(
		"Subscribe -- c1: /some/topic",
		"Subscribe -- c2: /some/topic",
		"Subscribe -- c2: /another/topic",
		"c1 -> /some/topic: [somemsg] (QoS 1, retained)",
		"message for c1: [somemsg] (QoS 1, retained)",
		"message for c2: [somemsg] (QoS 1, retained)",

		"c1 -> /another/topic: [anothermsg] (QoS 0)",
		"message for c2: [anothermsg] (QoS 0)",

		"Unsubscribe -- c2: /another/topic",
		"c1 -> /another/topic: [anothermsg] (QoS 0)",
		"c1 -> /some/topic: [anothermsg] (QoS 0)",
		"message for c1: [anothermsg] (QoS 0)",
		"message for c2: [anothermsg] (QoS 0)",

		"Unsubscribe -- c1: /some/topic",
		"c1 -> /some/topic: [anothermsg] (QoS 0)",
		"message for c2: [anothermsg] (QoS 0)",
	)
}
