#pragma once

#include "Cache.h"
#include <utility>
#include <string_view>
#include <kls/temp/STL.h>
#include <kls/journal/Journal.h>
#include <kls/coroutine/Async.h>

namespace count {
    using namespace kls;

    class Journal {
    public:
        explicit Journal(Cache &cache, std::string_view path);
        coroutine::ValueAsync<> rotate();
        coroutine::ValueAsync<> confirm();
        coroutine::ValueAsync<> value(int32_t key, int64_t value);
        coroutine::ValueAsync<> close();
        static coroutine::ValueAsync<temp::map<temp::string, int64_t>> replay(std::string_view path);
    private:
        Cache &m_cache;
        std::unordered_set<int32_t> m_id;
        std::shared_ptr<journal::AppendJournal> m_journal;
    };
}
