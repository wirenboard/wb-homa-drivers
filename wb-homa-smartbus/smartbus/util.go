package smartbus

import (
	"log"
	"reflect"
)

func visit(visitor interface{}, thing interface{}, prefix string, args... interface{}) {
	typeName := reflect.Indirect(reflect.ValueOf(thing)).Type().Name()
	methodName := prefix + typeName
	if method, found := reflect.TypeOf(visitor).MethodByName(methodName); !found {
		log.Printf("visit: no visitor method for %s", typeName)
		return
	} else {
		moreValues := make([]reflect.Value, len(args))
		for i, arg := range(args) {
			moreValues[i] = reflect.ValueOf(arg)
		}
		method.Func.Call(append([]reflect.Value{
			reflect.ValueOf(visitor),
			reflect.ValueOf(thing),
		}, moreValues...))
	}
}
