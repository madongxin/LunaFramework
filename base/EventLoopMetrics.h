#pragma once

/**
 * @file EventLoopMetrics.h
 * @brief EventLoop 一轮 tick、跨线程 todo 队列深度与批处理耗时
 *
 * ExchangeRecent* 取走窗口内最大值，供 10 秒打点。
 */

namespace EventLoopMetrics {

/** 记录一轮 loop 耗时（秒） */
void RecordTick(double seconds);
/** 最近一次 tick 秒数 */
double LastTickSeconds();
/** 进程内 tick 峰值秒数 */
double PeakTickSeconds();
/** 延迟桶上沿，单位毫秒。没有样本时为 0。 */
int TickP99Ms();

/** 跨线程任务队列深度，以及一轮 DoToDoList 的批大小和耗时。 */
void RecordTodoDepth(int depth);
void RecordBatch(int n, double seconds);
int LastTodoDepth();
int LastBatchSize();
double LastBatchSeconds();
/** 取走自上次调用以来的最大值，供 10 秒窗口打点。 */
int ExchangeRecentTodoMax();
int ExchangeRecentBatchMax();
int ExchangeRecentBatchMsMax();

}  // namespace EventLoopMetrics
