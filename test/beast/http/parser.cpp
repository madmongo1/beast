//
// Copyright (c) 2016-2019 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

// Test that header file is self-contained.
#include <boost/beast/http/parser.hpp>

#include "test_parser.hpp"

#include <boost/beast/_experimental/unit_test/suite.hpp>
#include <boost/beast/core/buffer_traits.hpp>
#include <boost/beast/core/buffers_suffix.hpp>
#include <boost/beast/core/flat_buffer.hpp>
#include <boost/beast/core/multi_buffer.hpp>
#include <boost/beast/core/ostream.hpp>
#include <boost/beast/http/read.hpp>
#include <boost/beast/http/string_body.hpp>
#include <boost/asio/buffers_iterator.hpp>
#include <boost/system/system_error.hpp>
#include <algorithm>
#include <iterator>

namespace boost {
namespace beast {
namespace http {

namespace {

template<std::size_t Min>
struct all_or_nothing_body : string_body
{
    using value_type = std::string;

    /*
     *     std::declval<typename T::reader&>().init(
boost::optional<std::uint64_t>(),
std::declval<error_code&>()),
std::declval<std::size_t&>() =
std::declval<typename T::reader&>().put(
    std::declval<net::const_buffer>(),
    std::declval<error_code&>()),
std::declval<typename T::reader&>().finish(
std::declval<error_code&>())
)>> : std::integral_constant<bool,
(std::is_constructible<typename T::reader,
    header<true, detail::fields_model>&,
        typename T::value_type&>::value &&
std::is_constructible<typename T::reader,
    header<false,detail::fields_model>&,
        typename T::value_type&>::value)
>

     */
    struct reader
    {
        template<bool isRequest, class Fields>
        reader(header<isRequest, Fields>& hdr, value_type& v)
            : v_(v)
        {
            boost::ignore_unused(hdr);
        }


        void init(boost::optional<std::uint64_t>, error_code&)
        {

        }

        template<class ConstBufferSequence>
        std::size_t put(ConstBufferSequence const& bufs, error_code& ec )
        {
            if (buffer_bytes(bufs) < Min)
            {
                ec = error::need_more;
                return 0;
            }
            else
            {
                std::size_t total = 0;
                for (auto i = net::buffer_sequence_begin(bufs) ; i != net::buffer_sequence_begin(bufs) ; ++i)
                {
                    auto&& buf = *i;
                    v_.append(reinterpret_cast<const char *>(buf.data()), buf.size());
                    total += buf.size();
                }
                return total;
            }
        }

        void finish(error_code& ec)
        {
            if (v_.size() < Min)
                ec = error::need_more;
        }

        value_type& v_;

    };
};

}

class parser_test
    : public beast::unit_test::suite
{
public:
    template<bool isRequest>
    using parser_type =
        parser<isRequest, string_body>;

    static
    net::const_buffer
    buf(string_view s)
    {
        return {s.data(), s.size()};
    }

    template<class ConstBufferSequence,
        bool isRequest>
    static
    void
    put(ConstBufferSequence const& buffers,
        basic_parser<isRequest>& p,
            error_code& ec)
    {
        buffers_suffix<ConstBufferSequence> cb{buffers};
        for(;;)
        {
            auto const used = p.put(cb, ec);
            cb.consume(used);
            if(ec)
                return;
            if(p.need_eof() &&
                buffer_bytes(cb) == 0)
            {
                p.put_eof(ec);
                if(ec)
                    return;
            }
            if(p.is_done())
                break;
        }
    }

    template<bool isRequest, class F>
    void
    doMatrix(string_view s0, F const& f)
    {
        // parse a single buffer
        {
            auto s = s0;
            error_code ec;
            parser_type<isRequest> p;
            put(net::buffer(s.data(), s.size()), p, ec);
            if(! BEAST_EXPECTS(! ec, ec.message()))
                return;
            f(p);
        }
        // parse two buffers
        for(auto n = s0.size() - 1; n >= 1; --n)
        {
            auto s = s0;
            error_code ec;
            parser_type<isRequest> p;
            p.eager(true);
            auto used =
                p.put(net::buffer(s.data(), n), ec);
            s.remove_prefix(used);
            if(ec == error::need_more)
                ec = {};
            if(! BEAST_EXPECTS(! ec, ec.message()))
                continue;
            BEAST_EXPECT(! p.is_done());
            used = p.put(
                net::buffer(s.data(), s.size()), ec);
            s.remove_prefix(used);
            if(! BEAST_EXPECTS(! ec, ec.message()))
                continue;
            BEAST_EXPECT(s.empty());
            if(p.need_eof())
            {
                p.put_eof(ec);
                if(! BEAST_EXPECTS(! ec, ec.message()))
                    continue;
            }
            if(BEAST_EXPECT(p.is_done()))
                f(p);
        }
    }

    void
    testParse()
    {
        doMatrix<false>(
            "HTTP/1.0 200 OK\r\n"
            "Server: test\r\n"
            "\r\n"
            "Hello, world!",
            [&](parser_type<false> const& p)
            {
                auto const& m = p.get();
                BEAST_EXPECT(! p.chunked());
                BEAST_EXPECT(p.need_eof());
                BEAST_EXPECT(p.content_length() == boost::none);
                BEAST_EXPECT(m.version() == 10);
                BEAST_EXPECT(m.result() == status::ok);
                BEAST_EXPECT(m.reason() == "OK");
                BEAST_EXPECT(m["Server"] == "test");
                BEAST_EXPECT(m.body() == "Hello, world!");
            }
        );
        doMatrix<false>(
            "HTTP/1.1 200 OK\r\n"
            "Server: test\r\n"
            "Expect: Expires, MD5-Fingerprint\r\n"
            "Transfer-Encoding: chunked\r\n"
            "\r\n"
            "5\r\n"
            "*****\r\n"
            "2;a;b=1;c=\"2\"\r\n"
            "--\r\n"
            "0;d;e=3;f=\"4\"\r\n"
            "Expires: never\r\n"
            "MD5-Fingerprint: -\r\n"
            "\r\n",
            [&](parser_type<false> const& p)
            {
                auto const& m = p.get();
                BEAST_EXPECT(! p.need_eof());
                BEAST_EXPECT(p.chunked());
                BEAST_EXPECT(p.content_length() == boost::none);
                BEAST_EXPECT(m.version() == 11);
                BEAST_EXPECT(m.result() == status::ok);
                BEAST_EXPECT(m.reason() == "OK");
                BEAST_EXPECT(m["Server"] == "test");
                BEAST_EXPECT(m["Transfer-Encoding"] == "chunked");
                BEAST_EXPECT(m["Expires"] == "never");
                BEAST_EXPECT(m["MD5-Fingerprint"] == "-");
                BEAST_EXPECT(m.body() == "*****--");
            }
        );
        doMatrix<false>(
            "HTTP/1.0 200 OK\r\n"
            "Server: test\r\n"
            "Content-Length: 5\r\n"
            "\r\n"
            "*****",
            [&](parser_type<false> const& p)
            {
                auto const& m = p.get();
                BEAST_EXPECT(m.body() == "*****");
            }
        );
        doMatrix<true>(
            "GET / HTTP/1.1\r\n"
            "User-Agent: test\r\n"
            "\r\n",
            [&](parser_type<true> const& p)
            {
                auto const& m = p.get();
                BEAST_EXPECT(m.method() == verb::get);
                BEAST_EXPECT(m.target() == "/");
                BEAST_EXPECT(m.version() == 11);
                BEAST_EXPECT(! p.need_eof());
                BEAST_EXPECT(! p.chunked());
                BEAST_EXPECT(p.content_length() == boost::none);
            }
        );
        doMatrix<true>(
            "GET / HTTP/1.1\r\n"
            "User-Agent: test\r\n"
            "X: \t x \t \r\n"
            "\r\n",
            [&](parser_type<true> const& p)
            {
                auto const& m = p.get();
                BEAST_EXPECT(m["X"] == "x");
            }
        );

        // test eager(true)
        {
            error_code ec;
            parser_type<true> p;
            p.eager(true);
            p.put(buf(
                "GET / HTTP/1.1\r\n"
                "User-Agent: test\r\n"
                "Content-Length: 1\r\n"
                "\r\n"
                "*")
                , ec);
            auto const& m = p.get();
            BEAST_EXPECT(! ec);
            BEAST_EXPECT(p.is_done());
            BEAST_EXPECT(p.is_header_done());
            BEAST_EXPECT(! p.need_eof());
            BEAST_EXPECT(m.method() == verb::get);
            BEAST_EXPECT(m.target() == "/");
            BEAST_EXPECT(m.version() == 11);
            BEAST_EXPECT(m["User-Agent"] == "test");
            BEAST_EXPECT(m.body() == "*");
        }
        {
            // test partial parsing of final chunk
            // parse through the chunk body
            error_code ec;
            flat_buffer b;
            parser_type<true> p;
            p.eager(true);
            ostream(b) <<
                "PUT / HTTP/1.1\r\n"
                "Transfer-Encoding: chunked\r\n"
                "\r\n"
                "1\r\n"
                "*";
            auto used = p.put(b.data(), ec);
            b.consume(used);
            BEAST_EXPECT(! ec);
            BEAST_EXPECT(! p.is_done());
            BEAST_EXPECT(p.get().body() == "*");
            ostream(b) <<
                "\r\n"
                "0;d;e=3;f=\"4\"\r\n"
                "Expires: never\r\n"
                "MD5-Fingerprint: -\r\n";
            // incomplete parse, missing the final crlf
            used = p.put(b.data(), ec);
            b.consume(used);
            BEAST_EXPECT(ec == error::need_more);
            ec = {};
            BEAST_EXPECT(! p.is_done());
            ostream(b) <<
                "\r\n"; // final crlf to end message
            used = p.put(b.data(), ec);
            b.consume(used);
            BEAST_EXPECTS(! ec, ec.message());
            BEAST_EXPECT(p.is_done());
        }
        // skip body
        {
            error_code ec;
            response_parser<string_body> p;
            p.skip(true);
            p.put(buf(
                "HTTP/1.1 200 OK\r\n"
                "Content-Length: 5\r\n"
                "\r\n"
                "*****")
                , ec);
            BEAST_EXPECTS(! ec, ec.message());
            BEAST_EXPECT(p.is_done());
            BEAST_EXPECT(p.is_header_done());
            BEAST_EXPECT(p.content_length() &&
                *p.content_length() == 5);
        }
    }

    //--------------------------------------------------------------------------

    template<class DynamicBuffer>
    void
    testNeedMore()
    {
        error_code ec;
        std::size_t used;
        {
            DynamicBuffer b;
            parser_type<true> p;
            ostream(b) <<
                "GET / HTTP/1.1\r\n";
            used = p.put(b.data(), ec);
            BEAST_EXPECTS(ec == error::need_more, ec.message());
            b.consume(used);
            ec = {};
            ostream(b) <<
                "User-Agent: test\r\n"
                "\r\n";
            used = p.put(b.data(), ec);
            BEAST_EXPECTS(! ec, ec.message());
            b.consume(used);
            BEAST_EXPECT(p.is_done());
            BEAST_EXPECT(p.is_header_done());
        }
    }

    void
    testGotSome()
    {
        error_code ec;
        parser_type<true> p;
        auto used = p.put(buf(""), ec);
        BEAST_EXPECT(ec == error::need_more);
        BEAST_EXPECT(! p.got_some());
        BEAST_EXPECT(used == 0);
        ec = {};
        used = p.put(buf("G"), ec);
        BEAST_EXPECT(ec == error::need_more);
        BEAST_EXPECT(p.got_some());
        BEAST_EXPECT(used == 0);
    }

    void
    testIssue818()
    {
        // Make sure that the parser clears pre-existing fields
        request<string_body> m;
        m.set(field::accept, "html/text");
        BEAST_EXPECT(std::distance(m.begin(), m.end()) == 1);
        request_parser<string_body> p{std::move(m)};
        BEAST_EXPECT(std::distance(m.begin(), m.end()) == 0);
        auto& m1 = p.get();
        BEAST_EXPECT(std::distance(m1.begin(), m1.end()) == 0);
    }

    void
    testIssue1187()
    {
        // make sure parser finishes on redirect
        error_code ec;
        parser_type<false> p;
        p.eager(true);
        p.put(buf(
            "HTTP/1.1 301 Moved Permanently\r\n"
            "Location: https://www.ebay.com\r\n"
            "\r\n\r\n"), ec);
        BEAST_EXPECTS(! ec, ec.message());
        BEAST_EXPECT(p.is_header_done());
        BEAST_EXPECT(! p.is_done());
        BEAST_EXPECT(p.need_eof());
    }

    void
    testIssue1880()
    {
        // A user raised the issue that multiple Content-Length fields and
        // values are permissible provided all values are the same.
        // See rfc7230 section-3.3.2
        // https://tools.ietf.org/html/rfc7230#section-3.3.2
        // Credit: Dimitry Bulsunov

        auto checkPass = [&](std::string const& message)
        {
            response_parser<string_body> parser;
            error_code ec;
            parser.put(net::buffer(message), ec);
            BEAST_EXPECTS(!ec.failed(), ec.message());
        };

        auto checkFail = [&](std::string const& message)
        {
            response_parser<string_body> parser;
            error_code ec;
            parser.put(net::buffer(message), ec);
            BEAST_EXPECTS(ec == error::bad_content_length, ec.message());
        };

        // multiple contents lengths the same
        checkPass(
            "HTTP/1.1 200 OK\r\n"
            "Content-Length: 0\r\n"
            "Content-Length: 0\r\n"
            "\r\n");

        // multiple contents lengths different
        checkFail(
            "HTTP/1.1 200 OK\r\n"
            "Content-Length: 0\r\n"
            "Content-Length: 1\r\n"
            "\r\n");

        // multiple content in same header
        checkPass(
            "HTTP/1.1 200 OK\r\n"
            "Content-Length: 0, 0, 0\r\n"
            "\r\n");

        // multiple content in same header but mismatch (case 1)
        checkFail(
            "HTTP/1.1 200 OK\r\n"
            "Content-Length: 0, 0, 1\r\n"
            "\r\n");

        // multiple content in same header but mismatch (case 2)
        checkFail(
            "HTTP/1.1 200 OK\r\n"
            "Content-Length: 0, 0, 0\r\n"
            "Content-Length: 1\r\n"
            "\r\n");
    }

    void testChunkedBodySize()
    {
        string_view resp =
            "HTTP/1.1 200 OK\r\n"
            "Server: test\r\n"
            "Transfer-Encoding: chunked\r\n"
            "\r\n"

            // chunk 1
            "4\r\n"
            "Wiki\r\n"

            // chunk 2
            "5\r\n"
            "pedia\r\n"

            // chunk 3
            "E\r\n"
            " in\r\n"
            "\r\n"
            "chunks.\r\n"

            // end
            "0\r\n"
            "\r\n";

        {  // body limit not exceeded
            response_parser<string_body> p;
            p.eager(true);
            p.body_limit(23);
            error_code ec;
            p.put(net::buffer(resp.data(), resp.size()), ec);
            BEAST_EXPECTS(ec.message() == "Success", ec.message());
            p.put_eof(ec);
            BEAST_EXPECTS(ec.message() == "Success", ec.message());
        }

        {  // body limit exceeded
            response_parser<string_body> p;
            p.eager(true);
            p.body_limit(22);
            error_code ec;
            p.put(net::buffer(resp.data(), resp.size()), ec);
            BEAST_EXPECTS(ec.message() == "body limit exceeded", ec.message());
            p.put_eof(ec);
            BEAST_EXPECTS(ec.message() == "partial message", ec.message());
        }
    }

    void testIssue1897()
    {
        string_view resp1 =
            "HTTP/1.1 200 OK\r\n"
            "Server: test\r\n"
            "Transfer-Encoding: chunked\r\n"
            "\r\n"

            // chunk 1
            "4\r\n"
            "Wiki\r\n"

            // chunk 2
            "5\r\n"
            "pedia\r\n";

        string_view resp2 =
            // chunk 3
            "E\r\n"
            " in\r\n"
            "\r\n"
            "chunks.\r\n"

            // end
            "0\r\n"
            "\r\n";

        response_parser<all_or_nothing_body<23>> p;
        p.eager(true);
        error_code ec;
        auto s1 = p.put(net::buffer(resp1.data(), resp1.size()), ec);
        BEAST_EXPECTS(ec.message() == "need more", ec.message());
        BEAST_EXPECTS(s1 == 30, std::to_string(s1));
        auto s2 = p.put(net::buffer(resp2.data(), resp2.size()), ec);
        BEAST_EXPECTS(ec.message() == "Success", ec.message());
        BEAST_EXPECTS(s2 == 30, std::to_string(s2));

    }

    void
    run() override
    {
        testIssue1897();
        testParse();
        testNeedMore<flat_buffer>();
        testNeedMore<multi_buffer>();
        testGotSome();
        testIssue818();
        testIssue1187();
        testIssue1880();
        testChunkedBodySize();
    }
};

BEAST_DEFINE_TESTSUITE(beast,http,parser);

} // http
} // beast
} // boost
