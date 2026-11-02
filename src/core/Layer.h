#pragma once
// =====================================================================
// 图层管理：图层(Layer)、层组(LayerGroup)、图层管理器(LayerManager)
// 支持图层创建、命名、排序、显示/隐藏及层组管理。
// =====================================================================
#include "Types.h"
#include <string>
#include <vector>
#include <algorithm>
#include <utility>

namespace pcb {

// ---------- 图层 ----------
struct Layer {
    std::string id;               // 唯一标识，如 "F.Cu"
    std::string name;             // 显示名，如 "Top Copper"
    LayerType type = LayerType::Other;
    bool visible = true;
    int order = 0;                // 排序权重（越小越靠前）
    Color color;

    bool operator==(const Layer& o) const {
        return id == o.id && name == o.name && type == o.type &&
               visible == o.visible && order == o.order && color == o.color;
    }
};

// ---------- 层组 ----------
struct LayerGroup {
    std::string name;
    std::vector<std::string> layerIds;
    bool visible = true;

    bool operator==(const LayerGroup& o) const {
        return name == o.name && layerIds == o.layerIds && visible == o.visible;
    }
};

// ---------- 图层管理器 ----------
class LayerManager {
public:
    // 创建图层；id 已存在则失败
    bool addLayer(Layer l) {
        if (findLayer(l.id)) return false;
        layers_.push_back(std::move(l));
        return true;
    }
    bool removeLayer(const std::string& id) {
        auto it = std::find_if(layers_.begin(), layers_.end(),
            [&](const Layer& l) { return l.id == id; });
        if (it == layers_.end()) return false;
        layers_.erase(it);
        // 同时从所有层组中移除该图层
        for (auto& g : groups_)
            g.layerIds.erase(std::remove(g.layerIds.begin(), g.layerIds.end(), id),
                             g.layerIds.end());
        return true;
    }
    Layer* findLayer(const std::string& id) {
        auto it = std::find_if(layers_.begin(), layers_.end(),
            [&](const Layer& l) { return l.id == id; });
        return it == layers_.end() ? nullptr : &*it;
    }
    const Layer* findLayer(const std::string& id) const {
        return const_cast<LayerManager*>(this)->findLayer(id);
    }
    bool setVisible(const std::string& id, bool v) {
        Layer* l = findLayer(id);
        if (!l) return false;
        l->visible = v;
        return true;
    }
    bool setOrder(const std::string& id, int order) {
        Layer* l = findLayer(id);
        if (!l) return false;
        l->order = order;
        return true;
    }
    // 按 order 升序排序
    void sortByOrder() {
        std::stable_sort(layers_.begin(), layers_.end(),
            [](const Layer& a, const Layer& b) { return a.order < b.order; });
    }
    std::vector<Layer>& layers() { return layers_; }
    const std::vector<Layer>& layers() const { return layers_; }

    // 层组
    bool addGroup(LayerGroup g) {
        if (findGroup(g.name)) return false;
        groups_.push_back(std::move(g));
        return true;
    }
    LayerGroup* findGroup(const std::string& name) {
        auto it = std::find_if(groups_.begin(), groups_.end(),
            [&](const LayerGroup& g) { return g.name == name; });
        return it == groups_.end() ? nullptr : &*it;
    }
    const LayerGroup* findGroup(const std::string& name) const {
        return const_cast<LayerManager*>(this)->findGroup(name);
    }
    std::vector<LayerGroup>& groups() { return groups_; }
    const std::vector<LayerGroup>& groups() const { return groups_; }

    bool operator==(const LayerManager& o) const {
        return layers_ == o.layers_ && groups_ == o.groups_;
    }

private:
    std::vector<Layer> layers_;
    std::vector<LayerGroup> groups_;
};

} // namespace pcb
