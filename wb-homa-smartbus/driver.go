package smartbus

import (
	"log"
	"strings"
	"strconv"
)

type MQTTMessage struct {
	Topic string
	Payload string
	QoS byte
	Retained bool
}

type MQTTMessageHandler func(message MQTTMessage)
type MQTTClientFactory func (handler MQTTMessageHandler) MQTTClient

type MQTTClient interface {
	Start()
	Stop()
	Publish(message MQTTMessage)
	Subscribe(topic string)
	Unsubscribe(topic string)
}

type Model interface {
	Name() string
	Title() string
	SetNewParameterHandler(handler func(name, paramType, value string))
	SetValueHandler(handler func(name, value string))
	SendValue(name, value string)
	QueryParams()
}

type Control struct {
	Name string
	Type string
	Value string
}

type ModelBase struct {
	DevName string
	DevTitle string
	ValueHandler func(name, value string)
	ParameterHandler func(name, paramType, value string)
}

func (model *ModelBase) Name() string {
	return model.DevName
}

func (model *ModelBase) Title() string {
	return model.DevTitle
}

func (model *ModelBase) SetNewParameterHandler(handler func(name, paramType, value string)) {
	model.ParameterHandler = handler
}

func (model *ModelBase) SetValueHandler(handler func(name, value string)) {
	model.ValueHandler = handler
}

type Driver struct {
	model Model
	client MQTTClient
	messageCh chan MQTTMessage
	quit chan struct{}
	nextOrder int
	// controls []Control
	// controlMap map[string]Control
}

func NewDriver(model Model, makeClient MQTTClientFactory) (drv *Driver) {
	drv = &Driver{
		model: model,
		messageCh: make(chan MQTTMessage),
		quit: make(chan struct{}),
		nextOrder: 1,
		// controls: make([]Control, 0, 100),
		// controlMap: make(map[string]Control),
	}
	drv.client = makeClient(drv.handleMessage)
	drv.model.SetNewParameterHandler(drv.handleParameter)
	drv.model.SetValueHandler(drv.publishValue)
	return
}

func (drv *Driver) handleMessage(message MQTTMessage) {
	drv.messageCh <- message
}

func (drv *Driver) topic(sub ...string) string {
	parts := append(append([]string(nil), "/devices", drv.model.Name()), sub...)
	return strings.Join(parts, "/")
}

func (drv *Driver) controlTopic(controlName string, sub ...string) string {
	parts := append(append([]string(nil), "controls", controlName), sub...)
	return drv.topic(parts...)
}

func (drv *Driver) publish(topic, payload string, qos byte) {
	drv.client.Publish(MQTTMessage{topic, payload, qos, true})
}

func (drv *Driver) publishMeta(topic string, payload string) {
	drv.publish(topic, payload, 1)
}

func (drv *Driver) publishValue(controlName, value string) {
	drv.publish(drv.controlTopic(controlName), value, 1)
}

func (drv *Driver) handleParameter(controlName, paramType, value string) {
	drv.publishMeta(drv.controlTopic(controlName, "meta", "type"), paramType)
	drv.publishMeta(drv.controlTopic(controlName, "meta", "order"), strconv.Itoa(drv.nextOrder))
	drv.nextOrder++
	drv.publishValue(controlName, value)
	// TBD: subscribe for non-read-only controls only
	log.Printf("subscribe to: %s", drv.controlTopic(controlName, "on"))
	drv.client.Subscribe(drv.controlTopic(controlName, "on"))
}

func (drv *Driver) doHandleMessage(msg MQTTMessage) {
	// /devices/<name>/controls/<control>/on
	log.Printf("TOPIC: %s", msg.Topic)
	log.Printf("MSG: %s\n", msg.Payload)
	parts := strings.Split(msg.Topic, "/")
	if len(parts) != 6 ||
		parts[1] != "devices" ||
		parts[2] != drv.model.Name() ||
		parts[3] != "controls" ||
		parts[5] != "on" {
		log.Printf("UNHANDLED TOPIC: %s", msg.Topic)
		return
	}
	controlName := parts[4]
	drv.model.SendValue(controlName, msg.Payload)
	drv.publishValue(controlName, msg.Payload)
}

func (drv *Driver) Start() {
	go func () {
		drv.client.Start()
		drv.publishMeta(drv.topic("meta", "name"), drv.model.Title())
		drv.model.QueryParams()
		for {
			select {
			case <- drv.quit:
				log.Printf("Driver: stopping the client")
				drv.client.Stop()
				return
			case msg := <- drv.messageCh:
				drv.doHandleMessage(msg)
			}
		}
	}()
}

func (drv *Driver) Stop() {
	log.Printf("----(Stop)")
	drv.quit <- struct{}{}
}
