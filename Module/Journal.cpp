#include "Journal.h"
#include "njp/Json.h"
#include <kls/journal/Journal.h>
#include <kls/essential/Unsafe.h>
#include <kls/coroutine/Operation.h>

using namespace njp;
using namespace kls;
using namespace njp::count;
using namespace kls::journal;
using namespace kls::coroutine;
using namespace kls::essential;

namespace {
    struct Entry {
        temp::unordered_map<int32_t, int64_t> values;
        temp::unordered_map<int32_t, temp::string> names;
    };

    temp_json zip_entry(const Entry &entry) {
        temp_json result{};
        result["v"] = entry.values;
        result["n"] = entry.names;
        return result;
    }

    Entry unzip_entry(const temp_json &json) { return Entry{.values = json['v'], .names = json['n']}; }

    class FileJournal : public IJournal {
    public:
        explicit FileJournal(std::string_view path);
        coroutine::ValueAsync<> rotate() override;
        coroutine::ValueAsync<> confirm() override;
        coroutine::ValueAsync<> values(Span<std::tuple<int32_t, std::string_view, int64_t>> records) override;
        coroutine::ValueAsync<> close() override;
    private:
        std::unordered_set<int32_t> m_id;
        std::shared_ptr<journal::AppendJournal> m_journal;
        Entry make_entry(Span<std::tuple<int32_t, std::string_view, int64_t>> records);
    };

    FileJournal::FileJournal(std::string_view path) : m_journal(create_file_journal(path)) {}

    ValueAsync<> FileJournal::rotate() {
        m_id.clear();
        co_await m_journal->register_checkpoint();
    }

    ValueAsync<> FileJournal::confirm() { return m_journal->check_checkpoint(); }

    Entry FileJournal::make_entry(Span<std::tuple<int32_t, std::string_view, int64_t>> records) {
        Entry entry{};
        for (auto &&[id, name, value]: records) {
            if (m_id.emplace(id).second) entry.names[id] = name;
            entry.values[id] = value;
        }
        return entry;
    }

    ValueAsync<> FileJournal::values(Span<std::tuple<int32_t, std::string_view, int64_t>> records) {
        const auto json = zip_entry(make_entry(records)).dump();
        co_await m_journal->append(Span<char>(json));
    }

    ValueAsync<> FileJournal::close() { return m_journal->close(); }

    template <class M1, class M2>
    void merge_update_with(M1& m1, M2&& m2) {
        for (auto&&[k, v]: m2) m1.insert_or_assign(k, std::move(v));
    }
    
    class ReplayHelper {
    public:
        ValueAsync<> run(std::string_view path) {
            auto recover = recover_file_journal(path);
            while (co_await recover.forward()) {
                JournalRecord record = recover.next();
                auto range = static_span_cast<char>(record.data);
                if (record.type == 1) update_checkpoint(range); else append_to_checkpoint(range);
            }
            m_checkpoints[m_checkpoint_max] = pack_checkpoint();
        }

        auto get_result() {
            temp::map<temp::string, int64_t> result{};
            for (auto i = m_checkpoint_min; i <= m_checkpoint_max; ++i) {
                merge_update_with(result, std::move(m_checkpoints[i]));
            }
            return result;
        }
    private:
        int64_t m_checkpoint_min{0}, m_checkpoint_max{0};
        temp::unordered_map<int32_t, int64_t> m_checkpoint_values{};
        temp::unordered_map<int32_t, temp::string> m_checkpoint_names{};
        temp::map<int64_t, temp::map<temp::string, int64_t>> m_checkpoints{};
        
        temp::map<temp::string, int64_t> pack_checkpoint() {
            temp::map<temp::string, int64_t> result{};
            for (auto &&[k, v]: m_checkpoint_values) result[m_checkpoint_names[k]] = v;
            m_checkpoint_names.clear(), m_checkpoint_values.clear();
            return result;
        }

        void update_checkpoint(Span<char> range) {
            Access<std::endian::little> access{range};
            const auto new_checkpoint_min = access.get<int64_t>(0);
            const auto new_checkpoint_max = access.get<int64_t>(8);
            if (new_checkpoint_max != m_checkpoint_max) {
                m_checkpoints[m_checkpoint_max] = pack_checkpoint();
                m_checkpoint_max = new_checkpoint_max;
            }
            m_checkpoint_min = new_checkpoint_min;
        }

        void append_to_checkpoint(Span<char> range) {
            auto entry = unzip_entry(temp_json::parse(std::string_view{range.begin(), range.end()}));
            merge_update_with(m_checkpoint_names, std::move(entry.names));
            merge_update_with(m_checkpoint_values, std::move(entry.values));
        }
    };
}

std::shared_ptr<IJournal> IJournal::create(std::string_view path) {
    return std::make_shared<FileJournal>(path);
}

ValueAsync<temp::map<temp::string, int64_t>> IJournal::replay(std::string_view path) {
    ReplayHelper helper{};
    co_await helper.run(path);
    co_return helper.get_result();
}
