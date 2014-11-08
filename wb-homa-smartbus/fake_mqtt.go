package smartbus

import (
	"fmt"
	"testing"
	MQTT "git.eclipse.org/gitroot/paho/org.eclipse.paho.mqtt.golang.git"
)

func FormatMQTTMessage(message MQTT.Message) string {
	suffix := ""
	if message.RetainedFlag() {
		suffix = ", retained"
	}
	return fmt.Sprintf("[%s] (QoS %d%s)",
		string(message.Payload()), message.QoS(), suffix)
}

type SubscriptionList []*FakeMQTTClient
type SubscriptionMap map[string]SubscriptionList

type FakeMQTTBroker struct {
	Recorder
	subscriptions SubscriptionMap
}

func NewFakeMQTTBroker (t *testing.T) (broker *FakeMQTTBroker) {
	broker = &FakeMQTTBroker{subscriptions: make(SubscriptionMap)}
	broker.InitRecorder(t)
	return
}

func (broker *FakeMQTTBroker) Publish(origin, topic string, message *MQTT.Message) {
	broker.Rec("%s -> %s: %s", origin, topic, FormatMQTTMessage(*message))
	subs, found := broker.subscriptions[topic]
	if !found {
		return
	}
	for _, client := range subs {
		client.receive(message)
	}
}

func (broker *FakeMQTTBroker) Subscribe(client *FakeMQTTClient, topic string) {
	broker.Rec("Subscribe -- %s: %s", client.id, topic)
	subs, found := broker.subscriptions[topic]
	if (!found) {
		broker.subscriptions[topic] = SubscriptionList{ client }
	} else {
		for _, c := range subs {
			if c == client {
				return
			}
		}
		broker.subscriptions[topic] = append(subs, client)
	}
}

func (broker *FakeMQTTBroker) Unsubscribe(client *FakeMQTTClient, topic string) {
	broker.Rec("Unsubscribe -- %s: %s", client.id, topic)
	subs, found := broker.subscriptions[topic]
	if (!found) {
		return
	} else {
		newSubs := make(SubscriptionList, 0, len(subs))
		for _, c := range subs {
			if c != client {
				newSubs = append(newSubs, c)
			}
		}
		broker.subscriptions[topic] = newSubs
	}
}

func (broker *FakeMQTTBroker) MakeClient(id string, handler MQTTMessageHandler) *FakeMQTTClient {
	return &FakeMQTTClient{id, broker, handler}
}

type FakeMQTTClient struct {
	id string
	broker *FakeMQTTBroker
	handler MQTTMessageHandler
}

func (client *FakeMQTTClient) receive(message *MQTT.Message) {
	client.handler(*message)
}

func (client *FakeMQTTClient) Publish(topic string, message *MQTT.Message) {
	client.broker.Publish(client.id, topic, message)
}

func (client *FakeMQTTClient) Subscribe(topic string) {
	client.broker.Subscribe(client, topic)
}

func (client *FakeMQTTClient) Unsubscribe(topic string) {
	client.broker.Unsubscribe(client, topic)
}

// TBD: support wildcard topics
