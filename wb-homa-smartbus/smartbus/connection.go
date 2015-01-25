package smartbus

// SmartbusConnection provides higher-level interface for
// a SmartbusIO that uses endpoints to filter incoming
// messages by their destination and assign source
// addresses to outgoind messages.
type SmartbusConnection struct {
	smartbusIO SmartbusIO
	endpoints []*SmartbusEndpoint
}

func NewSmartbusConnection(smartbusIO SmartbusIO) *SmartbusConnection {
	conn := &SmartbusConnection{
		smartbusIO: smartbusIO,
		endpoints: make([]*SmartbusEndpoint, 0, 100),
	}
	conn.run()
	return conn
}

func (conn *SmartbusConnection) run() {
	readCh := conn.smartbusIO.Start()
	go func () {
		for msg := range readCh {
			for _, ep := range conn.endpoints {
				ep.maybeHandleMessage(&msg)
			}
		}
	}()
}

func (conn *SmartbusConnection) Send(msg SmartbusMessage) {
	conn.smartbusIO.Send(msg)
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
		make([]interface{}, 0, 16),
		make([]interface{}, 0, 16),
	}
	conn.endpoints = append(conn.endpoints, ep)
	return ep
}

func (conn *SmartbusConnection) Close() {
	conn.smartbusIO.Stop()
}

type SmartbusEndpoint struct {
	Connection *SmartbusConnection
	SubnetID uint8
	DeviceID uint8
	DeviceType uint16
	deviceMap map[uint16]*SmartbusDevice
	observers []interface{}
	inputSniffers []interface{}
	outputSniffers []interface{}
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
	ep.notify(ep.outputSniffers, &msg)
	ep.Connection.Send(msg)
}

func (ep *SmartbusEndpoint) Observe(observer interface{}) {
	ep.observers = append(ep.observers, observer)
}

func (ep *SmartbusEndpoint) AddInputSniffer(observer interface{}) {
	ep.inputSniffers = append(ep.inputSniffers, observer)
}

func (ep *SmartbusEndpoint) AddOutputSniffer(observer interface{}) {
	ep.outputSniffers = append(ep.outputSniffers, observer)
}

func (ep *SmartbusEndpoint) isMessageForUs(smartbusMsg *SmartbusMessage) bool {
	if smartbusMsg.Header.TargetSubnetID != ep.SubnetID &&
		smartbusMsg.Header.TargetSubnetID != BROADCAST_SUBNET {
		return false
	}
	return smartbusMsg.Header.TargetDeviceID == ep.DeviceID ||
		smartbusMsg.Header.TargetDeviceID == BROADCAST_DEVICE
}

func (ep *SmartbusEndpoint) maybeHandleMessage(smartbusMsg *SmartbusMessage) {
	if ep.isMessageForUs(smartbusMsg) {
		ep.notify(ep.observers, smartbusMsg)
	} else {
		ep.notify(ep.inputSniffers, smartbusMsg)
	}
}

func (ep *SmartbusEndpoint) notify(observers []interface{},
	smartbusMsg *SmartbusMessage) {
	for _, observer := range observers {
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

func (dev *SmartbusDevice) QueryModules() {
	dev.Send(&QueryModules{});
}

func (dev *SmartbusDevice) QueryModulesResponse(deviceCategory uint8, channelNo uint8) {
	dev.Send(&QueryModulesResponse{
		dev.Endpoint.SubnetID, dev.Endpoint.DeviceID, deviceCategory, channelNo,
		dev.Endpoint.SubnetID, dev.Endpoint.DeviceID,
	});
}

func (dev *SmartbusDevice) PanelControlResponse(Type uint8, Value uint8) {
	dev.Send(&PanelControlResponse{Type, Value})
}

func (dev *SmartbusDevice) QueryFanController(index uint8) {
	dev.Send(&QueryFanController{index})
}

func (dev *SmartbusDevice) QueryPanelButtonAssignment(buttonNo uint8, functionNo uint8) {
	dev.Send(&QueryPanelButtonAssignment{buttonNo, functionNo})
}

func (dev *SmartbusDevice) QueryPanelButtonAssignmentResponse(
	buttonNo uint8, functionNo uint8, command uint8,
	commandSubnetID uint8, commandDeviceID uint8,
	channelNo uint8, level uint8, duration uint16) {
	dev.Send(&QueryPanelButtonAssignmentResponse{
		buttonNo, functionNo, command,
		commandSubnetID, commandDeviceID,
		channelNo, level, duration,
	})
}

func (dev *SmartbusDevice) AssignPanelButton(
	buttonNo uint8, functionNo uint8, command uint8,
	commandSubnetID uint8, commandDeviceID uint8,
	channelNo uint8, level uint8, duration uint16) {
	dev.Send(&AssignPanelButton{
		buttonNo, functionNo, command,
		commandSubnetID, commandDeviceID,
		channelNo, level, duration, 0, // 0 for unknown field
	})
}

func (dev *SmartbusDevice) AssignPanelButtonResponse(buttonNo uint8, functionNo uint8) {
	dev.Send(&AssignPanelButtonResponse{buttonNo, functionNo})
}

func (dev *SmartbusDevice) SetPanelButtonModes(modes [PANEL_BUTTON_COUNT]string) {
	dev.Send(&SetPanelButtonModes{modes})
}

func (dev *SmartbusDevice) SetPanelButtonModesResponse(success bool) {
	dev.Send(&SetPanelButtonModesResponse{success})
}

func (dev *SmartbusDevice) ReadMACAddress() {
	dev.Send(&ReadMACAddress{})
}

func (dev *SmartbusDevice) ReadMACAddressResponse(macAddress [8]uint8, remark []uint8) {
	dev.Send(&ReadMACAddressResponse{macAddress, remark})
}
