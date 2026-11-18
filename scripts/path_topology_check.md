# 机器人是直连网关，还是经过 VIP:8081

只列命令，不在这里执行。在压测机和网关机各做一遍。`<vip>` 是对外的 8081，`<gw>` 是网关机自己的地址。

## 压测机：这条连接的下一跳是谁

```bash
ip route get <vip>
ip route get <gw>
ss -tin dst <vip> dport = :8081
ss -tin dst <gw>
```

看什么：`ip route get` 的 `dev` 和 `via`。`ss` 的对端如果是 `<vip>:8081`，连接目标是 VIP。对端如果是 `<gw>:8081` 或 `<gw>:8083`，是直连其中一台网关。

## 网关机：进来的源地址是不是压测机

```bash
ss -tin sport = :8081 or sport = :8083
ip route get <压测机地址>
iptables -t nat -S
iptables -t mangle -S
tc qdisc show
tc class show dev <iface>
```

看什么：

- 源地址等于压测机地址，中间没有改地址。
- 源地址变成了别的地址，路径上有 NAT。`iptables -t nat` 能看到本机有没有做。
- `tc qdisc` 出现 `tbf`、`htb`、`netem`，本机网卡上有限速或人为丢包。`pfifo_fast` 或 `fq` 本身不是这次要查的限速。

## 如果有单独的负载均衡机器

在那台机器上：

```bash
ipvsadm -Ln
ss -tin sport = :8081
tc qdisc show
iptables -t nat -S
```

看什么：`ipvsadm` 有 `TCP <vip>:8081` 并且后端是两台网关的 8081/8083，就是 L4。没有 ipvs、连接在网关机上直接是压测机地址，就是直连。

## 采样时要留下的原文

```bash
ip route get <vip> > route-vip.txt
ss -tin > ss-client.txt
tc qdisc show > tc-qdisc.txt
```

网关机同样留一份。没有 `ipvsadm` 就在文件里写「无数据」，不要写成没有规则。
