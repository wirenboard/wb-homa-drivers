package smartbus

import (
	"io"
	"bytes"
	"encoding/binary"
	"encoding/hex"
	"log"
	"fmt"
)

const (
	MIN_PACKET_SIZE = 11 // excluding sync
)

type MessageHeader struct {
	OrigSubnetID uint8
	OrigDeviceID uint8
	OrigDeviceType uint16
	Opcode uint16
	TargetSubnetID uint8
	TargetDeviceID uint8
}

type Message interface {
	Opcode() uint16
}

type MessageWriter interface {
	Write(writer io.Writer)
}

type MessageParser interface {
	Parse(reader io.Reader) (interface{}, error)
}

type PacketParser func(io.Reader) (interface{}, error)

func MakeSimplePacketParser(construct func() Message) PacketParser {
	return func(reader io.Reader) (interface{}, error) {
		msg := construct()
		if err := binary.Read(reader, binary.BigEndian, msg); err != nil {
			log.Printf("error reading the message: %v", err)
			return nil, err
		}
		return msg, nil
	}
}

var recognizedMessages map[uint16]PacketParser = make(map[uint16]PacketParser)

func RegisterMessage(construct func () Message) {
	msg := construct()
	msgParser, hasParser := msg.(MessageParser)
	var parser PacketParser
	if hasParser {
		parser = func(reader io.Reader) (interface{}, error) {
			return msgParser.Parse(reader)
		}
	} else {
		parser = MakeSimplePacketParser(construct)
	}
	recognizedMessages[msg.Opcode()] = parser
}

type SmartbusMessage struct {
	Header MessageHeader
	Message interface{}
}

func WriteMessage(writer io.Writer, fullCmd SmartbusMessage) {
	header := fullCmd.Header // make a copy because Opcode field is modified
	msg := fullCmd.Message.(Message)
	buf := bytes.NewBuffer(make([]byte, 0, 128))
	binary.Write(buf, binary.BigEndian, uint8(0)) // len placeholder
	header.Opcode = msg.Opcode()
	binary.Write(buf, binary.BigEndian, header)

	msgWriter, isWriter := msg.(MessageWriter)
	if isWriter {
		msgWriter.Write(buf)
	} else {
		binary.Write(buf, binary.BigEndian, msg)
	}

	bs := buf.Bytes()
	bs[0] = uint8(len(bs) + 2)

	binary.Write(writer, binary.BigEndian, uint16(0xaaaa))
	binary.Write(writer, binary.BigEndian, bs)
	binary.Write(writer, binary.BigEndian, crc16(bs))
}

func ReadSync(reader io.Reader) error {
	var b byte
	n := 0
	for n < 2 {
		err := binary.Read(reader, binary.BigEndian, &b)
		switch {
		case err != nil:
			return err
		case b == 0xaa:
			n++
		default:
			log.Printf("unsync byte: %02x", b)
			n = 0
		}
	}
	return nil
}

func ParsePacket(packet []byte) (*SmartbusMessage, error) {
	log.Printf("packet:\n%s", hex.Dump(packet))
	buf := bytes.NewBuffer(packet[1:]) // skip len
	var header MessageHeader
	if err := binary.Read(buf, binary.BigEndian, &header); err != nil {
		return nil, err
	}
	msgParser, found := recognizedMessages[header.Opcode]
	if !found {
		return nil, fmt.Errorf("opcode %04x not recognized", header.Opcode)
	}

	if msg, err := msgParser(buf); err != nil {
		return nil, err
	} else {
		return &SmartbusMessage{header, msg}, nil
	}
}

func ReadSmartbus(reader io.Reader, ch chan SmartbusMessage) {
	var err error
	defer close(ch)
	defer func () {
		switch {
		case err == io.EOF || err == io.ErrUnexpectedEOF:
			log.Printf("eof reached")
			return
		case err != nil:
			panic(err)
		}
	}()
	for {
		if err = ReadSync(reader); err != nil {
			log.Printf("ReadSync error: %v", err)
			break
		}
		var l byte
		if err = binary.Read(reader, binary.BigEndian, &l); err != nil {
			log.Printf("error reading packet length: %v", err)
			break
		}
		if l < MIN_PACKET_SIZE {
			log.Printf("packet too short")
			continue
		}
		var packet []byte = make([]byte, l)
		packet[0] = l
		if _, err = io.ReadFull(reader, packet[1:]); err != nil {
			log.Printf("error reading packet body (%d bytes): %v", l, err)
			break
		}

		crc := crc16(packet[:len(packet) - 2])
		if crc != binary.BigEndian.Uint16(packet[len(packet) - 2:]) {
			log.Printf("bad crc")
			continue
		}

		var msg *SmartbusMessage
		if msg, err = ParsePacket(packet); err != nil {
			log.Printf("error parsing packet: %v", err)
			continue
		}
		ch <- *msg
	}
}

func WriteSmartbus(writer io.Writer, ch chan SmartbusMessage) {
	for msg := range ch {
		WriteMessage(writer, msg)
	}
}
