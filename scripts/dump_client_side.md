# 压测机上要手动执行的命令

在压测机上执行。不要从网关机 ssh 过来。把输出存成文件，和网关的 `run/formal/parked-ss/` 放在一起对。

下面的 `<peer-port>` 用网关 `ss` 里看到的对端端口。`<iface>` 用压测机默认路由的网卡。`<pid>` 用机器人进程号。

## 1. 机器人进程在不在忙

```bash
ps -eo pid,pcpu,pmem,rss,nlwp,cmd | grep -E -i 'robot|loadtest|bot|luna' | grep -v grep
top -H -b -n 1 -p <pid>
```

看什么：进程 CPU 接近 100% 的核数，说明客户端自己算不过来，读 socket 会停。`nlwp` 是线程数。`top -H` 看是哪一个线程占着 CPU。

## 2. 这条连接的接收队列和接收窗口

```bash
ss -tinm sport = :<peer-port>
```

看什么：

- `Recv-Q` 一直变大：机器人没把字节读走。
- `skmem` 里的 `r` 是接收缓冲里的字节。
- `rcv_space` / `rcv_rtt` 是本机（压测机）的接收估计，不是网关的发送窗口。
- 缩进行里的 `cwnd`、`retrans`、`backoff`、`rto` 和网关那一份对照。两边同时大，才是这条路径卡住。

## 3. 网卡有没有在丢

```bash
ip -s link show dev <iface>
nstat -a IpInDiscards TcpExtListenDrops TcpExtTCPRcvQDrop
```

看什么：`RX dropped`、`RX errors` 在采样前后有没有增加。`nstat -a` 是累计值，不要加 `-z`，`-z` 会把内核计数清掉。

## 4. 客户端一侧的原文

```bash
ss -tinm > client-ss-$(date +%Y%m%d-%H%M%S).txt
```

看什么：和网关同一分钟的 `ss -tinm` 放一起。网关 `Send-Q` 大、这里 `Recv-Q` 也大，是客户端没读。网关 `Send-Q` 大、这里 `Recv-Q` 接近 0 而且 `cwnd` 很小，是路径上的丢包或窗口，不是客户端用户态没读。
