#pragma once
#include <algorithm>
#include <cstddef>
#include <vector>

namespace astra
{
    // Page identities are owned by the host. Each tab keeps its own path.
    template<class Page>
    class navigation
    {
        std::vector<std::vector<Page>> paths_;
        std::vector<Page> fallback_;
        std::size_t selected_{};
    public:
        void set_root(Page page) { if (fallback_.empty()) fallback_.push_back(page); }
        void add_tab(Page root) { paths_.push_back({root}); }
        std::size_t selected_tab() const { return selected_; }
        bool select_tab(std::size_t index)
        {
            if (index >= paths_.size()) return false;
            selected_ = index;
            return true;
        }
        const std::vector<Page>& path() const { return paths_.empty() ? fallback_ : paths_[selected_]; }
        bool push(Page page)
        {
            auto& p = paths_.empty() ? fallback_ : paths_[selected_];
            const auto found = std::find(p.begin(), p.end(), page);
            if (found != p.end()) { p.erase(found + 1, p.end()); return true; }
            p.push_back(page);
            return true;
        }
        bool back()
        {
            auto& p = paths_.empty() ? fallback_ : paths_[selected_];
            if (p.size() <= 1) return false;
            p.pop_back();
            return true;
        }
        bool to_depth(std::size_t depth)
        {
            auto& p = paths_.empty() ? fallback_ : paths_[selected_];
            if (depth >= p.size()) return false;
            p.resize(depth + 1);
            return true;
        }
    };
}
