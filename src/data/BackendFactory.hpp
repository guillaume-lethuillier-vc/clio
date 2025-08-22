//------------------------------------------------------------------------------
/*
    This file is part of clio: https://github.com/XRPLF/clio
    Copyright (c) 2022, the clio developers.

    Permission to use, copy, modify, and distribute this software for any
    purpose with or without fee is hereby granted, provided that the above
    copyright notice and this permission notice appear in all copies.

    THE  SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
    WITH  REGARD  TO  THIS  SOFTWARE  INCLUDING  ALL  IMPLIED  WARRANTIES  OF
    MERCHANTABILITY  AND  FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
    ANY  SPECIAL,  DIRECT,  INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
    WHATSOEVER  RESULTING  FROM  LOSS  OF USE, DATA OR PROFITS, WHETHER IN AN
    ACTION  OF  CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
    OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/
//==============================================================================

#pragma once

#include "data/BackendInterface.hpp"
#include "data/LedgerCacheInterface.hpp"
#include "util/config/ConfigDefinition.hpp"
#include "util/log/Logger.hpp"

// NOTE(NODE-2688): TEMPORARILY DISABLED: Cassandra backend causing global static initialization crashes
// #include "data/CassandraBackend.hpp"
#include "data/ClickHouseBackend.hpp"
// #include "data/cassandra/SettingsProvider.hpp"
#include "data/clickhouse/SettingsProvider.hpp"

#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/predicate.hpp>

#include <memory>
#include <stdexcept>
#include <string>

namespace data {

/**
 * @brief A factory function that creates the backend based on a config.
 *
 * @param config The clio config to use
 * @param cache The ledger cache to use
 * @return A shared_ptr<BackendInterface> with the selected implementation
 */
inline std::shared_ptr<BackendInterface>
makeBackend(util::config::ClioConfigDefinition const& config, data::LedgerCacheInterface& cache)
{
    static util::Logger const log{"Backend"};  // NOLINT(readability-identifier-naming)
    LOG(log.info()) << "Constructing BackendInterface";

    auto const readOnly = config.get<bool>("read_only");

    auto const type = config.get<std::string>("database.type");
    std::shared_ptr<BackendInterface> backend = nullptr;

                    // NOTE(NODE-2688): TEMPORARILY DISABLED: Cassandra backend causing global static initialization crashes
                    /*
                if (boost::iequals(type, "cassandra")) {
                     try {
                         auto const cfg = config.getObject("database." + type);
                         backend = std::make_shared<data::cassandra::CassandraBackend>(
                             data::cassandra::SettingsProvider{cfg}, cache, readOnly
                         );
                     } catch (std::exception const& ex) {
                         LOG(log.error()) << "Failed to create Cassandra backend: " << ex.what();
                         throw std::runtime_error("Failed to create Cassandra backend: " + std::string(ex.what()));
                     }
                 } else 
                  */
                if (boost::iequals(type, "clickhouse")) {
        try {
            auto const cfg = config.getObject("database." + type);
            backend = std::make_shared<data::clickhouse::ClickHouseBackend>(
                data::clickhouse::SettingsProvider{cfg}, cache, readOnly
            );
        } catch (std::exception const& ex) {
            LOG(log.error()) << "Failed to create ClickHouse backend: " << ex.what();
            throw std::runtime_error("Failed to create ClickHouse backend: " + std::string(ex.what()));
        }
    } else {
        throw std::runtime_error("Unsupported database type: " + type);
    }

    if (!backend)
        throw std::runtime_error("Invalid database type");

    auto const rng = backend->hardFetchLedgerRangeNoThrow();
    if (rng)
        backend->setRange(rng->minSequence, rng->maxSequence);

    LOG(log.info()) << "Constructed BackendInterface Successfully";
    return backend;
}
}  // namespace data
