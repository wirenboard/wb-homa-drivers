package smartbus

import (
	"fmt"
	"log"
	"io"
	"strings"
	"strconv"
)

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
	deviceMap map[string]*SmartbusModelDevice
	ep *SmartbusEndpoint
}

func NewSmartbusModel(stream io.ReadWriteCloser, subnetID uint8,
	deviceID uint8, deviceType uint16) (model *SmartbusModel) {
	model = &SmartbusModel{ deviceMap: make(map[string]*SmartbusModelDevice) }
	conn := NewSmartbusConnection(stream)
	model.ep = conn.MakeSmartbusEndpoint(subnetID, deviceID, deviceType)
	model.ep.Observe(model)
	model.ep.Observe(NewMessageDumper())
	return
}

func (model *SmartbusModel) QueryDevices() {
	// NOOP
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

func (dev *SmartbusModelDevice) updateChannelStatus(channelStatus []bool) {
	updateCount := len(dev.channelStatus)
	if updateCount > len(channelStatus) {
		updateCount = len(channelStatus)
	}

	for i := 0; i < updateCount; i++ {
		if dev.channelStatus[i] == channelStatus[i] {
			continue
		}
		v := "0"
		dev.channelStatus[i] = channelStatus[i]
		if dev.channelStatus[i] {
			v = "1"
		}
		dev.Observer.OnValue(dev, fmt.Sprintf("Channel %d", i + 1), v)
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

func (dev *SmartbusModelDevice) SendValue(name, value string) {
	channelNo, err := strconv.Atoi(strings.TrimPrefix(name, "Channel "))
	if err != nil {
		log.Printf("bad channel name: %s", name)
	}
	level := uint8(LIGHT_LEVEL_OFF)
	if value == "1" {
		level = LIGHT_LEVEL_ON
	}
	dev.smartDev.SingleChannelControl(uint8(channelNo), level, 0)
}
