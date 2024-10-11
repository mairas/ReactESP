#ifndef REACTESP_SRC_EVENT_LOOP_H_
#define REACTESP_SRC_EVENT_LOOP_H_

#include <queue>

#include "events.h"

namespace reactesp {

/**
 * @brief Asynchronous event loop supporting timed (repeating and
 * non-repeating), interrupt and stream events.
 */
class EventLoop {
  friend class Event;
  friend class TimedEvent;
  friend class RepeatEvent;
  friend class UntimedEvent;
  friend class ISREvent;

 public:
  /**
   * @brief Construct a new EventLoop object.
   */
  EventLoop()
      : timed_queue(), untimed_list(), isr_event_list() {
    timed_queue_mutex_ = xSemaphoreCreateRecursiveMutex();
    untimed_list_mutex_ = xSemaphoreCreateRecursiveMutex();
    isr_event_list_mutex_ = xSemaphoreCreateRecursiveMutex();

    // Initialize the mutexes

    xSemaphoreGiveRecursive(timed_queue_mutex_);
    xSemaphoreGiveRecursive(untimed_list_mutex_);
    xSemaphoreGiveRecursive(isr_event_list_mutex_);
  }

  // Disabling copy constructors
  EventLoop(const EventLoop&) = delete;
  EventLoop(EventLoop&&) = delete;

  int getTimedEventQueueSize() { return timed_queue.size(); }
  int getUntimedEventQueueSize() { return untimed_list.size(); }
  int getISREventQueueSize() { return isr_event_list.size(); }
  int getEventQueueSize() {
    return getTimedEventQueueSize() + getUntimedEventQueueSize() +
           getISREventQueueSize();
  }

  uint64_t getTimedEventCount() { return timed_event_counter; }
  uint64_t getUntimedEventCount() { return untimed_event_counter; }
  uint64_t getEventCount() {
    return getTimedEventCount() + getUntimedEventCount();
  }

  uint64_t getTickCount() { return tick_counter; }

  void tick();

  /**
   * @brief Create a new DelayEvent
   *
   * @param delay Delay, in milliseconds
   * @param callback Callback function
   * @return DelayEvent*
   */
  DelayEvent* onDelay(uint32_t delay, react_callback callback);
  /**
   * @brief Create a new DelayEvent
   *
   * @param delay Delay, in microseconds
   * @param callback Callback function
   * @return DelayEvent*
   */
  DelayEvent* onDelayMicros(uint64_t delay, react_callback callback);
  /**
   * @brief Create a new RepeatEvent
   *
   * @param delay Interval, in milliseconds
   * @param callback Callback function
   * @return RepeatEvent*
   */
  RepeatEvent* onRepeat(uint32_t interval, react_callback callback);
  /**
   * @brief Create a new RepeatEvent
   *
   * @param delay Interval, in microseconds
   * @param callback Callback function
   * @return RepeatEvent*
   */
  RepeatEvent* onRepeatMicros(uint64_t interval, react_callback callback);
  /**
   * @brief Create a new StreamEvent
   *
   * @param stream Arduino Stream object to monitor
   * @param callback Callback function
   * @return StreamEvent*
   */
  StreamEvent* onAvailable(Stream& stream, react_callback callback);
  /**
   * @brief Create a new ISREvent (interrupt event)
   *
   * @param pin_number GPIO pin number
   * @param mode One of CHANGE, RISING, FALLING
   * @param callback Interrupt handler to call. This should be a very simple
   * function, ideally setting a flag variable or incrementing a counter. The
   * function should be defined with ICACHE_RAM_ATTR.
   * @return ISREvent*
   */
  ISREvent* onInterrupt(uint8_t pin_number, int mode, react_callback callback);
  /**
   * @brief Create a new TickEvent
   *
   * @param callback Callback function to be called at every loop execution
   * @return TickEvent*
   */
  TickEvent* onTick(react_callback callback);

  void remove(TimedEvent* event);
  void remove(UntimedEvent* event);
  void remove(ISREvent* event);

  /**
   * @brief Remove an event from the list of active events
   *
   * @param event Event to remove
   */
  void remove(Event* event);

 protected:
  // Timed events are stored in a priority queue, sorted by trigger time. It
  // pretty much always suffices to just access the top element of the queue.
  // Element removal is always done by invalidating the element.
  std::priority_queue<TimedEvent*, std::vector<TimedEvent*>, TriggerTimeCompare>
      timed_queue;
  // Untimed events are stored in a vector, which is traversed in order.
  // Elements are rarely removed from the middle of the list, so a vector is
  // acceptable.
  std::vector<UntimedEvent*> untimed_list;
  // ISR events are stored in a vector. The list is traversed or modified
  // infrequently.
  std::vector<ISREvent*> isr_event_list;

  // Semaphores for accessing the above queues and lists
  SemaphoreHandle_t timed_queue_mutex_;
  SemaphoreHandle_t untimed_list_mutex_;
  SemaphoreHandle_t isr_event_list_mutex_;

  uint64_t timed_event_counter = 0;
  uint64_t untimed_event_counter = 0;
  uint64_t tick_counter = 0;

  void tickTimed();
  void tickUntimed();
};

// Provide compatibility aliases for the old naming scheme

using ReactESP = EventLoop;
using TimedReaction = TimedEvent;
using UntimedReaction = UntimedEvent;
using DelayReaction = DelayEvent;
using RepeatReaction = RepeatEvent;
using ISRReaction = ISREvent;
using StreamReaction = StreamEvent;
using TickReaction = TickEvent;

}  // namespace reactesp

#endif  // REACTESP_SRC_EVENT_LOOP_H_
