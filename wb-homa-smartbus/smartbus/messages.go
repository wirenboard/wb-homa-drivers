package smartbus

// FIXME: this module needs some spaghetti reduction

import (
	"fmt"
)

const (
	LIGHT_LEVEL_OFF = 0
	LIGHT_LEVEL_ON = 100
	PANEL_BUTTON_COUNT = 16
)

const (
	QUERY_MODULES_DEV_DIMMER = 0x01
	QUERY_MODULES_DEV_RELAY = 0x02
	QUERY_MODULES_DEV_HVAC = 0x03
	QUERY_MODULES_DEV_SENSORS = 0x04
	QUERY_MODULES_DEV_Z_AUDIO = 0x05

	BUTTON_COMMAND_INVALID = 0x00
	BUTTON_COMMAND_SINGLE_CHANNEL_LIGHTING_CONTROL = 0x59

	PANEL_CONTROL_TYPE_INVALID = 0x00
	PANEL_CONTROL_TYPE_IR_RECEIVER = 0x01
	PANEL_CONTROL_TYPE_BUTTON_LOCK = 0x02
	PANEL_CONTROL_TYPE_AC_ON_OFF = 0x03
	PANEL_CONTROL_TYPE_COOLING_SET_POINT = 0x04
	PANEL_CONTROL_TYPE_FAN_SPEED = 0x05
	PANEL_CONTROL_TYPE_AC_MODE = 0x06
	PANEL_CONTROL_TYPE_HEAT_SET_POINT = 0x07
	PANEL_CONTROL_TYPE_AUTO_SET_POINT = 0x08
	PANEL_CONTROL_TYPE_GO_TO_PAGE = 0x16
)

// ------

// SingleChannelControlCommand toggles single relay output
type SingleChannelControlCommand struct {
	ChannelNo uint8
	Level uint8
	Duration uint16 // FIXME: is it really duration? ("running time")
}

func (*SingleChannelControlCommand) Opcode() uint16 { return 0x0031 }

// ------

// SingleChannelControlResponse is sent as a response to SingleChannelControlCommand
type SingleChannelControlResponse struct {
	ChannelNo uint8
	Success bool `sbus:"success"`
	Level uint8
	ChannelStatus []bool `sbus:"channelStatus"`
}

func (*SingleChannelControlResponse) Opcode() uint16 { return 0x0032 }

// ------

// ZoneBeastBroadcast packets are sent by ZoneBeast at regular intervals
type ZoneBeastBroadcast struct {
	ZoneStatus []uint8 `sbus:"zoneStatus"`
	ChannelStatus []bool `sbus:"channelStatus"`
}

func (*ZoneBeastBroadcast) Opcode() uint16 { return 0xefff }

// ------

// QueryModules is sent by DDP upon a button press
type QueryModules struct {}

func (*QueryModules) Opcode() uint16 { return 0x286 }

// ------

// QueryModulesResponse is sent as a response to QueryModules
type QueryModulesResponse struct {
	ControlledDeviceSubnetID uint8
	ControlledDeviceID uint8
	DeviceCategory uint8
	ChannelNo uint8
	HVACSubnetID uint8
	HVACDeviceID uint8
}

type QueryModulesResponseRaw struct {
	ControlledDeviceSubnetID uint8
	ControlledDeviceID uint8
	DeviceCategory uint8
	Param1 uint8
	Param2 uint8
	Param3 uint8
	Param4 uint8
}

func (*QueryModulesResponse) Opcode() uint16 { return 0x287 }

func (msg *QueryModulesResponse) FromRaw(parseRaw func (interface {}) error) (err error) {
	var raw QueryModulesResponseRaw
	if err = parseRaw(&raw); err != nil {
		return
	}

	// TBD: if more such structures appear, may as well
	// copy similarly named fields right in smartbus.go
	msg.ControlledDeviceSubnetID = raw.ControlledDeviceSubnetID
	msg.ControlledDeviceID = raw.ControlledDeviceID
	msg.DeviceCategory = raw.DeviceCategory

	switch msg.DeviceCategory {
	case QUERY_MODULES_DEV_DIMMER, QUERY_MODULES_DEV_RELAY:
		msg.ChannelNo = raw.Param1
	case QUERY_MODULES_DEV_HVAC:
		msg.HVACSubnetID = raw.Param1
		msg.HVACDeviceID = raw.Param2
	case QUERY_MODULES_DEV_SENSORS, QUERY_MODULES_DEV_Z_AUDIO:
		break
	default:
		err = fmt.Errorf("bad flag QueryModulesResponse DeviceCategory field %02x",
			msg.DeviceCategory)
	}
	return
}

func (msg *QueryModulesResponse) ToRaw() (interface{}, error) {
	raw := &QueryModulesResponseRaw{
		ControlledDeviceSubnetID: msg.ControlledDeviceSubnetID,
		ControlledDeviceID: msg.ControlledDeviceID,
		DeviceCategory: msg.DeviceCategory,
		Param1: 0,
		Param2: 0,
		Param3: 0,
		Param4: 0,
	}
	switch msg.DeviceCategory {
	case QUERY_MODULES_DEV_DIMMER, QUERY_MODULES_DEV_RELAY:
		raw.Param1 = msg.ChannelNo
		raw.Param2 = 0x64 // perhaps actually on/off/brightness value (not documented)
	case QUERY_MODULES_DEV_HVAC:
		raw.Param1 = msg.HVACSubnetID
		raw.Param2 = msg.HVACDeviceID
	default:
		return nil, fmt.Errorf("bad device category %v", msg.DeviceCategory)
	}

	return raw, nil
}

// -----

// PanelControlResponse is sent by the panel
// (should be sent as a response to similiarly structured 0xe3d8,
// but seems to be sent on its own, too)
type PanelControlResponse struct {
	Type uint8
	Value uint8
}

func (*PanelControlResponse) Opcode() uint16 { return 0xe3d9 }

// ------

// QueryFanController is sent by the panel to fan controller devices
type QueryFanController struct {
	Index uint8 // spec says there's no such field, but it's present
}

func (*QueryFanController) Opcode() uint16 { return 0x0033 }

// ------

type QueryPanelButtonAssignment struct {
	ButtonNo uint8
	FunctionNo uint8
}

func (*QueryPanelButtonAssignment) Opcode() uint16 { return 0xe000 }

type QueryPanelButtonAssignmentResponse struct {
	ButtonNo uint8
	FunctionNo uint8
	Command uint8
	CommandSubnetID uint8
	CommandDeviceID uint8
	ChannelNo uint8
	Level uint8
	Duration uint16
}

func (*QueryPanelButtonAssignmentResponse) Opcode() uint16 { return 0xe001 }

// ------

type AssignPanelButton struct {
	ButtonNo uint8
	FunctionNo uint8
	Command uint8
	CommandSubnetID uint8
	CommandDeviceID uint8
	ChannelNo uint8
	Level uint8
	Duration uint16
	Unknown uint8
}

func (*AssignPanelButton) Opcode() uint16 { return 0xe002 }

// ------

type AssignPanelButtonResponse struct {
	ButtonNo uint8
	FunctionNo uint8
}

func (*AssignPanelButtonResponse) Opcode() uint16 { return 0xe003 }

// ------

type SetPanelButtonModes struct {
	Modes [16]string `sbus:"panelButtonModes"`
}

func (*SetPanelButtonModes) Opcode() uint16 { return 0xe00a }

// ------

type SetPanelButtonModesResponse struct {
	Success bool `sbus:"success"`
}

func (*SetPanelButtonModesResponse) Opcode() uint16 { return 0xe00b }

// ------

type ReadMACAddress struct {}

func (*ReadMACAddress) Opcode() uint16 { return 0xf003 }

// ------

type ReadMACAddressResponse struct {
	MAC [8]uint8
	Remark []uint8 `sbus:"remark"`
}

func (*ReadMACAddressResponse) Opcode() uint16 { return 0xf004 }

// ------

func init () {
	RegisterMessage(func () Message { return new(SingleChannelControlCommand) })
	RegisterMessage(func () Message { return new(SingleChannelControlResponse) })
	RegisterMessage(func () Message { return new(ZoneBeastBroadcast) })
	RegisterMessage(func () Message { return new(QueryModules) })
	RegisterMessage(func () Message { return new(QueryModulesResponse) })
	RegisterMessage(func () Message { return new(PanelControlResponse) })
	RegisterMessage(func () Message { return new(QueryFanController) })
	RegisterMessage(func () Message { return new(QueryPanelButtonAssignment) })
	RegisterMessage(func () Message { return new(QueryPanelButtonAssignmentResponse) })
	RegisterMessage(func () Message { return new(AssignPanelButton) })
	RegisterMessage(func () Message { return new(AssignPanelButtonResponse) })
	RegisterMessage(func () Message { return new(SetPanelButtonModes) })
	RegisterMessage(func () Message { return new(SetPanelButtonModesResponse) })
	RegisterMessage(func () Message { return new(ReadMACAddress) })
	RegisterMessage(func () Message { return new(ReadMACAddressResponse) })
}
