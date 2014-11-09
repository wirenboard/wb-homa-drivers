package smartbus

import (
	MQTT "git.eclipse.org/gitroot/paho/org.eclipse.paho.mqtt.golang.git"
)

const DISCONNECT_WAIT_MS = 100

type PahoMQTTClient struct {
	handler MQTTMessageHandler
	innerClient *MQTT.MqttClient
}

func NewPahoMQTTClient(server, clientID string, handler MQTTMessageHandler) (client *PahoMQTTClient) {
	client = &PahoMQTTClient{handler: handler}
	opts := MQTT.NewClientOptions().AddBroker(server).SetClientId(clientID)
	opts.SetDefaultPublishHandler(client.handleMessage)
	client.innerClient = MQTT.NewClient(opts)
	return
}

func (client *PahoMQTTClient) handleMessage(mc *MQTT.MqttClient, msg MQTT.Message) {
	client.handler(MQTTMessage{msg.Topic(), string(msg.Payload()),
		byte(msg.QoS()), msg.RetainedFlag()})
}

func (client *PahoMQTTClient) Start() {
	_, err := client.innerClient.Start()
	if err != nil {
		panic(err)
	}
}

func (client *PahoMQTTClient) Stop() {
	client.innerClient.Disconnect(DISCONNECT_WAIT_MS)
}

func (client *PahoMQTTClient) Publish(message MQTTMessage) {
	m := MQTT.NewMessage([]byte(message.Payload))
	m.SetQoS(MQTT.QoS(message.QoS))
	m.SetRetainedFlag(message.Retained)
	if ch := client.innerClient.PublishMessage(message.Topic, m); ch != nil {
		<- ch
	} else {
		panic("PublishMessage() failed")
	}
}

func (client *PahoMQTTClient) Subscribe(topic string) {
	filter, _ := MQTT.NewTopicFilter(topic, 1)
	if receipt, err := client.innerClient.StartSubscription(nil, filter); err != nil {
		panic(err)
	} else {
		<-receipt
	}
}

func (client *PahoMQTTClient) Unsubscribe(topic string) {
	if receipt, err := client.innerClient.EndSubscription(topic); err != nil {
		panic(err)
	} else {
		<-receipt
	}
}
