package smartbus

import (
	"io"
	"encoding/binary"
	"log"
	"fmt"
	"errors"
	"reflect"
)

func ReadChannelStatusField(reader io.Reader, value reflect.Value) error {
	var n uint8
	if err := binary.Read(reader, binary.BigEndian, &n); err != nil {
		log.Printf("error reading channel status count: %v", err)
		return err
	}
	status := make([]bool, n)
	if n > 0 {
		bs := make([]uint8, (n + 7) / 8)
		if _, err := io.ReadFull(reader, bs); err != nil {
			log.Printf("error reading channel status: %v", err)
			return err
		}
		for i := range status {
			status[i] = (bs[i / 8] >> uint(i % 8)) & 1 == 1
		}
	}

	value.Set(reflect.ValueOf(status))
	return nil
}

func WriteChannelStatusField(writer io.Writer, value reflect.Value) error {
	status := value.Interface().([]bool)
	if err := binary.Write(writer, binary.BigEndian, uint8(len(status))); err != nil {
		return err
	}
	if len(status) == 0 {
		return nil
	}
	bs := make([]uint8, (len(status) + 7) / 8)
	for i, v := range status {
		if v {
			bs[i / 8] |= 1 << uint(i % 8)
		}
	}
	return binary.Write(writer, binary.BigEndian, bs)
}

// -----

func ReadZoneStatusField(reader io.Reader, value reflect.Value) error {
	var n uint8
	if err := binary.Read(reader, binary.BigEndian, &n); err != nil {
		log.Printf("error reading zone status count: %v", err)
		return err
	}

	status := make([]uint8, n)
	if n > 0 {
		if _, err := io.ReadFull(reader, status); err != nil {
			log.Printf("error reading zone status: %v", err)
			return err
		}
	}

	value.Set(reflect.ValueOf(status))
	return nil
}

func WriteZoneStatusField(writer io.Writer, value reflect.Value) error {
	status := value.Interface().([]uint8)
	if err := binary.Write(writer, binary.BigEndian, uint8(len(status))); err != nil {
		return err
	}
	if len(status) > 0 {
		return binary.Write(writer, binary.BigEndian, status)
	}
	return nil
}

// ------

func ReadRemarkField(reader io.Reader, value reflect.Value) error {
	buf := make([]uint8, 64)
	n, err := reader.Read(buf)
	if err != nil && err != io.EOF {
		return err
	}
	if n == 64 {
		// FIXME: the limit is actually perhaps less than 64,
		// but spec is very onclear concerning it
		return errors.New("remark field too long")
	}
	value.Set(reflect.ValueOf(buf[:n]))
	return nil
}

func WriteRemarkField(writer io.Writer, value reflect.Value) error {
	buf := value.Interface().([]uint8)
	if len(buf) >= 64 {
		// FIXME: the limit is actually perhaps less than 64,
		// but spec is very onclear concerning it
		return errors.New("remark field too long")
	}
	return binary.Write(writer, binary.BigEndian, buf)
}

// ------

type FieldReadFunc func (reader io.Reader, value reflect.Value) error
type FieldWriteFunc func (writer io.Writer, value reflect.Value) error

type converter struct {
	read FieldReadFunc
	write FieldWriteFunc
}

func uint8converter(fromRaw func (uint8) (interface{}, error),
	toRaw func (interface{}) (uint8, error)) converter {
	return converter{
		func (reader io.Reader, value reflect.Value) (err error) {
			var b uint8
			if err = binary.Read(reader, binary.BigEndian, &b); err != nil {
				return
			}
			if r, err := fromRaw(b); err == nil {
				value.Set(reflect.ValueOf(r))
			}
			return
		},
		func (writer io.Writer, value reflect.Value) error {
			if r, err := toRaw(value.Interface()); err != nil {
				return err
			} else {
				return binary.Write(writer, binary.BigEndian, &r)
			}
		},
	}
}

func uint8MapConverter(m map[uint8]interface{}) converter {
	revMap := make(map[interface{}]uint8)
	for k, v := range m {
		revMap[v] = k
	}
	return uint8converter(
		func (in uint8) (interface{}, error) {
			v, found := m[in]
			if !found {
				return nil, fmt.Errorf("cannot map uint8 value: %v", in)
			}
			return v, nil
		},
		func (in interface{}) (uint8, error) {
			v, found := revMap[in]
			if !found {
				return 0, fmt.Errorf("cannot reverse map value to uint8: %v", in)
			}
			return v, nil
		})
}

func uint8NameListConverter(names []string) converter {
	m := make(map[uint8]interface{})
	for i, v := range names {
		m[uint8(i)] = v
	}
	return uint8MapConverter(m)
}

func arrayConverter(itemConverter converter) converter {
	return converter{
		func (reader io.Reader, value reflect.Value) (err error) {
			for i, n := 0, value.Len(); i < n; i++ {
				if err = itemConverter.read(reader, value.Index(i)); err != nil {
					return
				}
			}
			return
		},
		func (writer io.Writer, value reflect.Value) (err error) {
			for i, n := 0, value.Len(); i < n; i++ {
				if err = itemConverter.write(writer, value.Index(i)); err != nil {
					return
				}
			}
			return
		},
	}
}

var converterMap map[string]converter = map[string]converter{
	"success": uint8MapConverter(map[uint8]interface{} {
		0xf8: true,
		0xf5: false,
	}),
	"channelStatus": {ReadChannelStatusField, WriteChannelStatusField},
	"zoneStatus": {ReadZoneStatusField, WriteZoneStatusField},
	"panelButtonModes": arrayConverter(
		uint8NameListConverter(
			[]string {
				"Invalid",
				"SingleOnOff",
				"SingleOn",
				"SingleOff",
				"CombinationOn",
				"CombinationOff",
				"PressOnReleaseOff",
				"CombinationOnOff",
				"SeparateLeftRightPressOnReleaseOff",
				"SeparateLeftRightCombinationOnOff",
				"LeftOffRightOn",
			})),
	"remark": {ReadRemarkField, WriteRemarkField},
};

func ReadTaggedField(reader io.Reader, value reflect.Value, tag string) error {
	converter, found := converterMap[tag]
	if !found {
		return fmt.Errorf("no converter for tag %v", tag)
	}
	return converter.read(reader, value)
}

func ParseMessage(reader io.Reader, message interface{}) (err error) {
	d := reflect.ValueOf(message)
	if d.Kind() != reflect.Ptr {
		return errors.New("message pointer expected")
	}
	v := d.Elem()
	t := v.Type()
	for i, n := 0, t.NumField(); i < n; i++ {
		field := t.Field(i)
		fieldVal := v.Field(i);
		tag := field.Tag.Get("sbus")
		if tag == "" {
			err = binary.Read(reader, binary.BigEndian, fieldVal.Addr().Interface())
		} else {
			err = ReadTaggedField(reader, fieldVal, tag)
		}
		if err != nil {
			return
		}
	}
	return
}

func WriteTaggedField(writer io.Writer, value reflect.Value, tag string) error {
	converter, found := converterMap[tag]
	if !found {
		return fmt.Errorf("no converter for tag %v", tag)
	}
	return converter.write(writer, value)
}

func WriteMessage(writer io.Writer, message interface{}) (err error) {
	d := reflect.ValueOf(message)
	if d.Kind() != reflect.Ptr {
		return errors.New("message pointer expected")
	}
	v := d.Elem()
	t := v.Type()
	for i, n := 0, t.NumField(); i < n; i++ {
		field := t.Field(i)
		fieldVal := v.Field(i);
		tag := field.Tag.Get("sbus")
		if tag == "" {
			err = binary.Write(writer, binary.BigEndian, fieldVal.Addr().Interface())
		} else {
			err = WriteTaggedField(writer, fieldVal, tag)
		}
		if err != nil {
			return
		}
	}
	return
}

// message registration & packet parsing

type Message interface {
	Opcode() uint16
}

type PreprocessedMessage interface {
	FromRaw(func(interface{}) error) error
	ToRaw() (interface{}, error)
}

type PacketParser func(io.Reader) (interface{}, error)

func MakeSimplePacketParser(construct func() Message) PacketParser {
	return func(reader io.Reader) (interface{}, error) {
		msg := construct()
		if err := ParseMessage(reader, msg); err != nil {
			log.Printf("error parsing the message: %v", err)
			return nil, err
		}
		return msg, nil
	}
}

func MakePreprocessedPacketParser(construct func() Message) PacketParser {
	return func(reader io.Reader) (interface{}, error) {
		msg := construct()
		preprocess := msg.(PreprocessedMessage)
		err := preprocess.FromRaw(func (raw interface{}) error {
			if err := ParseMessage(reader, raw); err != nil {
				log.Printf("error parsing the message: %v", err)
				return err
			}
			return nil
		})
		if err == nil {
			return msg, nil
		} else {
			return nil, err
		}
	}
}

var recognizedMessages map[uint16]PacketParser = make(map[uint16]PacketParser)

func RegisterMessage(construct func () Message) {
	msg := construct()
	var parser PacketParser
	switch msg.(type) {
	case PreprocessedMessage:
		parser = MakePreprocessedPacketParser(construct)
	default:
		parser = MakeSimplePacketParser(construct)
	}
	recognizedMessages[msg.Opcode()] = parser
}
