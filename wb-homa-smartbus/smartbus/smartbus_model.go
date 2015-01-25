package smartbus

import (
	"fmt"
	"log"
	"strings"
	"strconv"
)

const (
	NUM_VIRTUAL_RELAYS = 15
)

type Connector func() (SmartbusIO, error)

type RealDeviceModel interface {
	DeviceModel
	Type() uint16
}

type DeviceConstructor func (model *SmartbusModel, smartDev *SmartbusDevice) RealDeviceModel

var smartbusDeviceModelTypes map[uint16]DeviceConstructor = make(map[uint16]DeviceConstructor)

func RegisterDeviceModelType(construct DeviceConstructor) {
	smartbusDeviceModelTypes[construct(nil, nil).Type()] = construct
}

type VirtualRelayDevice struct {
	DeviceBase
	channelStatus [NUM_VIRTUAL_RELAYS]bool
}

func (dm *VirtualRelayDevice) Publish() {
	for i, status := range dm.channelStatus {
		v := "0"
		if status {
			v = "1"
		}
		controlName := fmt.Sprintf("VirtualRelay%d", i + 1)
		dm.Observer.OnNewControl(dm, controlName, "text", v)
	}
}

func (dm *VirtualRelayDevice) SetRelayOn(channelNo int, on bool) {
	if channelNo < 1 || channelNo > NUM_VIRTUAL_RELAYS {
		log.Printf("WARNING: invalid virtual relay channel %d", channelNo)
		return
	}
	if dm.channelStatus[channelNo - 1] == on {
		return
	}
	dm.channelStatus[channelNo - 1] = on
	v := "0"
	if on {
		v = "1"
	}
	controlName := fmt.Sprintf("VirtualRelay%d", channelNo)
	dm.Observer.OnValue(dm, controlName, v)
}

func (dm *VirtualRelayDevice) RelayStatus() []bool {
	return dm.channelStatus[:]
}

func (dm *VirtualRelayDevice) SendValue(name, value string) bool {
	// virtual relays cannot be changed
	return false
}

func NewVirtualRelayDevice () *VirtualRelayDevice {
	r := &VirtualRelayDevice{}
	r.DevName = "sbusvrelay"
	r.DevTitle = "Smartbus Virtual Relays"
	return r
}

type SmartbusModel struct {
	ModelBase
	connector Connector
	deviceMap map[uint16]RealDeviceModel
	subnetID uint8
	deviceID uint8
	deviceType uint16
	ep *SmartbusEndpoint
	virtualRelays *VirtualRelayDevice
}

func NewSmartbusModel(connector Connector, subnetID uint8,
	deviceID uint8, deviceType uint16) (model *SmartbusModel) {
	model = &SmartbusModel{
		connector: connector,
		subnetID: subnetID,
		deviceID: deviceID,
		deviceType: deviceType,
		deviceMap: make(map[uint16]RealDeviceModel),
		virtualRelays: NewVirtualRelayDevice(),
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
	model.Observer.OnNewDevice(model.virtualRelays)
	model.virtualRelays.Publish()
	return err
}

func (model *SmartbusModel) ensureDevice(header *MessageHeader) RealDeviceModel {
	deviceKey := (uint16(header.OrigSubnetID) << 8) + uint16(header.OrigDeviceID)
	var dev, found = model.deviceMap[deviceKey]
	if found {
		return dev
	}

	construct, found := smartbusDeviceModelTypes[header.OrigDeviceType]
	if !found {
		log.Printf("unrecognized device type %04x @ %02x:%02x",
			header.OrigDeviceType, header.OrigSubnetID, header.OrigDeviceID);
		return nil
	}

	smartDev := model.ep.GetSmartbusDevice(header.OrigSubnetID, header.OrigDeviceID)
	dev = construct(model, smartDev)
	model.deviceMap[deviceKey] = dev
	fmt.Printf("NEW DEVICE: %#v (name: %v)\n", dev, dev.Name())
	model.Observer.OnNewDevice(dev)
	return dev
}

func (model *SmartbusModel) OnAnything(msg Message, header *MessageHeader) {
	dev := model.ensureDevice(header)
	visit(dev, msg, "On")
}

func (model *SmartbusModel) SetVirtualRelayOn(channelNo int, on bool) {
	model.virtualRelays.SetRelayOn(channelNo, on)
}

func (model *SmartbusModel) VirtualRelayStatus() []bool {
	return model.virtualRelays.RelayStatus()
}

type DeviceModelBase struct {
	nameBase string
	titleBase string
	model *SmartbusModel
	smartDev *SmartbusDevice
	Observer DeviceObserver
}

func (dm *DeviceModelBase) Name() string {
	return fmt.Sprintf("%s%02x%02x", dm.nameBase, dm.smartDev.SubnetID, dm.smartDev.DeviceID)
}

func (dm *DeviceModelBase) Title() string {
	return fmt.Sprintf("%s %02x:%02x", dm.titleBase, dm.smartDev.SubnetID, dm.smartDev.DeviceID)
}

func (dev *DeviceModelBase) Observe(observer DeviceObserver) {
	dev.Observer = observer
}

type ZoneBeastDeviceModel struct {
	DeviceModelBase
	channelStatus []bool
}

func NewZoneBeastDeviceModel(model *SmartbusModel, smartDev *SmartbusDevice) RealDeviceModel {
	return &ZoneBeastDeviceModel{
		DeviceModelBase{
			nameBase: "zonebeast",
			titleBase: "Zone Beast",
			model: model,
			smartDev: smartDev,
		},
		make([]bool, 0, 100),
	}
}

func (dm *ZoneBeastDeviceModel) Type() uint16 { return 0x139c }

func (dm *ZoneBeastDeviceModel) SendValue(name, value string) bool {
	log.Printf("ZoneBeastDeviceModel.SendValue(%v, %v)", name, value)
	channelNo, err := strconv.Atoi(strings.TrimPrefix(name, "Channel "))
	if err != nil {
		log.Printf("bad channel name: %s", name)
		return false
	}
	level := uint8(LIGHT_LEVEL_OFF)
	if value == "1" {
		level = LIGHT_LEVEL_ON
	}
	dm.smartDev.SingleChannelControl(uint8(channelNo), level, 0)
	// No need to echo the value back.
	// It will be echoed after the device response
	return false
}

func (dm *ZoneBeastDeviceModel) OnSingleChannelControlResponse(msg *SingleChannelControlResponse) {
	if !msg.Success {
		log.Printf("ERROR: unsuccessful SingleChannelControlCommand")
		return
	}

	dm.updateSingleChannel(int(msg.ChannelNo - 1), msg.Level != 0)
}

func (dm *ZoneBeastDeviceModel) OnZoneBeastBroadcast(msg *ZoneBeastBroadcast) {
	dm.updateChannelStatus(msg.ChannelStatus)
}

func (dm *ZoneBeastDeviceModel) updateSingleChannel(n int, isOn bool) {
	if n >= len(dm.channelStatus) {
		log.Printf("SmartbusModelDevice.updateSingleChannel(): bad channel number: %d", n)
		return
	}

	if dm.channelStatus[n] == isOn {
		return
	}

	dm.channelStatus[n] = isOn
	v := "0"
	if isOn {
		v = "1"
	}
	dm.Observer.OnValue(dm, fmt.Sprintf("Channel %d", n + 1), v)
}

func (dm *ZoneBeastDeviceModel) updateChannelStatus(channelStatus []bool) {
	updateCount := len(dm.channelStatus)
	if updateCount > len(channelStatus) {
		updateCount = len(channelStatus)
	}

	for i := 0; i < updateCount; i++ {
		dm.updateSingleChannel(i, channelStatus[i])
	}

	for i := updateCount; i < len(channelStatus); i++ {
		dm.channelStatus = append(dm.channelStatus, channelStatus[i])
		v := "0"
		if dm.channelStatus[i] {
			v = "1"
		}
		controlName := fmt.Sprintf("Channel %d", i + 1)
		dm.Observer.OnNewControl(dm, controlName, "switch", v)
	}
}

type DDPDeviceModel struct {
	DeviceModelBase
 	buttonAssignmentReceived []bool
 	buttonAssignment []int
	isNew bool
	pendingAssignmentButtonNo int
	pendingAssignment int
}

func NewDDPDeviceModel(model *SmartbusModel, smartDev *SmartbusDevice) RealDeviceModel {
	return &DDPDeviceModel{
		DeviceModelBase{
			nameBase: "ddp",
			titleBase: "DDP",
			model: model,
			smartDev: smartDev,
		},
		make([]bool, PANEL_BUTTON_COUNT),
		make([]int, PANEL_BUTTON_COUNT),
		true,
		-1,
		-1,
	}
}

func ddpControlName(buttonNo uint8) string {
	return fmt.Sprintf("Page%dButton%d",
		(buttonNo - 1) / 4 + 1,
		(buttonNo - 1) % 4 + 1)
}

func (dm *DDPDeviceModel) Type() uint16 { return 0x0095 }

func (dm *DDPDeviceModel) OnQueryModules(msg *QueryModules) {
	// NOTE: something like this can be used for button 'learning':
	// dm.smartDev.QueryModulesResponse(QUERY_MODULES_DEV_RELAY, 0x08)
	if dm.isNew {
		dm.isNew = false
		dm.smartDev.QueryPanelButtonAssignment(1, 1)
	}
}

func (dm *DDPDeviceModel) OnQueryPanelButtonAssignmentResponse(msg *QueryPanelButtonAssignmentResponse) {
	// FunctionNo = 1 because we're only querying the first function
	// in the list currently (multiple functions may be needed for CombinationOn mode etc.)
	if msg.ButtonNo == 0 || msg.ButtonNo > PANEL_BUTTON_COUNT || msg.FunctionNo != 1 {
		log.Printf("bad button/fn number: %d/%d", msg.ButtonNo, msg.FunctionNo)
	}

	v := -1
	if msg.Command == BUTTON_COMMAND_SINGLE_CHANNEL_LIGHTING_CONTROL &&
		msg.CommandSubnetID == dm.model.subnetID &&
		msg.CommandDeviceID == dm.model.deviceID {
		v = int(msg.ChannelNo)
	}
	dm.buttonAssignment[msg.ButtonNo - 1] = v

	controlName := ddpControlName(msg.ButtonNo)

	if dm.buttonAssignmentReceived[msg.ButtonNo - 1] {
		dm.Observer.OnValue(dm, controlName, strconv.Itoa(v))
	} else {
		dm.buttonAssignmentReceived[msg.ButtonNo - 1] = true
		dm.Observer.OnNewControl(dm, controlName, "text", strconv.Itoa(v))
	}

	// TBD: this is not quite correct, should wait w/timeout etc.
	if msg.ButtonNo < PANEL_BUTTON_COUNT {
		dm.smartDev.QueryPanelButtonAssignment(msg.ButtonNo + 1, 1)
	}
}

func (dm *DDPDeviceModel) OnSetPanelButtonModesResponse(msg *SetPanelButtonModesResponse) {
	// FIXME
	if dm.pendingAssignmentButtonNo <= 0 {
		log.Printf("SetPanelButtonModesResponse without pending assignment")
	}
	dm.smartDev.AssignPanelButton(
		uint8(dm.pendingAssignmentButtonNo),
		1,
		BUTTON_COMMAND_SINGLE_CHANNEL_LIGHTING_CONTROL,
		dm.model.subnetID,
		dm.model.deviceID,
		uint8(dm.pendingAssignment),
		100,
		0)
}

func (dm *DDPDeviceModel) OnAssignPanelButtonResponse(msg *AssignPanelButtonResponse) {
	if dm.pendingAssignmentButtonNo >= 0 &&
		int(msg.ButtonNo) == dm.pendingAssignmentButtonNo &&
		msg.FunctionNo == 1 {
		dm.Observer.OnValue(dm, ddpControlName(msg.ButtonNo),
			strconv.Itoa(dm.pendingAssignment))
	} else {
		log.Printf("ERROR: mismatched AssignPanelButtonResponse: %v/%v (pending %d)",
			msg.ButtonNo, msg.FunctionNo, dm.pendingAssignmentButtonNo)
	}
	// FIXME (retry)
	dm.pendingAssignmentButtonNo = -1
	dm.pendingAssignment = -1
}

func (dm *DDPDeviceModel) OnSingleChannelControlCommand(msg *SingleChannelControlCommand) {
	dm.model.SetVirtualRelayOn(int(msg.ChannelNo), msg.Level > 0)
	dm.smartDev.SingleChannelControlResponse(msg.ChannelNo, true, msg.Level,
		dm.model.VirtualRelayStatus())
}

func (dm *DDPDeviceModel) SendValue(name, value string) bool {
	// FIXME
	if dm.pendingAssignmentButtonNo > 0 {
		log.Printf("ERROR: button assignment queueing not implemented yet!")
	}

	s1 := strings.TrimPrefix(name, "Page")
	idx := strings.Index(s1, "Button")

	pageNo, err := strconv.Atoi(s1[:idx])
	if err != nil {
		log.Printf("bad button param: %s", name)
		return false
	}
 
	pageButtonNo, err := strconv.Atoi(s1[idx + 6:])
	if err != nil {
		log.Printf("bad button param: %s", name)
		return false
	}

	buttonNo := (pageNo - 1) * 4 + pageButtonNo

	newAssignment, err := strconv.Atoi(value)
	if err != nil || newAssignment <= 0 || newAssignment > NUM_VIRTUAL_RELAYS{
		log.Printf("bad button assignment value: %s", value)
		return false
	}
	
	for _, isReceived := range dm.buttonAssignmentReceived {
		if !isReceived {
			// TBD: fix this
			log.Printf("cannot assign button: DDP device data not ready yet")
			return false
		}
	}

	var modes [PANEL_BUTTON_COUNT]string
	for i, assignment := range dm.buttonAssignment {
		if buttonNo == i + 1 {
			assignment = newAssignment
			dm.buttonAssignment[i] = newAssignment
		}
		if assignment <= 0 || assignment > NUM_VIRTUAL_RELAYS {
			modes[i] = "Invalid"
		} else {
			modes[i] = "SingleOnOff"
		}
	}
	dm.smartDev.SetPanelButtonModes(modes)

	dm.pendingAssignmentButtonNo = buttonNo
	dm.pendingAssignment = newAssignment

	return false
}

func init () {
	RegisterDeviceModelType(NewZoneBeastDeviceModel)
	RegisterDeviceModelType(NewDDPDeviceModel)
}
