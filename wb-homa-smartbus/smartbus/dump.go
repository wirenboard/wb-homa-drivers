package smartbus

import (
	"fmt"
	"log"
)

type MessageFormatter struct {
	AddMessage func (format string, args... interface{})
}

func formatChannelStatus(status []bool) (result string) {
	for _, s := range status {
		if s {
			result += "x"
		} else {
			result += "-"
		}
	}
	return
}

func (f *MessageFormatter) log(header *MessageHeader, format string, args... interface{}) {
	f.AddMessage("%02x/%02x (type %04x) -> %02x/%02x: %s",
		header.OrigSubnetID,
		header.OrigDeviceID,
		header.OrigDeviceType,
		header.TargetSubnetID,
		header.TargetDeviceID,
		fmt.Sprintf(format, args...))
}

func (f *MessageFormatter) OnSingleChannelControlCommand(msg *SingleChannelControlCommand, hdr *MessageHeader) {
	f.log(hdr,
		"<SingleChannelControlCommand %v/%v/%v>",
		msg.ChannelNo,
		msg.Level,
		msg.Duration)
}

func (f *MessageFormatter) OnSingleChannelControlResponse(msg *SingleChannelControlResponse,
	hdr *MessageHeader) {
	f.log(hdr,
		"<SingleChannelControlResponse %v/%v/%v/%s>",
		msg.ChannelNo,
		msg.Success,
		msg.Level,
		formatChannelStatus(msg.ChannelStatus))
}

func (f *MessageFormatter) OnZoneBeastBroadcast(msg *ZoneBeastBroadcast,
	hdr *MessageHeader) {
	f.log(hdr,
		"<ZoneBeastBroadcast %v/%s>",
		msg.ZoneStatus,
		formatChannelStatus(msg.ChannelStatus))
}

type MessageDumper struct {
	MessageFormatter
}

func NewMessageDumper() *MessageDumper {
	return &MessageDumper{
		MessageFormatter{
			func (format string, args... interface{}) {
				s := fmt.Sprintf(format, args...)
				log.Printf("GOT MESSAGE: %s", s)
			},
		},
	}
}
