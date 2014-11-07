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

type SingleChannelControlCommand struct {
	ChannelNo uint8
	Level uint8
	Duration uint16 // FIXME: is it really duration? ("running time")
}

type SingleChannelControlResponse struct {
	ChannelNo uint8
	Success bool
	Level uint8
	ChannelStatus []bool
}

func ReadChannelStatus(reader io.Reader) ([]bool, error) {
	var n uint8
	if err := binary.Read(reader, binary.BigEndian, &n); err != nil {
		log.Printf("error reading channel status header: %v", err)
		return nil, err
	}
	status := make([]bool, n)
	if n > 0 {
		bs := make([]uint8, (n + 7) / 8)
		if _, err := io.ReadFull(reader, bs); err != nil {
			log.Printf("error reading channel status: %v", err)
			return nil, err
		}
		for i := range status {
			status[i] = (bs[i / 8] >> uint(i % 8)) & 1 == 1
		}
	}

	return status, nil
}

type SingleChannelControlResponseHeaderRaw struct {
	ChannelNo uint8
	Flag uint8
	Level uint8
}

func ParseSingleChannelControlResponse(reader io.Reader) (interface{}, error) {
	var hdr SingleChannelControlResponseHeaderRaw
	if err := binary.Read(reader, binary.BigEndian, &hdr); err != nil {
		log.Printf("ParseSingleChannelControlResponse: error reading the header: %v", err)
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

type PacketParser func(io.Reader) (interface{}, error)

var RecognizedCommands map[uint16]PacketParser = map[uint16]PacketParser{
	0x0032: ParseSingleChannelControlResponse,
}

type FullCommand struct {
	Header CommandHeader
	Command interface{}
}

func (cmd *SingleChannelControlCommand) Opcode() uint16 { return 0x0031; }

func WriteCommand(writer io.Writer, fullCmd FullCommand) {
	header := fullCmd.Header // make a copy because Opcode field is modified
	cmd := fullCmd.Command.(Command)
	buf := bytes.NewBuffer(make([]byte, 0, 128))
	binary.Write(buf, binary.BigEndian, uint8(0)) // len placeholder
	header.Opcode = cmd.Opcode()
	binary.Write(buf, binary.BigEndian, header)
	binary.Write(buf, binary.BigEndian, cmd)

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
	log.Printf("packet: %s", hex.Dump(packet))
	buf := bytes.NewBuffer(packet[1:]) // skip len
	var header CommandHeader
	if err := binary.Read(buf, binary.BigEndian, &header); err != nil {
		return nil, err
	}
	cmdParser, found := RecognizedCommands[header.Opcode]
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
			log.Printf("eof reached");
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
