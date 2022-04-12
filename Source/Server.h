#pragma once

#include "Json.h"
#include <map>
#include <functional>
#include <unordered_map>
#include <kls/Format.h>
#include <kls/phttp/Protocol.h>
#include <kls/coroutine/Async.h>

namespace count {
    using namespace kls;

    struct UserError : std::runtime_error {
        using std::runtime_error::runtime_error;
    };

    class Server {
        struct Endpoint {
            coroutine::ValueAsync<> executor;
            std::unique_ptr<phttp::ServerEndpoint> endpoint;
        };
    public:
        Server(io::Peer peer, int backlog);
        template<class Fn>
        requires requires(std::decay_t<Fn> fn, const temp_json &arg) {
            { fn(arg) } -> std::same_as<coroutine::ValueAsync<temp_json>>;
        }
        void handles(std::string_view method, std::string_view resource, Fn fn) {
            m_handlers[kls::format("{}{}", method, resource)] = fn;
        }
        coroutine::ValueAsync<> run();
        coroutine::ValueAsync<> close();
    private:
        std::mutex m_lock{};
        std::atomic_bool m_active{true};
        std::unique_ptr<phttp::Host> m_host;
        std::unordered_map<void*, Endpoint> m_running{};
        std::unordered_map<std::string, std::function<coroutine::ValueAsync<temp_json>(const temp_json &)>> m_handlers;
        coroutine::ValueAsync<> serve_endpoint(phttp::ServerEndpoint* ep);
        coroutine::ValueAsync<> serve_endpoint_inner(phttp::ServerEndpoint &ep);
        static phttp::Response create_success_response(phttp::Headers &&headers, phttp::Block &&result_block);
        static phttp::Response create_error_response(phttp::Headers &&headers, int code, std::string_view message);
    };
}
