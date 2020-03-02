//
// Copyright (c) 2016-2019 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_IMPL_FLAT_STATIC_BUFFER_IPP
#define BOOST_BEAST_IMPL_FLAT_STATIC_BUFFER_IPP

#include <boost/beast/core/flat_static_buffer.hpp>
#include <boost/beast/core/detail/buffer.hpp>
#include <boost/throw_exception.hpp>
#include <algorithm>
#include <cstring>
#include <iterator>
#include <memory>
#include <stdexcept>

namespace boost {
namespace beast {

/*  Layout:

      begin_     in_          out_        last_      end_
        |<------->|<---------->|<---------->|<------->|
                  |  readable  |  writable  |
*/

void
flat_static_buffer_base::
clear() noexcept
{
    in_ = begin_;
    out_ = begin_;
    last_ = begin_;
}

auto
flat_static_buffer_base::
prepare(std::size_t n) ->
    mutable_buffers_type
{
    if(n <= dist(out_, end_))
    {
        last_ = out_ + n;
        return {out_, n};
    }
    auto const len = size();
    if(n > capacity() - len)
        BOOST_THROW_EXCEPTION(std::length_error{
            "buffer overflow"});
    if(len > 0)
        std::memmove(begin_, in_, len);
    in_ = begin_;
    out_ = in_ + len;
    last_ = out_ + n;
    return {out_, n};
}

void
flat_static_buffer_base::
consume(std::size_t n) noexcept
{
    if(n >= size())
    {
        in_ = begin_;
        out_ = in_;
        return;
    }
    in_ += n;
}

void
flat_static_buffer_base::
reset(void* p, std::size_t n) noexcept
{
    begin_ = static_cast<char*>(p);
    in_ = begin_;
    out_ = begin_;
    last_ = begin_;
    end_ = begin_ + n;
}

auto
flat_static_buffer_base::
data_impl(std::size_t pos, std::size_t n)
-> mutable_buffers_type
{
    auto result = data();
    result += (std::min)(pos, result.size());
    result = mutable_buffers_type(result.data(),
        (std::min)(n, result.size()));
    return result;
}

auto
flat_static_buffer_base::
data_impl(std::size_t pos, std::size_t n) const
-> const_buffers_type
{
    auto result = data();
    result += (std::min)(pos, result.size());
    result = const_buffers_type(result.data(),
        (std::min)(n, result.size()));
    return result;
}

void
flat_static_buffer_base::
shrink_impl(std::size_t n)
{
    boost::ignore_unused(prepare(0));
    n = (std::min)(size(), n);
    out_ -= n;
}


} // beast
} // boost

#endif