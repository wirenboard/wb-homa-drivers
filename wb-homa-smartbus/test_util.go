package smartbus

import (
	"fmt"
	"testing"
	"github.com/stretchr/testify/assert"
)

type Recorder struct {
	t *testing.T
	logs []string
	ch chan struct{}
}

func (rec *Recorder) InitRecorder(t *testing.T) {
	rec.t = t
	rec.ch = make(chan struct{}, 1000)
	rec.Reset()
}

func (rec *Recorder) Rec(format string, args... interface{}) {
	rec.logs = append(
		rec.logs,
		fmt.Sprintf(format, args...))
	rec.ch <- struct{}{}
}

func (rec *Recorder) Verify(logs... string) {
	if logs == nil {
		assert.Equal(rec.t, 0, len(rec.logs), "rec log count")
	} else {
		<- rec.ch
		assert.Equal(rec.t, logs, rec.logs, "rec logs")
	}
	rec.Reset()
}

func (rec *Recorder) Reset() {
	rec.logs = make([]string, 0, 1000)
}
