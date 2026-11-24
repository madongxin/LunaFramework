#pragma once

/**
 * @file ProcSelfStats.h
 * @brief 读 /proc/self 得到 RSS/VIRT/fd/TCP 队列快照
 *
 * 对齐 top(RES/VIRT)；TCP 队列对本进程 fd 做 ioctl(SIOCOUTQ/SIOCINQ)。
 */

#include <cstdint>
#include <string>
#include <sys/types.h>

struct ProcResourceSnapshot {
    pid_t pid = 0;
    int64_t vm_rss_kb = -1;
    int64_t vm_hwm_kb = -1;
    int64_t vm_size_kb = -1;
    int64_t vm_peak_kb = -1;
    int threads = -1;
    int open_fds = -1;
    bool has_smaps_rollup = false;
    int64_t smaps_rss_kb = -1;
    int64_t smaps_pss_kb = -1;
    int map_entries = -1;
    int64_t maps_vsize_total_kb = -1;
    bool read_status_ok = false;
    uint64_t tcp_send_queue_bytes = 0;
    uint64_t tcp_recv_queue_bytes = 0;
};

/** 填充快照；pid==0 表示本进程。失败字段保持 -1/false */
bool FillProcResourceSnapshot(ProcResourceSnapshot *out, pid_t pid = 0);
/** 单行人类可读摘要（GM/日志） */
std::string ProcResourceSnapshotFormatLine(const ProcResourceSnapshot &s);
