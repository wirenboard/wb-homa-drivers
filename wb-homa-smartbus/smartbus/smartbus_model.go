package smartbus

import (
	"fmt"
	"log"
	"strings"
	"strconv"
)

type Connector func() (SmartbusIO, error)

type SmartbusDeviceItem struct {
	Name, Title string
}

var smartbusDeviceTypes map[uint16]SmartbusDeviceItem = map[uint16]SmartbusDeviceItem{
	0x139c: {"zonebeast", "Zone Beast"},
}

func smartbusNameFromHeader(header *MessageHeader) (string, string, bool) {
	item, found := smartbusDeviceTypes[header.OrigDeviceType]
	if !found {
		log.Printf("unhandled device type: %04x", header.OrigDeviceType)
		return "", "", false
	}
	name := fmt.Sprintf("%s%02x%02x", item.Name, header.OrigSubnetID, header.OrigDeviceID)
	title := fmt.Sprintf("%s %02x:%02x", item.Title, header.OrigSubnetID, header.OrigDeviceID)
	return name, title, true
}

type SmartbusModel struct {
	ModelBase
	connector Connector
	deviceMap map[string]*SmartbusModelDevice
	subnetID uint8
	deviceID uint8
	deviceType uint16
	ep *SmartbusEndpoint
}

func NewSmartbusModel(connector Connector, subnetID uint8,
	deviceID uint8, deviceType uint16) (model *SmartbusModel) {
	model = &SmartbusModel{
		connector: connector,
		subnetID: subnetID,
		deviceID: deviceID,
		deviceType: deviceType,
		deviceMap: make(map[string]*SmartbusModelDevice),
	}
	return
}

func (model *SmartbusModel) Start() error {
	smartbusIO, err := model.connector()
	if err != nil {
		return err
	}
	conn := NewSmartbusConnection(smartbusIO)
	model.ep = conn.MakeSmartbusEndpoint(model.subnetID, model.deviceID, model.deviceType)
	model.ep.Observe(model)
	model.ep.Observe(NewMessageDumper("MESSAGE FOR US"))
	model.ep.AddInputSniffer(NewMessageDumper("NOT FOR US"))
	model.ep.AddOutputSniffer(NewMessageDumper("OUTGOING"))
	return err
}

func (model *SmartbusModel) ensureDevice(header *MessageHeader) *SmartbusModelDevice {
	name, title, found := smartbusNameFromHeader(header)
	if !found {
		return nil
	}
	var dev *SmartbusModelDevice
	dev, found = model.deviceMap[name]
	if found {
		return dev
	}

	dev = &SmartbusModelDevice{}
	dev.DevName = name
	dev.DevTitle = title
	dev.smartDev = model.ep.GetSmartbusDevice(header.OrigSubnetID, header.OrigDeviceID)
	dev.channelStatus = make([]bool, 0, 100)
	model.deviceMap[name] = dev
	model.Observer.OnNewDevice(dev)

	return dev
}

func (model *SmartbusModel) OnSingleChannelControlResponse(msg *SingleChannelControlResponse,
	header *MessageHeader) {
	if !msg.Success {
		log.Printf("ERROR: unsuccessful SingleChannelControlCommand")
		return
	}

	dev := model.ensureDevice(header)
	if dev != nil {
		dev.updateSingleChannel(int(msg.ChannelNo - 1), msg.Level != 0)
	}
}

func (model *SmartbusModel) OnZoneBeastBroadcast(msg *ZoneBeastBroadcast, header *MessageHeader) {
	dev := model.ensureDevice(header)
	if dev != nil {
		dev.updateChannelStatus(msg.ChannelStatus)
	}
}

type SmartbusModelDevice struct {
	DeviceBase
	smartDev *SmartbusDevice
	channelStatus []bool
}

func (dev *SmartbusModelDevice) updateSingleChannel(n int, isOn bool) {
	if n >= len(dev.channelStatus) {
		log.Printf("SmartbusModelDevice.updateSingleChannel(): bad channel number: %d", n)
		return
	}

	if dev.channelStatus[n] == isOn {
		return
	}

	dev.channelStatus[n] = isOn
	v := "0"
	if isOn {
		v = "1"
	}
	dev.Observer.OnValue(dev, fmt.Sprintf("Channel %d", n + 1), v)
}

func (dev *SmartbusModelDevice) updateChannelStatus(channelStatus []bool) {
	updateCount := len(dev.channelStatus)
	if updateCount > len(channelStatus) {
		updateCount = len(channelStatus)
	}

	for i := 0; i < updateCount; i++ {
		dev.updateSingleChannel(i, channelStatus[i])
	}

	for i := updateCount; i < len(channelStatus); i++ {
		dev.channelStatus = append(dev.channelStatus, channelStatus[i])
		v := "0"
		if dev.channelStatus[i] {
			v = "1"
		}
		controlName := fmt.Sprintf("Channel %d", i + 1)
		dev.Observer.OnNewControl(dev, controlName, "switch", v)
	}
}

func (dev *SmartbusModelDevice) SendValue(name, value string) bool {
	channelNo, err := strconv.Atoi(strings.TrimPrefix(name, "Channel "))
	if err != nil {
		log.Printf("bad channel name: %s", name)
	}
	level := uint8(LIGHT_LEVEL_OFF)
	if value == "1" {
		level = LIGHT_LEVEL_ON
	}
	dev.smartDev.SingleChannelControl(uint8(channelNo), level, 0)
	// No need to echo the value back.
	// It will be echoed after the device response
	return false
}
