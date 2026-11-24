#pragma once

/**
 * @file MailExpireScanner.h
 * @brief 邮件过期定时扫描（挂到主 EventLoop::RunEvery）
 *
 * 周期性调用 MailService::ScanExpire；间隔优先参数，否则 mail.cnf，再否则 60s。
 */

class EventLoop;

class MailExpireScanner {
public:
    static MailExpireScanner &Instance();

    /** 注册周期性 ScanExpire；interval_sec<=0 时使用 mail.cnf 配置。 */
    void StartPeriodic(EventLoop *loop, double interval_sec = 0.0);

private:
    MailExpireScanner() = default;
    /** RunEvery 回调：扫描一批过期邮件并打日志。 */
    void OnTick();
};
