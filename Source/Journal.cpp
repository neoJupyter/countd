#include "Journal.h"
#include "Cache.h"
#include <filesystem>
#include <unordered_set>
#include <kls/io/Block.h>

namespace count {
    using namespace kls;
    using namespace kls::coroutine;
    using namespace kls::essential;

    // entry-types in journal files:
    // 0. checkpoint: 0x00, int64(checkpoint)
    // 1. rebind id: 0x01, int32(id), pascal_string
    // 2. set value: 0x02, int32(id), int64(value)
    // checkpoint file: int64(checkpoint), int64(file_id), int64(offset)
    class FileJournal : public Journal {
    public:
        int64_t check() override {

        }

        ValueAsync<void> value(temp::vector<std::pair<int32_t, int64_t>> store) override {

        }

        ValueAsync<void> rotate() override {

        }

        ValueAsync<void> confirm(int64_t id) override {

        }

        ValueAsync<temp::vector<std::pair<std::string_view, int64_t>>> replay(int64_t id) override {

        }

    private:
        Cache &m_cache;
        int64_t m_rotation{1};
        std::filesystem::path m_directory;
        std::unordered_set<int32_t> m_seen{};
        std::unique_ptr<io::Block> m_current{nullptr};

        // perform the initial synchronization and merging of stale journal entries
        ValueAsync<void> initialize() {

        }
    };
}