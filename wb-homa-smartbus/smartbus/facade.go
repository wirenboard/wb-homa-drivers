package smartbus

import (
	"io"
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

func connect(serialAddress string) (io.ReadWriteCloser, error) {
	if strings.HasPrefix(serialAddress, "/") {
		return serial.OpenPort(&serial.Config{
			Name: serialAddress,
			Baud: 9600,
			Parity: serial.ParityEven,
			Size: serial.Byte8,
		})
	} else {
		return net.Dial("tcp", serialAddress)
	}
}

func NewSmartbusTCPDriver(serialAddress, brokerAddress string) (*Driver, error) {
	conn, err := connect(serialAddress)
	if err != nil {
		return nil, err
	}
	model := NewSmartbusModel(conn, DRIVER_SUBNET, DRIVER_DEVICE_ID, DRIVER_DEVICE_TYPE)
	driver := NewDriver(model, func (handler MQTTMessageHandler) MQTTClient {
		return NewPahoMQTTClient(brokerAddress, DRIVER_CLIENT_ID, handler)
	})
	return driver, nil
}

