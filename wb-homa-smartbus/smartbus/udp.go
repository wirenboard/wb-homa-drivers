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
	nets []net.IPNet
	conn *net.UDPConn
	outgoingIP net.IP
	smartbusGwAddress net.UDPAddr
	readCh chan SmartbusMessage
	writeCh chan SmartbusMessage
}

func allBroadcast (b []byte) bool {
	for _, c := range b {
		if c != 255 {
			return false
		}
	}
	return true
}

var v6Prefix = []byte{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0xff, 0xff}

func broadcast(ip net.IP, mask net.IPMask) net.IP {
	if len(mask) == net.IPv6len && len(ip) == net.IPv4len && allBroadcast(mask[:12]) {
		mask = mask[12:]
	}
	if len(mask) == net.IPv4len && len(ip) == net.IPv6len && bytes.Equal(ip[:12], v6Prefix) {
		ip = ip[12:]
	}
	n := len(ip)
	if n != len(mask) {
		return nil
	}
	r := make(net.IP, 16)
	copy(r, v6Prefix)
	j := 16 - n
	for i := 0; i < n; i++ {
		r[j] = ip[i] | ^mask[i]
		j++
	}
	return r
}

func NewDatagramIO () (*DatagramIO, error) {
	if nets, err := getNetworks(); err != nil {
		return nil, err
	} else {
		addr, err := net.ResolveUDPAddr("udp4", "0.0.0.0:" + strconv.Itoa(SmartbusPort))
		if err != nil {
			return nil, err
		}
		conn, err := net.ListenUDP("udp", addr)
		if err != nil {
			return nil, err
		}
		return &DatagramIO{
			nets: nets,
			conn: conn,
			readCh: make(chan SmartbusMessage),
			writeCh: make(chan SmartbusMessage),
		}, nil
	}
}

func getNetworks () ([]net.IPNet, error) {
	addrs, err := net.InterfaceAddrs()
	if err != nil {
		return nil, err
	}
	var nets []net.IPNet
	for _, addr := range addrs {
            switch v := addr.(type) {
            case *net.IPNet:
		    if v.IP.To4() != nil {
			    nets = append(nets, *v)
		    }
            }
	}
	return nets, nil
}

func (dgramIO *DatagramIO) maybeDiscoverOutgoingIP(udpAddr net.UDPAddr) bool {
	for _, ifAddr := range dgramIO.nets {
		if ifAddr.IP.Mask(ifAddr.Mask).Equal(udpAddr.IP.Mask(ifAddr.Mask)) {
			log.Println("matching src address:", ifAddr.IP)
			dgramIO.outgoingIP = ifAddr.IP
			dgramIO.smartbusGwAddress = net.UDPAddr{
				broadcast(udpAddr.IP, ifAddr.Mask),
				SmartbusPort,
				"",
			}
			log.Println("smartbusGwAddress:", dgramIO.smartbusGwAddress)
			return true
		}
	}

	return false
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
	if msg, err := ParsePacket(packet[len(udpSignature) + 4:]); err != nil {
		log.Printf("failed to parse smartbus packet: %s", err)
	} else {
		dgramIO.readCh <- *msg
	}
}

func (dgramIO *DatagramIO) doSend(msg SmartbusMessage) {
	buf := bytes.NewBuffer(make([]byte, 0, 128))
	// dgramIO.smartbusGwAddress.IP[15] = 255
	log.Println("outip:", dgramIO.smartbusGwAddress.IP[15])
	binary.Write(buf, binary.BigEndian, dgramIO.outgoingIP[:4])
	binary.Write(buf, binary.BigEndian, udpSignature[:len(udpSignature) - 2])
	WritePacket(buf, msg)
	if _, err := dgramIO.conn.WriteToUDP(buf.Bytes(), &dgramIO.smartbusGwAddress); err != nil {
		log.Printf("UDP SEND ERROR: %s", err)
	}
}

func (dgramIO *DatagramIO) Start() chan SmartbusMessage {
	ready := make(chan struct{})
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
			log.Println("waiting...")
			n, from, err := dgramIO.conn.ReadFrom(b)
			if err != nil {
				log.Printf("udp oops: %s", err)
				return
			}
			udpFrom := from.(*net.UDPAddr)
			log.Println("initial packet from: ", udpFrom)
			log.Println(hex.Dump(b[:n]))
			if (dgramIO.maybeDiscoverOutgoingIP(*udpFrom)) {
				ready <- struct{}{}
				break
			}
			dgramIO.processUdpPacket(b[:n])
		}
		for {
			n, err := dgramIO.conn.Read(b)
			if err != nil {
				return
			}
			dgramIO.processUdpPacket(b[:n])
		}
	}()
	go func () {
		<- ready
		for msg := range dgramIO.writeCh {
			dgramIO.doSend(msg)
		}
	}()
	return dgramIO.readCh
}

func (dgramIO *DatagramIO) Send(msg SmartbusMessage) {
	dgramIO.writeCh <- msg
}

func (dgramIO *DatagramIO) Stop() {
	close(dgramIO.writeCh)
	dgramIO.conn.Close()
}
