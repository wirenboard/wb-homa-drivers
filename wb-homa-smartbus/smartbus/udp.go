package smartbus

import (
	"io"
	"net"
	"log"
	"bytes"
	"strconv"
	"encoding/hex"
	"encoding/binary"
)

const (
	SmartbusPort = 6000
)

var udpSignature []byte = []byte {
	// The signature is: SMARTCLOUD\xaa\xaa
	0x53, 0x4d, 0x41, 0x52, 0x54, 0x43, 0x4c, 0x4f, 0x55, 0x44, 0xaa, 0xaa,
}

type DatagramIO struct {
	conn *net.UDPConn
	outgoingIP net.IP
	smartbusGwAddress net.UDPAddr
	readCh chan SmartbusMessage
	writeCh chan interface{}
	rawReadCh chan []byte
}

func allBroadcast (b []byte) bool {
	for _, c := range b {
		if c != 255 {
			return false
		}
	}
	return true
}

func NewDatagramIO (rawReadCh chan []byte) (*DatagramIO, error) {
	addr, err := net.ResolveUDPAddr("udp4", "0.0.0.0:" + strconv.Itoa(SmartbusPort))
	if err != nil {
		return nil, err
	}
	conn, err := net.ListenUDP("udp", addr)
	if err != nil {
		return nil, err
	}
	return &DatagramIO{
		conn: conn,
		readCh: make(chan SmartbusMessage),
		writeCh: make(chan interface{}),
		rawReadCh: rawReadCh,
		outgoingIP: net.IPv4(255, 255, 255, 255),
		smartbusGwAddress: net.UDPAddr{
			net.IPv4(255, 255, 255, 255),
			6000,
			"",
		},
	}, nil
}

func (dgramIO *DatagramIO) processUdpPacket(packet []byte) {
	if len(packet) < len(udpSignature) + 4 {
		log.Println("packet too short:", hex.Dump(packet))
		return
	}
	if !bytes.Equal(udpSignature, packet[4:len(udpSignature) + 4]) {
		log.Println("invalid udp packet:", hex.Dump(packet))
		return
	}
	if dgramIO.rawReadCh != nil {
		dgramIO.rawReadCh <- packet[len(udpSignature) + 4:]
		return
	}
	if msg, err := ParseFrame(packet[len(udpSignature) + 4:]); err != nil {
		log.Printf("failed to parse smartbus packet: %s", err)
	} else {
		dgramIO.readCh <- *msg
	}
}

func (dgramIO *DatagramIO) doSend(msg interface{}) {
	buf := bytes.NewBuffer(make([]byte, 0, 128))
	// dgramIO.smartbusGwAddress.IP[15] = 255
	binary.Write(buf, binary.BigEndian, dgramIO.outgoingIP[:4])
	binary.Write(buf, binary.BigEndian, udpSignature[:len(udpSignature) - 2])
	switch msg.(type) {
	case SmartbusMessage:
		WriteFrame(buf, msg.(SmartbusMessage))
	case []byte:
		WritePreBuiltFrame(buf, msg.([]byte))
	default:
		panic("udp: unsupported message object type")
	}
	if _, err := dgramIO.conn.WriteToUDP(buf.Bytes(), &dgramIO.smartbusGwAddress); err != nil {
		log.Printf("UDP SEND ERROR: %s", err)
	}
}

func (dgramIO *DatagramIO) Start() chan SmartbusMessage {
	go func () {
		var err error
		defer func () {
			close(dgramIO.readCh)
			// FIXME: check err values when closing the socket from Stop()
			switch {
			case err == io.EOF || err == io.ErrUnexpectedEOF:
				log.Printf("eof reached")
				return
			case err != nil:
				log.Printf("NOTE: connection error: %s", err)
				return
			}
		}()
		b := make([]byte, 8192)
		for {
			n, err := dgramIO.conn.Read(b)
			if err != nil {
				return
			}
			dgramIO.processUdpPacket(b[:n])
		}
	}()
	go func () {
		for msg := range dgramIO.writeCh {
			dgramIO.doSend(msg)
		}
	}()
	return dgramIO.readCh
}

func (dgramIO *DatagramIO) Send(msg SmartbusMessage) {
	dgramIO.writeCh <- msg
}

func (dgramIO *DatagramIO) SendRaw(msg []byte) {
	dgramIO.writeCh <- msg
}

func (dgramIO *DatagramIO) Stop() {
	close(dgramIO.writeCh)
	dgramIO.conn.Close()
}
