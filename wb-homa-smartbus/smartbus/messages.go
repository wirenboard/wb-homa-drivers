package smartbus

// FIXME: this module needs some spaghetti reduction

import (
	"io"
	"log"
	"encoding/binary"
	"fmt"
)

const (
	LIGHT_LEVEL_OFF = 0
	LIGHT_LEVEL_ON = 100
	SET_PANEL_BUTTON_MODES_COUNT = 16
)

const (
	QUERY_MODULES_DEV_DIMMER = 0x01
	QUERY_MODULES_DEV_RELAY = 0x02
	QUERY_MODULES_DEV_HVAC = 0x03
	QUERY_MODULES_DEV_SENSORS = 0x04
	QUERY_MODULES_DEV_Z_AUDIO = 0x05

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

func SuccessFlagToBool (flag uint8) (bool, error) {
	switch flag {
	case 0xf8:
		return true, nil
	case 0xf5:
		return false, nil
	default:
		return false, fmt.Errorf("bad success flag value %02x", flag)
	}
}

func BoolToSuccessFlag (value bool) uint8 {
	if (value) {
		return 0xf8
	}
	return 0xf5
}

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
	Success bool
	Level uint8
	ChannelStatus []bool
}

func (*SingleChannelControlResponse) Opcode() uint16 { return 0x0032 }

type SingleChannelControlResponseHeaderRaw struct {
	ChannelNo uint8
	Flag uint8
	Level uint8
}

func (*SingleChannelControlResponse) Parse(reader io.Reader) (interface{}, error) {
	var rr SingleChannelControlResponseHeaderRaw
	if err := binary.Read(reader, binary.BigEndian, &rr); err != nil {
		log.Printf("SingleChannelControlResponse.Parse(): error reading the header: %v", err)
		return nil, err
	}

	if success, err := SuccessFlagToBool(rr.Flag); err != nil {
		return nil, err
	} else {
		if status, err := ReadChannelStatus(reader); err != nil {
			log.Printf("ParseSingleChannelControlResponse: error reading status: %v", err)
			return nil, err
		} else {
			return &SingleChannelControlResponse{rr.ChannelNo, success, rr.Level, status}, nil
		}
	}
}

func (msg *SingleChannelControlResponse) Write(writer io.Writer) {
	flag := BoolToSuccessFlag(msg.Success)
	rr := SingleChannelControlResponseHeaderRaw{msg.ChannelNo, flag, msg.Level}
	binary.Write(writer, binary.BigEndian, &rr)
	WriteChannelStatus(writer, msg.ChannelStatus)
}

// ------

// ZoneBeastBroadcast packets are sent by ZoneBeast at regular intervals
type ZoneBeastBroadcast struct {
	ZoneStatus []uint8
	ChannelStatus []bool
}

func (*ZoneBeastBroadcast) Opcode() uint16 { return 0xefff }

func (*ZoneBeastBroadcast) Parse(reader io.Reader) (interface{}, error) {
	var msg ZoneBeastBroadcast
	if zoneStatus, err := ReadZoneStatus(reader); err != nil {
		log.Printf("ZoneBeastBroadcast.Parse(): error reading zone status: %v", err)
		return nil, err
	} else {
		msg.ZoneStatus = zoneStatus
	}

	if channelStatus, err := ReadChannelStatus(reader); err != nil {
		log.Printf("ZoneBeastBroadcast.Parse(): error reading channel status: %v", err)
		return nil, err
	} else {
		msg.ChannelStatus = channelStatus
	}

	return &msg, nil
}

func (msg *ZoneBeastBroadcast) Write(writer io.Writer) {
	WriteZoneStatus(writer, msg.ZoneStatus)
	WriteChannelStatus(writer, msg.ChannelStatus)
}

// QueryModules is sent by DDP upon a button press
type QueryModules struct {}

func (*QueryModules) Opcode() uint16 { return 0x286 }

// func (*QueryModules) Parse(reader io.Reader) (interface{}, error) {
// 	return &QueryModules{}, nil
// }

// func (msg *QueryModules) Write(writer io.Writer) {}

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

func (*QueryModulesResponse) Parse(reader io.Reader) (interface{}, error) {
	var raw QueryModulesResponseRaw
	if err := binary.Read(reader, binary.BigEndian, &raw); err != nil {
		log.Printf("QueryModulesResponse.Parse(): read failed", err)
		return nil, err
	}

	r := &QueryModulesResponse{
		ControlledDeviceSubnetID: raw.ControlledDeviceSubnetID,
		ControlledDeviceID: raw.ControlledDeviceID,
		DeviceCategory: raw.DeviceCategory,
	}
	switch r.DeviceCategory {
	case QUERY_MODULES_DEV_DIMMER, QUERY_MODULES_DEV_RELAY:
		r.ChannelNo = raw.Param1
	case QUERY_MODULES_DEV_HVAC:
		r.HVACSubnetID = raw.Param1
		r.HVACDeviceID = raw.Param2
	case QUERY_MODULES_DEV_SENSORS, QUERY_MODULES_DEV_Z_AUDIO:
		break
	default:
		return nil, fmt.Errorf("bad flag QueryModulesResponse DeviceCategory field %02x",
			r.DeviceCategory)
	}
	return r, nil
}

func (msg *QueryModulesResponse) Write(writer io.Writer) {
	raw := QueryModulesResponseRaw{
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
		/* FIXME: there should be a way to fail without panicking for this func */
	}
	binary.Write(writer, binary.BigEndian, &raw)
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
	Unknown uint8 // spec says there's no such field, but it's present
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
	CommandSubnetID int8
	CommandDeviceID uint8
	ChannelNo uint8
	Level uint8
	Duration uint16
}

func (*QueryPanelButtonAssignmentResponse) Opcode() uint16 { return 0xe001 }

// ------

var modeList []string = []string {
	"Invalid",
	"SingleOnOff",
	"SingleOn",
	"SingleOff",
	"CombinationOn",
	"CombinationOff",
	"PressOnReleaseOff",
	"CombinationOnOff",
	"SeparateLeftRightPressOnReleaseOff",
	"SeparateLeftRightCombinationOnOff",
	"LeftOffRightOn",
}

func ButtonModeStringToByte(mode string) uint8 {
	for i, v := range modeList {
		if mode == v {
			return uint8(i)
		}
	}
	return 0
}

func ButtonModeByteToString(rawMode uint8) string {
	if int(rawMode) > len(modeList) {
		return modeList[0] // invalid mode
	}
	return modeList[int(rawMode)]
}

// ------

type AssignPanelButton struct {
	ButtonNo uint8
	FunctionNo uint8
	Command uint8
	CommandSubnetID int8
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
	Modes []string
}

func (*SetPanelButtonModes) Opcode() uint16 { return 0xe00a }

func (*SetPanelButtonModes) Parse(reader io.Reader) (interface{}, error) {
	rawModes := make([]uint8, SET_PANEL_BUTTON_MODES_COUNT)
	if _, err := io.ReadFull(reader, rawModes); err != nil {
		log.Printf("SetPanelButtonModes.Parse(): read failed", err)
		return nil, err
	}

	r := &SetPanelButtonModes{make([]string, SET_PANEL_BUTTON_MODES_COUNT)}
	for i, rawMode := range rawModes {
		r.Modes[i] = ButtonModeByteToString(rawMode)
	}

	return r, nil
}

func (msg *SetPanelButtonModes) Write(writer io.Writer) {
	rawModes := make([]uint8, SET_PANEL_BUTTON_MODES_COUNT)
	for i, mode := range msg.Modes {
		rawModes[i] = ButtonModeStringToByte(mode)
	}
	binary.Write(writer, binary.BigEndian, rawModes)
}

// ------

type SetPanelButtonModesResponse struct {
	Success bool
}

type SetPanelButtonModesResponseRaw struct {
	Flag uint8
}

func (*SetPanelButtonModesResponse) Opcode() uint16 { return 0xe00b }

func (*SetPanelButtonModesResponse) Parse(reader io.Reader) (interface{}, error) {
	var rr SetPanelButtonModesResponseRaw
	if err := binary.Read(reader, binary.BigEndian, &rr); err != nil {
		log.Printf("SetPanelButtonModesResponseRaw.Parse(): error reading the header: %v", err)
		return nil, err
	}
	if success, err := SuccessFlagToBool(rr.Flag); err != nil {
		return nil, err
	} else {
		return &SetPanelButtonModesResponse{success}, nil
	}
}

func (msg *SetPanelButtonModesResponse) Write(writer io.Writer) {
	flag := BoolToSuccessFlag(msg.Success)
	rr := SetPanelButtonModesResponseRaw{flag}
	binary.Write(writer, binary.BigEndian, &rr)
}

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
}
