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

type CommandHeader struct {
	OrigSubnetID uint8
	OrigDeviceID uint8
	OrigDeviceType uint16
	Opcode uint16
	TargetSubnetID uint8
	TargetDeviceID uint8
}

type Command interface {
	Opcode() uint16
}

type CommandWriter interface {
	Write(writer io.Writer)
}

type CommandParser interface {
	Parse(reader io.Reader) (interface{}, error)
}

type PacketParser func(io.Reader) (interface{}, error)

func MakeSimplePacketParser(construct func() Command) PacketParser {
	return func(reader io.Reader) (interface{}, error) {
		cmd := construct()
		if err := binary.Read(reader, binary.BigEndian, cmd); err != nil {
			log.Printf("error reading the command: %v", err)
			return nil, err
		}
		return cmd, nil
	}
}

var recognizedCommands map[uint16]PacketParser = make(map[uint16]PacketParser)

func RegisterCommand(construct func () Command) {
	cmd := construct()
	cmdParser, hasParser := cmd.(CommandParser)
	var parser PacketParser
	if hasParser {
		parser = func(reader io.Reader) (interface{}, error) {
			return cmdParser.Parse(reader)
		}
	} else {
		parser = MakeSimplePacketParser(construct)
	}
	recognizedCommands[cmd.Opcode()] = parser
}

type FullCommand struct {
	Header CommandHeader
	Command interface{}
}

func WriteCommand(writer io.Writer, fullCmd FullCommand) {
	header := fullCmd.Header // make a copy because Opcode field is modified
	cmd := fullCmd.Command.(Command)
	buf := bytes.NewBuffer(make([]byte, 0, 128))
	binary.Write(buf, binary.BigEndian, uint8(0)) // len placeholder
	header.Opcode = cmd.Opcode()
	binary.Write(buf, binary.BigEndian, header)

	cmdWriter, isWriter := cmd.(CommandWriter)
	if isWriter {
		cmdWriter.Write(buf)
	} else {
		binary.Write(buf, binary.BigEndian, cmd)
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

func ParsePacket(packet []byte) (*FullCommand, error) {
	log.Printf("packet:\n%s", hex.Dump(packet))
	buf := bytes.NewBuffer(packet[1:]) // skip len
	var header CommandHeader
	if err := binary.Read(buf, binary.BigEndian, &header); err != nil {
		return nil, err
	}
	cmdParser, found := recognizedCommands[header.Opcode]
	if !found {
		return nil, fmt.Errorf("opcode %04x not recognized", header.Opcode)
	}

	if cmd, err := cmdParser(buf); err != nil {
		return nil, err
	} else {
		return &FullCommand{header, cmd}, nil
	}
}

func ReadSmartbus(reader io.Reader, ch chan FullCommand) {
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

		var cmd *FullCommand
		if cmd, err = ParsePacket(packet); err != nil {
			log.Printf("error parsing packet: %v", err)
			continue
		}
		ch <- *cmd
	}
}

func WriteSmartbus(writer io.Writer, ch chan FullCommand) {
	for cmd := range ch {
		WriteCommand(writer, cmd)
	}
}
