package smartbus

import (
	"io"
	"sync"
)

type SmartbusConnection struct {
	stream io.ReadWriteCloser
	mutex sync.Mutex
	readCh chan SmartbusMessage
	writeCh chan SmartbusMessage
	endpoints []*SmartbusEndpoint
}

func NewSmartbusConnection(stream io.ReadWriteCloser) *SmartbusConnection {
	conn := &SmartbusConnection{
		stream: stream,
		readCh: make(chan SmartbusMessage),
		writeCh: make(chan SmartbusMessage),
		endpoints: make([]*SmartbusEndpoint, 0, 100),
	}
	conn.run()
	return conn
}

func (conn *SmartbusConnection) run() {
	go ReadSmartbus(conn.stream, &conn.mutex, conn.readCh)
	go WriteSmartbus(conn.stream, &conn.mutex, conn.writeCh)
	go func () {
		for msg := range conn.readCh {
			for _, ep := range conn.endpoints {
				ep.maybeHandleMessage(&msg)
			}
		}
	}()
}

func (conn *SmartbusConnection) Send(msg SmartbusMessage) {
	conn.writeCh <- msg
}

func (conn *SmartbusConnection) MakeSmartbusEndpoint(subnetID uint8,
	deviceID uint8, deviceType uint16) *SmartbusEndpoint {
	ep := &SmartbusEndpoint{
		conn,
		subnetID,
		deviceID,
		deviceType,
		make(map[uint16]*SmartbusDevice),
		make([]interface{}, 0, 16),
	}
	conn.endpoints = append(conn.endpoints, ep)
	return ep
}

func (conn *SmartbusConnection) Close() {
	close(conn.writeCh) // this kills WriteSmartbus goroutine
	conn.stream.Close() // this kills ReadSmartbus goroutine by causing read error
}

type SmartbusEndpoint struct {
	Connection *SmartbusConnection
	SubnetID uint8
	DeviceID uint8
	DeviceType uint16
	deviceMap map[uint16]*SmartbusDevice
	observers []interface{}
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

func (ep *SmartbusEndpoint) GetBroadcastDevice() *SmartbusDevice {
	return ep.GetSmartbusDevice(BROADCAST_SUBNET, BROADCAST_DEVICE)
}

func (ep *SmartbusEndpoint) Send(msg SmartbusMessage) {
	msg.Header.OrigSubnetID = ep.SubnetID
	msg.Header.OrigDeviceID = ep.DeviceID
	msg.Header.OrigDeviceType = ep.DeviceType
	ep.Connection.Send(msg)
}

func (ep *SmartbusEndpoint) Observe(observer interface{}) {
	ep.observers = append(ep.observers, observer)
}

func (ep *SmartbusEndpoint) maybeHandleMessage(smartbusMsg *SmartbusMessage) {
	if smartbusMsg.Header.TargetSubnetID != ep.SubnetID &&
		smartbusMsg.Header.TargetSubnetID != BROADCAST_SUBNET {
		return
	}
	if smartbusMsg.Header.TargetDeviceID != ep.DeviceID &&
		smartbusMsg.Header.TargetDeviceID != BROADCAST_DEVICE {
		return
	}
	ep.doHandleMessage(smartbusMsg)
}

func (ep *SmartbusEndpoint) doHandleMessage(smartbusMsg *SmartbusMessage) {
	for _, observer := range ep.observers {
		visit(observer, smartbusMsg.Message, "On", &smartbusMsg.Header)
	}
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

func (dev *SmartbusDevice) SingleChannelControlResponse(channelNo uint8,
	success bool, level uint8, status[] bool) {
	dev.Send(&SingleChannelControlResponse{channelNo, success, level, status})
}

func (dev *SmartbusDevice) ZoneBeastBroadcast(zoneStatus []uint8, channelStatus []bool) {
	dev.Send(&ZoneBeastBroadcast{zoneStatus, channelStatus})
}
