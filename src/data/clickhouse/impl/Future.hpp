//------------------------------------------------------------------------------
/*
    This file is part of clio: https://github.com/XRPLF/clio
    Copyright (c) 2024, the clio developers.

    Permission to use, copy, modify, and distribute this software for any
    purpose with or without fee is hereby granted, provided that the above
    copyright notice and this permission notice appear in all copies.

    THE  SOFTWARE  IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
    WITH  REGARD  TO  THIS  SOFTWARE  INCLUDING  ALL  IMPLIED  WARRANTIES  OF
    MERCHANTABILITY  AND  FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
    ANY  SPECIAL,  DIRECT,  INDIRECT,  OR  CONSEQUENTIAL  DAMAGES  OR  ANY
    DAMAGES  WHATSOEVER  RESULTING  FROM  LOSS  OF  USE,  DATA  OR  PROFITS,
    WHETHER  IN  AN  ACTION  OF  CONTRACT,  NEGLIGENCE  OR  OTHER  TORTIOUS
    ACTION,  ARISING  OUT  OF  OR  IN  CONNECTION  WITH  THE  USE  OR
    PERFORMANCE OF THIS SOFTWARE.
*/
//==============================================================================

#pragma once

#include "data/clickhouse/Types.hpp"
#include <boost/asio/spawn.hpp>
#include <functional>
#include <memory>
#include <future>

namespace data::clickhouse::impl {

/**
 * @brief Represents a future result from an async ClickHouse operation.
 */
struct Future {
    using FnType = std::function<ResultOrError(boost::asio::yield_context)>;
    
    explicit Future(FnType&& fn);
    
    /**
     * @brief Wait for the async operation to complete and return the result.
     */
    MaybeError await() const;
    
    /**
     * @brief Get the result of the async operation.
     */
    ResultOrError get() const;

private:
    FnType fn_;
    mutable std::unique_ptr<std::future<ResultOrError>> cached_result_;
};

/**
 * @brief Future with callback support.
 */
class FutureWithCallback : public Future {
public:
    using CallbackType = std::function<void(ResultOrError)>;
    
    explicit FutureWithCallback(FnType&& fn, CallbackType&& callback);
    FutureWithCallback(FutureWithCallback const&) = delete;
    FutureWithCallback(FutureWithCallback&&) = default;

private:
    CallbackType callback_;
};

} // namespace data::clickhouse::impl
