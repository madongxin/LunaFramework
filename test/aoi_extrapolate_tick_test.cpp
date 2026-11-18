#include "MoveSendConfig.h"

#include <cmath>
#include <cstdlib>
#include <iostream>

namespace {

int fails = 0;
void Expect(bool c, const char *m) {
    if (!c) {
        std::cerr << "FAIL: " << m << "\n";
        ++fails;
    }
}

// Legacy H1 arithmetic: authority-vs-authority with still auth + vx≠0 → every tick drifted.
// Kept so the bug signature stays documented; C1 must not use this comparison.
void TestH1LegacyAuthorityDriftSignature() {
    const float speed = 10.f;
    const float dt = 0.2f;
    const float rec_x = 0.f, rec_z = 0.f, rec_vx = speed, rec_vz = 0.f;
    const float auth_x = 0.f, auth_z = 0.f;
    const float pred_x = rec_x + rec_vx * dt;
    const float pred_z = rec_z + rec_vz * dt;
    const float ex = pred_x - auth_x;
    const float ez = pred_z - auth_z;
    const float err_cm = std::sqrt(ex * ex + ez * ez) * 100.f;
    Expect(err_cm > 100.f, "H1 legacy: still-authority exceeds 100cm");
    Expect(err_cm > 199.f && err_cm < 201.f, "H1 legacy: error is about 200cm");
}

// C1: last emit + error use presented. Steady 10m/s glide → pred tracks presented → no drift.
void TestC1PresentedNoDriftOnSteadyGlide() {
    const float speed = 10.f;
    const float dt = 0.2f;
    float rec_x = 0.f, rec_z = 0.f;
    const float rec_vx = speed, rec_vz = 0.f;
    // Presented advanced one present-slice with the same velocity.
    const float cur_x = rec_x + rec_vx * dt;
    const float cur_z = rec_z + rec_vz * dt;
    const float pred_x = rec_x + rec_vx * dt;
    const float pred_z = rec_z + rec_vz * dt;
    const float ex = pred_x - cur_x;
    const float ez = pred_z - cur_z;
    const float err_cm = std::sqrt(ex * ex + ez * ez) * 100.f;
    Expect(err_cm < 1.f, "C1: steady presented tracks prediction within 1cm");
}

// C1: 20s steady near glide at 200ms ticks → emits ≈ first + keyframes only (not every tick).
void TestC1Steady20sEmitBudget() {
    const float speed = 10.f;
    const float tick_s = 0.2f;
    const int ticks = static_cast<int>(20.0 / tick_s);  // 100
    const int key_ms = 4000;
    const float err_limit_cm = 100.f;
    float rec_x = 0.f, rec_z = 0.f;
    float rec_vx = speed, rec_vz = 0.f;
    int64_t rec_ms = 0;
    bool has = false;
    int emits = 0;
    int drifted_emits = 0;
    for (int i = 0; i < ticks; ++i) {
        const int64_t now = static_cast<int64_t>((i + 1) * tick_s * 1000.0);
        const float cur_x = speed * static_cast<float>(now) / 1000.f;
        const float cur_z = 0.f;
        bool send = false;
        bool drifted = false;
        if (!has) {
            send = true;
        } else {
            const float elapsed = static_cast<float>(now - rec_ms) / 1000.f;
            const float pred_x = rec_x + rec_vx * elapsed;
            const float pred_z = rec_z + rec_vz * elapsed;
            const float ex = pred_x - cur_x;
            const float ez = pred_z - cur_z;
            const float err_cm = std::sqrt(ex * ex + ez * ez) * 100.f;
            drifted = err_cm > err_limit_cm;
            const bool keyframe = (now - rec_ms) >= key_ms;
            send = drifted || keyframe;
        }
        if (!send)
            continue;
        ++emits;
        if (drifted)
            ++drifted_emits;
        rec_ms = now;
        rec_x = cur_x;
        rec_z = cur_z;
        rec_vx = speed;
        rec_vz = 0.f;
        has = true;
    }
    // first + keyframes at 4s,8s,12s,16s,20s ≈ 6; never 100 interval ticks
    Expect(emits >= 5 && emits <= 8, "C1: 20s steady emits about first+keyframes");
    Expect(drifted_emits == 0, "C1: 20s steady has zero drifted emits");
    Expect(emits < ticks / 2, "C1: emits far below present-tick count");
    std::cerr << "C1 20s emits=" << emits << " drifted=" << drifted_emits << " ticks=" << ticks << "\n";
}

void TestExtrapolateOnDefault() {
    unsetenv("LUNASERVER_AOI_EXTRAPOLATE");
    Expect(AoiExtrapolateEnabled(), "extrapolate default on");
    setenv("LUNASERVER_AOI_EXTRAPOLATE", "0", 1);
    Expect(!AoiExtrapolateEnabled(), "extrapolate explicit 0 off");
    unsetenv("LUNASERVER_AOI_EXTRAPOLATE");
}

void TestEgressEnforceOnDefault() {
    unsetenv("LUNASERVER_EGRESS_ENFORCE");
    Expect(EgressEnforce(), "egress enforce default on");
    setenv("LUNASERVER_EGRESS_ENFORCE", "0", 1);
    Expect(!EgressEnforce(), "egress enforce explicit 0 off");
    unsetenv("LUNASERVER_EGRESS_ENFORCE");
}

void TestFarKeyframeDefault() {
    unsetenv("LUNASERVER_AOI_FAR_KEYFRAME_MS");
    Expect(ExtrapFarKeyframeMs() == 10000, "far keyframe default 10000");
}

// C1b: fixed 0.2s present step vs short wall elapsed since Move emit → false drifted.
void TestC1bFixedStepVsShortWallDrift() {
    const float speed = 10.f;
    const float present_step_s = 0.2f;  // old AdvancePresented kTickSec
    const float wall_s = 0.05f;         // Move emit 后很快又到 present
    const float presented_move = speed * present_step_s;
    const float pred_move = speed * wall_s;
    const float err_cm = std::fabs(presented_move - pred_move) * 100.f;
    Expect(err_cm > 100.f, "C1b: fixed 0.2 step vs 50ms wall exceeds 100cm");
    // wall-aligned step would match pred
    const float aligned_move = speed * wall_s;
    const float aligned_err = std::fabs(aligned_move - pred_move) * 100.f;
    Expect(aligned_err < 1.f, "C1b: wall-aligned present step tracks pred");
}

// C1c: Move emit at T=180 updates rec but not last_present → present at T=200 false drifted.
// Fix: sync last_present_ms = emit_ms so next present dt matches pred elapsed.
void TestC1cEmitSyncsPresentClock() {
    const float speed = 10.f;
    const float err_limit_cm = 100.f;
    const int64_t t_present0 = 0;
    const int64_t t_emit = 180;
    const int64_t t_present1 = 200;
    float pos = 0.f;
    int64_t last_present_ms = t_present0;
    float rec_x = 0.f;
    float rec_vx = speed;
    int64_t rec_ms = t_present0;

    // Move 插缝 emit：位置未变，只刷新 rec（旧 bug 不改 last_present）。
    rec_ms = t_emit;
    rec_x = pos;
    const int64_t last_present_broken = last_present_ms;  // still 0
    const int64_t last_present_fixed = t_emit;            // C1c

    auto err_after_present = [&](int64_t last_pres) {
        float dt = static_cast<float>(t_present1 - last_pres) / 1000.f;
        if (dt > 0.25f)
            dt = 0.25f;
        const float cur = pos + speed * dt;  // vx 积分
        const float elapsed = static_cast<float>(t_present1 - rec_ms) / 1000.f;
        const float pred = rec_x + rec_vx * elapsed;
        return std::fabs(pred - cur) * 100.f;
    };

    const float broken_cm = err_after_present(last_present_broken);
    const float fixed_cm = err_after_present(last_present_fixed);
    Expect(broken_cm > err_limit_cm, "C1c: unsynced last_present exceeds 100cm after Move emit");
    Expect(fixed_cm < 1.f, "C1c: synced last_present tracks pred within 1cm");
    std::cerr << "C1c broken_cm=" << broken_cm << " fixed_cm=" << fixed_cm << "\n";
}

// C1c: 20s with mid-interval Move emits every 200ms; with sync → zero drifted.
void TestC1cSteadyWithMidIntervalEmits() {
    const float speed = 10.f;
    const float tick_s = 0.2f;
    const int ticks = 50;  // 10s
    const int key_ms = 4000;
    const float err_limit_cm = 100.f;
    float pos = 0.f;
    float rec_x = 0.f;
    float rec_vx = speed;
    int64_t rec_ms = 0;
    int64_t last_present_ms = 0;
    bool has = false;
    int emits = 0;
    int drifted_emits = 0;
    for (int i = 0; i < ticks; ++i) {
        const int64_t t0 = static_cast<int64_t>(i * tick_s * 1000.0);
        const int64_t t_emit = t0 + 180;  // Move 插缝
        const int64_t t1 = static_cast<int64_t>((i + 1) * tick_s * 1000.0);

        // Mid-interval Move emit（位置未变）+ C1c 同步 last_present。
        rec_ms = t_emit;
        rec_x = pos;
        rec_vx = speed;
        last_present_ms = t_emit;
        has = true;
        ++emits;

        // Present at t1 with vx integration from last_present
        float dt = static_cast<float>(t1 - last_present_ms) / 1000.f;
        if (dt > 0.25f)
            dt = 0.25f;
        if (dt < 0.f)
            dt = 0.f;
        pos += speed * dt;
        last_present_ms = t1;

        const float elapsed = static_cast<float>(t1 - rec_ms) / 1000.f;
        const float pred = rec_x + rec_vx * elapsed;
        const float err_cm = std::fabs(pred - pos) * 100.f;
        if (err_cm > err_limit_cm) {
            ++drifted_emits;
            rec_ms = t1;
            rec_x = pos;
            last_present_ms = t1;
            ++emits;
        } else if ((t1 - rec_ms) >= key_ms) {
            rec_ms = t1;
            rec_x = pos;
            last_present_ms = t1;
            ++emits;
        }
    }
    Expect(drifted_emits == 0, "C1c: mid-interval emits with sync have zero drifted");
    Expect(emits >= ticks, "C1c: mid-interval path still emits each Move pulse");
    std::cerr << "C1c mid-interval drifted=" << drifted_emits << " emits=" << emits << "\n";
}

// C1d: Move 插缝时 presented 未 Advance，若用冻结 cur 比墙钟 pred → 假 drifted。
// 应用 live = presented + v*(now-last_present) 后应与 pred 对齐。
void TestC1dLivePresentVsWallPred() {
    const float speed = 10.f;
    const float err_limit_cm = 100.f;
    const int64_t last_present_ms = 0;
    const int64_t rec_ms = 0;
    const float presented = 0.f;
    const float rec_x = 0.f;
    const float rec_vx = speed;
    const int64_t now = 150;  // Move 插缝 150ms → 1.5m > 1m 阈值

    const float frozen_cur = presented;
    const float elapsed = static_cast<float>(now - rec_ms) / 1000.f;
    const float pred = rec_x + rec_vx * elapsed;
    const float frozen_err = std::fabs(pred - frozen_cur) * 100.f;
    Expect(frozen_err > err_limit_cm, "C1d: frozen presented vs wall pred exceeds 100cm");

    const float pdt = static_cast<float>(now - last_present_ms) / 1000.f;
    const float live = presented + speed * pdt;
    const float live_err = std::fabs(pred - live) * 100.f;
    Expect(live_err < 1.f, "C1d: live-integrated presented tracks pred");
    std::cerr << "C1d frozen_cm=" << frozen_err << " live_cm=" << live_err << "\n";
}

// C1e-1: 片延迟 1s 时若 present 只走 0.25s、pred 走满 1s → 假 drifted。
void TestC1eBacklogCapFalseDrift() {
    const float speed = 10.f;
    const float err_limit_cm = 100.f;
    const float aoi0 = 0.f;
    const float rec_x = 0.f;
    const float backlog_s = 1.f;
    const float old_cap = 0.25f;
    const float advanced_capped = aoi0 + speed * old_cap;
    const float pred_full = rec_x + speed * backlog_s;
    const float broken = std::fabs(pred_full - advanced_capped) * 100.f;
    Expect(broken > err_limit_cm, "C1e: 0.25 cap vs 1s pred exceeds 100cm");
    const float advanced_full = aoi0 + speed * backlog_s;
    const float fixed = std::fabs(pred_full - advanced_full) * 100.f;
    Expect(fixed < 1.f, "C1e: full wall present step tracks pred");
    std::cerr << "C1e backlog broken_cm=" << broken << " fixed_cm=" << fixed << "\n";
}

// C1e-2: 到点前 live 钳权威、pred 不钳 → 假 drifted；两边同钳应对齐。
void TestC1eArrivalPredClamp() {
    const float speed = 10.f;
    const float err_limit_cm = 100.f;
    const float auth = 10.f;
    const float aoi = 9.0f;
    const float rec_x = 9.0f;
    const float elapsed = 0.25f;
    float live = aoi + speed * elapsed;
    if (live > auth)
        live = auth;
    float pred_raw = rec_x + speed * elapsed;
    const float broken = std::fabs(pred_raw - live) * 100.f;
    Expect(broken > err_limit_cm, "C1e: unclamped pred vs clamped live exceeds 100cm");
    float pred = pred_raw;
    if (pred > auth)
        pred = auth;
    const float fixed = std::fabs(pred - live) * 100.f;
    Expect(fixed < 1.f, "C1e: clamped pred tracks clamped live");
    std::cerr << "C1e arrival broken_cm=" << broken << " fixed_cm=" << fixed << "\n";
}

// C1f: pred 墙钟封顶 2s、呈现继续积分 → 外推安静 >2s 后假 drifted，keyframe(4s) 到不了。
void TestC1fPredCapBlocksKeyframe() {
    const float speed = 10.f;
    const float err_limit_cm = 100.f;
    const int key_ms = 4000;
    float aoi = 0.f;
    const float rec_x = 0.f;
    const int64_t rec_ms = 0;
    int64_t last = 0;
    int drifted_with_cap = 0;
    int drifted_no_cap = 0;
    int keyframe_hits = 0;
    for (int64_t t = 200; t <= 4000; t += 200) {
        const float dt = static_cast<float>(t - last) / 1000.f;
        aoi += speed * dt;
        last = t;
        const float elapsed_full = static_cast<float>(t - rec_ms) / 1000.f;
        const float pred_capped = rec_x + speed * (elapsed_full < 2.f ? elapsed_full : 2.f);
        const float pred_full = rec_x + speed * elapsed_full;
        if (std::fabs(pred_capped - aoi) * 100.f > err_limit_cm)
            ++drifted_with_cap;
        if (std::fabs(pred_full - aoi) * 100.f > err_limit_cm)
            ++drifted_no_cap;
        if (t - rec_ms >= key_ms)
            ++keyframe_hits;
    }
    Expect(drifted_with_cap > 0, "C1f: 2s pred cap causes false drifted before keyframe");
    Expect(drifted_no_cap == 0, "C1f: uncapped pred has zero drifted through 4s");
    Expect(keyframe_hits > 0, "C1f: keyframe age reachable at 4s");
    std::cerr << "C1f drifted_cap=" << drifted_with_cap << " drifted_nocap=" << drifted_no_cap
              << " keyframe_hits=" << keyframe_hits << "\n";
}

// C1g: 呈现已推进后，AllowMoveAoi 若仍用更旧的 last_move_server_ms 当 now → 假 drifted + 时钟回拨。
void TestC1gStaleNowVsAdvancedPresent() {
    const float speed = 10.f;
    const float err_limit_cm = 100.f;
    float aoi = 0.f;
    float rec_x = 0.f;
    int64_t rec_ms = 1000;
    int64_t last_present = 1000;
    aoi = 0.f;
    // present @1200
    aoi += speed * 0.2f;
    last_present = 1200;
    // RefreshViewers 误用 last_move_server_ms=1000
    const int64_t stale_now = 1000;
    float cur = aoi;  // now < last_present → 不积分 live
    float elapsed = static_cast<float>(stale_now - rec_ms) / 1000.f;
    float pred = rec_x + speed * elapsed;
    float broken = std::fabs(pred - cur) * 100.f;
    Expect(broken > err_limit_cm, "C1g: stale now vs advanced aoi exceeds 100cm");
    // 修复：now = max(stale, last_present)
    int64_t now = stale_now;
    if (now < last_present)
        now = last_present;
    cur = aoi;  // pdt=0
    elapsed = static_cast<float>(now - rec_ms) / 1000.f;
    pred = rec_x + speed * elapsed;
    float fixed = std::fabs(pred - cur) * 100.f;
    Expect(fixed < 1.f, "C1g: clamped now tracks advanced aoi");
    std::cerr << "C1g broken_cm=" << broken << " fixed_cm=" << fixed << "\n";
}

// C1f: 20s 匀速 + 中途 Move 插缝（live 比 pred、emit 同步时钟）→ drifted=0 且有 keyframe。
void TestC1fSteady20sWithMovesAndKeyframes() {
    const float speed = 10.f;
    const float err_limit_cm = 100.f;
    const int key_ms = 4000;
    float aoi = 0.f;
    float rec_x = 0.f;
    float rec_vx = speed;
    int64_t rec_ms = 0;
    int64_t last_present = 0;
    bool has = false;
    int emits = 0;
    int drifted = 0;
    int keyframes = 0;
    for (int64_t t = 0; t <= 20000; t += 20) {
        // Move 插缝 every 100ms (not on present ticks)
        if (t > 0 && t % 100 == 0 && t % 200 != 0) {
            float pdt = static_cast<float>(t - last_present) / 1000.f;
            float live = aoi + speed * pdt;
            if (!has) {
                has = true;
                rec_ms = t;
                rec_x = live;
                last_present = t;
                aoi = live;
                ++emits;
            } else {
                float elapsed = static_cast<float>(t - rec_ms) / 1000.f;
                float pred = rec_x + rec_vx * elapsed;
                float err = std::fabs(pred - live) * 100.f;
                bool vel_ch = false;
                bool drift = err > err_limit_cm;
                bool key = (t - rec_ms) >= key_ms;
                if (vel_ch || drift || key) {
                    if (drift && !key)
                        ++drifted;
                    if (key && !drift)
                        ++keyframes;
                    rec_ms = t;
                    rec_x = live;
                    aoi = live;
                    last_present = t;
                    ++emits;
                }
            }
        }
        // present every 200ms
        if (t > 0 && t % 200 == 0) {
            float dt = static_cast<float>(t - last_present) / 1000.f;
            if (dt < 0.001f)
                dt = 0.001f;
            aoi += speed * dt;
            last_present = t;
            if (!has) {
                has = true;
                rec_ms = t;
                rec_x = aoi;
                ++emits;
                continue;
            }
            float elapsed = static_cast<float>(t - rec_ms) / 1000.f;
            float pred = rec_x + rec_vx * elapsed;
            float err = std::fabs(pred - aoi) * 100.f;
            bool drift = err > err_limit_cm;
            bool key = (t - rec_ms) >= key_ms;
            if (drift || key) {
                if (drift && !key)
                    ++drifted;
                if (key && !drift)
                    ++keyframes;
                rec_ms = t;
                rec_x = aoi;
                ++emits;
            }
        }
    }
    Expect(drifted == 0, "C1f: 20s steady+moves has zero drifted");
    Expect(keyframes >= 4, "C1f: 20s steady reaches multiple keyframes");
    Expect(emits < 40, "C1f: emits stay near first+keyframes not every move");
    std::cerr << "C1f 20s emits=" << emits << " drifted=" << drifted << " keyframes=" << keyframes
              << "\n";
}

}  // namespace

int main() {
    TestH1LegacyAuthorityDriftSignature();
    TestC1PresentedNoDriftOnSteadyGlide();
    TestC1Steady20sEmitBudget();
    TestC1bFixedStepVsShortWallDrift();
    TestC1cEmitSyncsPresentClock();
    TestC1cSteadyWithMidIntervalEmits();
    TestC1dLivePresentVsWallPred();
    TestC1eBacklogCapFalseDrift();
    TestC1eArrivalPredClamp();
    TestC1fPredCapBlocksKeyframe();
    TestC1fSteady20sWithMovesAndKeyframes();
    TestC1gStaleNowVsAdvancedPresent();
    TestExtrapolateOnDefault();
    TestEgressEnforceOnDefault();
    TestFarKeyframeDefault();
    if (fails) {
        std::cerr << fails << " failed\n";
        return 1;
    }
    std::cout << "OK aoi_extrapolate_tick_test (C1..C1g + default-on flags)\n";
    return 0;
}
