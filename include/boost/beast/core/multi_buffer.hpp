//
// Copyright (c) 2016-2019 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_MULTI_BUFFER_HPP
#define BOOST_BEAST_MULTI_BUFFER_HPP

#include <boost/beast/core/detail/config.hpp>
#include <boost/beast/core/detail/allocator.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/core/empty_value.hpp>
#include <boost/core/exchange.hpp>
#include <boost/intrusive/list.hpp>
#include <boost/type_traits/type_with_alignment.hpp>
#include <iterator>
#include <limits>
#include <memory>
#include <vector>
#include <type_traits>

namespace boost {
namespace beast {


struct storage_element
{
    char* begin_data_;
    char* end_data_;
    char* begin_used_;
    char* end_used_;

    template<class Allocator = std::allocator<char>>
    storage_element(std::size_t required_capacity, Allocator alloc = Allocator())
    : begin_data_(std::allocator_traits<Allocator>::allocate(alloc, required_capacity))
    , end_data_(begin_data_ + required_capacity)
    , begin_used_(begin_data_)
    , end_used_(begin_data_)
    {

    }

    storage_element(storage_element const&) = delete;

    storage_element(storage_element && r) noexcept
    : begin_data_(exchange(r.begin_data_, nullptr))
    , end_data_(r.end_data_)
    , begin_used_(r.begin_used_)
    , end_used_(r.end_used_)
    {
    }

    storage_element& operator=(storage_element const&) = delete;

    storage_element& operator=(storage_element && r) noexcept
    {
        BOOST_ASSERT(begin_data_ == nullptr);
        begin_data_ = exchange(r.begin_data_, nullptr);
        end_data_ = r.end_data_;
        begin_used_ = r.begin_used_;
        end_used_ = r.end_used_;

        return *this;
    }


    ~storage_element() noexcept
    {
        BOOST_ASSERT(!begin_data_);
    }

    char*
    data() noexcept
    {
        return begin_used_;
    }

    char const*
    data() const noexcept
    {
        return begin_used_;
    }

    std::size_t
    size() const noexcept
    {
        return
            static_cast<std::size_t>(
                std::distance(begin_used_, end_used_));
    }

    std::size_t
    available() const noexcept
    {
        return
            static_cast<std::size_t>(
                std::distance(end_used_, end_data_));
    }

    std::size_t
    capacity() const noexcept
    {
        return
            static_cast<std::size_t>(
                std::distance(begin_data_, end_data_));
    }

    void
    acquire(std::size_t n) noexcept
    {
        BOOST_ASSERT(available() >= n);
        end_used_ += n;
    }

    void
    consume(std::size_t n) noexcept
    {
        BOOST_ASSERT(size() >= n);
        begin_used_ += n;
    }

    void
    clear() noexcept
    {
        begin_used_ = end_used_ = data();
    }

    operator net::mutable_buffer()
    {
        return net::mutable_buffer(data(), size());
    }

    operator net::const_buffer() const
    {
        return net::const_buffer(data(), size());
    }

    // boilerplate

    auto
    data_elements()
    ->
    std::tuple<char*&, char*&, char*&, char*&>
    {
        return std::tie(begin_data_, end_data_, begin_used_, end_used_);
    }

    void swap(storage_element& r) noexcept
    {
        auto me = data_elements();
        auto you = r.data_elements();
        me.swap(you);
    }

    template<class Allocator = std::allocator<char>>
    void
    destroy(Allocator alloc = Allocator()) noexcept
    {
        if (begin_data_)
        {
            std::allocator_traits<Allocator>::
                deallocate(
                    alloc,
                    begin_data_,
                    capacity());
            begin_data_ = nullptr;
        }
    }
};

struct discount
{
    std::size_t amount;
    std::iterator_traits<storage_element*>::difference_type where;

    discount&
    operator+=(int n)
    {
        where -= n;
        return *this;
    }

    bool applies() const
    {
        return
            where == 0 &&
            amount != 0;
    }
};

template<class IsConst>
struct buffer_sequence_iterator
{
    struct iterator_category  : std::random_access_iterator_tag {};
    using value_type = typename std::conditional<IsConst::value, net::const_buffer, net::mutable_buffer>::type;
    using difference_type = std::ptrdiff_t;
    using pointer = typename std::add_pointer<value_type>::type;
    using reference = typename std::add_lvalue_reference<value_type>::type;

    using element_ptr_type = typename std::conditional<IsConst::value, storage_element const*, storage_element*>::type;

    buffer_sequence_iterator(element_ptr_type f, discount initial_discount, discount final_discount)
        : f_(f)
        , initial_discount_(initial_discount)
        , final_discount_(final_discount)
    {
    }

    value_type
    operator*() const
    {
        auto result = trim(value_type(*f_));
        return result;
    }

    value_type
    trim(value_type result) const
    {
        if (initial_discount_.applies())
            result += initial_discount_.amount;

        if (final_discount_.applies())
        {
            using faux_pointer =
                typename std::conditional<
                    IsConst::value,
                    const char*,
                    char*>::type;

            result = value_type(
                static_cast<faux_pointer>(result.data()) + final_discount_.amount,
                result.size() - final_discount_.amount);
        }

        return result;
    }

    bool
    operator==(buffer_sequence_iterator const& rhs) const noexcept
    {
        return f_ == rhs.f_;
    }

    bool
    operator!=(buffer_sequence_iterator const& rhs) const noexcept
    {
        return f_ != rhs.f_;
    }

    auto
    operator++() -> buffer_sequence_iterator&
    {
        return (*this) += 1;
    }

    auto
    operator++(int) -> buffer_sequence_iterator
    {
        auto result = *this;
        ++result;
        return result;
    }

    auto operator+=(difference_type n) -> buffer_sequence_iterator&
    {
        f_ += n;
        initial_discount_ += n;
        final_discount_ += n;
        return *this;
    }

    auto operator-=(difference_type n) -> buffer_sequence_iterator&
    {
        return (*this) += -n;
    }

    friend auto
    operator+(buffer_sequence_iterator it, difference_type n) -> buffer_sequence_iterator
    {
        it += n;
        return it;
    }

    friend auto
    operator+(difference_type n, buffer_sequence_iterator it) -> buffer_sequence_iterator
    {
        it += n;
        return it;
    }

    friend auto
    operator-(buffer_sequence_iterator it, difference_type n) -> buffer_sequence_iterator
    {
        it -= n;
        return it;
    }

    friend auto
    operator-(buffer_sequence_iterator a, buffer_sequence_iterator const& b)
    -> difference_type
    {
        return a.f_ - b.f_;
    }

    friend auto
    operator-(difference_type n, buffer_sequence_iterator it) -> buffer_sequence_iterator
    {
        it -= n;
        return it;
    }

    value_type operator[](std::size_t n) const
    {
        return *((*this) + n);
    }

    friend bool
    operator<(buffer_sequence_iterator lhs, buffer_sequence_iterator rhs)
    {
        //note: reversed polarity
        return lhs.f_ > rhs.f_;
    }

    friend bool
    operator>(buffer_sequence_iterator lhs, buffer_sequence_iterator rhs)
    {
        //note: reversed polarity
        return lhs.f_ < rhs.f_;
    }

    friend bool
    operator>=(buffer_sequence_iterator lhs, buffer_sequence_iterator rhs)
    {
        return !(lhs < rhs);
    }

    friend bool
    operator<=(buffer_sequence_iterator lhs, buffer_sequence_iterator rhs)
    {
        return !(lhs > rhs);
    }

    auto
    element_ptr() const
    -> element_ptr_type
    {
        return f_;
    }

    discount&
    initial_discount()
    {
        return initial_discount_;
    }

    discount&
    final_discount()
    {
        return final_discount_;
    }

private:
    element_ptr_type f_;
    discount initial_discount_, final_discount_;
};

template<class Allocator>
struct storage_element_container
{
    // Fancy pointers are not supported
    static_assert(std::is_pointer<typename
                  std::allocator_traits<Allocator>::pointer>::value,
                  "Allocator must use regular pointers");

    static bool constexpr default_nothrow =
        std::is_nothrow_default_constructible<Allocator>::value;

    using allocator_type =
        typename std::allocator_traits<Allocator>::
            template rebind_alloc <storage_element>;

    using storage_type = std::vector<storage_element, allocator_type>;

    using data_allocator_type =
        typename std::allocator_traits<allocator_type>::
            template rebind_alloc <char>;

    storage_element_container(
        std::size_t limit = std::numeric_limits<std::size_t>::max(),
        Allocator alloc = Allocator())
    : store_(allocator_type(alloc))
    , limit_(limit)
    {
    }

    storage_element_container(storage_element_container&& r) noexcept
    : store_(std::move(r.store_))
    , limit_(r.limit_)
    {
    }

    storage_element_container& operator=(storage_element_container&& r) noexcept
    {
        destroy();
        store_ = std::move(r.store_);
        limit_ = r.limit_;
        return *this;
    }

    ~storage_element_container() noexcept
    {
        destroy();
    }

    storage_element_container(storage_element_container const& r) = delete;
    storage_element_container& operator=(storage_element_container const& r) = delete;


    /// Models either a MutableBufferSequence or a BufferSequence
    template<class IsConst>
    struct buffer_sequence
    {
        using value_type = typename std::conditional<IsConst::value, net::const_buffer, net::mutable_buffer>::type;
        using element_type = typename std::conditional<
            IsConst::value,
            storage_element const,
            storage_element>::type;

        using finger = typename std::add_pointer<element_type>::type;
        using iterator = buffer_sequence_iterator<IsConst>;

        buffer_sequence(finger first, finger last) noexcept
        : begin_(first,
                 discount { 0, 0 },
                 discount { 0, std::distance(first, last) - 1})
        , end_(begin_)
        {
            end_ += std::distance(first, last);
        }

        iterator
        begin() const noexcept
        {
            return begin_;
        }

        iterator
        end() const noexcept
        {
            return end_;
        }

        /// \brief Adjust the buffer sequence so that is becomes a sub-sequence of itself
        /// \param pos the position in the old sequence which will be represented by the first
        ///            position of the resulting sequence
        /// \param limit the maximum length of the resulting sequence
        void adjust(std::size_t pos, std::size_t limit)
        {
            // short-circuit empty range
            if (begin_ == end_)
                return;

            auto size = net::buffer_size(*this);
            size = size > pos ? size - pos : 0;
            limit = (std::min)(limit, size);

            // special case for zero limit
            if (limit == 0)
            {
                begin_ = end_ = iterator(nullptr, discount { 0, 0 }, discount { 0, 0 });
                return;
            }

            auto first = begin_.element_ptr();
            auto initial_discount = begin_.initial_discount();
            if (initial_discount.applies())
                pos += initial_discount.amount;
            auto final_discount = begin_.final_discount();

            auto last = end_.element_ptr();

            while(first != last && pos)
            {
                if (first->size() < pos)
                {
                    pos -= first->size();
                    ++first;
                    initial_discount = discount { pos, 0 };
                }
                else
                {
                    initial_discount = discount { pos, 0 };
                    pos = 0;
                }
            }

            auto current = first;
            auto current_discount = initial_discount;
            auto current_size = [&]
            {
                auto size = current->size();
                if (current_discount.applies())
                    size -= current_discount.amount;
                return size;
            };

            final_discount = discount { limit - current_size() , std::distance(first, current) };
            while (current != last)
            {
                auto size = current_size();
                if (limit <= size)
                {
                    final_discount = discount { size - limit , std::distance(first, current) };
                    ++current;
                    break;
                }
                else
                {
                    limit -= size;
                    ++current;
                }
            }

            begin_ = iterator(first, initial_discount, final_discount);
            end_ = begin_ + std::distance(first, last);
        }

    private:
        iterator begin_, end_;
    };

    using mutable_buffer_sequence = buffer_sequence<std::false_type>;
    using const_buffer_sequence = buffer_sequence<std::true_type>;

    auto make_sequence() const noexcept -> const_buffer_sequence
    {
        return const_buffer_sequence(store_.data(), store_.data() + store_.size());
    }

    auto make_sequence() noexcept -> mutable_buffer_sequence
    {
        return mutable_buffer_sequence(store_.data(), store_.data() + store_.size());
    }

    template<class IsConst>
    friend
    void
    adjust(buffer_sequence<IsConst>& input, std::size_t pos, std::size_t limit)
    {
        input.adjust(pos, limit);
    }

    template<class IsConst>
    friend
    auto
    adjusted(buffer_sequence<IsConst> input, std::size_t pos, std::size_t limit)
    ->
    buffer_sequence<IsConst>
    {
        input.adjust(pos, limit);
        return input;
    }

    void
    add(std::size_t required_space)
    {
        if (store_.size())
        {
            auto& last = *store_.end();
            if (last.available() <= required_space)
            {
                last.acquire(required_space);
                return;
            }
        }

        if (required_space > min_block_size_)
            min_block_size_ = round_up(required_space);

        auto make_element = [&]
        {
            auto element = storage_element(min_block_size_, data_allocator_type(store_.get_allocator()));
            element.acquire(required_space);
            return element;
        };

        store_.push_back(make_element());
    }

    void
    consume(std::size_t n)
    {
        auto iter = store_.begin();
        while (n && iter != store_.end())
        {
            if (n >= iter->size())
            {
                n -= iter->size();
                iter = erase_element(iter);
            }
            else
            {
                iter->consume(n);
                n = 0;
                // ++iter
            }
        }

    }

private:

    data_allocator_type
    element_allocator() const
    {
        return data_allocator_type(store_.get_allocator());
    }

    void
    destroy()
    {
        auto alloc = data_allocator_type(store_.get_allocator());
        for (auto& elem : store_)
            elem.destroy(alloc);
        store_.clear();
    }

    auto
    erase_element(typename storage_type::iterator where)
    -> typename storage_type::iterator
    {
        where->destroy(element_allocator());
        return store_.erase(where);
    }


    static
    std::size_t
    round_up(std::size_t required)
    {
        // @todo: think of a strategy for sensibly rounding up the size of allocated blocks
        return required;
    }



    storage_type store_;
    std::size_t limit_;
    std::size_t min_block_size_ = 4096;
};

/** A dynamic buffer providing sequences of variable length.

    A dynamic buffer encapsulates memory storage that may be
    automatically resized as required, where the memory is
    divided into two regions: readable bytes followed by
    writable bytes. These memory regions are internal to
    the dynamic buffer, but direct access to the elements
    is provided to permit them to be efficiently used with
    I/O operations.

    The implementation uses a sequence of one or more byte
    arrays of varying sizes to represent the readable and
    writable bytes. Additional byte array objects are
    appended to the sequence to accommodate changes in the
    desired size. The behavior and implementation of this
    container is most similar to `std::deque`.

    Objects of this type meet the requirements of <em>DynamicBuffer</em>
    and have the following additional properties:

    @li A mutable buffer sequence representing the readable
    bytes is returned by @ref data when `this` is non-const.

    @li Buffer sequences representing the readable and writable
    bytes, returned by @ref data and @ref prepare, may have
    length greater than one.

    @li A configurable maximum size may be set upon construction
    and adjusted afterwards. Calls to @ref prepare that would
    exceed this size will throw `std::length_error`.

    @li Sequences previously obtained using @ref data remain
    valid after calls to @ref prepare or @ref commit.

    @tparam Allocator The allocator to use for managing memory.
*/
template<class Allocator>
class basic_multi_buffer
#if ! BOOST_BEAST_DOXYGEN
    : private boost::empty_value<Allocator>
#endif
{
    // Fancy pointers are not supported
    static_assert(std::is_pointer<typename
        std::allocator_traits<Allocator>::pointer>::value,
        "Allocator must use regular pointers");

    static bool constexpr default_nothrow =
        std::is_nothrow_default_constructible<Allocator>::value;

    // Storage for the list of buffers representing the input
    // and output sequences. The allocation for each element
    // contains `element` followed by raw storage bytes.
    class element
        : public boost::intrusive::list_base_hook<
            boost::intrusive::link_mode<
                boost::intrusive::normal_link>>
    {
        using size_type = typename
            detail::allocator_traits<Allocator>::size_type;

        size_type const size_;

    public:
        element(element const&) = delete;

        explicit
        element(size_type n) noexcept
            : size_(n)
        {
        }

        size_type
        size() const noexcept
        {
            return size_;
        }

        char*
        data() const noexcept
        {
            return const_cast<char*>(
                reinterpret_cast<char const*>(this + 1));
        }
    };

    template<bool>
    class readable_bytes;

    using size_type = typename
        detail::allocator_traits<Allocator>::size_type;

    using align_type = typename
        boost::type_with_alignment<alignof(element)>::type;

    using rebind_type = typename
        beast::detail::allocator_traits<Allocator>::
            template rebind_alloc<align_type>;

    using alloc_traits =
        beast::detail::allocator_traits<rebind_type>;

    using list_type = typename boost::intrusive::make_list<
        element, boost::intrusive::constant_time_size<true>>::type;

    using iter = typename list_type::iterator;

    using const_iter = typename list_type::const_iterator;

    using pocma = typename
        alloc_traits::propagate_on_container_move_assignment;

    using pocca = typename
        alloc_traits::propagate_on_container_copy_assignment;

    static_assert(std::is_base_of<std::bidirectional_iterator_tag,
        typename std::iterator_traits<iter>::iterator_category>::value,
            "BidirectionalIterator type requirements not met");

    static_assert(std::is_base_of<std::bidirectional_iterator_tag,
        typename std::iterator_traits<const_iter>::iterator_category>::value,
            "BidirectionalIterator type requirements not met");

    std::size_t max_;
    list_type list_;        // list of allocated buffers
    iter out_;              // element that contains out_pos_
    size_type in_size_ = 0; // size of the input sequence
    size_type in_pos_ = 0;  // input offset in list_.front()
    size_type out_pos_ = 0; // output offset in *out_
    size_type out_end_ = 0; // output end offset in list_.back()

public:
    /// The type of allocator used.
    using allocator_type = Allocator;

    /// Destructor
    ~basic_multi_buffer();

    /** Constructor

        After construction, @ref capacity will return zero, and
        @ref max_size will return the largest value which may
        be passed to the allocator's `allocate` function.
    */
    basic_multi_buffer() noexcept(default_nothrow);

    /** Constructor

        After construction, @ref capacity will return zero, and
        @ref max_size will return the specified value of `limit`.

        @param limit The desired maximum size.
    */
    explicit
    basic_multi_buffer(
        std::size_t limit) noexcept(default_nothrow);

    /** Constructor

        After construction, @ref capacity will return zero, and
        @ref max_size will return the largest value which may
        be passed to the allocator's `allocate` function.

        @param alloc The allocator to use for the object.

        @esafe

        No-throw guarantee.
    */
    explicit
    basic_multi_buffer(Allocator const& alloc) noexcept;

    /** Constructor

        After construction, @ref capacity will return zero, and
        @ref max_size will return the specified value of `limit`.

        @param limit The desired maximum size.

        @param alloc The allocator to use for the object.

        @esafe

        No-throw guarantee.
    */
    basic_multi_buffer(
        std::size_t limit, Allocator const& alloc) noexcept;

    /** Move Constructor

        The container is constructed with the contents of `other`
        using move semantics. The maximum size will be the same
        as the moved-from object.

        Buffer sequences previously obtained from `other` using
        @ref data or @ref prepare remain valid after the move.

        @param other The object to move from. After the move, the
        moved-from object will have zero capacity, zero readable
        bytes, and zero writable bytes.

        @esafe

        No-throw guarantee.
    */
    basic_multi_buffer(basic_multi_buffer&& other) noexcept;

    /** Move Constructor

        Using `alloc` as the allocator for the new container, the
        contents of `other` are moved. If `alloc != other.get_allocator()`,
        this results in a copy. The maximum size will be the same
        as the moved-from object.

        Buffer sequences previously obtained from `other` using
        @ref data or @ref prepare become invalid after the move.

        @param other The object to move from. After the move,
        the moved-from object will have zero capacity, zero readable
        bytes, and zero writable bytes.

        @param alloc The allocator to use for the object.

        @throws std::length_error if `other.size()` exceeds the
        maximum allocation size of `alloc`.
    */
    basic_multi_buffer(
        basic_multi_buffer&& other,
        Allocator const& alloc);

    /** Copy Constructor

        This container is constructed with the contents of `other`
        using copy semantics. The maximum size will be the same
        as the copied object.

        @param other The object to copy from.

        @throws std::length_error if `other.size()` exceeds the
        maximum allocation size of the allocator.
    */
    basic_multi_buffer(basic_multi_buffer const& other);

    /** Copy Constructor

        This container is constructed with the contents of `other`
        using copy semantics and the specified allocator. The maximum
        size will be the same as the copied object.

        @param other The object to copy from.

        @param alloc The allocator to use for the object.

        @throws std::length_error if `other.size()` exceeds the
        maximum allocation size of `alloc`.
    */
    basic_multi_buffer(basic_multi_buffer const& other,
        Allocator const& alloc);

    /** Copy Constructor

        This container is constructed with the contents of `other`
        using copy semantics. The maximum size will be the same
        as the copied object.

        @param other The object to copy from.

        @throws std::length_error if `other.size()` exceeds the
        maximum allocation size of the allocator.
    */
    template<class OtherAlloc>
    basic_multi_buffer(basic_multi_buffer<
        OtherAlloc> const& other);

    /** Copy Constructor

        This container is constructed with the contents of `other`
        using copy semantics. The maximum size will be the same
        as the copied object.

        @param other The object to copy from.

        @param alloc The allocator to use for the object.

        @throws std::length_error if `other.size()` exceeds the
        maximum allocation size of `alloc`.
    */
    template<class OtherAlloc>
    basic_multi_buffer(
        basic_multi_buffer<OtherAlloc> const& other,
        allocator_type const& alloc);

    /** Move Assignment

        The container is assigned with the contents of `other`
        using move semantics. The maximum size will be the same
        as the moved-from object.

        Buffer sequences previously obtained from `other` using
        @ref data or @ref prepare remain valid after the move.

        @param other The object to move from. After the move,
        the moved-from object will have zero capacity, zero readable
        bytes, and zero writable bytes.
    */
    basic_multi_buffer&
    operator=(basic_multi_buffer&& other);

    /** Copy Assignment

        The container is assigned with the contents of `other`
        using copy semantics. The maximum size will be the same
        as the copied object.

        After the copy, `this` will have zero writable bytes.

        @param other The object to copy from.

        @throws std::length_error if `other.size()` exceeds the
        maximum allocation size of the allocator.
    */
    basic_multi_buffer& operator=(
        basic_multi_buffer const& other);

    /** Copy Assignment

        The container is assigned with the contents of `other`
        using copy semantics. The maximum size will be the same
        as the copied object.

        After the copy, `this` will have zero writable bytes.

        @param other The object to copy from.

        @throws std::length_error if `other.size()` exceeds the
        maximum allocation size of the allocator.
    */
    template<class OtherAlloc>
    basic_multi_buffer& operator=(
        basic_multi_buffer<OtherAlloc> const& other);

    /// Returns a copy of the allocator used.
    allocator_type
    get_allocator() const
    {
        return this->get();
    }

    /** Set the maximum allowed capacity

        This function changes the currently configured upper limit
        on capacity to the specified value.

        @param n The maximum number of bytes ever allowed for capacity.

        @esafe

        No-throw guarantee.
    */
    void
    max_size(std::size_t n) noexcept
    {
        max_ = n;
    }

    /** Guarantee a minimum capacity

        This function adjusts the internal storage (if necessary)
        to guarantee space for at least `n` bytes.

        Buffer sequences previously obtained using @ref data remain
        valid, while buffer sequences previously obtained using
        @ref prepare become invalid.

        @param n The minimum number of byte for the new capacity.
        If this value is greater than the maximum size, then the
        maximum size will be adjusted upwards to this value.

        @throws std::length_error if n is larger than the maximum
        allocation size of the allocator.

        @esafe

        Strong guarantee.
    */
    void
    reserve(std::size_t n);

    /** Reallocate the buffer to fit the readable bytes exactly.

        Buffer sequences previously obtained using @ref data or
        @ref prepare become invalid.

        @esafe

        Strong guarantee.
    */
    void
    shrink_to_fit();

    /** Set the size of the readable and writable bytes to zero.

        This clears the buffer without changing capacity.
        Buffer sequences previously obtained using @ref data or
        @ref prepare become invalid.

        @esafe

        No-throw guarantee.
    */
    void
    clear() noexcept;

    /// Exchange two dynamic buffers
    template<class Alloc>
    friend
    void
    swap(
        basic_multi_buffer<Alloc>& lhs,
        basic_multi_buffer<Alloc>& rhs) noexcept;

    //--------------------------------------------------------------------------

#if BOOST_BEAST_DOXYGEN
    /// The ConstBufferSequence used to represent the readable bytes.
    using const_buffers_type = __implementation_defined__;

    /// The MutableBufferSequence used to represent the readable bytes.
    using mutable_data_type = __implementation_defined__;

    /// The MutableBufferSequence used to represent the writable bytes.
    using mutable_buffers_type = __implementation_defined__;
#else
    using const_buffers_type = readable_bytes<false>;
    using mutable_data_type = readable_bytes<true>;
    class mutable_buffers_type;
#endif

    /// Returns the number of readable bytes.
    size_type
    size() const noexcept
    {
        return in_size_;
    }

    /// Return the maximum number of bytes, both readable and writable, that can ever be held.
    size_type
    max_size() const noexcept
    {
        return max_;
    }

    /// Return the maximum number of bytes, both readable and writable, that can be held without requiring an allocation.
    std::size_t
    capacity() const noexcept;

    /** Returns a constant buffer sequence representing the readable bytes

        @note The sequence may contain multiple contiguous memory regions.
    */
    const_buffers_type
    data() const noexcept;

    /** Returns a constant buffer sequence representing the readable bytes

        @note The sequence may contain multiple contiguous memory regions.
    */
    const_buffers_type
    cdata() const noexcept
    {
        return data();
    }

    /** Returns a mutable buffer sequence representing the readable bytes.

        @note The sequence may contain multiple contiguous memory regions.
    */
    mutable_data_type
    data() noexcept;

    /** Returns a mutable buffer sequence representing writable bytes.
    
        Returns a mutable buffer sequence representing the writable
        bytes containing exactly `n` bytes of storage. Memory may be
        reallocated as needed.

        All buffer sequences previously obtained using @ref prepare are
        invalidated. Buffer sequences previously obtained using @ref data
        remain valid.

        @param n The desired number of bytes in the returned buffer
        sequence.

        @throws std::length_error if `size() + n` exceeds `max_size()`.

        @esafe

        Strong guarantee.
    */
    mutable_buffers_type
    prepare(size_type n);

    /** Append writable bytes to the readable bytes.

        Appends n bytes from the start of the writable bytes to the
        end of the readable bytes. The remainder of the writable bytes
        are discarded. If n is greater than the number of writable
        bytes, all writable bytes are appended to the readable bytes.

        All buffer sequences previously obtained using @ref prepare are
        invalidated. Buffer sequences previously obtained using @ref data
        remain valid.

        @param n The number of bytes to append. If this number
        is greater than the number of writable bytes, all
        writable bytes are appended.

        @esafe

        No-throw guarantee.
    */
    void
    commit(size_type n) noexcept;

    /** Remove bytes from beginning of the readable bytes.

        Removes n bytes from the beginning of the readable bytes.

        All buffers sequences previously obtained using
        @ref data or @ref prepare are invalidated.

        @param n The number of bytes to remove. If this number
        is greater than the number of readable bytes, all
        readable bytes are removed.

        @esafe

        No-throw guarantee.
    */
    void
    consume(size_type n) noexcept;

private:
    template<class OtherAlloc>
    friend class basic_multi_buffer;

    template<class OtherAlloc>
    void copy_from(basic_multi_buffer<OtherAlloc> const&);
    void move_assign(basic_multi_buffer& other, std::false_type);
    void move_assign(basic_multi_buffer& other, std::true_type) noexcept;
    void copy_assign(basic_multi_buffer const& other, std::false_type);
    void copy_assign(basic_multi_buffer const& other, std::true_type);
    void swap(basic_multi_buffer&) noexcept;
    void swap(basic_multi_buffer&, std::true_type) noexcept;
    void swap(basic_multi_buffer&, std::false_type) noexcept;
    void destroy(list_type& list) noexcept;
    void destroy(const_iter it);
    void destroy(element& e);
    element& alloc(std::size_t size);
    void debug_check() const;
};

/// A typical multi buffer
using multi_buffer = basic_multi_buffer<std::allocator<char>>;

} // beast
} // boost

#include <boost/beast/core/impl/multi_buffer.hpp>

#endif