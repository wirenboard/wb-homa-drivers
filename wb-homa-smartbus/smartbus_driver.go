package main

import (
	"github.com/contactless/wb-homa-drivers/wb-homa-smartbus/smartbus"
	"flag"
	"time"
)

func main () {
	serial := flag.String("serial", "/dev/ttyNSC1", "serial port address (/dev/... or host:port)")
	broker := flag.String("broker", "tcp://localhost:1883", "MQTT broker url")
	gw := flag.Bool("gw", false, "Provide UDP gateway")
	flag.Parse()
	if driver, err := smartbus.NewSmartbusTCPDriver(*serial, *broker, *gw); err != nil {
		panic(err)
	} else {
		if err := driver.Start(); err != nil {
			panic(err)
		}
		for {
			time.Sleep(1 * time.Second)
		}
	}
}
