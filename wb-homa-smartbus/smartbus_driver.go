package main

import (
	"github.com/contactless/wb-homa-drivers/wb-homa-smartbus/smartbus"
	"flag"
	"time"
)

func main () {
	serial := flag.String("serial", "localhost:10011", "the address of serial port redirector")
	broker := flag.String("broker", "tcp://localhost:1883", "MQTT broker url")
	flag.Parse()
	if driver, err := smartbus.NewSmartbusTCPDriver(*serial, *broker); err != nil {
		panic(err)
	} else {
		driver.Start()
		for {
			time.Sleep(1 * time.Second)
		}
	}
}
