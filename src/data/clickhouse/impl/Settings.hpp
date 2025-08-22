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

#include <cstdint>
#include <chrono>
#include <optional>
#include <string>
#include <thread>

namespace data::clickhouse::impl {

/**
 * @brief Bundles all ClickHouse settings in one place.
 */
struct Settings {
    static constexpr std::size_t kDEFAULT_CONNECTION_TIMEOUT = 10000;

    /** @brief Connect timeout specified in milliseconds */
    std::chrono::milliseconds connectionTimeout = std::chrono::milliseconds{kDEFAULT_CONNECTION_TIMEOUT};

    /** @brief Request timeout specified in milliseconds */
    std::chrono::milliseconds requestTimeout = std::chrono::milliseconds{0};  // no timeout at all

    /** @brief Connection information */
    std::string host = "127.0.0.1";
    uint16_t port = 8123;
    std::string database = "default";

    /** @brief Username/login */
    std::optional<std::string> username = std::nullopt;

    /** @brief Password to match the `username` */
    std::optional<std::string> password = std::nullopt;
};

} // namespace data::clickhouse::impl
