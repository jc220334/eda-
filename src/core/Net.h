#pragma once
// =====================================================================
// 网络：网络(Net)与网络表(NetTable)
// 网络以稳定 id 作为引用键，name 为显示名（如 GND / VCC）。
// =====================================================================
#include <string>
#include <vector>
#include <algorithm>

namespace pcb {

// ---------- 网络 ----------
struct Net {
    std::string id;    // 稳定引用键（默认 "net:<name>"）
    std::string name;  // 显示名
    bool operator==(const Net& o) const { return id == o.id && name == o.name; }
};

// ---------- 网络表 ----------
class NetTable {
public:
    Net* addNet(const std::string& name, const std::string& id = "") {
        std::string realId = id.empty() ? ("net:" + name) : id;
        if (findById(realId)) return nullptr;   // id 冲突
        nets_.push_back(Net{realId, name});
        return &nets_.back();
    }
    bool removeNet(const std::string& id) {
        auto it = std::find_if(nets_.begin(), nets_.end(),
            [&](const Net& n) { return n.id == id; });
        if (it == nets_.end()) return false;
        nets_.erase(it);
        return true;
    }
    Net* findById(const std::string& id) {
        auto it = std::find_if(nets_.begin(), nets_.end(),
            [&](const Net& n) { return n.id == id; });
        return it == nets_.end() ? nullptr : &*it;
    }
    const Net* findById(const std::string& id) const {
        return const_cast<NetTable*>(this)->findById(id);
    }
    Net* findByName(const std::string& name) {
        auto it = std::find_if(nets_.begin(), nets_.end(),
            [&](const Net& n) { return n.name == name; });
        return it == nets_.end() ? nullptr : &*it;
    }
    const Net* findByName(const std::string& name) const {
        return const_cast<NetTable*>(this)->findByName(name);
    }
    std::vector<Net>& nets() { return nets_; }
    const std::vector<Net>& nets() const { return nets_; }
    bool operator==(const NetTable& o) const { return nets_ == o.nets_; }

private:
    std::vector<Net> nets_;
};

} // namespace pcb
