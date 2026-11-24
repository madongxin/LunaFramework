#pragma once

/**
 * @file LoginAdmission.h
 * @brief 登录/重连进程级并发准入（inflight 总预算 + 分阶段预算）
 *
 * 坐落在 auth-flow 单连接互斥与 offload 队列之前：限制全进程同时进行的
 * Login/Reconnect 数量。重连可使用预算尾部预留（reconnect_reserve），新登录不可占用预留。
 * 分阶段（Auth/Session/Bind/Enter）预算与外层 login slot 独立，默认与总 cap 相同。
 * 不替代 GatewayConnGuard 的 per-IP 建连限流。
 *
 * 环境变量：LUNASERVER_LOGIN_INFLIGHT_MAX、LUNASERVER_LOGIN_RECONNECT_RESERVE 等。
 */

/**
 * 进程本地登录/重连 inflight 预算。
 * TryAcquire/Release 管外层槽；StagePermit::Try 管分阶段槽（RAII 释放）。
 */
class LoginAdmission {
public:
    /** 登录编排阶段：各自独立 inflight 计数。 */
    enum class Stage { Auth = 0, Session = 1, Bind = 2, Enter = 3 };

    /**
     * 分阶段许可的 RAII 句柄。
     * 持有时占用该 stage 的一个槽；析构或移走后释放。bool 转换表示是否申请成功。
     */
    class StagePermit {
    public:
        StagePermit() = default;
        StagePermit(StagePermit &&other) noexcept;
        StagePermit(const StagePermit &) = delete;
        StagePermit &operator=(const StagePermit &) = delete;
        ~StagePermit();
        /** 是否成功占到阶段槽。 */
        explicit operator bool() const { return held_; }
        /**
         * 尝试占用指定阶段的一个槽。
         * reconnect=true 时可使用该阶段预算的重连锁定策略（与外层一致）。
         */
        static StagePermit Try(Stage stage, bool reconnect);

    private:
        StagePermit(Stage stage, bool held) : stage_(stage), held_(held) {}
        Stage stage_ = Stage::Auth;
        bool held_ = false;
        friend class LoginAdmission;
    };

    static LoginAdmission &Instance();

    /**
     * 尝试占用一个外层登录槽。
     * reconnect=true 时可使用预算尾部预留；新登录在「仅剩预留」时失败。
     */
    bool TryAcquire(bool reconnect);
    /** 释放一次 TryAcquire 成功占用的外层槽；须成对调用。 */
    void Release();

private:
    LoginAdmission();

    int max_inflight_ = 256;
    int reconnect_reserve_ = 32;
    int inflight_ = 0;
    int stage_max_[4] = {256, 256, 256, 256};
    int stage_inflight_[4] = {0, 0, 0, 0};
};
