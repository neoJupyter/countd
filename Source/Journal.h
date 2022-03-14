#pragma once

#include <utility>
#include <string_view>
#include <kls/temp/STL.h>
#include <kls/coroutine/ValueAsync.h>

namespace count {
    using namespace kls;
    using namespace kls::coroutine;
    using namespace kls::essential;

    struct Journal {
        virtual int64_t check() = 0;
        virtual ValueAsync<void> rotate() = 0;
        virtual ValueAsync<void> confirm(int64_t id) = 0;
        virtual ValueAsync<temp::vector<std::pair<std::string_view, int64_t>>> replay(int64_t id) = 0;
        virtual ValueAsync<void> value(temp::vector<std::pair<int32_t, int64_t>> store) = 0;
    };
}
