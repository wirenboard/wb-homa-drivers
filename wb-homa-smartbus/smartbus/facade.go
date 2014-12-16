package smartbus

import (
	"net"
	"strings"
        serial "github.com/ivan4th/goserial"
)

// FIXME
const (
	DRIVER_SUBNET = 0x01
	DRIVER_DEVICE_ID = 0x14
	DRIVER_DEVICE_TYPE = 0x0095
	DRIVER_CLIENT_ID = "smartbus"
)

func connect(serialAddress string) (SmartbusIO, error) {
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
			return NewStreamIO(serial), nil
		}
	case serialAddress == "udp":
		if dgramIO, err := NewDatagramIO(); err != nil {
			return nil, err
		} else {
			return dgramIO, nil
		}
	case strings.HasPrefix(serialAddress, "tcp://"):
		if conn, err := net.Dial("tcp", serialAddress[6:]); err != nil {
			return nil, err
		} else {
			return NewStreamIO(conn), nil
		}
	}

	if conn, err := net.Dial("tcp", serialAddress); err != nil {
		return nil, err
	} else {
		return NewStreamIO(conn), nil
	}
}

func NewSmartbusTCPDriver(serialAddress, brokerAddress string) (*Driver, error) {
	model := NewSmartbusModel(func () (SmartbusIO, error) {
		return connect(serialAddress)
	}, DRIVER_SUBNET, DRIVER_DEVICE_ID, DRIVER_DEVICE_TYPE)
	driver := NewDriver(model, func (handler MQTTMessageHandler) MQTTClient {
		return NewPahoMQTTClient(brokerAddress, DRIVER_CLIENT_ID, handler)
	})
	return driver, nil
}
