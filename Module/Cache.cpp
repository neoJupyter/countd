#include "Cache.h"
#include <kls/coroutine/Timed.h>
#include <kls/coroutine/Trigger.h>

using namespace njp;
using namespace kls;
using namespace njp::count;
using namespace coroutine;

namespace {
    struct Hash {
        using hash_type = std::hash<std::string_view>;
        using is_transparent = void;
        size_t operator()(const char *str) const { return hash_type{}(str); }
        size_t operator()(std::string_view str) const { return hash_type{}(str); }
        size_t operator()(std::string const &str) const { return hash_type{}(str); }
    };

    struct Inverse {
        FifoExecutorTrigger trigger{};
        bool fetched{false};
        std::string name{};
        int32_t reference_count{0};
        int64_t last_use_rotation{};
        int64_t last_cached_value{};
        int64_t cached_value{};
        std::exception_ptr fetch_fail{};

        class Await : public AddressSensitive {
        public:
            explicit Await(Inverse *flex) noexcept: m_i(flex) {}
            [[nodiscard]] constexpr bool await_ready() const noexcept { return false; } // NOLINT
            bool await_suspend(std::coroutine_handle<> h) { return (m_e.set_handle(h), m_i->trigger.trap(m_e)); }
            void await_resume() { if (m_i->fetch_fail) std::rethrow_exception(m_i->fetch_fail); }
        private:
            Inverse *m_i;
            FifoExecutorAwaitEntry m_e{};
        };

        Await operator co_await() noexcept { return Await(this); }
    };

    class Cache : public ICache {
        using Clock = std::chrono::steady_clock;
        using Batch = std::map<std::string, int64_t>;
    public:
        Cache(std::shared_ptr<IStorage> storage, std::shared_ptr<IJournal> journal)
                : m_storage(std::move(storage)), m_journal(std::move(journal)), m_worker_future{consume()}  {}

        int64_t store(int32_t id, int64_t v) noexcept override { return m_inverse[id].cached_value = v; }
        [[nodiscard]] int64_t load(int32_t id) const noexcept override { return m_inverse[id].cached_value; }
        temp::vector<int32_t> acquires(const temp::vector<temp::string> &names) override;
        ValueAsync<> ensure(const temp::vector<int32_t> &ids) override;
        ValueAsync<> commit(const temp::vector<int32_t> &ids) override;
        ValueAsync<> close() override;
    private:
        int32_t m_top{0};
        int64_t m_rotation{0};
        std::deque<int32_t> m_free{};
        std::deque<Inverse> m_inverse{};
        std::unordered_map<std::string, int32_t, Hash, std::equal_to<>> m_forward{};
        std::shared_ptr<IStorage> m_storage;
        std::shared_ptr<IJournal> m_journal;

        int32_t assign_id();
        int32_t acquire_single(std::string_view name);
        void release(const temp::vector<int32_t> &ids) noexcept;
        ValueAsync<> fetch_missing(const temp::vector<int32_t> &ids);
        ValueAsync<> perform_fetch(const temp::vector<int32_t> &ids, const temp::vector<std::string_view> &names);

        Batch m_this_batch{};
        bool m_should_stop{false};
        ValueAsync<> m_worker_future;

        ValueAsync<> consume();
        Cache::Batch rotate() noexcept;
        ValueAsync<bool> store(const Batch &batch);
    };

    ValueAsync<> Cache::close() {
        m_should_stop = true;
        co_await std::move(m_worker_future);
    }

    void Cache::release(const temp::vector<int32_t> &ids) noexcept {
        for (const auto id: ids) {
            auto &entry = m_inverse[id];
            if (--entry.reference_count == 0) entry.last_use_rotation = m_rotation;
        }
    }

    temp::vector<int32_t> Cache::acquires(const temp::vector<temp::string> &names) {
        temp::vector<int32_t> result{};
        result.reserve(names.size());
        for (auto &&name: names) result.push_back(acquire_single(name));
        return result;
    }

    ValueAsync<> Cache::fetch_missing(const temp::vector<int32_t> &ids) {
        temp::vector<int32_t> to_fetch_id{};
        temp::vector<std::string_view> to_fetch_name{};
        for (const auto id: ids) {
            if (auto &&x = m_inverse[id]; !x.fetched) {
                x.fetched = true;
                to_fetch_id.push_back(id);
                to_fetch_name.push_back(x.name);
            }
        }
        co_await perform_fetch(to_fetch_id, to_fetch_name);
    }

    ValueAsync<> Cache::perform_fetch(const temp::vector<int32_t> &ids, const temp::vector<std::string_view> &names) {
        try {
            const auto values = co_await m_storage->fetch(names);
            for (int index = 0; const auto id: ids) {
                auto &&x = m_inverse[id];
                x.cached_value = x.last_cached_value = values[index++];
            }
        }
        catch (...) { for (const auto id: ids) m_inverse[id].fetch_fail = std::current_exception(); }
        for (const auto id: ids) m_inverse[id].trigger.pull();
    }

    ValueAsync<> Cache::ensure(const temp::vector<int32_t> &ids) {
        co_await fetch_missing(ids);
        try { for (const auto id: ids) co_await m_inverse[id]; }
        catch (...) { (release(ids), throw); } // if any of the fetches failed release everything immediately
    }

    ValueAsync<> Cache::commit(const temp::vector<int32_t> &ids) {
        temp::vector<std::tuple<int32_t, std::string_view, int64_t>> batch{};
        batch.reserve(ids.size());
        for (const auto id: ids) {
            auto &&x = m_inverse[id];
            if (x.last_cached_value != x.cached_value) {
                x.last_cached_value = x.cached_value;
                m_this_batch[x.name] = x.cached_value;
                batch.push_back({id, x.name, x.cached_value});
            }
        }
        release(ids);
        co_await m_journal->values({batch});
    }

    Cache::Batch Cache::rotate() noexcept {
        // collect stale ids
        auto size = m_inverse.size();
        for (auto i = 0; i < size; ++i) {
            auto &x = m_inverse[i];
            if (x.reference_count == 0 && x.last_use_rotation < m_rotation) {
                m_forward.erase(m_forward.find(x.name));
                std::destroy_at(&x), std::construct_at(&x);
                m_free.push_back(i);
            }
        }
        ++m_rotation;
        // return the current batch of values to commit
        return std::exchange(m_this_batch, {});
    }

    ValueAsync<bool> Cache::store(const Cache::Batch &batch) {
        try {
            temp::unordered_map<std::string_view, int64_t> map{};
            for (auto &&[k, v]: batch) map[k] = v;
            co_await m_storage->store(std::move(map));
        }
        catch (...) { co_return false; }
        co_return true;
    }

    ValueAsync<> Cache::consume() {
        std::deque<Batch> stacked{};
        do {
            auto time_point = Clock::now() + std::chrono::seconds(5);
            if (auto batch = rotate(); !batch.empty()) {
                stacked.push_back(std::move(batch));
                co_await m_journal->rotate();
            }
            while (!stacked.empty()) {
                if (!co_await store(stacked.front())) break;
                stacked.pop_front();
                co_await m_journal->confirm();
            }
            co_await delay_until(time_point);
        } while (!m_should_stop);
    }

    int32_t Cache::acquire_single(std::string_view name) {
        auto iter = m_forward.find(name);
        if (iter != m_forward.end()) {
            auto id = iter->second;
            return (++m_inverse[id].reference_count, id);
        } else {
            auto id = assign_id();
            auto &entry = m_inverse[id];
            auto &&[it, _] = m_forward.insert_or_assign(std::string(name), id);
            return (entry.name = it->first, entry.reference_count = 1, id);
        }
    }

    int32_t Cache::assign_id() {
        if (!m_free.empty()) {
            auto id = m_free.back();
            return (m_free.pop_back(), id);
        }
        return (m_inverse.emplace_back(), ++m_top);
    }
}

std::shared_ptr<ICache> ICache::create(std::shared_ptr<IStorage> storage, std::shared_ptr<IJournal> journal) {
    return std::make_shared<Cache>(std::move(storage), std::move(journal));
}
