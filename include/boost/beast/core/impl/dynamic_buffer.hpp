//
// Copyright (c) 2020 Richard Hodges (hodges.r@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_CORE_IMPL_DYNAMIC_BUFFER_HPP
#define BOOST_BEAST_CORE_IMPL_DYNAMIC_BUFFER_HPP

#include <boost/beast/core/dynamic_buffer.hpp>
#include <memory>

namespace boost {
namespace beast {

template<class DynamicBuffer_v0>
dynamic_buffer_v0_proxy<DynamicBuffer_v0>::
dynamic_buffer_v0_proxy(
    DynamicBuffer_v0& storage) noexcept
: storage_(std::addressof(storage))
{
    boost::ignore_unused(storage_->prepare(0));
}

#if BOOST_BEAST_DOXYGEN

template<class DynamicBuffer_v0>
dynamic_buffer_v0_proxy<DynamicBuffer_v0>::
dynamic_buffer_v0_proxy(
    dynamic_buffer_v0_proxy const&) noexcept
= default;

template<class DynamicBuffer_v0>
auto
dynamic_buffer_v0_proxy<DynamicBuffer_v0>::
operator=(
    dynamic_buffer_v0_proxy const&) noexcept ->
dynamic_buffer_v0_proxy&
= default;

template<class DynamicBuffer_v0>
dynamic_buffer_v0_proxy<DynamicBuffer_v0>::
~dynamic_buffer_v0_proxy() noexcept
= default;

#endif // BOOST_BEAST_DOXYGEN

template<class DynamicBuffer_v0>
auto
dynamic_buffer_v0_proxy<DynamicBuffer_v0>::
size() const ->
std::size_t
{
    return storage_->size();
}

template<class DynamicBuffer_v0>
auto
dynamic_buffer_v0_proxy<DynamicBuffer_v0>::
max_size() const ->
std::size_t
{
    return storage_->max_size();
}

template<class DynamicBuffer_v0>
auto
dynamic_buffer_v0_proxy<DynamicBuffer_v0>::
capacity() const ->
std::size_t
{
    return storage_->capacity();
}

template<class DynamicBuffer_v0>
auto
dynamic_buffer_v0_proxy<DynamicBuffer_v0>::
consume(std::size_t n) ->
void
{
    storage_->consume(n);
}

template<class DynamicBuffer_v0>
auto
dynamic_buffer_v0_proxy<DynamicBuffer_v0>::
data(std::size_t pos, std::size_t n) const ->
const_buffers_type
{
    return detail::dynamic_buffer_v2_access::
    data(static_cast<DynamicBuffer_v0 const&>(*storage_),
         pos, n);
}

template<class DynamicBuffer_v0>
auto
dynamic_buffer_v0_proxy<DynamicBuffer_v0>::
data(std::size_t pos, std::size_t n) ->
mutable_buffers_type
{
    return detail::dynamic_buffer_v2_access::
    data(*storage_, pos, n);
}

template<class DynamicBuffer_v0>
auto
dynamic_buffer_v0_proxy<DynamicBuffer_v0>::
grow(std::size_t n) ->
void
{
    storage_->commit(net::buffer_size(storage_->prepare(n)));
}

template<class DynamicBuffer_v0>
auto
dynamic_buffer_v0_proxy<DynamicBuffer_v0>::
shrink(std::size_t n) ->
void
{
    detail::dynamic_buffer_v2_access::
    shrink(*storage_, n);
}

// ---------------------------------------------------------

template<class DynamicBuffer_v0>
auto
dynamic_buffer(DynamicBuffer_v0& target) ->
#if BOOST_BEAST_DOXYGEN
dynamic_buffer_v0_proxy<DynamicBuffer_v0>
#else
typename std::enable_if<
    detail::is_dynamic_buffer_v0<DynamicBuffer_v0>::value,
    dynamic_buffer_v0_proxy<DynamicBuffer_v0>>::type
#endif
{
return
dynamic_buffer_v0_proxy<
    DynamicBuffer_v0>(
    target);
}

template<class DynamicBuffer_v2>
auto
dynamic_buffer(DynamicBuffer_v2 buffer) ->
#if BOOST_BEAST_DOXYGEN
DynamicBuffer_v2
#else
typename std::enable_if<
    boost::asio::is_dynamic_buffer_v2<DynamicBuffer_v2>::value,
    DynamicBuffer_v2>::type
#endif
{
    return buffer;
}

} // beast
} // boost

#endif
