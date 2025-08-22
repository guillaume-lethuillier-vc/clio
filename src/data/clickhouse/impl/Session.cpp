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

#include "data/clickhouse/impl/Session.hpp"
#include <sstream>
#include <iostream>
#include <stdexcept>
#include <boost/json.hpp>

namespace data::clickhouse::impl {

Session::Session(const Settings& settings) 
    : settings_(settings), lastError_(""), valid_(true) {
}

std::string Session::buildRequestParams(const std::string& sql, bool isQuery) const {
    std::ostringstream oss;
    
    // NOTE(NODE-2688): for CREATE, DROP, ALTER, etc. statements, send SQL directly in POST body
    // For SELECT queries, use query= parameter
    if (isQuery) {
        oss << "query=" << sql;
    } else {
        // other statement: send SQL directly
        oss << sql;
    }
    
    return oss.str();
}

util::requests::RequestBuilder Session::createRequestBuilder() const {
    auto builder = util::requests::RequestBuilder(
        settings_.host, 
        std::to_string(settings_.port)
    );
    
    // build URL with authentication and database parameters
    std::ostringstream urlParams;
    bool hasParams = false;
    
    // db parameter (if specified)
    if (!settings_.database.empty()) {
        if (hasParams) urlParams << "&";
        urlParams << "database=" << settings_.database;
        hasParams = true;
    }
    
    // authentication
    if (!settings_.getUsername().empty()) {
        if (hasParams) urlParams << "&";
        urlParams << "user=" << settings_.getUsername();
        hasParams = true;
    }
    if (!settings_.getPassword().empty()) {
        if (hasParams) urlParams << "&";
        urlParams << "password=" << settings_.getPassword();
        hasParams = true;
    }
    
    if (hasParams) {
        builder.setTarget("/?" + urlParams.str());
    } else {
        builder.setTarget("/");
    }
    
    return builder;
}

Result Session::parseResponse(const std::string& response) const {
    Result result;
    
    if (response.empty()) {
        return result; 
    }
    
    try {
        std::istringstream responseStream(response);
        std::string line;
        bool firstRow = true;
        
        while (std::getline(responseStream, line)) {
            line.erase(0, line.find_first_not_of(" \t\r\n"));
            line.erase(line.find_last_not_of(" \t\r\n") + 1);
            
            if (line.empty()) continue;
            
            try {
                boost::json::value jsonRow = boost::json::parse(line);
                if (!jsonRow.is_object()) continue;
                
                const auto& jsonObject = jsonRow.as_object();
                
                // extract column names from the first row
                if (firstRow) {
                    result.columnNames.clear();
                    for (const auto& [key, value] : jsonObject) {
                        result.columnNames.emplace_back(key);
                    }
                    firstRow = false;
                }
                
                // extract row data
                std::vector<std::string> row;
                row.reserve(result.columnNames.size());
                
                for (const auto& columnName : result.columnNames) {
                    auto it = jsonObject.find(columnName);
                    if (it != jsonObject.end()) {
                        if (it->value().is_string()) {
                            row.emplace_back(it->value().as_string().c_str());
                        } else if (it->value().is_null()) {
                            row.emplace_back(""); // NULL values
                        } else {
                            row.emplace_back(boost::json::serialize(it->value()));
                        }
                    } else {
                        row.emplace_back(""); // missing column
                    }
                }
                
                result.rows.emplace_back(std::move(row));
                
            } catch (const std::exception& e) {
                // if the JSON parsing fails, treat as plain text
                if (result.columnNames.empty()) {
                    result.columnNames = {"result"};
                }
                result.rows.emplace_back(std::vector<std::string>{line});
                lastError_ = "JSON parsing failed for line: " + std::string(e.what());
            }
        }
        
    } catch (const std::exception& e) {
        // if the parsing fails, treat the response as a single result
        result.columnNames = {"result"};
        result.rows = {{response}};
        lastError_ = "Response parsing warning (using fallback): " + std::string(e.what());
    }
    
    return result;
}

Result Session::query(const std::string& sql, boost::asio::yield_context yield) const {
    if (!valid_) {
        lastError_ = "Session not valid";
        return Result{};
    }
    
    try {
        auto builder = createRequestBuilder();
        builder.setTarget("/")
               .addData(buildRequestParams(sql, true));
        
        auto response = builder.postPlain(yield);
        if (response) {
            return parseResponse(*response);
        } else {
            lastError_ = "HTTP request failed: " + response.error().message();
            return Result{};
        }
        
    } catch (const std::exception& e) {
        lastError_ = "Query failed: " + std::string(e.what());
        return Result{};
    }
}

bool Session::execute(const std::string& sql, boost::asio::yield_context yield) const {
    if (!valid_) {
        lastError_ = "Session not valid";
        return false;
    }
    
    try {
        auto builder = createRequestBuilder();
        auto params = buildRequestParams(sql, false);  // SQL only, no auth params
        
        builder.addData(params);
        
        auto response = builder.postPlain(yield);
        if (!response.has_value()) {
            lastError_ = "HTTP request failed";
            return false;
        }

        // check if ClickHouse returned an error
        std::string responseStr = *response;
        if (responseStr.find("Code:") != std::string::npos && 
            (responseStr.find("Exception") != std::string::npos || 
             responseStr.find("Error") != std::string::npos)) {
            lastError_ = "ClickHouse error: " + responseStr;
            return false;
        }
        
        return true;
        
    } catch (const std::exception& e) {
        lastError_ = "Execute failed: " + std::string(e.what());
        return false;
    }
}

bool Session::isValid() const {
    return valid_;
}

std::string Session::getLastError() const {
    return lastError_;
}

} // namespace data::clickhouse::impl
