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

#include "data/clickhouse/Handle.hpp"
#include "data/clickhouse/impl/Future.hpp"
#include "data/clickhouse/Schema.hpp"

namespace data::clickhouse {

Handle::Handle(Settings const& clusterSettings)
    : cluster_(clusterSettings), session_(clusterSettings) {
    initialize(clusterSettings);
}

Handle::Handle(std::string_view contactPoints)
    : cluster_(Settings::defaultSettings()), session_(Settings::defaultSettings()) {
    initialize(contactPoints);
}

Handle::~Handle() = default;

void
Handle::initialize(Settings const& settings)
{
    cluster_ = impl::Cluster(settings);
    if (cluster_.isValid()) {
        auto session = cluster_.createSession();
        if (session) {
            session_ = std::move(*session);
        }
    }
}

void
Handle::initialize(std::string_view /*contactPoints*/)
{
    // parse contact points and create settings
    auto settings = Settings::defaultSettings();
    // TODO(NODE-2688): parse contactPoints string and set host/port
    initialize(settings);
}

Future
Handle::asyncConnect() const
{
    return Future{[this](boost::asio::yield_context) -> ResultOrError {
        if (!cluster_.isValid()) {
            return Error{ClickHouseError{"Cluster not valid: " + cluster_.getLastError()}};
        }

        if (session_.isValid()) {
            return {};
        } else {
            return Error{ClickHouseError{"Failed to connect to ClickHouse: " + session_.getLastError()}};
        }
    }};
}

MaybeError
Handle::connect() const
{
    return asyncConnect().await();
}

Future
Handle::asyncExecute(std::string const& query) const
{
    return Future{[this, query](boost::asio::yield_context yield) -> ResultOrError {
        if (!session_.isValid()) {
            return Error{ClickHouseError{"Not connected to ClickHouse"}};
        }

        if (session_.execute(query, yield)) {
            return {};
        } else {
            return Error{ClickHouseError{"Failed to execute query: " + query + " - " + session_.getLastError()}};
        }
    }};
}

MaybeError
Handle::execute(std::string const& query) const
{
    return asyncExecute(query).await();
}

MaybeError
Handle::executeEach(std::vector<std::string> const& queries) const
{
    for (auto const& query : queries) {
        if (auto const result = execute(query); !result) {
            return result;
        }
    }
    return {};
}

Future
Handle::asyncQuery(std::string const& query) const
{
    return Future{[this, query](boost::asio::yield_context yield) -> ResultOrError {
        if (!session_.isValid()) {
            return Error{ClickHouseError{"Not connected to ClickHouse"}};
        }

        auto result = session_.query(query, yield);
        if (result.columnNames.empty() && result.rows.empty()) {
            return Error{ClickHouseError{"Query failed: " + session_.getLastError()}};
        }
        return result;
    }};
}

ResultOrError
Handle::query(std::string const& query) const
{
    auto future = asyncQuery(query);
    return future.get();
}

bool
Handle::isConnected() const
{
    return cluster_.isValid() && session_.isValid();
}

template<typename SettingsProviderType>
MaybeError
Handle::initializeSchema(SettingsProviderType const& settingsProvider) const
{
    Schema<SettingsProviderType> schema(settingsProvider);
    
    auto dbResult = execute(schema.createDatabase);
    if (dbResult.has_value()) {
        return dbResult.value();
    }
    
    auto tableResult = executeEach(schema.createSchema);
    if (tableResult.has_value()) {
        return tableResult.value();
    }
    
    return {}; // Success - no error
}

}  // namespace data::clickhouse

