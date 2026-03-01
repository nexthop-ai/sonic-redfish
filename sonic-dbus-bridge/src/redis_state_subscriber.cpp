#include "redis_state_subscriber.hpp"
#include "logger.hpp"
#include <cstring>

namespace sonic::dbus_bridge
{

RedisStateSubscriber::RedisStateSubscriber()
    : subContext_(nullptr), getContext_(nullptr), running_(false)
{
    LOG_INFO( "[RedisStateSubscriber] Constructor called");
}

RedisStateSubscriber::~RedisStateSubscriber()
{
    LOG_INFO( "[RedisStateSubscriber] Destructor called");
    stop();
}

bool RedisStateSubscriber::start(const std::string& host, int port,
                                 KeyspaceCallback callback)
{
    LOG_INFO( "[RedisStateSubscriber] ========================================");
    LOG_INFO( "[RedisStateSubscriber] Starting subscriber");
    LOG_INFO( "[RedisStateSubscriber] Redis: %s:%d", host.c_str(), port);
    
    if (running_)
    {
        LOG_WARNING( "[RedisStateSubscriber] Subscriber already running");
        return false;
    }
    
    callback_ = callback;
    
    // Create subscription context
    struct timeval timeout = {2, 0};
    subContext_ = redisConnectWithTimeout(host.c_str(), port, timeout);
    
    if (!subContext_ || subContext_->err)
    {
        LOG_ERROR( "[RedisStateSubscriber] Subscription connection failed: %s",
               subContext_ ? subContext_->errstr : "allocation failed");
        if (subContext_)
        {
            redisFree(subContext_);
            subContext_ = nullptr;
        }
        return false;
    }
    
    LOG_INFO( "[RedisStateSubscriber] Subscription connection established");
    
    // Create GET context (for HGETALL)
    getContext_ = redisConnectWithTimeout(host.c_str(), port, timeout);
    
    if (!getContext_ || getContext_->err)
    {
        LOG_ERROR( "[RedisStateSubscriber] GET connection failed: %s",
               getContext_ ? getContext_->errstr : "allocation failed");
        redisFree(subContext_);
        subContext_ = nullptr;
        if (getContext_)
        {
            redisFree(getContext_);
            getContext_ = nullptr;
        }
        return false;
    }
    
    LOG_INFO( "[RedisStateSubscriber] GET connection established");
    
    // Select STATE_DB (DB 6) for both contexts
    redisReply* reply = (redisReply*)redisCommand(subContext_, "SELECT 6");
    if (!reply || reply->type == REDIS_REPLY_ERROR)
    {
        LOG_ERROR( "[RedisStateSubscriber] Failed to select STATE_DB on subscription context");
        if (reply) freeReplyObject(reply);
        redisFree(subContext_);
        redisFree(getContext_);
        subContext_ = nullptr;
        getContext_ = nullptr;
        return false;
    }
    freeReplyObject(reply);
    
    reply = (redisReply*)redisCommand(getContext_, "SELECT 6");
    if (!reply || reply->type == REDIS_REPLY_ERROR)
    {
        LOG_ERROR( "[RedisStateSubscriber] Failed to select STATE_DB on GET context");
        if (reply) freeReplyObject(reply);
        redisFree(subContext_);
        redisFree(getContext_);
        subContext_ = nullptr;
        getContext_ = nullptr;
        return false;
    }
    freeReplyObject(reply);
    
    LOG_INFO( "[RedisStateSubscriber] STATE_DB (DB 6) selected on both contexts");
    
    // Subscribe to keyspace notifications for SWITCH_HOST_STATE
    LOG_INFO( "[RedisStateSubscriber] Subscribing to __keyspace@6__:SWITCH_HOST_STATE");
    reply = (redisReply*)redisCommand(subContext_,
        "SUBSCRIBE __keyspace@6__:SWITCH_HOST_STATE");
    
    if (!reply || reply->type == REDIS_REPLY_ERROR)
    {
        LOG_ERROR( "[RedisStateSubscriber] Failed to subscribe to keyspace notifications");
        if (reply) freeReplyObject(reply);
        redisFree(subContext_);
        redisFree(getContext_);
        subContext_ = nullptr;
        getContext_ = nullptr;
        return false;
    }
    freeReplyObject(reply);
    
    LOG_INFO( "[RedisStateSubscriber] Subscribed successfully");
    
    // Start subscriber thread
    running_ = true;
    subscriberThread_ = std::thread(&RedisStateSubscriber::subscriberLoop, this);
    
    LOG_INFO( "[RedisStateSubscriber] Subscriber thread started");
    LOG_INFO( "[RedisStateSubscriber] ========================================");
    
    return true;
}

bool RedisStateSubscriber::startMultiKey(const std::string& host, int port,
                                          const std::vector<std::string>& keys,
                                          KeyspaceCallback callback)
{
    LOG_INFO( "[RedisStateSubscriber] ========================================");
    LOG_INFO( "[RedisStateSubscriber] Starting multi-key subscriber");
    LOG_INFO( "[RedisStateSubscriber] Host: %s, Port: %d", host.c_str(), port);
    LOG_INFO( "[RedisStateSubscriber] Subscribing to %zu keys", keys.size());

    if (running_)
    {
        LOG_WARNING( "[RedisStateSubscriber] Already running");
        return false;
    }

    if (keys.empty())
    {
        LOG_ERROR( "[RedisStateSubscriber] No keys provided");
        return false;
    }

    callback_ = callback;

    // Create two Redis contexts: one for subscribing, one for getting data
    subContext_ = redisConnect(host.c_str(), port);
    if (!subContext_ || subContext_->err)
    {
        LOG_ERROR( "[RedisStateSubscriber] Failed to connect to Redis (subscribe): %s",
                  subContext_ ? subContext_->errstr : "connection error");
        if (subContext_) redisFree(subContext_);
        subContext_ = nullptr;
        return false;
    }

    getContext_ = redisConnect(host.c_str(), port);
    if (!getContext_ || getContext_->err)
    {
        LOG_ERROR( "[RedisStateSubscriber] Failed to connect to Redis (get): %s",
                  getContext_ ? getContext_->errstr : "connection error");
        if (getContext_) redisFree(getContext_);
        redisFree(subContext_);
        getContext_ = nullptr;
        subContext_ = nullptr;
        return false;
    }

    LOG_INFO( "[RedisStateSubscriber] Connected to Redis");

    // Select STATE_DB (DB 6) on both contexts
    redisReply* reply = (redisReply*)redisCommand(subContext_, "SELECT 6");
    if (!reply || reply->type == REDIS_REPLY_ERROR)
    {
        LOG_ERROR( "[RedisStateSubscriber] Failed to select STATE_DB on subscribe context");
        if (reply) freeReplyObject(reply);
        redisFree(subContext_);
        redisFree(getContext_);
        subContext_ = nullptr;
        getContext_ = nullptr;
        return false;
    }
    freeReplyObject(reply);

    reply = (redisReply*)redisCommand(getContext_, "SELECT 6");
    if (!reply || reply->type == REDIS_REPLY_ERROR)
    {
        LOG_ERROR( "[RedisStateSubscriber] Failed to select STATE_DB on get context");
        if (reply) freeReplyObject(reply);
        redisFree(subContext_);
        redisFree(getContext_);
        subContext_ = nullptr;
        getContext_ = nullptr;
        return false;
    }
    freeReplyObject(reply);

    LOG_INFO( "[RedisStateSubscriber] STATE_DB (DB 6) selected on both contexts");

    // Subscribe to keyspace notifications for all keys
    for (const auto& key : keys)
    {
        std::string channel = "__keyspace@6__:" + key;
        LOG_INFO( "[RedisStateSubscriber] Subscribing to %s", channel.c_str());

        reply = (redisReply*)redisCommand(subContext_, "SUBSCRIBE %s", channel.c_str());

        if (!reply || reply->type == REDIS_REPLY_ERROR)
        {
            LOG_ERROR( "[RedisStateSubscriber] Failed to subscribe to %s", channel.c_str());
            if (reply) freeReplyObject(reply);
            redisFree(subContext_);
            redisFree(getContext_);
            subContext_ = nullptr;
            getContext_ = nullptr;
            return false;
        }
        freeReplyObject(reply);
    }

    LOG_INFO( "[RedisStateSubscriber] Subscribed to all %zu keys successfully", keys.size());

    // Start subscriber thread
    running_ = true;
    subscriberThread_ = std::thread(&RedisStateSubscriber::subscriberLoop, this);

    LOG_INFO( "[RedisStateSubscriber] Subscriber thread started");
    LOG_INFO( "[RedisStateSubscriber] ========================================");

    return true;
}

void RedisStateSubscriber::stop()
{
    if (!running_)
    {
        return;
    }
    
    LOG_INFO( "[RedisStateSubscriber] Stopping subscriber");
    
    running_ = false;
    
    if (subscriberThread_.joinable())
    {
        subscriberThread_.join();
        LOG_INFO( "[RedisStateSubscriber] Subscriber thread joined");
    }
    
    if (subContext_)
    {
        redisFree(subContext_);
        subContext_ = nullptr;
    }
    
    if (getContext_)
    {
        redisFree(getContext_);
        getContext_ = nullptr;
    }
    
    LOG_INFO( "[RedisStateSubscriber] Subscriber stopped");
}

void RedisStateSubscriber::subscriberLoop()
{
    LOG_INFO( "[RedisStateSubscriber] Subscriber loop started");

    while (running_)
    {
        redisReply* reply;
        if (redisGetReply(subContext_, (void**)&reply) != REDIS_OK)
        {
            LOG_ERROR( "[RedisStateSubscriber] Redis subscriber error: %s", subContext_->errstr);
            break;
        }

        if (!reply)
        {
            LOG_WARNING( "[RedisStateSubscriber] Received null reply");
            continue;
        }

        // Expected format: ["message", channel, message]
        if (reply->type == REDIS_REPLY_ARRAY && reply->elements == 3)
        {
            std::string messageType = reply->element[0]->str;
            std::string channel = reply->element[1]->str;
            std::string message = reply->element[2]->str;

            LOG_DEBUG( "[RedisStateSubscriber] Received: type=%s, channel=%s, message=%s",
                   messageType.c_str(), channel.c_str(), message.c_str());

            if (messageType == "message" && message == "hset")
            {
                LOG_INFO( "[RedisStateSubscriber] HSET detected on %s", channel.c_str());
                handleKeyspaceNotification(channel);
            }
        }

        freeReplyObject(reply);
    }

    LOG_INFO( "[RedisStateSubscriber] Subscriber loop ended");
}

void RedisStateSubscriber::handleKeyspaceNotification(const std::string& channel)
{
    LOG_INFO( "[RedisStateSubscriber] ========================================");
    LOG_INFO( "[RedisStateSubscriber] Handling keyspace notification");
    LOG_INFO( "[RedisStateSubscriber] Channel: %s", channel.c_str());

    // Channel format: __keyspace@6__:SWITCH_HOST_STATE
    // Extract key name
    size_t pos = channel.find_last_of(':');
    if (pos == std::string::npos)
    {
        LOG_WARNING( "[RedisStateSubscriber] Invalid channel format: %s", channel.c_str());
        return;
    }

    std::string key = channel.substr(pos + 1);
    LOG_INFO( "[RedisStateSubscriber] Key: %s", key.c_str());

    // Get all fields from the hash
    std::map<std::string, std::string> fields = hgetall(key);

    if (fields.empty())
    {
        LOG_WARNING( "[RedisStateSubscriber] No fields found for key: %s", key.c_str());
        LOG_INFO( "[RedisStateSubscriber] ========================================");
        return;
    }

    LOG_INFO( "[RedisStateSubscriber] Retrieved %zu fields from %s", fields.size(), key.c_str());

    // Invoke callback for each field
    for (const auto& [field, value] : fields)
    {
        LOG_INFO( "[RedisStateSubscriber]   %s = %s", field.c_str(), value.c_str());

        if (callback_)
        {
            callback_(key, field, value);
        }
    }

    LOG_INFO( "[RedisStateSubscriber] ========================================");
}

std::map<std::string, std::string> RedisStateSubscriber::hgetall(const std::string& key)
{
    std::map<std::string, std::string> result;

    LOG_DEBUG( "[RedisStateSubscriber] HGETALL %s", key.c_str());

    redisReply* reply = (redisReply*)redisCommand(getContext_, "HGETALL %s", key.c_str());

    if (!reply)
    {
        LOG_ERROR( "[RedisStateSubscriber] HGETALL failed: connection lost");
        return result;
    }

    if (reply->type != REDIS_REPLY_ARRAY)
    {
        LOG_ERROR( "[RedisStateSubscriber] HGETALL failed: unexpected reply type %d", reply->type);
        freeReplyObject(reply);
        return result;
    }

    // Parse field-value pairs
    for (size_t i = 0; i + 1 < reply->elements; i += 2)
    {
        std::string field = reply->element[i]->str;
        std::string value = reply->element[i + 1]->str;
        result[field] = value;

        LOG_DEBUG( "[RedisStateSubscriber]   HGETALL result: %s = %s", field.c_str(), value.c_str());
    }

    freeReplyObject(reply);
    return result;
}

} // namespace sonic::dbus_bridge

