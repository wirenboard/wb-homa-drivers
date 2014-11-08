package smartbus

import (
	"testing"
)

func TestDriver(t *testing.T) {
	broker := NewFakeMQTTBroker(t)
	model := NewFakeModel(t, "somedev", "SomeDev", map[string]string {
		"paramOne": "switch",
		"paramTwo": "switch",
	})
	client := broker.MakeClient("tst", func (msg MQTTMessage) {
		t.Logf("tst: message %v", msg)
	})
	client.Start()
	driver := NewDriver(model, func (handler MQTTMessageHandler) MQTTClient {
		return broker.MakeClient("driver", handler)
	})

	driver.Start()

	broker.Verify(
		"driver -> /devices/somedev/meta/name: [SomeDev] (QoS 1, retained)",
		"driver -> /devices/somedev/controls/paramOne/meta/type: [switch] (QoS 1, retained)",
		"driver -> /devices/somedev/controls/paramOne/meta/order: [1] (QoS 1, retained)",
		"driver -> /devices/somedev/controls/paramOne: [0] (QoS 1, retained)",
		"Subscribe -- driver: /devices/somedev/controls/paramOne/on",
		"driver -> /devices/somedev/controls/paramTwo/meta/type: [switch] (QoS 1, retained)",
		"driver -> /devices/somedev/controls/paramTwo/meta/order: [2] (QoS 1, retained)",
		"driver -> /devices/somedev/controls/paramTwo: [0] (QoS 1, retained)",
		"Subscribe -- driver: /devices/somedev/controls/paramTwo/on",
	)

	client.Publish(MQTTMessage{"/devices/somedev/controls/paramOne/on", "1", 1, false})
	broker.Verify(
		"tst -> /devices/somedev/controls/paramOne/on: [1] (QoS 1)",
		"driver -> /devices/somedev/controls/paramOne: [1] (QoS 1, retained)",
	)
	model.Verify(
		"send: paramOne = 1",
	)

	model.ReceiveValue("paramTwo", "1")
	broker.Verify(
		"driver -> /devices/somedev/controls/paramTwo: [1] (QoS 1, retained)",
	)
	
	driver.Stop()
	broker.Verify(
		"stop: driver",
	)
	model.Verify()
}
