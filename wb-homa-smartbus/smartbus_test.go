package smartbus

import (
	"bytes"
	"encoding/hex"
	"testing"
	"github.com/stretchr/testify/assert"
)

const (
	SAMPLE_SUBNET = 0x01
	SAMPLE_ORIG_DEVICE_ID = 0x14
	SAMPLE_ORIG_DEVICE_TYPE = 0x0095
	SAMPLE_TARGET_DEVICE_ID = 0x1c
	SAMPLE_TARGET_DEVICE_TYPE = 0x139c
)

type FrameTestCase struct {
	Name string
	Opcode uint16
	Packet []byte
	SmartbusMessage SmartbusMessage
}

var frameTestCases []FrameTestCase = []FrameTestCase {
	{
		Name: "SingleChannelControlCommand",
		Opcode: 0x0031,
		SmartbusMessage: SmartbusMessage{
			MessageHeader{
				OrigSubnetID: SAMPLE_SUBNET,
				OrigDeviceID: SAMPLE_ORIG_DEVICE_ID,
				OrigDeviceType: SAMPLE_ORIG_DEVICE_TYPE,
				TargetSubnetID: SAMPLE_SUBNET,
				TargetDeviceID: SAMPLE_TARGET_DEVICE_ID,
			},
			&SingleChannelControlCommand{
				ChannelNo: 7,
				Level: 100,
				Duration: 0,
			},
		},
		Packet: []byte{
			0xaa, // Sync1
			0xaa, // Sync2
			0x0f, // Len
			0x01, // OrigSubnetID
			0x14, // OrigDeviceID
			0x00, // OrigDeviceType(hi)
			0x95, // OrigDeviceType(lo)
			0x00, // Opcode(hi)
			0x31, // Opcode(lo)
			0x01, // TargetSubnetID
			0x1c, // TargetDeviceID
			0x07, // [data] LightChannelNo
			0x64, // [data] Level (100 = On)
			0x00, // Duration(hi)
			0x00, // Duration(lo)
			0x60, // CRC(hi)
			0x66, // CRC(lo)
		},
	},
	{
		Name: "SingleChannelControlResponse",
		Opcode: 0x0032,
		SmartbusMessage: SmartbusMessage{
			MessageHeader{
				OrigSubnetID: SAMPLE_SUBNET,
				OrigDeviceID: SAMPLE_TARGET_DEVICE_ID,
				OrigDeviceType: SAMPLE_TARGET_DEVICE_TYPE,
				TargetSubnetID: 0xff, // broadcast
				TargetDeviceID: 0xff, // broadcast
			},
			&SingleChannelControlResponse{
				ChannelNo: 7,
				Success: true,
				Level: 0,
				ChannelStatus: []bool{
					false, false, false, false, false, false, true, false,
					false, false, false, false, false, false, false,
				},
			},
		},
		Packet: []byte{
			0xaa, // Sync1
			0xaa, // Sync2
			0x11, // Len
			0x01, // OrigSubnetID
			0x1c, // OrigDeviceID
			0x13, // OrigDeviceType(hi)
			0x9c, // OrigDeviceType(lo)
			0x00, // Opcode(hi)
			0x32, // Opcode(lo)
			0xff, // TargetSubnetID
			0xff, // TargetDeviceID
			0x07, // [data] LightChannelNo
			0xf8, // [data] Flag (0xf8=ok, 0xf5=fail)
			0x00, // [data] Level (0=Off, 100=On)
			0x0f, // [data] NumberOfChannels
			0x40, // [data] <channel data>
			0x00, // [data] <channel data>
			0x49, // CRC(hi)
			0x59, // CRC(lo)
		},
	},
	{
		Name: "ZoneBeastBroadcast",
		Opcode: 0xefff,
		SmartbusMessage: SmartbusMessage{
			MessageHeader{
				OrigSubnetID: SAMPLE_SUBNET,
				OrigDeviceID: SAMPLE_TARGET_DEVICE_ID,
				OrigDeviceType: SAMPLE_TARGET_DEVICE_TYPE,
				TargetSubnetID: 0xff, // broadcast
				TargetDeviceID: 0xff, // broadcast
			},
			&ZoneBeastBroadcast{
				ZoneStatus: []uint8 { 0 },
				ChannelStatus: []bool{
					false, false, false, false, true, false, false, false,
					false, false, false, false, false, false, false,
				},
			},
		},
		Packet: [] byte {
			0xaa, // Sync1
			0xaa, // Sync2
			0x10, // Len
			0x01, // OrigSubnetID
			0x1c, // OrigDeviceID
			0x13, // OrigDeviceType(hi)
			0x9c, // OrigDeviceType(lo)
			0xef, // Opcode(hi)
			0xff, // Opcode(lo)
			0xff, // TargetSubnetID
			0xff, // TargetDeviceID
			0x01, // [data] NumberOfZones
			0x00, // [data] <zone 0 status> (0 = sequence, >0 = scene)
			0x0f, // [data] NumberOfChannels
			0x10, // [data] <channel data>
			0x00, // [data] <channel data>
			0x27, // CRC(hi)
			0xf2, // CRC(lo)
		},
	},
}

func VerifyRead(t *testing.T, ftc FrameTestCase) {
	r := bytes.NewBuffer(ftc.Packet)
	ch := make(chan SmartbusMessage)

	go ReadSmartbus(r, ch)
	var msg *SmartbusMessage
	for c := range ch {
		if msg == nil {
			msg = &c
		} else {
			t.Errorf("VerifyRead: %s: more than one message received", ftc.Name)
		}
	}

	if msg == nil {
		t.Fatalf("VerifyRead: no message received")
	}

	if msg.Header.Opcode != ftc.Opcode {
		t.Fatalf("VerifyRead: %s: bad opcode in decoded frame: %04x instead of %04x",
			ftc.Name, msg.Header.Opcode, ftc.Opcode)
	}
	msg.Header.Opcode = 0 // for comparison

	assert.Equal(t, ftc.SmartbusMessage.Header, msg.Header, "VerifyRead: %s - header diff", ftc.Name)
	assert.Equal(t, ftc.SmartbusMessage.Message, msg.Message, "VerifyRead: %s - message", ftc.Name)
}

func VerifyWrite(t *testing.T, ftc FrameTestCase) {
	ch := make(chan SmartbusMessage)
	w := bytes.NewBuffer(make([]byte, 0, 128))

	go WriteSmartbus(w, ch)
	ch <- ftc.SmartbusMessage
	close(ch)

	if !bytes.Equal(ftc.Packet, w.Bytes()) {
		// Tbd: hex dump
		t.Fatalf("VerifyWrite: %s: bad packet\n EXP:\n%s ACT:\n%s\n",
			ftc.Name, hex.Dump(ftc.Packet), hex.Dump(w.Bytes()))
	}
}

func TestSingleFrame(t *testing.T) {
	for _, ftc := range frameTestCases {
		VerifyRead(t, ftc)
		VerifyWrite(t, ftc)
	}
}

// TBD: test resync, short packets etc.
// TBD: only decode messages with our own address or broadcast address as the target
// TBD: test invalid CRC
// TBD: report bad sync/len/etc.
// TBD: sync between R/W goroutines -- don't attempt writing while reading
// TBD: test cases may be used for higher level API testing, too
//      (just refer to them by name; perhaps should rename frameTestCases var)
// TBD: make sure unrecognized messages are skipped
