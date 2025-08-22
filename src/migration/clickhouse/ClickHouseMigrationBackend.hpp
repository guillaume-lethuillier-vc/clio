//------------------------------------------------------------------------------
/*
    This file is part of clio: https://github.com/XRPLF/clio
    Copyright (c) 2024, the clio developers.

    Permission to use, copy, modify, and distribute this software for any
    purpose with or without fee is hereby granted, provided that the above
    copyright notice and this permission notice appear in all copies.

    THE  SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
    WITH  REGARD  TO  THIS  SOFTWARE  INCLUDING  ALL  IMPLIED  WARRANTIES  OF
    MERCHANTABILITY  AND  FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
    ANY  SPECIAL,  DIRECT,  INDIRECT, OR  CONSEQUENTIAL  DAMAGES  OR  ANY
    DAMAGES  WHATSOEVER  RESULTING  FROM  LOSS  OF  USE,  DATA  OR  PROFITS,
    WHETHER  IN  AN  ACTION  OF  CONTRACT,  NEGLIGENCE  OR  OTHER  TORTIOUS
    ACTION,  ARISING  OUT  OF  OR  IN  CONNECTION  WITH  THE  USE  OR
    PERFORMANCE OF THIS SOFTWARE.
*/
//==============================================================================

#pragma once

#include "data/ClickHouseBackend.hpp"
#include "data/LedgerCacheInterface.hpp"
#include "data/clickhouse/SettingsProvider.hpp"
#include "data/clickhouse/Schema.hpp"
#include "util/log/Logger.hpp"

#include <boost/asio/spawn.hpp>
#include <fmt/format.h>

#include <cstdint>
#include <string>
#include <utility>

namespace migration::clickhouse {

/**
 * @brief The backend for the migration. It is a subclass of the ClickHouseBackend and provides the migration specific
 * functionalities.
 */
class ClickHouseMigrationBackend : public data::clickhouse::ClickHouseBackend {
    util::Logger log_{"Migration"};
    data::clickhouse::SettingsProvider settingsProvider_;
    data::clickhouse::Schema<data::clickhouse::SettingsProvider> schema_;

public:
    /**
     * @brief Construct a new ClickHouse Migration Backend object. The backend is not readonly.
     *
     * @param settingsProvider The settings provider
     * @param cache The ledger cache to use
     */
    explicit ClickHouseMigrationBackend(
        data::clickhouse::SettingsProvider settingsProvider,
        data::LedgerCacheInterface& cache
    )
        : data::clickhouse::ClickHouseBackend{std::move(settingsProvider), cache, false /* not readonly */}
        , settingsProvider_(std::move(settingsProvider))
        , schema_{settingsProvider_}
    {
    }

    /**
     * @brief Initialize the database schema (create database and tables).
     */
    void initializeSchema()
    {
        LOG(log_.info()) << "Initializing ClickHouse schema...";

        // Create database
        if (auto const res = handle_.execute(schema_.getCreateDatabase()); not res) {
            throw std::runtime_error("Could not create database: " + res.error().message());
        }
        LOG(log_.info()) << "Database created successfully";

        // Create tables
        auto const schemaStatements = schema_.getCreateSchema();
        LOG(log_.info()) << "Creating " << schemaStatements.size() << " tables...";
        
        if (auto const res = handle_.executeEach(schemaStatements); not res) {
            throw std::runtime_error("Could not create schema: " + res.error().message());
        }
        LOG(log_.info()) << "Schema created successfully";
    }

    /**
     * @brief Write migrator status to the database.
     *
     * @param migratorName The name of the migrator
     * @param status The status to write
     */
    void writeMigratorStatus(std::string const& migratorName, std::string const& status)
    {
        auto const query = fmt::format(
            "INSERT INTO {} (migrator_name, status, timestamp) VALUES ('{}', '{}', now())",
            data::clickhouse::qualifiedTableName(settingsProvider_, "migrator_status"),
            migratorName,
            status
        );

        if (auto const res = handle_.execute(query); not res) {
            LOG(log_.error()) << "Failed to write migrator status: " << res.error().message();
            throw std::runtime_error("Failed to write migrator status: " + res.error().message());
        }
    }

    /**
     * @brief Get migrator status from the database.
     *
     * @param migratorName The name of the migrator
     * @return The status string
     */
    std::optional<std::string> getMigratorStatus(std::string const& migratorName) const
    {
        auto const query = fmt::format(
            "SELECT status FROM {} WHERE migrator_name = '{}' ORDER BY timestamp DESC LIMIT 1",
            data::clickhouse::qualifiedTableName(settingsProvider_, "migrator_status"),
            migratorName
        );

        if (auto const res = handle_.query(query); res) {
            if (!res->rows.empty()) {
                return res->rows[0][0];
            }
        }
        return std::nullopt;
    }

    /**
     * @brief Fetch migrator status from the database (async version for migration system).
     *
     * @param migratorName The name of the migrator
     * @param yield The yield context
     * @return The status string
     */
    std::optional<std::string> fetchMigratorStatus(std::string const& migratorName, boost::asio::yield_context yield) const
    {
        (void)yield;
        return getMigratorStatus(migratorName);
    }

    /**
     * @brief Get the underlying ClickHouse handle.
     */
    data::clickhouse::Handle const& getHandle() const { return handle_; }
};

}  // namespace migration::clickhouse
