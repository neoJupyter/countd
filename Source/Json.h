#pragma once

#include <kls/temp/STL.h>
#include <nlohmann/json.hpp>
#include <kls/phttp/Protocol.h>

namespace count {
    using json = nlohmann::basic_json<>;

    using temp_json = nlohmann::basic_json<
            std::map, std::vector, kls::temp::string,
            bool, std::int64_t, std::uint64_t, double,
            kls::temp::allocator, nlohmann::adl_serializer, kls::temp::vector<std::uint8_t>
    >;

    class JsonRemoteException : public std::exception {
    public:
        JsonRemoteException(
                std::string_view method, std::string_view path, std::string_view request,
                int code, std::string_view message, std::string_view response
        );
        [[nodiscard]] const char *what() const noexcept override;
        [[nodiscard]] auto method() const noexcept { return m_method; }
        [[nodiscard]] auto path() const noexcept { return m_path; }
        [[nodiscard]] auto request() const noexcept { return m_request; }
        [[nodiscard]] auto code() const noexcept { return m_code; };
        [[nodiscard]] auto message() const noexcept { return m_message; }
        [[nodiscard]] auto response() const noexcept { return m_response; }
    private:
        int m_code;
        std::string m_what_data;
        std::string_view m_method, m_path, m_request, m_message, m_response;
    };

    class JsonClient {
    public:
        explicit JsonClient(std::unique_ptr<kls::phttp::Endpoint> endpoint);
        kls::coroutine::ValueAsync<temp_json> exec(std::string_view m, std::string_view p, const temp_json &request);
        kls::coroutine::ValueAsync<> close();
    private:
        std::unique_ptr<kls::phttp::ClientEndpoint> m_endpoint;
    };

    template<class T, class Json>
    auto from_json(const Json &json) {
        T result = json;
        return result;
    }

    template<class Json, class T>
    Json to_json(const T &o) { return Json{o}; }

    template<class Json>
    Json json_from_block(const kls::phttp::Block& block) {
        const auto range = kls::static_span_cast<char>(block.content());
        return Json::parse(std::string_view(range.begin(), range.end()));
    }

    template<class Json>
    kls::phttp::Block json_to_block(const Json &json) {
        auto content = json.dump();
        auto block = kls::phttp::Block(int32_t(content.size()), kls::temp::resource());
        kls::copy(kls::Span<>{kls::Span<char>{content}}, block.content());
        return block;
    }
}