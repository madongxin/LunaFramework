#pragma once

/**
 * @file ProcessMetricsSnapshot.h
 * @brief 进程级 CPU/RSS/FD 快照结构体与采集入口
 */

#include <cstdint>
#include <string>

struct ProcessMetricsSnapshot {
    int64_t time_unix = 0;
    double cpu_seconds_total = -1.0;
    int64_t rss_bytes = -1;
    int64_t vm_size_bytes = -1;
    int64_t vm_max_bytes = -1;
    int open_fds = -1;
    int max_fds = -1;
    int threads = -1;
    double start_time_seconds = -1.0;
    int64_t io_read_bytes = -1;
    int64_t io_write_bytes = -1;
    double eventloop_tick_sec = 0.0;
    double eventloop_tick_peak_sec = 0.0;
    double logic_handle_sec = 0.0;
    double logic_handle_peak_sec = 0.0;
    std::string thread_states_json;
    uint64_t tcp_send_queue_bytes = 0;
    uint64_t tcp_recv_queue_bytes = 0;
};

/** 采集本进程指标并填入 out；供 Prometheus / 监控页 */
bool FillProcessMetricsSnapshot(ProcessMetricsSnapshot *out);
