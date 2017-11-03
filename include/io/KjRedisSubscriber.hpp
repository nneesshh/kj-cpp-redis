#pragma once

//------------------------------------------------------------------------------
/**
@class KjRedisSubscriber

(C) 2016 n.lee
*/

#include <functional>
#include <map>
#include <mutex>
#include <string>

#include "KjRedisConnection.hpp"

namespace cpp_redis {

class KjRedisSubscriber {
public:
  //! ctor & dtor
  KjRedisSubscriber();
  ~KjRedisSubscriber();

  //! copy ctor & assignment operator
  KjRedisSubscriber(const KjRedisSubscriber&) = delete;
  KjRedisSubscriber& operator=(const KjRedisSubscriber&) = delete;

public:
  //! handle connection
  typedef std::function<void(KjRedisSubscriber&)> disconnection_handler_t;
  void connect(const std::string& host = "127.0.0.1", std::size_t port = 6379, const disconnection_handler_t& disconnection_handler = nullptr);
  void disconnect();
  bool is_connected();

  //! ability to authenticate on the redis server if necessary
  //! this method should not be called repeatedly as the storage of reply_callback is NOT threadsafe
  //! calling repeatedly auth() is undefined concerning the execution of the associated callbacks
  typedef std::function<void(CRedisReply&)> reply_callback_t;
  KjRedisSubscriber& auth(const std::string& password, const reply_callback_t& reply_callback = nullptr);

  //! subscribe - unsubscribe
  typedef std::function<void(const std::string&, const std::string&)> subscribe_callback_t;
  typedef std::function<void(int64_t)> acknowledgement_callback_t;
  KjRedisSubscriber& subscribe(const std::string& channel, const subscribe_callback_t& callback, const acknowledgement_callback_t& acknowledgement_callback = nullptr);
  KjRedisSubscriber& psubscribe(const std::string& pattern, const subscribe_callback_t& callback, const acknowledgement_callback_t& acknowledgement_callback = nullptr);
  KjRedisSubscriber& unsubscribe(const std::string& channel);
  KjRedisSubscriber& punsubscribe(const std::string& pattern);

  //! commit pipelined transaction
  KjRedisSubscriber& commit();

private:
  struct callback_holder {
    subscribe_callback_t subscribe_callback;
    acknowledgement_callback_t acknowledgement_callback;
  };

private:
  void connection_receive_handler(network::redis_connection&, reply& reply);
  void connection_disconnection_handler(network::redis_connection&);

  void handle_acknowledgement_reply(const std::vector<reply>& reply);
  void handle_subscribe_reply(const std::vector<reply>& reply);
  void handle_psubscribe_reply(const std::vector<reply>& reply);

  void call_acknowledgement_callback(const std::string& channel, const std::map<std::string, callback_holder>& channels, std::mutex& channels_mtx, int64_t nb_chans);

private:
  //! redis connection
  network::KjRedisConnection _client;

  //! (p)subscribed channels and their associated channels
  std::map<std::string, callback_holder> m_subscribed_channels;
  std::map<std::string, callback_holder> m_psubscribed_channels;

  //! disconnection handler
  disconnection_handler_t m_disconnection_handler;

  //! thread safety
  std::mutex m_psubscribed_channels_mutex;
  std::mutex m_subscribed_channels_mutex;

  //! auth reply callback
  reply_callback_t m_auth_reply_callback;
};

} //! cpp_redis
