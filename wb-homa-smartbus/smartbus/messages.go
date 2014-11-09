package smartbus

import (
	"io"
	"log"
	"encoding/binary"
	"fmt"
)

const (
	LIGHT_LEVEL_OFF = 0
	LIGHT_LEVEL_ON = 100
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
	var hdr SingleChannelControlResponseHeaderRaw
	if err := binary.Read(reader, binary.BigEndian, &hdr); err != nil {
		log.Printf("SingleChannelControlResponse.Parse(): error reading the header: %v", err)
		return nil, err
	}

	var success bool
	switch hdr.Flag {
	case 0xf8:
		success = true
	case 0xf5:
		success = false
	default:
		return nil, fmt.Errorf("bad flag SingleChannelControlResponse flag value %02x",
			hdr.Flag)
	}

	if status, err := ReadChannelStatus(reader); err != nil {
		log.Printf("ParseSingleChannelControlResponse: error reading status: %v", err)
		return nil, err
	} else {
		return &SingleChannelControlResponse{hdr.ChannelNo, success, hdr.Level, status}, nil
	}
}

func (msg *SingleChannelControlResponse) Write(writer io.Writer) {
	flag := uint8(0xf8)
	if !msg.Success {
		flag = 0xf5
	}
	hdr := SingleChannelControlResponseHeaderRaw{msg.ChannelNo, flag, msg.Level}
	binary.Write(writer, binary.BigEndian, &hdr)
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

// -----

func init () {
	RegisterMessage(func () Message { return new(SingleChannelControlCommand) })
	RegisterMessage(func () Message { return new(SingleChannelControlResponse) })
	RegisterMessage(func () Message { return new(ZoneBeastBroadcast) })
}
