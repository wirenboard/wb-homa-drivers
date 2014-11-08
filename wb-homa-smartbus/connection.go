package smartbus

import (
	"io"
	"sync"
)

const (
	LIGHT_LEVEL_OFF = 0
	LIGHT_LEVEL_ON = 100
)

type SmartbusConnection struct {
	stream io.ReadWriter
	mutex sync.Mutex
	readCh chan SmartbusMessage
	writeCh chan SmartbusMessage
}

func NewSmartbusConnection(stream io.ReadWriter) *SmartbusConnection {
	conn := &SmartbusConnection{
		stream: stream,
		readCh: make(chan SmartbusMessage),
		writeCh: make(chan SmartbusMessage),
	}
	conn.run()
	return conn
}

func (conn *SmartbusConnection) run() {
	go ReadSmartbus(conn.stream, &conn.mutex, conn.readCh)
	go WriteSmartbus(conn.stream, &conn.mutex, conn.writeCh)
}

func (conn *SmartbusConnection) Send(msg SmartbusMessage) {
	conn.writeCh <- msg
}

func (conn *SmartbusConnection) MakeSmartbusEndpoint(subnetID uint8,
	deviceID uint8, deviceType uint16) *SmartbusEndpoint {
	return &SmartbusEndpoint{conn, subnetID, deviceID, deviceType, make(map[uint16]*SmartbusDevice)}
}

type SmartbusEndpoint struct {
	Connection *SmartbusConnection
	SubnetID uint8
	DeviceID uint8
	DeviceType uint16
	deviceMap map[uint16]*SmartbusDevice
}

func deviceKey(subnetID uint8, deviceID uint8) uint16 {
	return (uint16(subnetID) << 8) + uint16(deviceID)
}

func (ep *SmartbusEndpoint) GetSmartbusDevice(subnetID uint8, deviceID uint8) *SmartbusDevice {
	dev, found := ep.deviceMap[deviceKey(subnetID, deviceID)]
	if !found {
		dev = &SmartbusDevice{ep, subnetID, deviceID, 0}
		ep.deviceMap[deviceKey(subnetID, deviceID)] = dev
	}
	return dev
}

func (ep *SmartbusEndpoint) Send(msg SmartbusMessage) {
	msg.Header.OrigSubnetID = ep.SubnetID
	msg.Header.OrigDeviceID = ep.DeviceID
	msg.Header.OrigDeviceType = ep.DeviceType
	ep.Connection.Send(msg)
}
 
type SmartbusDevice struct {
	Endpoint *SmartbusEndpoint
	SubnetID uint8
	DeviceID uint8
	DeviceType uint16 // filled in when a packet is received from the device
}

func (dev *SmartbusDevice) Send(msg Message) {
	dev.Endpoint.Send(
		SmartbusMessage{
			MessageHeader{
				TargetSubnetID: dev.SubnetID,
				TargetDeviceID: dev.DeviceID,
			},
			msg,
		})
}

func (dev *SmartbusDevice) SingleChannelControl(channelNo uint8, level uint8, duration uint16) {
	dev.Send(&SingleChannelControlCommand{channelNo, level, duration})
}
