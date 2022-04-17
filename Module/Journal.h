#pragma once

#include <utility>
#include <string_view>
#include <kls/temp/STL.h>
#include <kls/coroutine/Async.h>

namespace njp::count {
    using namespace kls;

    struct IJournal {
        virtual coroutine::ValueAsync<> rotate() = 0;
        virtual coroutine::ValueAsync<> confirm() = 0;
        virtual coroutine::ValueAsync<> values(Span<std::tuple<int32_t, std::string_view, int64_t>> records) = 0;
        virtual coroutine::ValueAsync<> close() = 0;
        static std::shared_ptr<IJournal> create(std::string_view path);
        static coroutine::ValueAsync<temp::map<temp::string, int64_t>> replay(std::string_view path);
    };
}
