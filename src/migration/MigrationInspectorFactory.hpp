//------------------------------------------------------------------------------
/*
    This file is part of clio: https://github.com/XRPLF/clio
    Copyright (c) 2025, the clio developers.

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
#include "migration/MigrationInspectorInterface.hpp"
#include "migration/MigratiorStatus.hpp"
#include "migration/clickhouse/ClickHouseMigrationManager.hpp"
// NOTE(NODE-2688): TEMPORARILY DISABLED: Cassandra backend causing global static initialization crashes
// #include "migration/cassandra/CassandraMigrationManager.hpp"

#include "util/Assert.hpp"
#include "util/config/ConfigDefinition.hpp"
#include "util/log/Logger.hpp"

#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/predicate.hpp>

#include <memory>
#include <utility>

namespace migration {

/**
 * @brief A factory function that creates migration inspector instance and initializes the migration table if needed.
 *
 * @param config The config.
 * @param backend The backend instance. It should be initialized before calling this function.
 * @return A shared_ptr<MigrationInspectorInterface> instance
 */
inline std::shared_ptr<MigrationInspectorInterface>
makeMigrationInspector(
    util::config::ClioConfigDefinition const& config,
    std::shared_ptr<BackendInterface> const& backend
)
{
    ASSERT(backend != nullptr, "Backend is not initialized");

    auto const type = config.get<std::string>("database.type");

    static util::Logger const log{"Migration"};
    
    if (boost::iequals(type, "clickhouse")) {
        LOG(log.info()) << "Creating ClickHouse migration inspector";
        try {
            return std::make_shared<migration::clickhouse::ClickHouseMigrationInspector>(backend);
        } catch (std::exception const& ex) {
            LOG(log.error()) << "Failed to create ClickHouse migration inspector: " << ex.what();
        }
    }
    
    // NOTE(NODE-2688): TEMPORARILY DISABLED: Cassandra backend causing global static initialization crashes
    /*
     } else if (boost::iequals(type, "cassandra")) {
         LOG(log.info()) << "Creating Cassandra migration inspector";
         try {
             return std::make_shared<migration::cassandra::CassandraMigrationInspector>(backend);
         } catch (std::exception const& ex) {
             LOG(log.error()) << "Failed to create Cassandra migration inspector: " << ex.what();
         }
     }
    */

    LOG(log.info()) << "Migration not supported for database type: " << type << " (only clickhouse is supported), using dummy inspector";
    
    struct DummyMigrationInspector : public MigrationInspectorInterface {
        std::vector<std::tuple<std::string, MigratorStatus>> allMigratorsStatusPairs() const override { 
            return {}; 
        }
        std::vector<std::string> allMigratorsNames() const override { 
            return {}; 
        }
        MigratorStatus getMigratorStatusByName(std::string const& /* name */) const override { 
            return MigratorStatus::Migrated; 
        }
        std::string getMigratorDescriptionByName(std::string const& /* name */) const override { 
            return "Migration not supported for this database type"; 
        }
        bool isBlockingClio() const override { 
            return false; 
        }
    };
    
    return std::make_shared<DummyMigrationInspector>();
}

}  // namespace migration
