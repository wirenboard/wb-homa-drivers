package smartbus

import (
	"testing"
)

type FakeModel struct {
	Recorder
	ModelBase
	paramTypes map[string]string
	paramValues map[string]string
}

func NewFakeModel(t *testing.T, name string, title string,
	paramTypes map[string]string) (model *FakeModel) {
	model = &FakeModel{}
	model.DevName = name
	model.DevTitle = title
	model.InitRecorder(t)
	model.paramTypes = make(map[string]string)
	model.paramValues = make(map[string]string)
	for k, v := range paramTypes {
		model.paramTypes[k] = v
		model.paramValues[k] = "0"
	}
	return
}

func (model *FakeModel) SendValue(name, value string) {
	if _, found := model.paramTypes[name]; !found {
		model.T().Fatalf("trying to send unknown param %s (value %s)",
			name, value)
	}
	model.paramValues[name] = value
	model.Rec("send: %s = %s", name, value)
}

func (model *FakeModel) QueryParams() {
	for k, v := range model.paramTypes {
		model.ParameterHandler(k, v, model.paramValues[k])
	}
}

func (model *FakeModel) ReceiveValue(name, value string) {
	if _, found := model.paramValues[name]; !found {
		model.T().Fatalf("trying to send unknown param %s (value %s)",
			name, value)
	} else {
		model.paramValues[name] = value
		model.ValueHandler(name, value)
	}
}
