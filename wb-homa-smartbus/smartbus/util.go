package smartbus

import (
	"log"
	"reflect"
)

func doVisit(visitor interface{}, thing interface{}, methodName string, args []interface{}) bool {
	if method, found := reflect.TypeOf(visitor).MethodByName(methodName); !found {
		return false
	} else {
		moreValues := make([]reflect.Value, len(args))
		for i, arg := range(args) {
			moreValues[i] = reflect.ValueOf(arg)
		}
		method.Func.Call(append([]reflect.Value{
			reflect.ValueOf(visitor),
			reflect.ValueOf(thing),
		}, moreValues...))
		return true
	}
}

func visit(visitor interface{}, thing interface{}, prefix string, args... interface{}) {
	typeName := reflect.Indirect(reflect.ValueOf(thing)).Type().Name()
	methodName := prefix + typeName
	if !doVisit(visitor, thing, methodName, args) &&
		!doVisit(visitor, thing, prefix + "Anything", args) {
		log.Printf("visit: no visitor method for %s", typeName)
		return
	}
}
