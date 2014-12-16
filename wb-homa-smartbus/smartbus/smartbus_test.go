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
	SAMPLE_ORIG_DEVICE_ID = 0x14
	SAMPLE_ORIG_DEVICE_TYPE = 0x0095
	SAMPLE_TARGET_DEVICE_ID = 0x1c
	SAMPLE_TARGET_DEVICE_TYPE = 0x139c
)

type MessageTestCase struct {
	Name string
	Opcode uint16
	Packet []uint8
	SmartbusMessage SmartbusMessage
}

var messageTestCases []MessageTestCase = []MessageTestCase {
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
	ep := conn.MakeSmartbusEndpoint(SAMPLE_SUBNET, SAMPLE_ORIG_DEVICE_ID, SAMPLE_ORIG_DEVICE_TYPE)
	ep.Observe(handler)
	dev := ep.GetSmartbusDevice(SAMPLE_SUBNET, SAMPLE_TARGET_DEVICE_ID)

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
	ep := conn.MakeSmartbusEndpoint(SAMPLE_SUBNET, SAMPLE_TARGET_DEVICE_ID, SAMPLE_TARGET_DEVICE_TYPE)
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

	handler1 := NewFakeHandler(t)
	conn1 := NewSmartbusConnection(NewStreamIO(p))
	ep1 := conn1.MakeSmartbusEndpoint(SAMPLE_SUBNET, SAMPLE_ORIG_DEVICE_ID, SAMPLE_ORIG_DEVICE_TYPE)
	ep1.Observe(handler1)
	dev1 := ep1.GetSmartbusDevice(SAMPLE_SUBNET, SAMPLE_TARGET_DEVICE_ID)

	handler2 := NewFakeHandler(t)
	conn2 := NewSmartbusConnection(NewStreamIO(r))
	ep2 := conn2.MakeSmartbusEndpoint(SAMPLE_SUBNET, SAMPLE_TARGET_DEVICE_ID, SAMPLE_TARGET_DEVICE_TYPE)
	ep2.Observe(handler2)
	dev2 := ep2.GetBroadcastDevice()

	dev1.SingleChannelControl(7, LIGHT_LEVEL_ON, 0)
	handler2.Verify("01/14 (type 0095) -> 01/1c: <SingleChannelControlCommand 7/100/0>")

	dev2.SingleChannelControlResponse(7, true, LIGHT_LEVEL_ON,
		parseChannelStatus("---------------"))
 	handler1.Verify("01/1c (type 139c) -> ff/ff: <SingleChannelControlResponse 7/true/100/--------------->")

	dev2.ZoneBeastBroadcast([]byte{ 0 }, parseChannelStatus("------x--------"))
	handler1.Verify("01/1c (type 139c) -> ff/ff: <ZoneBeastBroadcast [0]/------x-------->")

	conn1.Close()
	conn2.Close()
}
