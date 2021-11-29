# 使用 eBPF 跟踪 Go 

[TOC]

该仓库用于演示 eBPF 跟踪 Go 语言函数参数并进行修改，环境搭建参见“环境准备”。

todo: 补充 uprobe 原理（objdump 和 gdb 截图）

## Go 函数参数跟踪和修改

### 样例代码

被跟踪的 Go 语言 tracee.go 文件内容如下：

```go
package main

import "fmt"

//go:noinline
func ebpfDemo(a1 int, a2 bool, a3 float32) (r1 int64, r2 int32, r3 string) {
	fmt.Printf("ebpfDemo:: a1=%d, a2=%t a3=%t.2f\n", a1, a2, a3)
	return 100, 200, "test for ebpf"
}

func main() {
	r1, r2, r3 := ebpfDemo(100, true, 66.88)
	fmt.Printf("main:: r1=%d, r2=%t 43=%t.2f\n", r1, r2, r3)

	return;
}
```

其中 `//go:noinline` 明确告诉编译器不适用内联优化，相关内容可参考：[Golang bcc/BPF Function Tracing](https://www.brendangregg.com/blog/2017-01-31/golang-bcc-bpf-function-tracing.html) 或者[中文翻译](https://colobu.com/2017/09/22/golang-bcc-bpf-function-tracing/)。



### 跟踪程序代码

```bash
$ go mod init
```

跟踪和修改 ebpfDemo 入参的 BPF 程序代码如下:

```c
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
```



其中 `bpf_probe_write_user ` 用于修改 `ebpfDemo` 的第一个入参。

单次运行 tracee 结果如下：

```bash
bpf_demo/bpf_go_tracer/tracee$ make build && make run
go build -o tracee main.go
./tracee
ebpfDemo:: a1=100, a2=true a3=66.88
main:: r1=100, r2=200 r3=test for ebpf
```



启动 tracer 程序（make build && make run），然后再次运行 tracee （make run），tracee 和 tracer 的输出如下：

```bash
bpf_demo/bpf_go_tracer/tracer$ make build && make run
go build -o tracer main.go
sudo ./tracer --binary ../tracee/tracee --func main.ebpfDemo
Trace ../tracee/tracee on func [main.ebpfDemo]
[100 0 0 0 0 0 0 0 1 0 0 0 143 194 133 66 0 0 0 0]
ebpfDemo:: a1=100, a2=1 a3=66.8./tracee/tracee on func [main.ebpfDemo] # 输出

```



tracee 的输出结果如下：

```bash
bpf_demo/bpf_go_tracer/tracee$ make run
./tracee
ebpfDemo:: a1=2021, a2=true a3=66.88   # 入参已经从原来的 a1=100 =>  a1=2021
main:: r1=100, r2=200 r3=test for ebpf
```



通过运行结果我们发现通过 BPF 程序完成了 `ebpfDemo` 的参数跟踪和参数修改。



>  **此种方式的局限性如下：**
>  
 > * eBPF 在分析 Go 语言 ABI 的时候局限于简单类型，更加复杂的类型（用户自定义结构体/指针/引用/通道接口等），比较适用的场景是函数的调用次数，函数延迟，函数返回值等；
 >
 > * 基于 uprobe 需要被跟踪的程序带有符号表（not stripped）
 >
 > * eBPF 需要特权用户，一般情况下会限制适用范围；



## 环境准备

本文我们测试的环境如下：

*  Ubuntu 20.04 LTS ， amd64
* bcc  v0.23.0（源码安装）
* iovisor/gobpf  v0.2.0 
* Go1.15 linux/amd64 （最新 1.17 版本调用方式可能有变化）

需要注意 iovisor/gobpf 需要与 bcc 版本相对应，否则可能会报接口函数签名不一致，本质上 gobpf 库只是 bcc 底层开发库的一层 cgo 包装。

在 Ubuntu 20.04 LTS 系统中，仓库中对应的包存在一些问题，所以必须采用源码方式进行安装，报错信息如下：

```bash
vendor/github.com/iovisor/gobpf/bcc/module.go:32:10: fatal error: bcc/bcc_common.h: No such file or directory
```

详情可参见 Github [Issue 214](https://github.com/iovisor/gobpf/issues/214)。



### Ubuntu 20.04 环境安装

这里我们使用 [multipass](https://multipass.run/) 进行环境安装：

```bash
$ multipass find # 该命令可以显示当前全部的版本
$ multipass launch -n ubuntu  -c 4 -m 4G -d 40G  20.04
```

直接使用 `multipass launch -n ubuntu  -c 4 -m 4G -d 40G  20.04` 命令进行安装，详细参数介绍如下：

* -n ubuntu 创建虚拟机的名字，后续登录需要基于该名字；
* -c 虚拟机使用的 CPU 函数可以根据自己的情况进行调整；
* -m 虚拟机占用的内存大小；
* -d 虚拟机占用的磁盘，默认只有 5G，涉及编译可以根据自己情况调整；
* 20.04 为我们要安装的系统镜像名字，也可以使用别名，比如 focal（20.04）；

系统安装成功后，使用 `multipass shell ubuntu` 即可进行登录。



BPF 程序运行需要依赖 linux-header文件，我们提前进行安装：

```bash
$ sudo apt-get install bpfcc-tools linux-headers-$(uname -r)
```



### BCC 源码安装

安装流程参考 [BCC 安装文档](https://github.com/iovisor/bcc/blob/master/INSTALL.md#ubuntu---source)

```bash
# 安装编译相关工具
$ sudo apt install -y bison build-essential cmake flex git libedit-dev \
  libllvm7 llvm-7-dev libclang-7-dev python zlib1g-dev libelf-dev libfl-dev python3-distutils
  
$ git clone https://github.com/iovisor/bcc.git
$ git checkout v0.23.0 -b branch_v0.23.0
$ git submodule update
$ mkdir bcc/build; cd bcc/build
$ cmake ..
$ make

# 需要特权模式
$ sudo make install  
```


### Go 安装

```bash
$ wget https://dl.google.com/go/go1.15.linux-amd64.tar.gz
$ sudo rm -rf /usr/local/go && sudo tar -C /usr/local -xzf go1.15.linux-amd64.tar.gz
$ export PATH=$PATH:/usr/local/go/bin
$ go version
```

##  参考文档

* [Introducing gobpf - Using eBPF from Go](https://kinvolk.io/blog/2016/11/introducing-gobpf-using-ebpf-from-go/) 主要介绍了 gobpf 库的使用姿势，文章比较老，可能有最新的使用优化。
* [ebpf 修改golang 函数参数](http://hushi55.github.io/2021/04/22/ebpf-modify-golang-args) 和 [Go internal ABI specification](http://hushi55.github.io/2021/04/19/Go-internal-ABI-specification) => [原文](https://go.googlesource.com/go/+/refs/heads/dev.regabi/src/cmd/compile/internal-abi.md)
* [Debugging with eBPF Part 1: Tracing Go function arguments in prod](https://www.cncf.io/blog/2021/11/17/debugging-with-ebpf-part-1-tracing-go-function-arguments-in-prod/) 作者 为 Pixie 的*Zain Asgar*，从原理到最终使用都有比较详细的介绍
  *  [YouTube 分享的视频](https://youtu.be/0mxUU_--dDM)  No-Instrumenttation Golang Logging with eBPF Golang Online Meetup 57
  * 样例仓库参见 [trace_example](https://github.com/pixie-io/pixie-demos/tree/main/simple-gotracing/trace_example)。
  * Pixie 产品追踪 Go 功能参见 [Dynamic Logging In Go (Alpha)](https://docs.px.dev/tutorials/custom-data/dynamic-go-logging/)
  * 系列文章 [Debugging with eBPF Part 2: Tracing full body HTTP request/responses](https://blog.px.dev/ebpf-http-tracing/) 介绍了追踪 HTTP 使用 kprobe 和 urprobe 的各种利弊和性能对比
  * 站点中 eBPF 相关文档 https://blog.px.dev/ebpf
* [Tracing Go Functions with eBPF Part 1](https://www.grant.pizza/blog/tracing-go-functions-with-ebpf-part-1/) 作者为 grantseltze，2021 年的 Gopher 分享参见这里 [Tracing Go Programs with eBPF!](Tracing Go Programs with eBPF!)， [YouTube 地址](https://www.youtube.com/watch?v=JxZL6ZEBBEg) GopherCon Europe 2021: Grant Seltzer Richman - Unlocking eBPF from Go
* [Tracing Go Functions with eBPF Part 2](http://hushi55.github.io/2021/04/22/ebpf-modify-golang-args)  作者的开源工具 [weaver](https://github.com/grantseltzer/weaver)，博客有不少 eBPF 文章，参见 https://www.grant.pizza/
  * [Youtube: GopherCon 2020: Grant Seltzer Richman - Tracing Go Programs with eBPF!](https://www.youtube.com/watch?v=OxLmd7szevI)
* YouTube [Tracing Go with eBPF / Florian Lehner @ GDG Berlin Golang 06/2021](https://www.youtube.com/watch?v=f1jr9nR25EA)   ✅ 有 eBPF 背景介绍和 Go 跟踪的各个方面包括函数堆栈等；
* 其他样例代码参见这里 [modify-arguments-on-the-fly](https://github.com/chenhengqi/bpf-examples/tree/main/modify-arguments-on-the-fly) 该样例代码不能稳定重现。
