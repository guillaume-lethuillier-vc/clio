//------------------------------------------------------------------------------
/*
    This file is part of clio: https://github.com/XRPLF/clio
    Copyright (c) 2024, the clio developers.

    Permission to use, copy, modify, and distribute this software for any
    purpose with or without fee is hereby granted, provided that the above
    copyright notice and this permission notice appear in all copies.

    THE  SOFTWARE  IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
    WITH  REGARD  TO  THIS  SOFTWARE INCLUDING  ALL  IMPLIED  WARRANTIES  OF
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
#include "data/clickhouse/impl/Session.hpp"
#include "util/log/Logger.hpp"

#include <fmt/format.h>
#include <string>
#include <string_view>
#include <vector>

namespace data::clickhouse {

/**
 * @brief Returns the table name qualified with the database and table prefix
 *
 * @tparam SettingsProviderType The settings provider type
 * @param provider The settings provider
 * @param name The name of the table
 * @return The qualified table name
 */
template <typename SettingsProviderType>
[[nodiscard]] std::string inline qualifiedTableName(SettingsProviderType const& provider, std::string_view name)
{
    std::string prefix = provider.getTablePrefix().value_or("");
    if (!prefix.empty()) {
        return fmt::format("{}.{}{}", provider.getDatabase(), prefix, name);
    }
    return fmt::format("{}.{}", provider.getDatabase(), name);
}

/**
 * @brief Manages the DB schema and provides access to prepared statements.
 */
template <typename SettingsProviderType>
class Schema {
    util::Logger log_{"ClickHouse"};

public:
    /**
     * @brief Construct a new Schema object
     *
     * @param settingsProvider The settings provider
     */
    explicit Schema(SettingsProviderType const& settingsProvider) : settingsProvider_{std::cref(settingsProvider)}
    {
    }

    /**
     * @brief Get the CREATE DATABASE statement
     */
    [[nodiscard]] std::string getCreateDatabase() const {
        return fmt::format(
            "CREATE DATABASE IF NOT EXISTS {}",
            settingsProvider_.get().getDatabase()
        );
    }

    /**
     * @brief Get the schema creation statements
     * 
     * NOTE(NODE-2688): based on Cassandra: src/data/cassandra/Schema.hpp
     */
    [[nodiscard]] std::vector<std::string> getCreateSchema() const {
        auto statements = std::vector<std::string>{
            // Ledgers table
            fmt::format(
                R"(
                CREATE TABLE IF NOT EXISTS {} (
                    sequence UInt32,
                    hash String,
                    parent_hash String,
                    close_time UInt32,
                    parent_close_time UInt32,
                    close_time_resolution UInt8,
                    close_flags UInt8,
                    ledger_hash String,
                    parent_close_time_resolution UInt8,
                    parent_ledger_hash String,
                    total_coins String,
                    parent_total_coins String,
                    fee_denominator UInt32,
                    parent_fee_denominator UInt32,
                    base_fee UInt32,
                    parent_base_fee UInt32,
                    reserve_base UInt32,
                    parent_reserve_base UInt32,
                    reserve_inc UInt32,
                    parent_reserve_inc UInt32,
                    blob String
                ) ENGINE = MergeTree()
                ORDER BY sequence
                )",
                qualifiedTableName(settingsProvider_.get(), "ledgers")
            ),

            // Ledger objects table
            fmt::format(
                R"(
                CREATE TABLE IF NOT EXISTS {} (
                    key String,
                    seq UInt32,
                    blob String
                ) ENGINE = MergeTree()
                ORDER BY (key, seq)
                )",
                qualifiedTableName(settingsProvider_.get(), "ledger_objects")
            ),

            // Transactions table
            fmt::format(
                R"(
                CREATE TABLE IF NOT EXISTS {} (
                    hash String,
                    seq UInt32,
                    date UInt32,
                    transaction String,
                    metadata String
                ) ENGINE = MergeTree()
                ORDER BY (hash, seq)
                )",
                qualifiedTableName(settingsProvider_.get(), "transactions")
            ),

            // Account transactions table
            fmt::format(
                R"(
                CREATE TABLE IF NOT EXISTS {} (
                    account String,
                    seq UInt32,
                    hash String,
                    date UInt32,
                    ledger_sequence UInt32
                ) ENGINE = MergeTree()
                ORDER BY (account, seq, hash)
                )",
                qualifiedTableName(settingsProvider_.get(), "account_transactions")
            ),

            // NFTs table
            fmt::format(
                R"(
                CREATE TABLE IF NOT EXISTS {} (
                    token_id String,
                    seq UInt32,
                    owner String,
                    is_burned UInt8,
                    uri String,
                    flags UInt32,
                    transfer_fee UInt32,
                    issuer String,
                    taxon UInt32,
                    blob String
                ) ENGINE = MergeTree()
                ORDER BY (token_id, seq)
                )",
                qualifiedTableName(settingsProvider_.get(), "nfts")
            ),

            // NFT transactions table
            fmt::format(
                R"(
                CREATE TABLE IF NOT EXISTS {} (
                    token_id String,
                    seq UInt32,
                    hash String,
                    date UInt32,
                    ledger_sequence UInt32
                ) ENGINE = MergeTree()
                ORDER BY (token_id, seq, hash)
                )",
                qualifiedTableName(settingsProvider_.get(), "nft_transactions")
            ),

            // MPT holders table
            fmt::format(
                R"(
                CREATE TABLE IF NOT EXISTS {} (
                    mpt_id String,
                    account String,
                    seq UInt32,
                    balance String
                ) ENGINE = MergeTree()
                ORDER BY (mpt_id, account, seq)
                )",
                qualifiedTableName(settingsProvider_.get(), "mpt_holders")
            ),

            // Successors table
            fmt::format(
                R"(
                CREATE TABLE IF NOT EXISTS {} (
                    key String,
                    seq UInt32,
                    successor String
                ) ENGINE = MergeTree()
                ORDER BY (key, seq)
                )",
                qualifiedTableName(settingsProvider_.get(), "successors")
            ),

            // Node messages table
            fmt::format(
                R"(
                CREATE TABLE IF NOT EXISTS {} (
                    uuid String,
                    message String,
                    timestamp DateTime DEFAULT now()
                ) ENGINE = MergeTree()
                ORDER BY (uuid, timestamp)
                )",
                qualifiedTableName(settingsProvider_.get(), "node_messages")
            ),

            // Migrator status table
            fmt::format(
                R"(
                CREATE TABLE IF NOT EXISTS {} (
                    migrator_name String,
                    status String,
                    timestamp DateTime DEFAULT now()
                ) ENGINE = MergeTree()
                ORDER BY migrator_name
                )",
                qualifiedTableName(settingsProvider_.get(), "migrator_status")
            )
        };
        
        return statements;
    }

    /**
     * @brief Prepare all statements for the schema.
     *
     * @param handle The ClickHouse handle to prepare statements on
     */
    void
    prepareStatements(Handle& /*handle*/)
    {
        // ClickHouse does not require prepared statements: it optimizes queries automatically
        // (this method exists for API compatibility with the Cassandra backend)
        LOG(log_.info()) << "ClickHouse schema ready (no statement preparation needed)";
    }

private:
    std::reference_wrapper<SettingsProviderType const> settingsProvider_;
};

}  // namespace data::clickhouse
