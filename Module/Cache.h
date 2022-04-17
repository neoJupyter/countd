#pragma once

#include "Storage.h"
#include "Journal.h"
#include <string_view>
#include <kls/temp/STL.h>

namespace njp::count {
    using namespace kls;

    struct ICache : kls::PmrBase {
        // computation load/store
        virtual int64_t store(int32_t id, int64_t v) noexcept = 0;
        [[nodiscard]] virtual int64_t load(int32_t id) const noexcept = 0;
        // program validation methods
        virtual temp::vector<int32_t> acquires(const temp::vector<temp::string> &names) = 0;
        // program execution methods
        virtual coroutine::ValueAsync<> ensure(const temp::vector<int32_t> &ids) = 0;
        virtual coroutine::ValueAsync<> commit(const temp::vector<int32_t> &ids) = 0;
        // control methods
        virtual coroutine::ValueAsync<> close() = 0;
        static std::shared_ptr<ICache> create(std::shared_ptr<IStorage> storage, std::shared_ptr<IJournal> journal);
    };
}