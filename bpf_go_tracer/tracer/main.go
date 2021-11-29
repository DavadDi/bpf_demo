package main

import (
	"bytes"
	"flag"
	"encoding/binary"
	"fmt"
	"log"
	"os"
	"os/signal"
	"github.com/iovisor/gobpf/bcc"
)

const ebpf_prog = `
#include <uapi/linux/ptrace.h>

BPF_PERF_OUTPUT(events);

typedef struct {
	u64   	arg1;
	char   	arg2;
	char    pad[3];
	float   arg3;
} args_event_t;

inline int get_arguments(struct pt_regs *ctx) {
		void* stackAddr = (void*)ctx->sp;
		args_event_t event = {};

		bpf_probe_read(&event.arg1, sizeof(event.arg1), stackAddr+8);
		bpf_probe_read(&event.arg2, sizeof(event.arg2), stackAddr+16);
		bpf_probe_read(&event.arg3, sizeof(event.arg3), stackAddr+20);

		long tmp = 2021;
		bpf_probe_write_user(stackAddr+8, &tmp, sizeof(tmp));

		events.perf_submit(ctx, &event, sizeof(event));

		return 0;
}
`

type argsEvent struct {
	Arg1 int64
	Arg2 byte
	Pad  [3]byte
	Arg3 float32
}

var tracePrg string
var traceFun string

func init() {
	flag.StringVar(&tracePrg, "binary", "", "The binary to probe")
	flag.StringVar(&traceFun, "func", "", "The function to probe")
}

func main() {
	flag.Parse()

	if len(tracePrg) == 0 || len(traceFun) == 0 {
		panic("Argument --binary and --func needs to be specified")
	}

	fmt.Printf("Trace %s on func [%s]\n", tracePrg, traceFun);

	bpfModule := bcc.NewModule(ebpf_prog, []string{})
	uprobeFd, err := bpfModule.LoadUprobe("get_arguments")

	if err != nil {
		log.Fatal(err)
	}

	err = bpfModule.AttachUprobe(tracePrg, traceFun, uprobeFd, -1)
	if err != nil {
		log.Fatal(err)
	}

	table := bcc.NewTable(bpfModule.TableId("events"), bpfModule)
	evtCh := make(chan []byte)
	lost := make(chan uint64)

	perfMap, err := bcc.InitPerfMap(table, evtCh, lost)
	if err != nil {
		log.Fatal(err)
	}

	sigCh := make(chan os.Signal, 1)
	signal.Notify(sigCh, os.Interrupt)

	go func() {
		var evt argsEvent

			for {
				data := <-evtCh
				fmt.Println(data)
				err := binary.Read(bytes.NewBuffer(data), binary.LittleEndian, &evt)
				if err != nil {
					fmt.Fprintf(os.Stderr, "Failed to decode received chroot event data: %s\n", err)
					continue
				}

				fmt.Printf("ebpfDemo:: a1=%d, a2=%d a3=%.2f\n", evt.Arg1, evt.Arg2, evt.Arg3)
		}
	}()

	perfMap.Start()
	<-sigCh
	perfMap.Stop()
}
