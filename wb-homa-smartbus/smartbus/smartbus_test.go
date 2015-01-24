package smartbus

import (
	"io"
	"net"
	"time"
	"bytes"
	"encoding/hex"
	"testing"
	"github.com/stretchr/testify/assert"
)

const (
	SAMPLE_SUBNET = 0x01
	SAMPLE_DDP_DEVICE_ID = 0x14
	SAMPLE_DDP_DEVICE_TYPE = 0x0095
	SAMPLE_RELAY_DEVICE_ID = 0x1c
	SAMPLE_RELAY_DEVICE_TYPE = 0x139c
	SAMPLE_APP_SUBNET = 0x03
	SAMPLE_APP_DEVICE_ID = 0xfe
	SAMPLE_APP_DEVICE_TYPE = 0xfffe
)

type MessageTestCase struct {
	Name string
	Opcode uint16
	Packet []uint8
	SmartbusMessage SmartbusMessage
}

// TBD: include string representation tests

var messageTestCases []MessageTestCase = []MessageTestCase {
	{
		Name: "SingleChannelControlCommand",
		Opcode: 0x0031,
		SmartbusMessage: SmartbusMessage{
			MessageHeader{
				OrigSubnetID: SAMPLE_SUBNET,
				OrigDeviceID: SAMPLE_DDP_DEVICE_ID,
				OrigDeviceType: SAMPLE_DDP_DEVICE_TYPE,
				TargetSubnetID: SAMPLE_SUBNET,
				TargetDeviceID: SAMPLE_RELAY_DEVICE_ID,
			},
			&SingleChannelControlCommand{
				ChannelNo: 7,
				Level: 100,
				Duration: 0,
			},
		},
		Packet: []uint8{
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
				OrigDeviceID: SAMPLE_RELAY_DEVICE_ID,
				OrigDeviceType: SAMPLE_RELAY_DEVICE_TYPE,
				TargetSubnetID: BROADCAST_SUBNET,
				TargetDeviceID: BROADCAST_DEVICE,
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
		Packet: []uint8{
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
				OrigDeviceID: SAMPLE_RELAY_DEVICE_ID,
				OrigDeviceType: SAMPLE_RELAY_DEVICE_TYPE,
				TargetSubnetID: BROADCAST_SUBNET,
				TargetDeviceID: BROADCAST_DEVICE,
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
	{
		Name: "QueryModules",
		Opcode: 0x0286,
		SmartbusMessage: SmartbusMessage{
			MessageHeader{
				OrigSubnetID: SAMPLE_SUBNET,
				OrigDeviceID: SAMPLE_DDP_DEVICE_ID,
				OrigDeviceType: SAMPLE_DDP_DEVICE_TYPE,
				TargetSubnetID: SAMPLE_SUBNET,
				TargetDeviceID: BROADCAST_SUBNET,
			},
			&QueryModules{},
		},
		Packet: [] byte {
			0xaa, // Sync1
			0xaa, // Sync2
			0x0b, // Len
			0x01, // OrigSubnetID
			0x14, // OrigDeviceID
			0x00, // OrigDeviceType(hi)
			0x95, // OrigDeviceType(lo)
			0x02, // Opcode(hi)
			0x86, // Opcode(lo)
			0x01, // TargetSubnetID
			0xff, // TargetDeviceID
			0xf9, // CRC(hi)
			0x5b, // CRC(lo)
		},
	},
	{
		Name: "QueryModulesResponse",
		Opcode: 0x0287,
		SmartbusMessage: SmartbusMessage{
			MessageHeader{
				OrigSubnetID: SAMPLE_SUBNET,
				OrigDeviceID: SAMPLE_RELAY_DEVICE_ID,
				OrigDeviceType: SAMPLE_RELAY_DEVICE_TYPE,
				TargetSubnetID: SAMPLE_SUBNET,
				TargetDeviceID: SAMPLE_DDP_DEVICE_ID,
			},
			&QueryModulesResponse{
			        ControlledDeviceSubnetID: SAMPLE_SUBNET,
				ControlledDeviceID: SAMPLE_RELAY_DEVICE_ID,
				DeviceCategory: QUERY_MODULES_DEV_RELAY,
				ChannelNo: 0x0a,
			},
		},
		Packet: [] byte {
			0xaa, // Sync1
			0xaa, // Sync2
			0x12, // Len
			0x01, // OrigSubnetID
			0x1c, // OrigDeviceID
			0x13, // OrigDeviceType(hi)
			0x9c, // OrigDeviceType(lo)
			0x02, // Opcode(hi)
			0x87, // Opcode(lo)
			0x01, // TargetSubnetID
			0x14, // TargetDeviceID
			0x01, // [data] ControlledDeviceSubnetID
			0x1c, // [data] ControlledDeviceID
			0x02, // [data] DeviceCategory: 2=Relay
			0x0a, // [data] Param1 = Relay channel no (for relay type)
			0x64, // [data] Param2 = N/A (but seems like 100 means 'currently on')
			0x00, // [data] Param3 = N/A
			0x00, // [data] Param4 = N/A
			0x0c, // CRC(hi)
			0x38, // CRC(lo)
		},
	},
	{
		Name: "PanelControlResponse",
		Opcode: 0xe3d9,
		SmartbusMessage: SmartbusMessage{
			MessageHeader{
				OrigSubnetID: SAMPLE_SUBNET,
				OrigDeviceID: SAMPLE_DDP_DEVICE_ID,
				OrigDeviceType: SAMPLE_DDP_DEVICE_TYPE,
				TargetSubnetID: BROADCAST_SUBNET,
				TargetDeviceID: BROADCAST_DEVICE,
			},
			&PanelControlResponse{
				Type: PANEL_CONTROL_TYPE_COOLING_SET_POINT,
				Value: 0x19,
			},
		},
		Packet: [] byte {
			0xaa, // Sync1
			0xaa, // Sync2
			0x0d, // Len
			0x01, // OrigSubnetID
			0x14, // OrigDeviceID
			0x00, // OrigDeviceType(hi)
			0x95, // OrigDeviceType(lo)
			0xe3, // Opcode(hi)
			0xd9, // Opcode(lo)
			0xff, // TargetSubnetID
			0xff, // TargetDeviceID
			0x04, // [data] Type
			0x19, // [data] Value
			0x7f, // CRC(hi)
			0xab, // CRC(lo)
		},
	},
	{
		Name: "QueryFanController",
		Opcode: 0x0033,
		SmartbusMessage: SmartbusMessage{
			MessageHeader{
				OrigSubnetID: SAMPLE_SUBNET,
				OrigDeviceID: SAMPLE_DDP_DEVICE_ID,
				OrigDeviceType: SAMPLE_DDP_DEVICE_TYPE,
				TargetSubnetID: SAMPLE_SUBNET,
				TargetDeviceID: SAMPLE_RELAY_DEVICE_ID,
			},
			&QueryFanController{
				Index: 0x07,
			},
		},
		Packet: [] byte {
			0xaa, // Sync1
			0xaa, // Sync2
			0x0c, // Len
			0x01, // OrigSubnetID
			0x14, // OrigDeviceID
			0x00, // OrigDeviceType(hi)
			0x95, // OrigDeviceType(lo)
			0x00, // Opcode(hi)
			0x33, // Opcode(lo)
			0x01, // TargetSubnetID
			0x1c, // TargetDeviceID
			0x07, // [data] Index -- undocumented (doc says there's no such field)
			0x05, // CRC(hi)
			0xdd, // CRC(lo)
		},
	},
	{
		Name: "QueryPanelButtonAssignment",
		Opcode: 0xe000,
		SmartbusMessage: SmartbusMessage{
			MessageHeader{
				OrigSubnetID: SAMPLE_APP_SUBNET,
				OrigDeviceID: SAMPLE_APP_DEVICE_ID,
				OrigDeviceType: SAMPLE_APP_DEVICE_TYPE,
				TargetSubnetID: SAMPLE_SUBNET,
				TargetDeviceID: SAMPLE_DDP_DEVICE_ID,
			},
			&QueryPanelButtonAssignment{
				ButtonNo: 2,
				FunctionNo: 1,
			},
		},
		Packet: [] byte {
			0xaa, // Sync1
			0xaa, // Sync2
			0x0d, // Len
			0x03, // OrigSubnetID
			0xfe, // OrigDeviceID
			0xff, // OrigDeviceType(hi)
			0xfe, // OrigDeviceType(lo)
			0xe0, // Opcode(hi)
			0x00, // Opcode(lo)
			0x01, // TargetSubnetID
			0x14, // TargetDeviceID
			0x02, // [data] ButtonNo
			0x01, // [data] FunctionNo
			0x60, // CRC(hi)
			0x62, // CRC(lo)
		},
	},
	{
		Name: "QueryPanelButtonAssignmentResponse",
		Opcode: 0xe001,
		SmartbusMessage: SmartbusMessage{
			MessageHeader{
				OrigSubnetID: SAMPLE_SUBNET,
				OrigDeviceID: SAMPLE_DDP_DEVICE_ID,
				OrigDeviceType: SAMPLE_DDP_DEVICE_TYPE,
				TargetSubnetID: SAMPLE_APP_SUBNET,
				TargetDeviceID: SAMPLE_APP_DEVICE_ID,
			},
			&QueryPanelButtonAssignmentResponse{
				ButtonNo: 2,
				FunctionNo: 1,
				Command: 0x59,
				CommandSubnetID: 0x01,
				CommandDeviceID: 0x99,
				ChannelNo: 8,
				Level: 100,
				Duration: 0,
			},
		},
		Packet: [] byte {
			0xaa, // Sync1
			0xaa, // Sync2
			0x14, // Len
			0x01, // OrigSubnetID
			0x14, // OrigDeviceID
			0x00, // OrigDeviceType(hi)
			0x95, // OrigDeviceType(lo)
			0xe0, // Opcode(hi)
			0x01, // Opcode(lo)
			0x03, // TargetSubnetID
			0xfe, // TargetDeviceID
			0x02, // [data] ButtonNo
			0x01, // [data] FunctionNo
			0x59, // [data] Command
			0x01, // [data] CommandSubnetID
			0x99, // [data] CommandDeviceID
			0x08, // [data] ChannelNo
			0x64, // [data] Level
			0x00, // [data] Duration(hi)
			0x00, // [data] Duration(lo)
			0x54, // CRC(hi)
			0x28, // CRC(lo)
		},
	},
	{
		Name: "AssignPanelButton",
		Opcode: 0xe002,
		SmartbusMessage: SmartbusMessage{
			MessageHeader{
				OrigSubnetID: SAMPLE_APP_SUBNET,
				OrigDeviceID: SAMPLE_APP_DEVICE_ID,
				OrigDeviceType: SAMPLE_APP_DEVICE_TYPE,
				TargetSubnetID: SAMPLE_SUBNET,
				TargetDeviceID: SAMPLE_DDP_DEVICE_ID,
			},
			&AssignPanelButton{
				ButtonNo: 1,
				FunctionNo: 1,
				Command: 0x59,
				CommandSubnetID: 0x01,
				CommandDeviceID: 0x99,
				ChannelNo: 1,
				Level: 100,
				Duration: 0,
				Unknown: 0,
			},
		},
		Packet: [] byte {
			0xaa, // Sync1
			0xaa, // Sync2
			0x15, // Len
			0x03, // OrigSubnetID
			0xfe, // OrigDeviceID
			0xff, // OrigDeviceType(hi)
			0xfe, // OrigDeviceType(lo)
			0xe0, // Opcode(hi)
			0x02, // Opcode(lo)
			0x01, // TargetSubnetID
			0x14, // TargetDeviceID
			0x01, // [data] ButtonNo
			0x01, // [data] FunctionNo
			0x59, // [data] Command
			0x01, // [data] CommandSubnetID
			0x99, // [data] CommandDeviceID
			0x01, // [data] ChannelNo
			0x64, // [data] Level
			0x00, // [data] Duration(hi)
			0x00, // [data] Duration(lo)
			0x00, // [data] Unknown
			0x50, // CRC(hi)
			0x13, // CRC(lo)
		},
	},
	{
		Name: "AssignPanelButtonResponse",
		Opcode: 0xe003,
		SmartbusMessage: SmartbusMessage{
			MessageHeader{
				OrigSubnetID: SAMPLE_SUBNET,
				OrigDeviceID: SAMPLE_DDP_DEVICE_ID,
				OrigDeviceType: SAMPLE_DDP_DEVICE_TYPE,
				TargetSubnetID: SAMPLE_APP_SUBNET,
				TargetDeviceID: SAMPLE_APP_DEVICE_ID,
			},
			&AssignPanelButtonResponse{
				ButtonNo: 1,
				FunctionNo: 1,
			},
		},
		Packet: [] byte {
			0xaa, // Sync1
			0xaa, // Sync2
			0x0d, // Len
			0x01, // OrigSubnetID
			0x14, // OrigDeviceID
			0x00, // OrigDeviceType(hi)
			0x95, // OrigDeviceType(lo)
			0xe0, // Opcode(hi)
			0x03, // Opcode(lo)
			0x03, // TargetSubnetID
			0xfe, // TargetDeviceID
			0x01, // [data] ButtonNo
			0x01, // [data] FunctionNo
			0x4b, // CRC(hi)
			0x84, // CRC(lo)
		},
	},
	{
		Name: "SetPanelButtonModes",
		Opcode: 0xe00a,
		SmartbusMessage: SmartbusMessage{
			MessageHeader{
				OrigSubnetID: SAMPLE_APP_SUBNET,
				OrigDeviceID: SAMPLE_APP_DEVICE_ID,
				OrigDeviceType: SAMPLE_APP_DEVICE_TYPE,
				TargetSubnetID: SAMPLE_SUBNET,
				TargetDeviceID: SAMPLE_DDP_DEVICE_ID,
			},
			&SetPanelButtonModes{
				Modes: [16]string {
					"Invalid",
					"SingleOnOff",
					"SingleOnOff",
					"SingleOnOff",
					"CombinationOn",
					"Invalid",
					"Invalid",
					"Invalid",
					"Invalid",
					"SingleOnOff",
					"SingleOnOff",
					"SingleOnOff",
					"SingleOnOff",
					"Invalid",
					"Invalid",
					"Invalid",
				},
			},
		},
		Packet: []byte {
			0xaa, // Sync1
			0xaa, // Sync2
			0x1b, // Len
			0x03, // OrigSubnetID
			0xfe, // OrigDeviceID
			0xff, // OrigDeviceType(hi)
			0xfe, // OrigDeviceType(lo)
			0xe0, // Opcode(hi)
			0x0a, // Opcode(lo)
			0x01, // TargetSubnetID
			0x14, // TargetDeviceID
			0x00, // [data] Modes[0]  = Invalid
			0x01, // [data] Modes[1]  = SingleOnOff
			0x01, // [data] Modes[2]  = SingleOnOff
			0x01, // [data] Modes[3]  = SingleOnOff
			0x04, // [data] Modes[4]  = CombinationOn
			0x00, // [data] Modes[5]  = Invalid
			0x00, // [data] Modes[6]  = Invalid
			0x00, // [data] Modes[7]  = Invalid
			0x00, // [data] Modes[8]  = Invalid
			0x01, // [data] Modes[9]  = SingleOnOff
			0x01, // [data] Modes[10] = SingleOnOff
			0x01, // [data] Modes[11] = SingleOnOff
			0x01, // [data] Modes[12] = SingleOnOff
			0x00, // [data] Modes[13] = Invalid
			0x00, // [data] Modes[14] = Invalid
			0x00, // [data] Modes[15] = Invalid
			0xb3, // CRC(hi)
			0xe9, // CRC(lo)
		},
	},
	{
		Name: "SetPanelButtonModesResponse",
		Opcode: 0xe00b,
		SmartbusMessage: SmartbusMessage{
			MessageHeader{
				OrigSubnetID: SAMPLE_SUBNET,
				OrigDeviceID: SAMPLE_DDP_DEVICE_ID,
				OrigDeviceType: SAMPLE_DDP_DEVICE_TYPE,
				TargetSubnetID: SAMPLE_APP_SUBNET,
				TargetDeviceID: SAMPLE_APP_DEVICE_ID,
			},
			&SetPanelButtonModesResponse{
				Success: true,
			},
		},
		Packet: []uint8{
			0xaa, // Sync1
			0xaa, // Sync2
			0x0c, // Len
			0x01, // OrigSubnetID
			0x14, // OrigDeviceID
			0x00, // OrigDeviceType(hi)
			0x95, // OrigDeviceType(lo)
			0xe0, // Opcode(hi)
			0x0b, // Opcode(lo)
			0x03, // TargetSubnetID
			0xfe, // TargetDeviceID
			0xf8, // [data] Flag (0xf8=ok, 0xf5=fail)
			0x91, // CRC(hi)
			0xbb, // CRC(lo)
		},
	},
}

// http://smarthomebus.com/dealers/Protocols/Smart%20Bus%20Commands%20V5.10.pdf page 88
// 00000000  1b 01 1c 13 9c 00 34 01  14 0f 00 00 64 00 00 00  |......4.....d...|
// 00000010  00 00 00 00 00 00 00 00  00 e6 34                 |..........4|
// opcode 0x0034 - response to QueryFanController

// 00000000  0b 01 14 00 95 ff 00 ff  ff e6 a4                 |...........|
// opcode 0xff00 seems to be some kind of broadcast query no one answers

// http://smarthomebus.com/dealers/Protocols/9in1%20Protocol%20v1.1.pdf p.20
// 00000000  15 01 14 00 95 02 84 01  ff 01 14 53 03 00 00 00  |...........S....|
// 00000010  00 30 c3 4f ff                                    |.0.O.|
// opcode 0x284 - check for adddress conflict

type FakeMutex struct {
	t *testing.T
	locked bool
	lockCount int
}

func NewFakeMutex(t *testing.T) *FakeMutex {
	return &FakeMutex{t, false, 0}
}

func (mutex *FakeMutex) Lock() {
	assert.False(mutex.t, mutex.locked, "recursive locking detected")
	mutex.locked = true
	mutex.lockCount++
}

func (mutex *FakeMutex) Unlock() {
	assert.True(mutex.t, mutex.locked, "unlocking a non-locked mutex")
	mutex.locked = false
}

func (mutex *FakeMutex) VerifyLocked(count int, msg... interface{}) {
	assert.True(mutex.t, mutex.locked, msg...)
	assert.Equal(mutex.t, count, mutex.lockCount, msg...)
}

func (mutex *FakeMutex) VerifyUnlocked(count int, msg... interface{}) {
	assert.False(mutex.t, mutex.locked, msg...)
	assert.Equal(mutex.t, count, mutex.lockCount, msg...)
}

func (mutex *FakeMutex) WaitForUnlock() {
	for i := 0; i < 50; i++ {
		if (!mutex.locked) {
			return
		}
		time.Sleep(10 * time.Millisecond)
	}
	mutex.t.Fatalf("timed out waiting for the fake mutex to be unlocked")
}

func VerifyRead(t *testing.T, mtc MessageTestCase, msg SmartbusMessage) {
	if msg.Header.Opcode != mtc.Opcode {
		t.Fatalf("VerifyRead: %s: bad opcode in decoded frame: %04x instead of %04x",
			mtc.Name, msg.Header.Opcode, mtc.Opcode)
	}
	msg.Header.Opcode = 0 // for comparison

	assert.Equal(t, mtc.SmartbusMessage.Header, msg.Header, "VerifyRead: %s - header diff", mtc.Name)
	assert.Equal(t, mtc.SmartbusMessage.Message, msg.Message, "VerifyRead: %s - message", mtc.Name)
}

func VerifyReadSingle(t *testing.T, mtc MessageTestCase, ch chan SmartbusMessage) {
	var msg *SmartbusMessage
	for c := range ch {
		if msg == nil {
			msg = &c
		} else {
			t.Errorf("VerifyReadSingle: %s: more than one message received", mtc.Name)
		}
	}

	if msg == nil {
		t.Fatalf("VerifyReadSingle: no message received")
	}

	VerifyRead(t, mtc, *msg)
}

func VerifyWrite(t *testing.T, mtc MessageTestCase, bs []byte) {
	if !bytes.Equal(mtc.Packet, bs) {
		// Tbd: hex dump
		t.Fatalf("VerifyWrite: %s: bad packet\n EXP:\n%s ACT:\n%s\n",
			mtc.Name, hex.Dump(mtc.Packet), hex.Dump(bs))
	}
}

func TestSingleFrame(t *testing.T) {
	for _, mtc := range messageTestCases {
		p, r := io.Pipe()

		ch := make(chan SmartbusMessage)
		mtx := NewFakeMutex(t)
		go ReadSmartbus(p, mtx, ch)

		n, err := r.Write(mtc.Packet)
		r.Close()
		assert.Equal(t, nil, err)
		assert.Equal(t, len(mtc.Packet), n)

		VerifyReadSingle(t, mtc, ch)
		mtx.VerifyUnlocked(1, "unlocked after ReadSmartbus")

		p, r = io.Pipe()
		ch = make(chan SmartbusMessage)
		go WriteSmartbus(r, mtx, ch)
		ch <- mtc.SmartbusMessage
		bs := make([]byte, len(mtc.Packet))
		if _, err := io.ReadFull(p, bs); err != nil {
			t.Fatalf("failed to read the datagram")
		}
		VerifyWrite(t, mtc, bs)
		mtx.WaitForUnlock()
		mtx.VerifyUnlocked(2, "unlocked after WriteSmartbus")
		close(ch)
	}
}

func TestMultiRead(t *testing.T) {
	cap := 0
	for _, mtc := range messageTestCases {
		cap += len(mtc.Packet)
	}
	buf := bytes.NewBuffer(make([]uint8, 0, cap))
	for _, mtc := range messageTestCases {
		buf.Write(mtc.Packet)
	}

	ch := make(chan SmartbusMessage)
	mtx := NewFakeMutex(t)
	go ReadSmartbus(buf, mtx, ch)
	for _, mtc := range messageTestCases {
		msg := <- ch
		VerifyRead(t, mtc, msg)
	}
	mtx.VerifyUnlocked(len(messageTestCases), "unlocked after ReadSmartbus")

	for _ = range ch {
		t.Fatalf("got excess messages from the channel")
	}
}

func TestResync(t *testing.T) {
	bs := append([]uint8{
		0xaa, // Sync1
		0x00, // oops! bad sync
		0xaa, // Sync1
		0xaa, // Sync2
		0x05, // Len (too short)
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
		0x01, // [data] LightChannelNo
		0x00, // [data] Level (100 = On)
		0x00, // Duration(hi)
		0x00, // Duration(lo)
		0xff, // CRC(hi) - BAD!
		0xff, // CRC(lo) - BAD!
	}, messageTestCases[0].Packet...)
	r := bytes.NewBuffer(bs)
	ch := make(chan SmartbusMessage)
	mtx := NewFakeMutex(t)
	go ReadSmartbus(r, mtx, ch)
	VerifyReadSingle(t, messageTestCases[0], ch)
	mtx.VerifyUnlocked(4, "unlocked after ReadSmartbus") // one lock for each initial sync byte
}

func TestReadLocking(t *testing.T) {
	mtc := messageTestCases[0]
	p, r := io.Pipe()
	ch := make(chan SmartbusMessage)
	mtx := NewFakeMutex(t)
	go ReadSmartbus(p, mtx, ch)

	time.Sleep(100 * time.Millisecond)
	mtx.VerifyUnlocked(0, "initially unlocked")

	r.Write(mtc.Packet[:1])
	mtx.VerifyLocked(1, "lock after sync")

	r.Write(mtc.Packet[1:len(mtc.Packet) - 1])
	mtx.VerifyLocked(1, "still locked after partial packet")

	r.Write(mtc.Packet[len(mtc.Packet) - 1:])
	mtx.VerifyUnlocked(1, "unlocked after complete packet")

	r.Close()
	mtx.VerifyUnlocked(1, "unlocked after the pipe is closed")

	VerifyReadSingle(t, mtc, ch)
}

func parseChannelStatus(statusStr string) (status []bool) {
	status = make([]bool, len(statusStr))
	for i, c := range statusStr {
		status[i] = c == 'x'
	}
	return
}

type FakeHandler struct {
	Recorder
	MessageFormatter
}

func NewFakeHandler (t *testing.T) (handler *FakeHandler) {
	handler = &FakeHandler{}
	handler.InitRecorder(t)
	handler.AddMessage = func (format string, args... interface{}) {
		handler.Rec(format, args...)
	}
	return
}

func TestSmartbusEndpointSend(t *testing.T) {
	p, r := net.Pipe() // we need bidirectional pipe here

	handler := NewFakeHandler(t)
	conn := NewSmartbusConnection(NewStreamIO(p))
	ep := conn.MakeSmartbusEndpoint(SAMPLE_SUBNET, SAMPLE_DDP_DEVICE_ID, SAMPLE_DDP_DEVICE_TYPE)
	ep.Observe(handler)
	dev := ep.GetSmartbusDevice(SAMPLE_SUBNET, SAMPLE_RELAY_DEVICE_ID)

	dev.SingleChannelControl(7, LIGHT_LEVEL_ON, 0)

	bs := make([]byte, len(messageTestCases[0].Packet))
	if _, err := io.ReadFull(r, bs); err != nil {
		t.Fatalf("failed to read the datagram")
	}
	VerifyWrite(t, messageTestCases[0], bs)
	handler.Verify()

	conn.Close()
	r.Close()
}

func TestSmartbusEndpointReceive(t *testing.T) {
	p, r := net.Pipe()
	handler := NewFakeHandler(t)
	conn := NewSmartbusConnection(NewStreamIO(p))
	ep := conn.MakeSmartbusEndpoint(SAMPLE_SUBNET, SAMPLE_RELAY_DEVICE_ID, SAMPLE_RELAY_DEVICE_TYPE)
	ep.Observe(handler)

	r.Write(messageTestCases[0].Packet)
	handler.Verify("01/14 (type 0095) -> 01/1c: <SingleChannelControlCommand 7/100/0>")
	r.Write(messageTestCases[1].Packet)
	handler.Verify("01/1c (type 139c) -> ff/ff: <SingleChannelControlResponse 7/true/0/------x-------->")

	conn.Close()
	r.Close()
}

func TestSmartbusEndpointSendReceive(t *testing.T) {
	p, r := net.Pipe()

	ddp_handler := NewFakeHandler(t)
	conn1 := NewSmartbusConnection(NewStreamIO(p))
	ddp_ep := conn1.MakeSmartbusEndpoint(SAMPLE_SUBNET, SAMPLE_DDP_DEVICE_ID, SAMPLE_DDP_DEVICE_TYPE)
	ddp_ep.Observe(ddp_handler)
	ddp_to_relay_dev := ddp_ep.GetSmartbusDevice(SAMPLE_SUBNET, SAMPLE_RELAY_DEVICE_ID)
	ddp_to_app_dev := ddp_ep.GetSmartbusDevice(SAMPLE_APP_SUBNET, SAMPLE_APP_DEVICE_ID)
	ddp_to_all_dev := ddp_ep.GetBroadcastDevice()

	relay_handler := NewFakeHandler(t)
	conn2 := NewSmartbusConnection(NewStreamIO(r))
	relay_ep := conn2.MakeSmartbusEndpoint(SAMPLE_SUBNET, SAMPLE_RELAY_DEVICE_ID, SAMPLE_RELAY_DEVICE_TYPE)
	relay_ep.Observe(relay_handler)
	relay_to_ddp_dev := relay_ep.GetSmartbusDevice(SAMPLE_SUBNET, SAMPLE_DDP_DEVICE_ID)
	relay_to_all_dev := relay_ep.GetBroadcastDevice()

	app_handler := NewFakeHandler(t)
	app_ep := conn2.MakeSmartbusEndpoint(SAMPLE_APP_SUBNET, SAMPLE_APP_DEVICE_ID, SAMPLE_APP_DEVICE_TYPE)
	app_ep.Observe(app_handler)
	app_to_ddp_dev := app_ep.GetSmartbusDevice(SAMPLE_SUBNET, SAMPLE_DDP_DEVICE_ID)

	ddp_to_relay_dev.SingleChannelControl(7, LIGHT_LEVEL_ON, 0)
	relay_handler.Verify("01/14 (type 0095) -> 01/1c: <SingleChannelControlCommand 7/100/0>")

	relay_to_all_dev.SingleChannelControlResponse(7, true, LIGHT_LEVEL_ON,
		parseChannelStatus("---------------"))
 	ddp_handler.Verify("01/1c (type 139c) -> ff/ff: <SingleChannelControlResponse 7/true/100/--------------->")

	relay_to_all_dev.ZoneBeastBroadcast([]byte{ 0 }, parseChannelStatus("------x--------"))
	ddp_handler.Verify("01/1c (type 139c) -> ff/ff: <ZoneBeastBroadcast [0]/------x-------->")

	// DDP commands
	ddp_to_all_dev.QueryModules()
	// fixme: actually it goes to 01/1f
	app_handler.Verify("01/14 (type 0095) -> ff/ff: <QueryModules>")
	relay_handler.Verify("01/14 (type 0095) -> ff/ff: <QueryModules>")

	relay_to_ddp_dev.QueryModulesResponse(QUERY_MODULES_DEV_RELAY, 0x0a)
	// note that channel number (10) is stringified as decimal
	ddp_handler.Verify("01/1c (type 139c) -> 01/14: <QueryModulesResponse 01/1c/02/10/00/00>")

	ddp_to_app_dev.PanelControlResponse(PANEL_CONTROL_TYPE_COOLING_SET_POINT, 25)
	app_handler.Verify("01/14 (type 0095) -> 03/fe: <PanelControlResponse Cooling Set Point=25>")

	ddp_to_relay_dev.QueryFanController(7)
	relay_handler.Verify("01/14 (type 0095) -> 01/1c: <QueryFanController 7>")

	app_to_ddp_dev.QueryPanelButtonAssignment(1, 2)
	ddp_handler.Verify("03/fe (type fffe) -> 01/14: <QueryPanelButtonAssignment 1/2>")

	ddp_to_app_dev.QueryPanelButtonAssignmentResponse(
		1, 2, 0x59, 0x01, 0x99, 8, 100, 0)
	app_handler.Verify("01/14 (type 0095) -> 03/fe: " +
		"<QueryPanelButtonAssignmentResponse 1/2/59/01/99/8/100/0>")

	app_to_ddp_dev.AssignPanelButton(
		1, 1, 0x59, 0x01, 0x99, 1, 100, 0)
	ddp_handler.Verify("03/fe (type fffe) -> 01/14: " +
		"<AssignPanelButton 1/1/59/01/99/1/100/0/0>") // the last 0 is 'unknown' field

	ddp_to_app_dev.AssignPanelButtonResponse(1, 1)
	app_handler.Verify("01/14 (type 0095) -> 03/fe: <AssignPanelButtonResponse 1/1>")

	app_to_ddp_dev.SetPanelButtonModes(
		[16]string {
			"Invalid",
			"SingleOnOff",
			"SingleOnOff",
			"SingleOnOff",
			"CombinationOn",
			"Invalid",
			"Invalid",
			"Invalid",
			"Invalid",
			"SingleOnOff",
			"SingleOnOff",
			"SingleOnOff",
			"SingleOnOff",
			"Invalid",
			"Invalid",
			"Invalid",
		})
	ddp_handler.Verify("03/fe (type fffe) -> 01/14: " +
		"<SetPanelButtonModes " +
		"1/1:Invalid,1/2:SingleOnOff,1/3:SingleOnOff,1/4:SingleOnOff," +
		"2/1:CombinationOn,2/2:Invalid,2/3:Invalid,2/4:Invalid," +
		"3/1:Invalid,3/2:SingleOnOff,3/3:SingleOnOff,3/4:SingleOnOff," +
		"4/1:SingleOnOff,4/2:Invalid,4/3:Invalid,4/4:Invalid>")

	ddp_to_app_dev.SetPanelButtonModesResponse(true)
	app_handler.Verify("01/14 (type 0095) -> 03/fe: <SetPanelButtonModesResponse true>")

	conn1.Close()
	conn2.Close()
}
