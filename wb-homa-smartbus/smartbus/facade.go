package smartbus

import (
	"io"
	"net"
	"log"
	"errors"
	"strings"
        serial "github.com/ivan4th/goserial"
)

// FIXME
const (
	DRIVER_SUBNET = 0x01
	DRIVER_DEVICE_ID = 0x99
	DRIVER_DEVICE_TYPE = 0x1234
	DRIVER_CLIENT_ID = "smartbus"
)

func createStreamIO(stream io.ReadWriteCloser, provideUdpGateway bool) (SmartbusIO, error) {
	if !provideUdpGateway {
		return NewStreamIO(stream, nil), nil
	}
	rawUdpReadCh := make(chan []byte)
	rawSerialReadCh := make(chan []byte)
	log.Println("using UDP gateway mode")
	dgramIO, err := NewDatagramIO(rawUdpReadCh)
	if err != nil {
		return nil, err
	}
	streamIO := NewStreamIO(stream, rawSerialReadCh)
	dgramIO.Start()
	go func () {
		for frame := range rawUdpReadCh {
			streamIO.SendRaw(frame)
		}
	}()
	go func () {
		for frame := range rawSerialReadCh {
			dgramIO.SendRaw(frame)
		}
	}()
	return streamIO, nil
}

func connect(serialAddress string, provideUdpGateway bool) (SmartbusIO, error) {
	switch {
	case strings.HasPrefix(serialAddress, "/"):
		if serial, err := serial.OpenPort(&serial.Config{
			Name: serialAddress,
			Baud: 9600,
			Parity: serial.ParityEven,
			Size: serial.Byte8,
		}); err != nil {
			return nil, err
		} else {
			return createStreamIO(serial, provideUdpGateway)
		}
	case serialAddress == "udp":
		if provideUdpGateway {
			return nil, errors.New("cannot provide UDP gw in udp device access mode")
		}
		if dgramIO, err := NewDatagramIO(nil); err != nil {
			return nil, err
		} else {
			return dgramIO, nil
		}
	case strings.HasPrefix(serialAddress, "tcp://"):
		if conn, err := net.Dial("tcp", serialAddress[6:]); err != nil {
			return nil, err
		} else {
			return createStreamIO(conn, provideUdpGateway)
		}
	}

	if conn, err := net.Dial("tcp", serialAddress); err != nil {
		return nil, err
	} else {
		return NewStreamIO(conn, nil), nil
	}
}

func NewSmartbusTCPDriver(serialAddress, brokerAddress string, provideUdpGateway bool) (*Driver, error) {
	model := NewSmartbusModel(func () (SmartbusIO, error) {
		return connect(serialAddress, provideUdpGateway)
	}, DRIVER_SUBNET, DRIVER_DEVICE_ID, DRIVER_DEVICE_TYPE)
	driver := NewDriver(model, func (handler MQTTMessageHandler) MQTTClient {
		return NewPahoMQTTClient(brokerAddress, DRIVER_CLIENT_ID, handler)
	})
	return driver, nil
}
