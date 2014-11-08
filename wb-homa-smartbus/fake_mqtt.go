package smartbus

import (
	"fmt"
	"testing"
)

func FormatMQTTMessage(message MQTTMessage) string {
	suffix := ""
	if message.Retained {
		suffix = ", retained"
	}
	return fmt.Sprintf("[%s] (QoS %d%s)",
		string(message.Payload), message.QoS, suffix)
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

func (broker *FakeMQTTBroker) Publish(origin string, message MQTTMessage) {
	broker.Rec("%s -> %s: %s", origin, message.Topic, FormatMQTTMessage(message))
	subs, found := broker.subscriptions[message.Topic]
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
	return &FakeMQTTClient{id, false, broker, handler}
}

type FakeMQTTClient struct {
	id string
	started bool
	broker *FakeMQTTBroker
	handler MQTTMessageHandler
}

func (client *FakeMQTTClient) receive(message MQTTMessage) {
	client.handler(message)
}

func (client *FakeMQTTClient) Start() {
	if (client.started) {
		client.broker.T().Fatalf("%s: client already started", client.id)
	}
	client.started = true
}

func (client *FakeMQTTClient) Stop() {
	client.ensureStarted()
	client.started = false
	client.broker.Rec("stop: %s", client.id)
}

func (client *FakeMQTTClient) ensureStarted() {
	if (!client.started) {
		client.broker.T().Fatalf("%s: client not started", client.id)
	}
}

func (client *FakeMQTTClient) Publish(message MQTTMessage) {
	client.ensureStarted()
	client.broker.Publish(client.id, message)
}

func (client *FakeMQTTClient) Subscribe(topic string) {
	client.ensureStarted()
	client.broker.Subscribe(client, topic)
}

func (client *FakeMQTTClient) Unsubscribe(topic string) {
	client.ensureStarted()
	client.broker.Unsubscribe(client, topic)
}
