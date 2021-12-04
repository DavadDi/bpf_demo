# Offensive BPF: Understanding and using bpf_probe_write_user

编译依赖于 [libbp-bootstrap](https://github.com/libbpf/libbpf-bootstrap)，编译和运行的机器需要支持 CO-RE，即内核编译选项默认内置支持了CONFIG_DEBUG_INFO_BTF=y，可以通过参看编译的 config 文件或者 /sys/kernel/btf/vmlinux 文件确认。

推荐 Ubuntu 21.10 已经内置支持了BTF，即 CO-RE 的底层依赖。


## 安装基本编译依赖

```bash
$ sudo apt-get update
$ sudo apt install -y bison build-essential cmake flex git libedit-dev   libllvm11 llvm-11-dev libclang-11-dev python zlib1g-dev libelf-dev libfl-dev python3-distutils


# 安装 llvm 和 clang, Ubuntu 21.10 默认 llvm13 版本
$ sudo apt-get install clang llvm
``` 

## 编译程序


```bash
$ git clone https://github.com/libbpf/libbpf-bootstrap.git
$ cd libbpf-bootstrap
$ git submodule update --init --recursive       # check out libbpf

$ cd examples/c
$ make
$ sudo ./bootstrap
```

以上编译完成后，将当前目录的所有文件 cp 到 example/c 目录中，重新进行编译即可。

Makefile 的差异如下：

```bash
$ git diff
diff --git a/examples/c/Makefile b/examples/c/Makefile
index 85a9fbe..1271172 100644
--- a/examples/c/Makefile
+++ b/examples/c/Makefile
@@ -13,7 +13,7 @@ INCLUDES := -I$(OUTPUT) -I../../libbpf/include/uapi -I$(dir $(VMLINUX))
 CFLAGS := -g -Wall
 ARCH := $(shell uname -m | sed 's/x86_64/x86/' | sed 's/aarch64/arm64/' | sed 's/ppc64le/powerpc/' | sed 's/mips.*/mips/')

-APPS = minimal bootstrap uprobe kprobe fentry
+APPS = minimal bootstrap obpf uprobe kprobe fentry

 # Get Clang's default includes on this system. We'll explicitly add these dirs
 # to the includes list when compiling with `-target bpf` because otherwise some
```

编译完成后会生成 obpf 可执行程序，运行 obpf 程序后，通过 ls 查看文件将得到错误的展示。

```bash
$ sudo ./obpf
libbpf: elf: skipping unrecognized data section(7) .rodata.str1.1
TIME     EVENT COMM             PID     PPID    FILENAME/EXIT COD
...
```

使用 ls 查看文件：

```bash
ubuntu@ubuntu21-10:~$ ls -hl
ls: cannot access 'xxxxxxas_admin_successful': No such file or directory
ls: cannot access 'xxxxxxle': No such file or directory
ls: cannot access 'xxxxxxfo': No such file or directory
ls: cannot access 'xxxxxxlogout': No such file or directory
ls: cannot access 'xxxxxxc': No such file or directory
total 4.0K
drwxrwxr-x 3 ubuntu ubuntu 4.0K Dec  3 22:26 bpf_demo
-????????? ? ?      ?         ?            ? xxxxxxas_admin_successful
-????????? ? ?      ?         ?            ? xxxxxxc
-????????? ? ?      ?         ?            ? xxxxxxfo
-????????? ? ?      ?         ?            ? xxxxxxle
-????????? ? ?      ?         ?            ? xxxxxxlogout
```

上述结果中的 xxxxxx 即为 bpf 程序写入。

通过 demsg -T 进行查看，可以得到以下提示信息：

```bash
$ sudo dmsg -T
...
[Fri Dec  3 23:12:16 2021] obpf[25207] is installing a program with bpf_probe_write_user helper that may corrupt user memory!
[Fri Dec  3 23:12:16 2021] obpf[25207] is installing a program with bpf_probe_write_user helper that may corrupt user memory!
[Fri Dec  3 23:12:16 2021] obpf[25207] is installing a program with bpf_probe_write_user helper that may corrupt user memory!
[Fri Dec  3 23:12:16 2021] obpf[25207] is installing a program with bpf_probe_write_user helper that may corrupt user memory!

```

bpf 程序打印的日志：

```bash
$ sudo cat /sys/kernel/debug/tracing/trace_pipe 
...
           <...>-25282   [000] d... 41502.606920: bpf_trace_printk: ** sys_enter_getdents64 ** OVERWRITING
           <...>-25282   [000] d... 41502.606920: bpf_trace_printk: ** RESULT 0
           <...>-25282   [000] d... 41502.606921: bpf_trace_printk: Loop 3: offset: 96, total len: 96
...
```

程序对应的博客为：[Offensive BPF: Understanding and using bpf_probe_write_user](https://embracethered.com/blog/posts/2021/offensive-bpf-libbpf-bpf_probe_write_user/)
