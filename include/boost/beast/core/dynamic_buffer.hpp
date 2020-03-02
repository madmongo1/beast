//
// Copyright (c) 2020 Richard Hodges (hodges.r@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_CORE_DYNAMIC_BUFFER_HPP
#define BOOST_BEAST_CORE_DYNAMIC_BUFFER_HPP

#include <boost/beast/core/detail/config.hpp>
#include <boost/beast/core/detail/buffers_pair.hpp>
#include <boost/beast/core/detail/dynamic_buffer_v0.hpp>
#include <boost/asio/associated_allocator.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/assert.hpp>
#include <boost/type_traits/make_void.hpp>

namespace boost {
namespace beast {

/** Models an object which wraps a reference to a DynamicBuffer_v0 in order to provide
    a DynamicBuffer_v2 interface and behaviour.

    @tparam DynamicBuffer_v0 An object which models a legacy DynamicBuffer_v0, such as a multi_buffer.

    @see buffers_adaptor
    @see flat_buffer
    @see flat_static_buffer
    @see multi_buffer
    @see static_buffer
 */
template<class DynamicBuffer_v0>
struct dynamic_buffer_v0_proxy
{
#if !BOOST_BEAST_DOXYGEN

    BOOST_STATIC_ASSERT(detail::is_dynamic_buffer_v0<DynamicBuffer_v0>::value);

#endif

    /** Constructor

     Construct a DynamicBuffer_v2 proxy from a reference to a DynamicBuffer_v0.

     @param storage a reference to a model of a DynamicBuffer_v0. The referenced object must survive for at least
            as long as the constructed dynamic_buffer_v0_proxy object and its address mut remain stable.
            Models of DynamicBuffer_V0 include
            - @ref buffers_adaptor
            - @ref flat_buffer
            - @ref flat_static_buffer
            - @ref multi_buffer
            - @ref static_buffer
     */

    dynamic_buffer_v0_proxy(
        DynamicBuffer_v0& storage) noexcept;

#if BOOST_BEAST_DOXYGEN


    /** Constructor

        Both wrappers remain valid. Operations performed on the underlying DynamicBuffer_v0 are observed
        on all copies of the wrapper object, provided they happen in the same implicit strand.
    */
    dynamic_buffer_v0_proxy(
        dynamic_buffer_v0_proxy const&) noexcept;

    /** Assignment

        Causes this object to wrap the same DynamicBuffer_v0 buffer as referenced by `other`.
    */
    dynamic_buffer_v0_proxy&
    operator=(
        dynamic_buffer_v0_proxy const& other) noexcept;

    /** Destructor

        Destroys the wrapper. The underlying DynamicBuffer_v0 object will be left in a
        valid state so that it may be manipulated through its v0 interface.
    */
    ~dynamic_buffer_v0_proxy() noexcept;

#endif // BOOST_BEAST_DOXYGEN

    // DynamicBuffer_v2 interface

    /** The type used to represent a sequence of constant buffers that refers to the underlying memory
        of the referenced DynamicBuffer_v0 object.
    */
    using const_buffers_type = typename DynamicBuffer_v0::const_buffers_type;

    /** The type used to represent a sequence of mutable buffers that refers to the underlying memory
        of the referenced DynamicBuffer_v0 object.
    */
    using mutable_buffers_type = typename DynamicBuffer_v0::mutable_buffers_type;

    /** Get the current size of the underlying memory.

     @note The function returns the sizeof of the input sequence of the referenced DynamicBuffer_v0.
           @see data. @see grow. @see shrink.

     @returns The current size of the underlying memory.
    */
    std::size_t
    size() const;

    /** Get the maximum size of the dynamic buffer.

     @returns The allowed maximum size of the underlying memory.
    */
    std::size_t
    max_size() const;

    /** Get the maximum size that the buffer may grow to without triggering reallocation.

     @returns The current capacity.
    */
    std::size_t
    capacity() const;

    /** Consume the specified number of bytes from the beginning of the referenced DynamicBuffer_v0.

     @note If n is greater than the size of the input sequence of the referenced DynamicBuffer_v0,
           the entire input sequence is consumed and no error is issued.
    */
    void
    consume(std::size_t n);

    /** Get a sequence of buffers that represents the underlying memory.

     @param pos Position of the first byte to represent in the buffer sequence.

     @param n The number of bytes to return in the buffer sequence. If the underlying memory is shorter,
              the buffer sequence represents as many bytes as are available.

     @note The returned object is invalidated by any dynamic_buffer_v0_proxy or DynamicBuffer_v2  member
           function that resizes or erases the input sequence of the referenced DynamicBuffer_v2.

     @returns A const_buffers_type containing a sequence of buffers representing the input area
              of the referenced DynamicBuffer_v0.
    */
    const_buffers_type
    data(std::size_t pos, std::size_t n) const;

    /** Get a sequence of buffers that represents the underlying memory.

     @param pos Position of the first byte to represent in the buffer sequence.

     @param n The number of bytes to return in the buffer sequence. If the underlying memory is shorter,
              the buffer sequence represents as many bytes as are available.

     @note The returned object is invalidated by any dynamic_buffer_v0_proxy or DynamicBuffer_v0 member
           function that resizes or erases the input sequence of the referenced DynamicBuffer_v0.

     @returns A mutable_buffers_type containing a sequence of buffers representing the input area
              of the referenced DynamicBuffer_v0.
    */
    mutable_buffers_type
    data(std::size_t pos, std::size_t n);

    /** Grow the underlying memory by the specified number of bytes.

     Resizes the input area of the referenced DynamicBuffer_v0 to accommodate an additional n
     bytes at the end.

     @note The operation is implemented in terms of ``commit(prepare(n).size())`` on the referenced
           DynamicBuffer_v0.

     @except std::length_error If ``size() + n > max_size()``.
    */
    void
    grow(std::size_t n);

    /** Shrink the underlying memory by the specified number of bytes.

     Erases `n` bytes from the end of the input area of the referenced DynamicBuffer_v0.
     If `n` is greater than the current size of the input area, the input areas is emptied.

     @note The operation is implemented in terms of ``commit(prepare(n).size())`` on the referenced
           DynamicBuffer_v0.

    */
    void
    shrink(std::size_t n);

private:

    DynamicBuffer_v0* storage_;
};

/** Convert a reference to a DynamicBuffer_v0 into a copyable net.ts dynamic_buffer.

    This function will automatically detect the type of dynamic buffer passed as an argument
    and will return a type modelling DynamicBuffer_v2 which will encapsulate, copy or reference
    the target as appropriate such that the returned object may be passed as an argument to
    any function which expects a model of DynamicBuffer_v2

    @tparam DynamicBuffer_v0

    @param target an lvalue reference to a model of DynamicBuffer_v0

    @return An object of type dynamic_buffer_v0_proxy<DynamicBuffer_v0> which models a net.ts copyable dynamic buffer.

    @note This overload is selected only when `target` is a model of a DynamicBuffer_v0

    @see dynamic_buffer_v0_proxy
    @see buffers_adaptor
    @see flat_buffer
    @see flat_static_buffer
    @see multi_buffer
    @see static_buffer
 */
template<class DynamicBuffer_v0>
auto
dynamic_buffer(DynamicBuffer_v0& target) ->
#if BOOST_BEAST_DOXYGEN
dynamic_buffer_v0_proxy<DynamicBuffer_v0>;
#else
typename std::enable_if<
    detail::is_dynamic_buffer_v0<DynamicBuffer_v0>::value,
    dynamic_buffer_v0_proxy<DynamicBuffer_v0>>::type;
#endif

/** Provide a passthrough for the homogeneous overload function set.

    This function will automatically detect the type of dynamic buffer passed as an argument
    and will return a type modelling DynamicBuffer_v2 which will encapsulate, copy or reference
    the target as appropriate such that the returned object may be passed as an argument to
    any function which expects a model of DynamicBuffer_v2

    @tparam DynamicBuffer_v2 A type which models the NTR of DynamicBuffer_v2

    @param target a model of a DynamicBuffer_v2

    @return A copy of `target`.

    @note This overload is selected only when `target` is a model of a DynamicBuffer_v2
 */
template<class DynamicBuffer_v2>
auto
dynamic_buffer(DynamicBuffer_v2 buffer) ->
#if BOOST_BEAST_DOXYGEN
DynamicBuffer_v2;
#else
typename std::enable_if<
    boost::asio::is_dynamic_buffer_v2<DynamicBuffer_v2>::value,
    DynamicBuffer_v2>::type;
#endif

/** Determine if `T` is convertible to a DynamicBuffer_v2 via a free function overload of <em>dynamic_buffer</em>.

    Metafunctions are used to perform compile time checking of template
    types. This type will be derived from `std::true_type` if `T` meets the requirements,
    else the type will be derived from `std::false_type`.

    @par Example

    Use with `static_assert`:

    @code
    template<class UnderlyingBufferStorage>
    void f(UnderlyingBufferStorage&& storage)
    {
        static_assert(convertible_to_dynamic_buffer_v2<UnderlyingBufferStorage>::value,
            "not convertible to DynamicBuffer_v2");
    ...
    @endcode

    Use with `std::enable_if` (SFINAE):

    @code
    template<class UnderlyingBufferStorage>
    typename std::enable_if<convertible_to_dynamic_buffer_v2<UnderlyingBufferStorage>::value>::type
    f(UnderlyingBufferStorage&& storage)
    {
        g(boost::beast::dynamic_buffer(storage));
    }
    @endcode

    @see dynamic_buffer
*/
#if BOOST_BEAST_DOXYGEN
template<class T>
using convertible_to_dynamic_buffer_v2 = __see_below__;
#else
template<class Type, class = void>
struct convertible_to_dynamic_buffer_v2
: std::false_type
{
};

template<class Type>
struct convertible_to_dynamic_buffer_v2<
    Type,
    void_t<decltype(dynamic_buffer(std::declval<Type>()))>>
: std::true_type
{
};
#endif

}
}

#include <boost/beast/core/impl/dynamic_buffer.hpp>

#endif //BOOST_BEAST_CORE_DYNAMIC_BUFFER_HPP
