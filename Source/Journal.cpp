#include "Json.h"
#include "Journal.h"
#include <kls/essential/Unsafe.h>
#include <kls/coroutine/Operation.h>

namespace count {
    using namespace kls;
    using namespace kls::journal;
    using namespace kls::coroutine;
    using namespace kls::essential;

    Journal::Journal(Cache &cache, std::string_view path) :
            m_cache(cache), m_journal(create_file_journal(path)) {}

    ValueAsync<> Journal::rotate() {
        m_id.clear();
        co_await m_journal->register_checkpoint();
    }

    ValueAsync<> Journal::confirm() { return m_journal->check_checkpoint(); }

    template<class T, class U>
    static temp::string kv_json(int type, T&& k, U&& v) {
        temp_json json{};
        json["t"] = type, json["k"] = std::forward<T>(k), json["v"] = std::forward<U>(v);
        return json.dump();
    }

    ValueAsync<> Journal::value(int32_t key, int64_t value) {
        auto value_entry = kv_json(0, key, value);
        if (!m_id.contains(key)) {
            auto name_entry = kv_json(1, key, m_cache.name(key));
            co_await awaits(
                    m_journal->append(Span<char>(name_entry)),
                    m_journal->append(Span<char>(value_entry))
            );
        }
        co_await m_journal->append(Span<char>(value_entry));
    }

    ValueAsync<> Journal::close() { return m_journal->close(); }

    static temp::map<temp::string, int64_t> pack_checkpoint(
            temp::unordered_map<int32_t, temp::string> &string_map,
            temp::unordered_map<int32_t, int64_t> &value_map
    ) {
        temp::map<temp::string, int64_t> result{};
        for (auto&&[k, v]: value_map) result[string_map[k]] = v;
        string_map.clear();
        value_map.clear();
        return result;
    }

    ValueAsync<temp::map<temp::string, int64_t>> Journal::replay(std::string_view path) {
        temp::map<int64_t, temp::map<temp::string, int64_t>> checkpoints{};
        temp::unordered_map<int32_t, temp::string> checkpoint_string_map{};
        temp::unordered_map<int32_t, int64_t> checkpoint_value_map{};
        int64_t checkpoint_min{0}, checkpoint_max{0};
        auto recover = recover_file_journal(path);
        while (co_await recover.forward()) {
            JournalRecord record = recover.next();
            auto range = static_span_cast<char>(record.data);
            if (record.type == 1) {
                // update the checkpoint collection
                essential::Access<std::endian::little> access{range};
                const auto new_checkpoint_min = access.get<int64_t>(0);
                const auto new_checkpoint_max = access.get<int64_t>(8);
                if (new_checkpoint_max != checkpoint_max) {
                    checkpoints[checkpoint_max] = pack_checkpoint(checkpoint_string_map, checkpoint_value_map);
                    checkpoint_max = new_checkpoint_max;
                }
                checkpoint_min = new_checkpoint_min;
            } else {
                // append to the current checkpoint
                auto json = temp_json::parse(std::string_view{range.begin(), range.end()});
                const int32_t type = json["t"];
                if (type == 0) checkpoint_value_map[json["k"]] = json["v"]; // value_type = 0
                if (type == 1) checkpoint_string_map[json["k"]] = json["v"]; // key_type = 1
                // do not care any other type for this journal version
            }
        }
        temp::map<temp::string, int64_t> result{};
        checkpoints[checkpoint_max] = pack_checkpoint(checkpoint_string_map, checkpoint_value_map);
        for (auto i = checkpoint_min; i <= checkpoint_max; ++i) result.merge(std::move(checkpoints[i]));
        co_return result;
    }
}