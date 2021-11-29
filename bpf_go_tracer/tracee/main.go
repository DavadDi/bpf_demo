package main

import "fmt"

//go:noinline
func ebpfDemo(a1 int, a2 bool, a3 float32) (r1 int64, r2 int32, r3 string) {

	fmt.Printf("ebpfDemo:: a1=%d, a2=%t a3=%.2f\n", a1, a2, a3)

	return 100, 200, "test for ebpf"
}

func main() {
	r1, r2, r3 := ebpfDemo(100, true, 66.88)
	fmt.Printf("main:: r1=%d, r2=%d r3=%s\n", r1, r2, r3)

	return;
}

