/*
 * node-rdkafka - Node.js wrapper for RdKafka C/C++ library
 * Copyright (c) 2016 Blizzard Entertainment
 *
 * This software may be modified and distributed under the terms
 * of the MIT license.  See the LICENSE.txt file for details.
 */

#ifndef SRC_CALLBACKS_H_
#define SRC_CALLBACKS_H_

#include <uv.h>
#include <nan.h>

#include <vector>

#include "deps/librdkafka/src-cpp/rdkafkacpp.h"
#include "src/common.h"
#include "src/message.h"

typedef Nan::Persistent<v8::Function,
  Nan::CopyablePersistentTraits<v8::Function> > PersistentCopyableFunction;
typedef std::vector<PersistentCopyableFunction> CopyableFunctionList;

namespace NodeKafka {

class Consumer;

namespace Callbacks {

class Dispatcher {
 public:
  Dispatcher();
  ~Dispatcher();
  void Dispatch(const int, v8::Local<v8::Value> []);
  void AddCallback(v8::Local<v8::Function>);
  bool HasCallbacks();
  virtual void Flush() = 0;
  void Execute();
 protected:
  std::vector<v8::Persistent<v8::Function, v8::CopyablePersistentTraits<v8::Function> > > callbacks;  // NOLINT

  uv_mutex_t async_lock;
 private:
  NAN_INLINE static NAUV_WORK_CB(AsyncMessage_) {
     Dispatcher *dispatcher =
            static_cast<Dispatcher*>(async->data);
     dispatcher->Flush();
  }

  uv_async_t *async;
};

struct event_t {
  RdKafka::Event::Type type;
  std::string message;

  RdKafka::Event::Severity severity;
  std::string fac;

  std::string broker_name;
  int throttle_time;
  int broker_id;

  explicit event_t(const RdKafka::Event &);
  ~event_t();
};

class EventDispatcher : public Dispatcher {
 public:
  EventDispatcher();
  ~EventDispatcher();
  void Add(const event_t &);
  void Flush();
 protected:
  std::vector<event_t> events;
};

class Event : public RdKafka::EventCb {
 public:
  Event();
  ~Event();
  void event_cb(RdKafka::Event&);
  EventDispatcher dispatcher;
};

class ConsumeDispatcher : public Dispatcher {
 public:
  ConsumeDispatcher();
  ~ConsumeDispatcher();
  void Add(NodeKafka::Message *);
  void Flush();
 protected:
  std::vector<NodeKafka::Message *> events;
};

class Consume : public RdKafka::ConsumeCb {
 public:
  Consume();
  ~Consume();
  void consume_cb(RdKafka::Message &, void *);
  ConsumeDispatcher dispatcher;
};

struct delivery_report_t {
  // If it is an error these will be set
  bool is_error;
  std::string error_string;
  RdKafka::ErrorCode error_code;

  // If it is not
  std::string topic_name;
  int32_t partition;
  int64_t offset;
  std::string key;

  explicit delivery_report_t(RdKafka::Message &);
  ~delivery_report_t();
};

class DeliveryReportDispatcher : public Dispatcher {
 public:
  DeliveryReportDispatcher();
  ~DeliveryReportDispatcher();
  void Flush();
  void Add(const delivery_report_t &);
 protected:
  std::vector<delivery_report_t> events;
};

class Delivery : public RdKafka::DeliveryReportCb {
 public:
  Delivery();
  ~Delivery();
  void dr_cb(RdKafka::Message&);
  DeliveryReportDispatcher dispatcher;
};

// Rebalance dispatcher

struct rebalance_event_t {
  RdKafka::ErrorCode err;
  std::vector<RdKafka::TopicPartition*> partitions;

  rebalance_event_t(RdKafka::ErrorCode _err,
        std::vector<RdKafka::TopicPartition*> _partitions):
        err(_err),
        partitions(_partitions) {}
};

class RebalanceDispatcher : public Dispatcher {
 public:
  RebalanceDispatcher();
  ~RebalanceDispatcher();
  void Add(const rebalance_event_t &);
  void Flush();
 protected:
  std::vector<rebalance_event_t> events;
};

class Rebalance : public RdKafka::RebalanceCb {
 public:
  explicit Rebalance(NodeKafka::Consumer* that);
  ~Rebalance();
  // NAN_DISALLOW_ASSIGN_COPY_MOVE?
  NodeKafka::Consumer* const that_;

  void rebalance_cb(RdKafka::KafkaConsumer *, RdKafka::ErrorCode,
    std::vector<RdKafka::TopicPartition*> &);
  RebalanceDispatcher dispatcher;
 private:
  int eof_cnt;
};

class Partitioner : public RdKafka::PartitionerCb {
 public:
  Partitioner();
  ~Partitioner();
  int32_t partitioner_cb( const RdKafka::Topic*, const std::string*, int32_t, void*);  // NOLINT
  Nan::Callback callback;  // NOLINT
  void SetCallback(v8::Local<v8::Function>);
 private:
  static unsigned int djb_hash(const char*, size_t);
  static unsigned int random(const RdKafka::Topic*, int32_t);
};

}  // namespace Callbacks

}  // namespace NodeKafka

#endif  // SRC_CALLBACKS_H_
