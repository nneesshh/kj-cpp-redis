//------------------------------------------------------------------------------
//  KjRedisConnection.cpp
//  (C) 2017 n.lee
//------------------------------------------------------------------------------

#include "cpp_redis/logger.hpp"
#include "cpp_redis/KjrError.hpp"
#include "KjRedisSubscriber.hpp"

namespace cpp_redis {

//------------------------------------------------------------------------------
/**

*/
KjRedisSubscriber::KjRedisSubscriber(void)
: m_auth_reply_callback(nullptr) {
  __CPP_REDIS_LOG(debug, "cpp_redis::KjRedisSubscriber created");
}

KjRedisSubscriber::~KjRedisSubscriber(void) {
  _client.disconnect(true);
  __CPP_REDIS_LOG(debug, "cpp_redis::KjRedisSubscriber destroyed");
}

void
KjRedisSubscriber::connect(const std::string& host, std::size_t port, const disconnection_handler_t& client_disconnection_handler) {
  __CPP_REDIS_LOG(debug, "cpp_redis::KjRedisSubscriber attempts to connect");

  auto disconnection_handler = std::bind(&KjRedisSubscriber::connection_disconnection_handler, this, std::placeholders::_1);
  auto receive_handler       = std::bind(&KjRedisSubscriber::connection_receive_handler, this, std::placeholders::_1, std::placeholders::_2);
  _client.connect(host, port, disconnection_handler, receive_handler);

  __CPP_REDIS_LOG(info, "cpp_redis::KjRedisSubscriber connected");

  m_disconnection_handler = client_disconnection_handler;
}

KjRedisSubscriber&
KjRedisSubscriber::auth(const std::string& password, const reply_callback_t& reply_callback) {
  __CPP_REDIS_LOG(debug, "cpp_redis::KjRedisSubscriber attempts to authenticate");

  _client.send({"AUTH", password});
  m_auth_reply_callback = reply_callback;

  __CPP_REDIS_LOG(info, "cpp_redis::KjRedisSubscriber AUTH command sent");

  return *this;
}

void
KjRedisSubscriber::disconnect(void) {
  __CPP_REDIS_LOG(debug, "cpp_redis::KjRedisSubscriber attempts to disconnect");
  _client.disconnect();
  __CPP_REDIS_LOG(info, "cpp_redis::KjRedisSubscriber disconnected");
}

bool
KjRedisSubscriber::is_connected(void) {
  return _client.is_connected();
}

KjRedisSubscriber&
KjRedisSubscriber::subscribe(const std::string& channel, const subscribe_callback_t& callback, const acknowledgement_callback_t& acknowledgement_callback) {
  std::lock_guard<std::mutex> lock(m_subscribed_channels_mutex);

  __CPP_REDIS_LOG(debug, "cpp_redis::KjRedisSubscriber attemps to subscribe to channel " + channel);
  m_subscribed_channels[channel] = {callback, acknowledgement_callback};
  _client.send({"SUBSCRIBE", channel});
  __CPP_REDIS_LOG(info, "cpp_redis::KjRedisSubscriber subscribed to channel " + channel);

  return *this;
}

KjRedisSubscriber&
KjRedisSubscriber::psubscribe(const std::string& pattern, const subscribe_callback_t& callback, const acknowledgement_callback_t& acknowledgement_callback) {
  std::lock_guard<std::mutex> lock(m_psubscribed_channels_mutex);

  __CPP_REDIS_LOG(debug, "cpp_redis::KjRedisSubscriber attemps to psubscribe to channel " + pattern);
  m_psubscribed_channels[pattern] = {callback, acknowledgement_callback};
  _client.send({"PSUBSCRIBE", pattern});
  __CPP_REDIS_LOG(info, "cpp_redis::KjRedisSubscriber psubscribed to channel " + pattern);

  return *this;
}

KjRedisSubscriber&
KjRedisSubscriber::unsubscribe(const std::string& channel) {
  std::lock_guard<std::mutex> lock(m_subscribed_channels_mutex);

  __CPP_REDIS_LOG(debug, "cpp_redis::KjRedisSubscriber attemps to unsubscribe from channel " + channel);
  auto it = m_subscribed_channels.find(channel);
  if (it == m_subscribed_channels.end()) {
    __CPP_REDIS_LOG(debug, "cpp_redis::KjRedisSubscriber was not subscribed to channel " + channel);
    return *this;
  }

  _client.send({"UNSUBSCRIBE", channel});
  m_subscribed_channels.erase(it);
  __CPP_REDIS_LOG(info, "cpp_redis::KjRedisSubscriber unsubscribed from channel " + channel);

  return *this;
}

KjRedisSubscriber&
KjRedisSubscriber::punsubscribe(const std::string& pattern) {
  std::lock_guard<std::mutex> lock(m_psubscribed_channels_mutex);

  __CPP_REDIS_LOG(debug, "cpp_redis::KjRedisSubscriber attemps to punsubscribe from channel " + pattern);
  auto it = m_psubscribed_channels.find(pattern);
  if (it == m_psubscribed_channels.end()) {
    __CPP_REDIS_LOG(debug, "cpp_redis::KjRedisSubscriber was not psubscribed to channel " + pattern);
    return *this;
  }

  _client.send({"PUNSUBSCRIBE", pattern});
  m_psubscribed_channels.erase(it);
  __CPP_REDIS_LOG(info, "cpp_redis::KjRedisSubscriber punsubscribed from channel " + pattern);

  return *this;
}

KjRedisSubscriber&
KjRedisSubscriber::commit(void) {
  try {
    __CPP_REDIS_LOG(debug, "cpp_redis::KjRedisSubscriber attempts to send pipelined commands");
    _client.commit();
    __CPP_REDIS_LOG(info, "cpp_redis::KjRedisSubscriber sent pipelined commands");
  }
  catch (const cpp_redis::redis_error& e) {
    __CPP_REDIS_LOG(error, "cpp_redis::KjRedisSubscriber could not send pipelined commands");
    throw e;
  }

  return *this;
}

void
KjRedisSubscriber::call_acknowledgement_callback(const std::string& channel, const std::map<std::string, callback_holder>& channels, std::mutex& channels_mtx, int64_t nb_chans) {
  std::lock_guard<std::mutex> lock(channels_mtx);

  auto it = channels.find(channel);
  if (it == channels.end())
    return;

  if (it->second.acknowledgement_callback) {
    __CPP_REDIS_LOG(debug, "cpp_redis::KjRedisSubscriber executes acknowledgement callback for channel " + channel);
    it->second.acknowledgement_callback(nb_chans);
  }
}

void
KjRedisSubscriber::handle_acknowledgement_reply(const std::vector<reply>& reply) {
  if (reply.size() != 3)
    return;

  const auto& title    = reply[0];
  const auto& channel  = reply[1];
  const auto& nb_chans = reply[2];

  if (!title.is_string()
      || !channel.is_string()
      || !nb_chans.is_integer())
    return;

  if (title.as_string() == "subscribe")
    call_acknowledgement_callback(channel.as_string(), m_subscribed_channels, m_subscribed_channels_mutex, nb_chans.as_integer());
  else if (title.as_string() == "psubscribe")
    call_acknowledgement_callback(channel.as_string(), m_psubscribed_channels, m_psubscribed_channels_mutex, nb_chans.as_integer());
}

void
KjRedisSubscriber::handle_subscribe_reply(const std::vector<reply>& reply) {
  if (reply.size() != 3)
    return;

  const auto& title   = reply[0];
  const auto& channel = reply[1];
  const auto& message = reply[2];

  if (!title.is_string()
      || !channel.is_string()
      || !message.is_string())
    return;

  if (title.as_string() != "message")
    return;

  std::lock_guard<std::mutex> lock(m_subscribed_channels_mutex);

  auto it = m_subscribed_channels.find(channel.as_string());
  if (it == m_subscribed_channels.end())
    return;

  __CPP_REDIS_LOG(debug, "cpp_redis::KjRedisSubscriber executes subscribe callback for channel " + channel.as_string());
  it->second.subscribe_callback(channel.as_string(), message.as_string());
}

void
KjRedisSubscriber::handle_psubscribe_reply(const std::vector<reply>& reply) {
  if (reply.size() != 4)
    return;

  const auto& title    = reply[0];
  const auto& pchannel = reply[1];
  const auto& channel  = reply[2];
  const auto& message  = reply[3];

  if (!title.is_string()
      || !pchannel.is_string()
      || !channel.is_string()
      || !message.is_string())
    return;

  if (title.as_string() != "pmessage")
    return;

  std::lock_guard<std::mutex> lock(m_psubscribed_channels_mutex);

  auto it = m_psubscribed_channels.find(pchannel.as_string());
  if (it == m_psubscribed_channels.end())
    return;

  __CPP_REDIS_LOG(debug, "cpp_redis::KjRedisSubscriber executes psubscribe callback for channel " + channel.as_string());
  it->second.subscribe_callback(channel.as_string(), message.as_string());
}

void
KjRedisSubscriber::connection_receive_handler(network::redis_connection&, reply& reply) {
  __CPP_REDIS_LOG(info, "cpp_redis::KjRedisSubscriber received reply");

  //! always return an array
  //! otherwise, if auth was defined, this should be the AUTH reply
  //! any other replies from the server are considered as unexepected
  if (!reply.is_array()) {
    if (m_auth_reply_callback) {
      __CPP_REDIS_LOG(debug, "cpp_redis::KjRedisSubscriber executes auth callback");

      m_auth_reply_callback(reply);
      m_auth_reply_callback = nullptr;
    }

    return;
  }

  auto& array = reply.as_array();

  //! Array size of 3 -> SUBSCRIBE if array[2] is a string
  //! Array size of 3 -> AKNOWLEDGEMENT if array[2] is an integer
  //! Array size of 4 -> PSUBSCRIBE
  //! Otherwise -> unexepcted reply
  if (array.size() == 3 && array[2].is_integer())
    handle_acknowledgement_reply(array);
  else if (array.size() == 3 && array[2].is_string())
    handle_subscribe_reply(array);
  else if (array.size() == 4)
    handle_psubscribe_reply(array);
}

void
KjRedisSubscriber::connection_disconnection_handler(network::redis_connection&) {
  __CPP_REDIS_LOG(warn, "cpp_redis::KjRedisSubscriber has been disconnected");

  if (m_disconnection_handler) {
    __CPP_REDIS_LOG(info, "cpp_redis::KjRedisSubscriber calls disconnection handler");
    m_disconnection_handler(*this);
  }
}

} //! cpp_redis
