package smartbus

import (
	"net"
	"testing"
)

func TestSmartbusDriver(t *testing.T) {
	p, r := net.Pipe()

	broker := NewFakeMQTTBroker(t)
	model := NewSmartbusModel(p, SAMPLE_SUBNET, SAMPLE_ORIG_DEVICE_ID, SAMPLE_ORIG_DEVICE_TYPE)
	client := broker.MakeClient("tst", func (msg MQTTMessage) {
		t.Logf("tst: message %v", msg)
	})
	client.Start()
	driver := NewDriver(model, func (handler MQTTMessageHandler) MQTTClient {
		return broker.MakeClient("driver", handler)
	})

	handler := NewFakeHandler(t)
	conn := NewSmartbusConnection(r)
	ep := conn.MakeSmartbusEndpoint(
		SAMPLE_SUBNET, SAMPLE_TARGET_DEVICE_ID, SAMPLE_TARGET_DEVICE_TYPE)
	ep.Observe(handler)
	dev := ep.GetBroadcastDevice()

	driver.Start()

	dev.ZoneBeastBroadcast([]byte{ 0 }, parseChannelStatus("---x"))
	broker.Verify(
		"driver -> /devices/zonebeast011c/meta/name: [Zone Beast 01:1c] (QoS 1, retained)",

		"driver -> /devices/zonebeast011c/controls/Channel 1/meta/type: [switch] (QoS 1, retained)",
		"driver -> /devices/zonebeast011c/controls/Channel 1/meta/order: [1] (QoS 1, retained)",
		"driver -> /devices/zonebeast011c/controls/Channel 1: [0] (QoS 1, retained)",
		"Subscribe -- driver: /devices/zonebeast011c/controls/Channel 1/on",

		"driver -> /devices/zonebeast011c/controls/Channel 2/meta/type: [switch] (QoS 1, retained)",
		"driver -> /devices/zonebeast011c/controls/Channel 2/meta/order: [2] (QoS 1, retained)",
		"driver -> /devices/zonebeast011c/controls/Channel 2: [0] (QoS 1, retained)",
		"Subscribe -- driver: /devices/zonebeast011c/controls/Channel 2/on",

		"driver -> /devices/zonebeast011c/controls/Channel 3/meta/type: [switch] (QoS 1, retained)",
		"driver -> /devices/zonebeast011c/controls/Channel 3/meta/order: [3] (QoS 1, retained)",
		"driver -> /devices/zonebeast011c/controls/Channel 3: [0] (QoS 1, retained)",
		"Subscribe -- driver: /devices/zonebeast011c/controls/Channel 3/on",

		"driver -> /devices/zonebeast011c/controls/Channel 4/meta/type: [switch] (QoS 1, retained)",
		"driver -> /devices/zonebeast011c/controls/Channel 4/meta/order: [4] (QoS 1, retained)",
		"driver -> /devices/zonebeast011c/controls/Channel 4: [1] (QoS 1, retained)",
		"Subscribe -- driver: /devices/zonebeast011c/controls/Channel 4/on",
	)

	dev.ZoneBeastBroadcast([]byte{ 0 }, parseChannelStatus("x---"))
	broker.Verify(
		"driver -> /devices/zonebeast011c/controls/Channel 1: [1] (QoS 1, retained)",
		"driver -> /devices/zonebeast011c/controls/Channel 4: [0] (QoS 1, retained)",
	)

	client.Publish(MQTTMessage{"/devices/zonebeast011c/controls/Channel 2/on", "1", 1, false})
	// note that SingleChannelControlResponse carries pre-command channel status
	handler.Verify("01/14 (type 0095) -> 01/1c: <SingleChannelControlCommand 2/100/0>")
	dev.SingleChannelControlResponse(2, true, LIGHT_LEVEL_ON, parseChannelStatus("x---"))
	broker.Verify(
		"tst -> /devices/zonebeast011c/controls/Channel 2/on: [1] (QoS 1)",
		"driver -> /devices/zonebeast011c/controls/Channel 2: [1] (QoS 1, retained)",
	)

	client.Publish(MQTTMessage{"/devices/zonebeast011c/controls/Channel 1/on", "0", 1, false})
	handler.Verify("01/14 (type 0095) -> 01/1c: <SingleChannelControlCommand 1/0/0>")
	dev.SingleChannelControlResponse(1, true, LIGHT_LEVEL_OFF, parseChannelStatus("xx--"))
	broker.Verify(
		"tst -> /devices/zonebeast011c/controls/Channel 1/on: [0] (QoS 1)",
		"driver -> /devices/zonebeast011c/controls/Channel 1: [0] (QoS 1, retained)",
	)

	// TBD: off (ch 1)

	driver.Stop()
	conn.Close()
	broker.Verify(
		"stop: driver",
	)
}

// TBD: outdated ZoneBeastBroadcast messages still arrive sometimes, need to fix this
