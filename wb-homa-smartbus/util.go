package smartbus

import (
	"log"
	"reflect"
)

func visit(visitor interface{}, thing interface{}, prefix string) {
	typeName := reflect.Indirect(reflect.ValueOf(thing)).Type().Name()
	methodName := prefix + typeName
	if method, found := reflect.TypeOf(visitor).MethodByName(methodName); !found {
		log.Printf("visit: no visitor method: %s", typeName)
		return
	} else {
		method.Func.Call([]reflect.Value{
			reflect.ValueOf(visitor),
			reflect.ValueOf(thing),
		})
	}
}
