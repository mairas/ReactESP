#ifndef REACTESP_H_
#define REACTESP_H_

#include <Arduino.h>

#include <forward_list>
#include <functional>
#include <queue>

namespace reactesp {

using react_callback = std::function<void()>;
using isr_react_callback = void (*)(void*);

// forward declarations

class EventLoop;

/**
 * @brief EventInterface defines the interface for all events
 */
struct EventInterface {
  /**
   * @brief Default virtual destructor
   */
  virtual ~EventInterface() = default;

  virtual void add(EventLoop* event_loop) = 0;
  virtual void remove(EventLoop* event_loop) = 0;
  virtual void tick(EventLoop* event_loop) = 0;
};

/**
 * @brief Events are code to be called when a given condition is fulfilled
 */
class Event : public EventInterface {
 protected:
  const react_callback callback;

 public:
  /**
   * @brief Construct a new Event object
   *
   * @param callback Function to be called when the event is triggered
   */
  Event(react_callback callback) : callback(callback) {}

  // Disabling copy and move semantics
  Event(const Event&) = delete;
  Event(Event&&) = delete;
  Event& operator=(const Event&) = delete;
  Event& operator=(Event&&) = delete;
};

/**
 * @brief TimedEvents are called based on elapsing of time.
 */
class TimedEvent : public Event {
 protected:
  const uint64_t interval;
  uint64_t last_trigger_time;
  bool enabled;

 public:
  /**
   * @brief Construct a new Timed Event object
   *
   * @param interval Interval or delay for the event, in milliseconds
   * @param callback Function to be called when the event is triggered
   */
  TimedEvent(uint32_t interval, react_callback callback)
      : Event(callback),
        interval((uint64_t)1000 * (uint64_t)interval),
        last_trigger_time(micros()),
        enabled(true) {}
  /**
   * @brief Construct a new Timed Event object
   *
   * @param interval Interval, in microseconds
   * @param callback Function to be called when the event is triggered
   */
  TimedEvent(uint64_t interval, react_callback callback)
      : Event(callback),
        interval(interval),
        last_trigger_time(micros()),
        enabled(true) {}

  bool operator<(const TimedEvent& other) const;
  void add(EventLoop* event_loop) override;
  void remove(EventLoop* event_loop) override;
  uint32_t getTriggerTime() const {
    return (last_trigger_time + interval) / 1000;
  }
  uint64_t getTriggerTimeMicros() const {
    return (last_trigger_time + interval);
  }
  bool isEnabled() const { return enabled; }
};

struct TriggerTimeCompare {
  bool operator()(TimedEvent* a, TimedEvent* b) { return *b < *a; }
};

/**
 * @brief Event that is triggered after a certain time delay
 */
class DelayEvent : public TimedEvent {
 public:
  /**
   * @brief Construct a new Delay Event object
   *
   * @param delay Delay, in milliseconds
   * @param callback Function to be called after the delay
   */
  DelayEvent(uint32_t delay, react_callback callback);
  /**
   * @brief Construct a new Delay Event object
   *
   * @param delay Delay, in microseconds
   * @param callback Function to be called after the delay
   */
  DelayEvent(uint64_t delay, react_callback callback);

  void tick(EventLoop* event_loop) override;
};

/**
 * @brief Event that is triggered repeatedly
 */
class RepeatEvent : public TimedEvent {
 public:
  /**
   * @brief Construct a new Repeat Event object
   *
   * @param interval Repetition interval, in milliseconds
   * @param callback Function to be called at every repetition
   */
  RepeatEvent(uint32_t interval, react_callback callback)
      : TimedEvent(interval, callback) {}
  /**
   * @brief Construct a new Repeat Event object
   *
   * @param interval Repetition interval, in microseconds
   * @param callback Function to be called at every repetition
   */
  RepeatEvent(uint64_t interval, react_callback callback)
      : TimedEvent(interval, callback) {}

  void tick(EventLoop* event_loop) override;
};

/**
 * @brief Events that are triggered based on something else than time
 */
class UntimedEvent : public Event {
 public:
  UntimedEvent(react_callback callback) : Event(callback) {}

  void add(EventLoop* event_loop) override;
  void remove(EventLoop* event_loop) override;
};

/**
 * @brief Event that is triggered when there is input available at the given
 *   Arduino Stream
 */
class StreamEvent : public UntimedEvent {
 private:
  Stream& stream;

 public:
  /**
   * @brief Construct a new Stream Event object
   *
   * @param stream Stream to monitor
   * @param callback Callback to call for new input
   */
  StreamEvent(Stream& stream, react_callback callback)
      : UntimedEvent(callback), stream(stream) {}

  void tick(EventLoop* event_loop) override;
};

/**
 * @brief Event that is triggered unconditionally at each execution loop
 */
class TickEvent : public UntimedEvent {
 public:
  /**
   * @brief Construct a new Tick Event object
   *
   * @param callback Function to be called at each execution loop
   */
  TickEvent(react_callback callback) : UntimedEvent(callback) {}

  void tick(EventLoop* event_loop) override;
};

/**
 * @brief Event that is triggered on an input pin change
 */
class ISREvent : public Event {
 private:
  const uint8_t pin_number;
  const int mode;
#ifdef ESP32
  // set to true once gpio_install_isr_service is called
  static bool isr_service_installed;
  static void isr(void* this_ptr);
#endif

 public:
  /**
   * @brief Construct a new ISREvent object
   *
   * @param pin_number GPIO pin number to which the interrupt is attached
   * @param mode Interrupt mode. One of RISING, FALLING, CHANGE
   * @param callback Interrupt callback. Keep this function short and add the
   * ICACHE_RAM_ATTR attribute.
   */
  ISREvent(uint8_t pin_number, int mode, react_callback callback)
      : Event(callback), pin_number(pin_number), mode(mode) {
#ifdef ESP32
    gpio_int_type_t intr_type;
    switch (mode) {
      case RISING:
        intr_type = GPIO_INTR_POSEDGE;
        break;
      case FALLING:
        intr_type = GPIO_INTR_NEGEDGE;
        break;
      case CHANGE:
        intr_type = GPIO_INTR_ANYEDGE;
        break;
      default:
        intr_type = GPIO_INTR_DISABLE;
        break;
    }
    // configure the IO pin
    gpio_set_intr_type((gpio_num_t)pin_number, intr_type);

    if (!isr_service_installed) {
      isr_service_installed = true;
      gpio_install_isr_service(ESP_INTR_FLAG_LOWMED);
    }
#endif
  }

  void add(EventLoop* event_loop) override;
  void remove(EventLoop* event_loop) override;
  void tick(EventLoop* event_loop) override {}
};

///////////////////////////////////////
// EventLoop main event loop implementation

/**
 * @brief Main event loop of a EventLoop program
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
      : timed_queue(), untimed_list(), isr_event_list(), isr_pending_list() {
  }

  // Disabling copy and move semantics
  EventLoop(const EventLoop&) = delete;
  EventLoop(EventLoop&&) = delete;
  EventLoop& operator=(const EventLoop&) = delete;
  EventLoop& operator=(EventLoop&&) = delete;

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
  ISREvent* onInterrupt(uint8_t pin_number, int mode,
                           react_callback callback);
  /**
   * @brief Create a new TickEvent
   *
   * @param callback Callback function to be called at every loop execution
   * @return TickEvent*
   */
  TickEvent* onTick(react_callback callback);

  /**
   * @brief Remove a event from the list of active events
   *
   * @param event Event to remove
   */
  void remove(Event* event);

 private:
  std::priority_queue<TimedEvent*, std::vector<TimedEvent*>,
                      TriggerTimeCompare>
      timed_queue;
  std::forward_list<UntimedEvent*> untimed_list;
  std::forward_list<ISREvent*> isr_event_list;
  std::forward_list<ISREvent*> isr_pending_list;

  void tickTimed();
  void tickUntimed();
  void tickISR();
  void add(Event* re);
};

}  // namespace reactesp

#endif
