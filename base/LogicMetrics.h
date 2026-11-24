#pragma once

/**
 * @file LogicMetrics.h
 * @brief GameLogic::Handle 单次业务耗时（last/peak）
 *
 * 当前业务在 IO 线程同步执行；拆逻辑线程后可在 loop 内复用 RecordHandle。
 */

namespace LogicMetrics {

/** 记录一次业务 Handle 耗时（秒） */
void RecordHandle(double seconds);
/** 最近一次 Handle 秒数 */
double LastHandleSeconds();
/** 进程内 Handle 峰值秒数 */
double PeakHandleSeconds();

}  // namespace LogicMetrics
