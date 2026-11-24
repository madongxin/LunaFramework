#pragma once

/**
 * @file MailConfig.h
 * @brief 邮件服务端数值配置（config/mail.cnf）
 *
 * 容量、过期天数、分页、批量上限、收藏上限、过期扫描间隔、玩家互寄限流等。
 * LoadFromConfig 失败时保留默认值并返回 false（可继续用默认跑通开发环境）。
 */

#include <cstdint>

struct MailConfigValues {
    int mailbox_capacity = 100;
    int mailbox_overflow = 120;
    int default_expire_days = 30;
    int page_size_default = 20;
    int batch_op_max = 20;
    int favorite_max = 50;
    int protect_hours = 24;
    double expire_scan_interval_sec = 60.0;
    int player_mail_per_minute = 10;
};

class MailConfig {
public:
    static MailConfig &Instance();

    /** 从 mail.cnf 加载；失败时保留默认值并返回 false。 */
    bool LoadFromConfig();

    const MailConfigValues &Values() const { return values_; }

private:
    MailConfig() = default;
    MailConfigValues values_;
};
