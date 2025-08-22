//------------------------------------------------------------------------------
/*
    This file is part of clio: https://github.com/XRPLF/clio
    Copyright (c) 2024, the clio developers.

    Permission to use, copy, modify, and distribute this software for any
    purpose with or without fee is hereby granted, provided that the above
    copyright notice and this permission notice appear in all copies.

    THE  SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
    WITH  REGARD  TO  THIS  SOFTWARE INCLUDING  ALL  IMPLIED  WARRANTIES  OF
    MERCHANTABILITY  AND  FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
    ANY  SPECIAL,  DIRECT,  INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
    WHATSOEVER  RESULTING  FROM  LOSS  OF USE, DATA OR PROFITS, WHETHER IN AN
    ACTION  OF  CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
    OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/
//==============================================================================

#include "data/clickhouse/SettingsProvider.hpp"
#include "data/clickhouse/impl/Settings.hpp"

#include <chrono>

namespace data::clickhouse {

SettingsProvider::SettingsProvider(util::config::ObjectView const& cfg) : config_{cfg}
{
    database_ = config_.get<std::string>("database");
}

Settings
SettingsProvider::getSettings() const
{
    return parseSettings();
}

Settings
SettingsProvider::parseSettings() const
{
    Settings settings;

    // Parse connection settings
    settings.connectionInfo.host = config_.get<std::string>("host");
    settings.connectionInfo.port = config_.get<uint16_t>("port");

    // Use the database from constructor or default
    settings.connectionInfo.database = database_;

    // Parse timeout settings
    if (auto const connectTimeout = config_.maybeValue<uint32_t>("connect_timeout")) {
        settings.connectionTimeout = std::chrono::milliseconds{*connectTimeout};
    }

    if (auto const requestTimeout = config_.maybeValue<uint32_t>("request_timeout")) {
        settings.requestTimeout = std::chrono::milliseconds{*requestTimeout};
    }

    // Parse thread and connection settings
    settings.threads = config_.get<uint32_t>("threads");
    settings.maxWriteRequestsOutstanding = config_.get<uint32_t>("max_write_requests_outstanding");
    settings.maxReadRequestsOutstanding = config_.get<uint32_t>("max_read_requests_outstanding");
    settings.coreConnectionsPerHost = config_.get<uint32_t>("core_connections_per_host");
    settings.writeBatchSize = config_.get<uint32_t>("write_batch_size");

    // Parse authentication settings
    if (auto const username = config_.maybeValue<std::string>("username")) {
        settings.username = *username;
    }

    if (auto const password = config_.maybeValue<std::string>("password")) {
        settings.password = *password;
    }

    return settings;
}

}  // namespace data::clickhouse
