//
// Copyright (c) 2017 Vinnie Falco (vinnie dot falco at gmail dot com)
// Copyright (c) 2020 Richard Hodges (hodges.r@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_CORE_DETAIL_WORK_GUARD_HPP
#define BOOST_BEAST_CORE_DETAIL_WORK_GUARD_HPP

#if defined(BOOST_ASIO_NO_TS_EXECUTORS)
#include <boost/asio/execution/outstanding_work.hpp>
#include <boost/asio/execution/any_executor.hpp>
#include <boost/asio/any_io_executor.hpp>
#else
#include <boost/asio/executor_work_guard.hpp>
#endif

namespace boost {
namespace beast {
namespace detail {

#if defined(BOOST_ASIO_NO_TS_EXECUTORS)

template<class Executor>
struct work_guard
{
    using executor_type = boost::asio::any_io_executor;

    work_guard()
    : impl_()
    {
    }

    work_guard(Executor const& exec)
    : impl_(construct(exec))
    {
    };

    void reset()
    {
        impl_ = executor_type();
    }

    executor_type
    get_executor() const
    {
        return impl_;
    }

private:

    static
    auto
    construct(Executor const& exec)
    -> executor_type
    {
        return boost::asio::prefer(
            exec,
            boost::asio::execution::outstanding_work.tracked);
    }

    executor_type impl_;
};
#else
    template<class Executor>
    using work_guard = net::executor_work_guard<Executor>;
#endif

}
}
}

#endif // BOOST_BEAST_CORE_DETAIL_EXECUTOR_TRACKER_HPP
