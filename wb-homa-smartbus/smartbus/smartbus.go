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

const (
	BROADCAST_SUBNET = 0xff
	BROADCAST_DEVICE = 0xff
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

type MutexLike interface {
	Lock()
	Unlock()
}

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

func WriteMessageRaw(writer io.Writer, header MessageHeader, writeMsg func(writer io.Writer)) {
	buf := bytes.NewBuffer(make([]byte, 0, 128))
	binary.Write(buf, binary.BigEndian, uint16(0xaaaa)) // signature
	binary.Write(buf, binary.BigEndian, uint8(0)) // len placeholder
	binary.Write(buf, binary.BigEndian, header)
	writeMsg(buf)
	bs := buf.Bytes()
	bs[2] = uint8(len(bs)) // minus 2 bytes of signature, but plus 2 bytes of crc
	binary.Write(buf, binary.BigEndian, crc16(bs[2:]))
	log.Printf("SEND:\n%s", hex.Dump(buf.Bytes()))
	// writing the buffer in parts may cause missed packets
	writer.Write(buf.Bytes())
}

func WriteMessage(writer io.Writer, fullMsg SmartbusMessage) {
	header := fullMsg.Header // make a copy because Opcode field is modified
	msg := fullMsg.Message.(Message)
	header.Opcode = msg.Opcode()

	WriteMessageRaw(writer, header, func (writer io.Writer) {
		msgWriter, isWriter := msg.(MessageWriter)
		if isWriter {
			msgWriter.Write(writer)
		} else {
			binary.Write(writer, binary.BigEndian, msg)
		}
	})
}

func ReadSync(reader io.Reader, mutex MutexLike) error {
	var b byte
	for {
		if err := binary.Read(reader, binary.BigEndian, &b); err != nil {
			return err
		}
		if b != 0xaa {
			log.Printf("unsync byte 0: %02x", b)
			continue
		}

		mutex.Lock()
		if err := binary.Read(reader, binary.BigEndian, &b); err != nil {
			mutex.Unlock()
			return err
		}
		if b == 0xaa {
			break
		}

		log.Printf("unsync byte 1: %02x", b)
		mutex.Unlock()
	}
	// the mutex is locked here
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

func ReadSmartbusPacket(reader io.Reader) ([]byte, bool) {
	var l byte
	if err := binary.Read(reader, binary.BigEndian, &l); err != nil {
		log.Printf("error reading packet length: %v", err)
		return nil, false
	}
	if l < MIN_PACKET_SIZE {
		log.Printf("packet too short")
		return nil, true
	}
	var packet []byte = make([]byte, l)
	packet[0] = l
	if _, err := io.ReadFull(reader, packet[1:]); err != nil {
		log.Printf("error reading packet body (%d bytes): %v", l, err)
		return nil, false
	}

	crc := crc16(packet[:len(packet) - 2])
	if crc != binary.BigEndian.Uint16(packet[len(packet) - 2:]) {
		log.Printf("bad crc")
		return nil, true
	}

	return packet, true
}

func ReadSmartbusRaw(reader io.Reader, mutex MutexLike, packetHandler func (packet []byte)) {
	var err error
	defer func () {
		switch {
		case err == io.EOF || err == io.ErrUnexpectedEOF:
			log.Printf("eof reached")
			return
		case err != nil:
			log.Printf("NOTE: connection error: %s", err)
			return
		}
	}()
	for {
		if err = ReadSync(reader, mutex); err != nil {
			log.Printf("ReadSync error: %v", err)
			// the mutex is not locked if ReadSync failed
			break
		}

		// the mutex is locked here
		packet, cont := ReadSmartbusPacket(reader)
		mutex.Unlock()
		if packet != nil {
			packetHandler(packet)
		}
		if !cont {
			break
		}
	}
}

func ReadSmartbus(reader io.Reader, mutex MutexLike, ch chan SmartbusMessage) {
	ReadSmartbusRaw(reader, mutex, func (packet[] byte) {
		if msg, err := ParsePacket(packet); err != nil {
			log.Printf("failed to parse smartbus packet: %s", err)
		} else {
			ch <- *msg
		}
	})
	close(ch)
}

func WriteSmartbus(writer io.Writer, mutex MutexLike, ch chan SmartbusMessage) {
	for msg := range ch {
		mutex.Lock()
		WriteMessage(writer, msg)
		mutex.Unlock()
	}
}
