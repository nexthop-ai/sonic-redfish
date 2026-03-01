// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2024 SONiC Project

#include "switch_redis_adapter.hpp"

#include <syslog.h>

namespace sonic::dbus_bridge
{

SwitchRedisAdapter::SwitchRedisAdapter(const std::string& host, int port)
    : host_(host), port_(port)
{
}

SwitchRedisAdapter::~SwitchRedisAdapter()
{
    if (stateDbContext_)
    {
        redisFree(stateDbContext_);
    }
}

bool SwitchRedisAdapter::connect()
{
    syslog(LOG_INFO, "Connecting to Switch Redis at %s:%d...", host_.c_str(), port_);

    // Connect to STATE_DB (DB 6) on switch
    stateDbContext_ = connectToDb(6);

    if (stateDbContext_)
    {
        syslog(LOG_INFO, "Connected to Switch STATE_DB");
        return true;
    }

    syslog(LOG_WARNING, "Failed to connect to Switch Redis at %s:%d", host_.c_str(), port_);
    return false;
}

redisContext* SwitchRedisAdapter::connectToDb(int dbIndex)
{
    struct timeval timeout = {2, 0}; // 2 seconds

    syslog(LOG_DEBUG, "Attempting TCP connection to Switch Redis %s:%d...",
           host_.c_str(), port_);

    redisContext* ctx = redisConnectWithTimeout(host_.c_str(), port_, timeout);

    if (!ctx)
    {
        syslog(LOG_ERR, "Switch Redis: Failed to allocate context");
        return nullptr;
    }

    if (ctx->err)
    {
        syslog(LOG_WARNING, "Switch Redis: Connection failed: %s", ctx->errstr);
        redisFree(ctx);
        return nullptr;
    }

    // Select STATE_DB
    redisReply* reply = static_cast<redisReply*>(
        redisCommand(ctx, "SELECT %d", dbIndex));

    if (!reply)
    {
        syslog(LOG_ERR, "Switch Redis: SELECT command failed");
        redisFree(ctx);
        return nullptr;
    }

    if (reply->type == REDIS_REPLY_ERROR)
    {
        syslog(LOG_ERR, "Switch Redis: SELECT DB %d failed: %s", dbIndex, reply->str);
        freeReplyObject(reply);
        redisFree(ctx);
        return nullptr;
    }

    freeReplyObject(reply);
    syslog(LOG_DEBUG, "Switch Redis: Selected database %d", dbIndex);
    return ctx;
}

std::optional<std::string> SwitchRedisAdapter::getSwitchVersion()
{
    return hget("VERSIONS|sonic", "build_version");
}

std::map<std::string, std::string> SwitchRedisAdapter::getVersionInfo()
{
    return hgetall("VERSIONS|sonic");
}

std::map<std::string, std::string> SwitchRedisAdapter::hgetall(const std::string& key)
{
    std::map<std::string, std::string> result;

    if (!stateDbContext_)
    {
        return result;
    }

    redisReply* reply = static_cast<redisReply*>(
        redisCommand(stateDbContext_, "HGETALL %s", key.c_str()));

    if (!reply)
    {
        return result;
    }

    if (reply->type == REDIS_REPLY_ARRAY && reply->elements > 0)
    {
        for (size_t i = 0; i < reply->elements; i += 2)
        {
            if (i + 1 < reply->elements)
            {
                std::string field(reply->element[i]->str, reply->element[i]->len);
                std::string value(reply->element[i + 1]->str, reply->element[i + 1]->len);
                result[field] = value;
            }
        }
    }

    freeReplyObject(reply);
    return result;
}

std::optional<std::string> SwitchRedisAdapter::hget(const std::string& key,
                                                     const std::string& field)
{
    if (!stateDbContext_)
    {
        return std::nullopt;
    }

    redisReply* reply = static_cast<redisReply*>(
        redisCommand(stateDbContext_, "HGET %s %s", key.c_str(), field.c_str()));

    if (!reply)
    {
        return std::nullopt;
    }

    std::optional<std::string> result;
    if (reply->type == REDIS_REPLY_STRING)
    {
        result = std::string(reply->str, reply->len);
    }

    freeReplyObject(reply);
    return result;
}

} // namespace sonic::dbus_bridge

