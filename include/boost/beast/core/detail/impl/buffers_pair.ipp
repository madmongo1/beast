//
// Copyright (c) 2016-2020 Vinnie Falco (vinnie dot falco at gmail dot com)
// Copyright (c) 2020 Richard Hodges (hodges.r@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_DETAIL_IMPL_BUFFERS_PAIR_IPP
#define BOOST_BEAST_DETAIL_IMPL_BUFFERS_PAIR_IPP

#include <boost/beast/core/detail/buffers_pair.hpp>

namespace boost {
namespace beast {
namespace detail {

template<bool isMutable>
buffers_pair<isMutable>::
buffers_pair(
    buffers_pair const &other,
    std::size_t pos,
    std::size_t n)
    : buffers_pair(other)
{
    if (pos >= b_[0].size())
    {
        pos -= b_[0].size();
        b_[0] = b_[1];
        b_[1] = {};
    }
    if (pos >= b_[0].size())
    {
        b_[0] = {};
        return;
    }
    b_[0] += pos;
    if (n <= b_[0].size())
    {
        b_[0] = {b_[0].data(), n};
        b_[1] = {};
        return;
    }
    n -= b_[0].size();
    if (n <= b_[1].size())
    {
        b_[1] = {b_[1].data(), n};
    }
}

#ifndef BOOST_BEAST_HEADER_ONLY

template
buffers_pair<true>::
buffers_pair(
    buffers_pair<true> const &,
    std::size_t,
    std::size_t);

template
buffers_pair<false>::
buffers_pair(
    buffers_pair const &,
    std::size_t,
    std::size_t);

#endif // BOOST_BEAST_HEADER_ONLY

}
}
}

#endif // BOOST_BEAST_DETAIL_IMPL_BUFFERS_PAIR_IPP
