#pragma once

#include <string_view>
#include <kls/temp/STL.h>

namespace count {
    class Cache {
        struct Hash {
            using hash_type = std::hash<std::string_view>;
            using is_transparent = void;
            size_t operator()(const char *str) const { return hash_type{}(str); }
            size_t operator()(std::string_view str) const { return hash_type{}(str); }
            size_t operator()(std::string const &str) const { return hash_type{}(str); }
        };

        struct Inverse {
            std::string_view name{};
            int32_t reference_count{0};
            int64_t last_use_rotation{};
            int64_t cached_value{};
        };
    public:
        [[nodiscard]] std::string_view name(int32_t id) const noexcept { return m_inverse[id].name; }
        [[nodiscard]] int64_t load(int32_t id) const noexcept { return m_inverse[id].cached_value; }
        int64_t store(int32_t id, int64_t v) noexcept { return m_inverse[id].cached_value = v; }

        kls::temp::vector<int32_t> acquires(const kls::temp::vector<kls::temp::string>& names) {
            return {};
        }

        void release(int32_t id) noexcept {
            auto& entry = m_inverse[id];
            if (--entry.reference_count == 0) entry.last_use_rotation = m_rotation;
        }

        kls::temp::map<kls::temp::string, int64_t> rotate() noexcept {
            auto size = m_inverse.size();
            for (auto i = 0; i < size; ++i) {
                auto &&x = m_inverse[i];
                if (x.reference_count == 0 && x.last_use_rotation < m_rotation) {
                    m_forward.erase(m_forward.find(x.name));
                    x = Inverse{};
                    m_free.push_back(i);
                }
            }
            ++m_rotation;
            return {};
        }
    private:
        std::unordered_map<std::string, int32_t, Hash, std::equal_to<>> m_forward{};
        std::deque<Inverse> m_inverse{};
        std::deque<int32_t> m_free{};
        int32_t m_top{0};
        int64_t m_rotation{0};

        int32_t id(std::string_view name) {
            auto iter = m_forward.find(name);
            if (iter != m_forward.end()) {
                auto id = iter->second;
                return (++m_inverse[id].reference_count, id);
            } else {
                auto id = get_id();
                auto& entry = m_inverse[id];
                auto&&[it, _] = m_forward.insert_or_assign(std::string(name), id);
                return (entry.name = it->first, entry.reference_count = 1, id);
            }
        }

        int32_t get_id() {
            if (!m_free.empty()) {
                auto id = m_free.back();
                return (m_free.pop_back(), id);
            }
            return (m_inverse.push_back({}), ++m_top);
        }
    };
}