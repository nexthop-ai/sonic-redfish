#include "redis_adapter.hpp"
#include "logger.hpp"
#include <cstring>

namespace sonic::dbus_bridge
{

RedisAdapter::RedisAdapter(const std::string& configDbHost, int configDbPort,
                           const std::string& stateDbHost, int stateDbPort)
    : configDbHost_(configDbHost), configDbPort_(configDbPort),
      stateDbHost_(stateDbHost), stateDbPort_(stateDbPort)
{
}

RedisAdapter::~RedisAdapter()
{
    if (configDbContext_)
    {
        redisFree(configDbContext_);
    }
    if (stateDbContext_)
    {
        redisFree(stateDbContext_);
    }
}

bool RedisAdapter::connect()
{
    LOG_INFO("Connecting to Redis databases...");

    // Connect to CONFIG_DB (DB 4)
    configDbContext_ = connectToDb(configDbHost_, configDbPort_, 4);
    if (configDbContext_)
    {
        LOG_INFO("Connected to CONFIG_DB");
    }
    else
    {
        LOG_WARNING("Failed to connect to CONFIG_DB");
    }

    // Connect to STATE_DB (DB 6)
    stateDbContext_ = connectToDb(stateDbHost_, stateDbPort_, 6);
    if (stateDbContext_)
    {
        LOG_INFO("Connected to STATE_DB");
    }
    else
    {
        LOG_WARNING("Failed to connect to STATE_DB");
    }

    // Return true if at least one connection succeeded
    return (configDbContext_ != nullptr) || (stateDbContext_ != nullptr);
}

redisContext* RedisAdapter::connectToDb(const std::string& host, int port, int dbIndex)
{
    struct timeval timeout = {2, 0}; // 2 seconds
    redisContext* ctx = nullptr;
    bool connected = false;

    // Try TCP connection first (most reliable for SONiC)
    LOG_DEBUG("Attempting TCP connection to %s:%d...", host.c_str(), port);
    ctx = redisConnectWithTimeout(host.c_str(), port, timeout);

    if (!ctx)
    {
        LOG_ERROR("TCP: Failed to allocate Redis context (out of memory?)");
    }
    else if (ctx->err)
    {
        LOG_DEBUG("TCP: Connection failed: %s (errno: %d)", ctx->errstr, ctx->err);
        redisFree(ctx);
        ctx = nullptr;
    }
    else
    {
        LOG_INFO("Connected to Redis via TCP: %s:%d", host.c_str(), port);
        connected = true;
    }

    // If TCP failed, try Unix socket as fallback
    if (!connected)
    {
        const char* unixSockets[] = {
            "/var/run/redis/redis.sock",
            "/run/redis/redis.sock",
            "/var/run/redis.sock",
            nullptr
        };

        for (int i = 0; unixSockets[i] != nullptr && !connected; i++)
        {
            LOG_DEBUG("Attempting Unix socket connection to %s...", unixSockets[i]);
            ctx = redisConnectUnixWithTimeout(unixSockets[i], timeout);

            if (!ctx)
            {
                LOG_ERROR("Unix socket: Failed to allocate Redis context");
            }
            else if (ctx->err)
            {
                LOG_DEBUG("Unix socket: Connection failed: %s (errno: %d)", ctx->errstr, ctx->err);
                redisFree(ctx);
                ctx = nullptr;
            }
            else
            {
                LOG_INFO("Connected to Redis via Unix socket: %s", unixSockets[i]);
                connected = true;
            }
        }
    }

    if (!connected || !ctx)
    {
        LOG_ERROR("All Redis connection attempts failed");
        return nullptr;
    }

    // Select database
    LOG_DEBUG("Selecting Redis database %d...", dbIndex);
    redisReply* reply = static_cast<redisReply*>(
        redisCommand(ctx, "SELECT %d", dbIndex));

    if (!reply)
    {
        LOG_ERROR("Failed to send SELECT command (connection lost?)");
        redisFree(ctx);
        return nullptr;
    }

    if (reply->type == REDIS_REPLY_ERROR)
    {
        LOG_ERROR("Failed to select DB %d: %s", dbIndex, reply->str);
        freeReplyObject(reply);
        redisFree(ctx);
        return nullptr;
    }

    freeReplyObject(reply);
    LOG_DEBUG("Selected database %d", dbIndex);
    return ctx;
}

DeviceMetadata RedisAdapter::getDeviceMetadata()
{
    DeviceMetadata metadata;
    
    if (!configDbContext_)
    {
        return metadata; // Return empty metadata
    }

    // Read DEVICE_METADATA|localhost hash
    auto fields = hgetall(configDbContext_, "DEVICE_METADATA|localhost");

    if (!fields.empty())
    {
        if (fields.count("platform")) metadata.platform = fields["platform"];
        if (fields.count("hwsku")) metadata.hwsku = fields["hwsku"];
        if (fields.count("hostname")) metadata.hostname = fields["hostname"];
        if (fields.count("mac")) metadata.mac = fields["mac"];
        if (fields.count("type")) metadata.type = fields["type"];
        if (fields.count("manufacturer")) metadata.manufacturer = fields["manufacturer"];
        if (fields.count("serial_number")) metadata.serialNumber = fields["serial_number"];
        if (fields.count("part_number")) metadata.partNumber = fields["part_number"];
        if (fields.count("model")) metadata.model = fields["model"];
    }

    return metadata;
}

ChassisState RedisAdapter::getChassisState()
{
    ChassisState state;
    
    if (!stateDbContext_)
    {
        return state; // Return default state (on)
    }

    // Try to read chassis state from STATE_DB
    auto powerState = hget(stateDbContext_, "CHASSIS_STATE|chassis0", "power_state");
    if (powerState)
    {
        state.powerState = *powerState;
    }
    
    return state;
}

std::map<std::string, std::string> RedisAdapter::hgetall(redisContext* ctx,
                                                          const std::string& key)
{
    std::map<std::string, std::string> result;
    
    redisReply* reply = static_cast<redisReply*>(
        redisCommand(ctx, "HGETALL %s", key.c_str()));
    
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
                std::string value(reply->element[i+1]->str, reply->element[i+1]->len);
                result[field] = value;
            }
        }
    }
    
    freeReplyObject(reply);
    return result;
}

std::optional<std::string> RedisAdapter::hget(redisContext* ctx,
                                               const std::string& key,
                                               const std::string& field)
{
    redisReply* reply = static_cast<redisReply*>(
        redisCommand(ctx, "HGET %s %s", key.c_str(), field.c_str()));
    
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

void RedisAdapter::freeReply(void* reply)
{
    if (reply)
    {
        freeReplyObject(reply);
    }
}

} // namespace sonic::dbus_bridge

