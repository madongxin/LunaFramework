#pragma once

/**
 * @file MapCatalog.h
 * @brief GameLogic 启动加载的只读地图目录；实例共享同一份 MapStaticData
 *
 * LoadDirectory/EnsureDefault 从 config/maps（或 map_data_dir）加载模板 JSON+SHA；
 * 运行期 Get 返回 shared_ptr<const MapStaticData>，禁止就地改网格。
 * 另缓存 public 容量、AOI 视距、tick Hz 与 manifest（场景策略/传送门）。
 *
 * 协作：MapRuntime::Enter 取静态数据；MapLineView/Placement 读模板策略。
 */

#include "MapStaticData.h"
#include "SceneKind.h"

#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

/** 只读地图模板目录单例。 */
class MapCatalog {
public:
    static MapCatalog &Instance();

    /** 扫描目录加载全部模板；失败写 err 并返回 false。 */
    bool LoadDirectory(const std::string &dir, std::string *err);
    /** 解析 config/maps（或已 Set 的 map_data_dir）；已加载则跳过。 */
    bool EnsureDefault(std::string *err);
    /** 覆盖默认地图数据目录（须在 EnsureDefault 前调用）。 */
    void SetMapDataDir(std::string dir);
    const std::string &map_data_dir() const { return map_data_dir_; }

    /** 公共图软容量（分线创建参考）。 */
    void SetPublicMapCapacity(uint32_t n);
    uint32_t public_map_capacity() const { return public_capacity_; }
    void SetAoiViewRadiusCells(int n);
    int aoi_view_radius_cells() const { return aoi_view_radius_; }
    void SetMapTickHz(int hz);
    int map_tick_hz() const { return map_tick_hz_; }

    /** 按模板 ID 取不可变静态数据；未加载返回 nullptr。 */
    std::shared_ptr<const MapStaticData> Get(uint64_t map_template_id) const;
    bool empty() const;
    uint32_t gameplay_config_version() const;
    uint32_t map_manifest_version() const;

    /** Manifest 单行：模板元数据 + 场景策略 + 传送门列表。 */
    struct ManifestEntry {
        uint64_t map_template_id = 0;
        uint64_t data_version = 0;
        std::string sha256;
        std::string scene_name;
        uint64_t visual_map_template_id = 0;
        MapScenePolicy policy;
        std::vector<MapPortal> portals;
    };
    std::vector<ManifestEntry> ManifestEntries() const;
    bool GetScenePolicy(uint64_t map_template_id, MapScenePolicy *out) const;
    bool GetPortal(uint64_t from_map_template_id, const std::string &portal_id,
                   MapPortal *out) const;
    std::vector<MapPortal> PortalsFor(uint64_t map_template_id) const;

    void ClearForTest();
    /** 测试注入（不经过目录/哈希文件）。 */
    void PutForTest(std::shared_ptr<const MapStaticData> data);

private:
    MapCatalog() = default;

    mutable std::mutex mu_;
    std::string map_data_dir_;
    bool loaded_ = false;
    uint32_t public_capacity_ = 50;
    int aoi_view_radius_ = 2;
    int map_tick_hz_ = 10;
    std::unordered_map<uint64_t, std::shared_ptr<const MapStaticData>> maps_;
    uint32_t gameplay_config_version_ = 0;
    uint32_t map_manifest_version_ = 0;
    std::vector<ManifestEntry> manifest_entries_;
};
