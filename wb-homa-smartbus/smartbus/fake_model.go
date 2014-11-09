package smartbus

import (
	"testing"
)

type FakeModel struct {
	Recorder
	ModelBase
	devices []*FakeDevice
}

type FakeDevice struct {
	DeviceBase
	model *FakeModel
	paramTypes map[string]string
	paramValues map[string]string
}

func NewFakeModel(t *testing.T) (model *FakeModel) {
	model = &FakeModel{devices: make([]*FakeDevice, 0, 100)}
	model.InitRecorder(t)
	return
}

func (model *FakeModel) QueryDevices() {
	for _, dev := range model.devices {
		model.Observer.OnNewDevice(dev)
		dev.QueryParams()
	}
}

func (model *FakeModel) MakeDevice(name string, title string,
	paramTypes map[string]string) (dev *FakeDevice) {
	dev = &FakeDevice{
		model: model,
		paramTypes: make(map[string]string),
		paramValues: make(map[string]string),
	}
	dev.DevName = name
	dev.DevTitle = title
	for k, v := range paramTypes {
		dev.paramTypes[k] = v
		dev.paramValues[k] = "0"
	}
	model.devices = append(model.devices, dev)
	return
}

func (dev *FakeDevice) SendValue(name, value string) {
	if _, found := dev.paramTypes[name]; !found {
		dev.model.T().Fatalf("trying to send unknown param %s (value %s)",
			name, value)
	}
	dev.paramValues[name] = value
	dev.model.Rec("send: %s.%s = %s", dev.DevName, name, value)
}

func (dev *FakeDevice) QueryParams() {
	for k, v := range dev.paramTypes {
		dev.Observer.OnNewControl(dev, k, v, dev.paramValues[k])
	}
}

func (dev *FakeDevice) ReceiveValue(name, value string) {
	if _, found := dev.paramValues[name]; !found {
		dev.model.T().Fatalf("trying to send unknown param %s (value %s)",
			name, value)
	} else {
		dev.paramValues[name] = value
		dev.Observer.OnValue(dev, name, value)
	}
}
