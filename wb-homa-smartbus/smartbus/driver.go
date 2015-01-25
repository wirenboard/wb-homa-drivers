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
	Start() error
	Observe(observer ModelObserver)
}

type DeviceModel interface {
	Name() string
	Title() string
	// SendValue sends the specified control value to the target device
	// and returns true if the value should be automatically echoed back
	SendValue(name, value string) bool
	Observe(observer DeviceObserver)
}

// TBD: Use ModelObserver interface

type ModelObserver interface {
	OnNewDevice(dev DeviceModel)
}

type DeviceObserver interface {
	OnNewControl(dev DeviceModel, name, paramType, value string)
	OnValue(dev DeviceModel, name, value string)
}

type ModelBase struct {
	Observer ModelObserver
}

func (model *ModelBase) Observe(observer ModelObserver) {
	model.Observer = observer
}

// fixme: move DeviceBase to fake_model.go (as a part of FakeModel)
type DeviceBase struct {
	DevName string
	DevTitle string
	Observer DeviceObserver
}

func (dev *DeviceBase) Name() string {
	return dev.DevName
}

func (dev *DeviceBase) Title() string {
	return dev.DevTitle
}

func (dev *DeviceBase) Observe(observer DeviceObserver) {
	dev.Observer = observer
}

// Driver transfers data between Model with MQTTClient
type Driver struct {
	model Model
	client MQTTClient
	messageCh chan MQTTMessage
	quit chan struct{}
	deviceMap map[string]DeviceModel
	nextOrder int
}

func NewDriver(model Model, makeClient MQTTClientFactory) (drv *Driver) {
	drv = &Driver{
		model: model,
		messageCh: make(chan MQTTMessage),
		quit: make(chan struct{}),
		nextOrder: 1,
		deviceMap: make(map[string]DeviceModel),
	}
	drv.client = makeClient(drv.handleMessage)
	drv.model.Observe(drv)
	return
}

func (drv *Driver) handleMessage(message MQTTMessage) {
	drv.messageCh <- message
}

func (drv *Driver) topic(dev DeviceModel, sub ...string) string {
	parts := append(append([]string(nil), "/devices", dev.Name()), sub...)
	return strings.Join(parts, "/")
}

func (drv *Driver) controlTopic(dev DeviceModel, controlName string, sub ...string) string {
	parts := append(append([]string(nil), "controls", controlName), sub...)
	return drv.topic(dev, parts...)
}

func (drv *Driver) publish(topic, payload string, qos byte) {
	drv.client.Publish(MQTTMessage{topic, payload, qos, true})
}

func (drv *Driver) publishMeta(topic string, payload string) {
	drv.publish(topic, payload, 1)
}

func (drv *Driver) publishValue(dev DeviceModel, controlName, value string) {
	drv.publish(drv.controlTopic(dev, controlName), value, 1)
}

func (drv *Driver) OnNewDevice(dev DeviceModel) {
	drv.deviceMap[dev.Name()] = dev
	drv.publishMeta(drv.topic(dev, "meta", "name"), dev.Title())
	dev.Observe(drv)
}

func (drv *Driver) OnNewControl(dev DeviceModel, controlName, paramType, value string) {
	drv.publishMeta(drv.controlTopic(dev, controlName, "meta", "type"), paramType)
	drv.publishMeta(drv.controlTopic(dev, controlName, "meta", "order"),
		strconv.Itoa(drv.nextOrder))
	drv.nextOrder++ // FIXME: should start the order from 1 for each device
	drv.publishValue(dev, controlName, value)
	// TBD: subscribe for non-read-only controls only
	log.Printf("subscribe to: %s", drv.controlTopic(dev, controlName, "on"))
	drv.client.Subscribe(drv.controlTopic(dev, controlName, "on"))
}

func (drv *Driver) OnValue(dev DeviceModel, controlName, value string) {
	drv.publishValue(dev, controlName, value)
}

func (drv *Driver) doHandleMessage(msg MQTTMessage) {
	// /devices/<name>/controls/<control>/on
	log.Printf("TOPIC: %s", msg.Topic)
	log.Printf("MSG: %s\n", msg.Payload)
	parts := strings.Split(msg.Topic, "/")
	if len(parts) != 6 ||
		parts[1] != "devices" ||
		parts[3] != "controls" ||
		parts[5] != "on" {
		log.Printf("UNHANDLED TOPIC: %s", msg.Topic)
		return
	}

	deviceName := parts[2]
	controlName := parts[4]
	dev, found := drv.deviceMap[deviceName]
	if !found {
		log.Printf("UNKNOWN DEVICE: %s", deviceName)
		return
	}
	if (dev.SendValue(controlName, msg.Payload)) {
		drv.publishValue(dev, controlName, msg.Payload)
	}
}

func (drv *Driver) Start() error {
	drv.client.Start()
	if err := drv.model.Start(); err != nil {
		return err
	}
	go func () {
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
	return nil
}

func (drv *Driver) Stop() {
	log.Printf("----(Stop)")
	drv.quit <- struct{}{}
}
