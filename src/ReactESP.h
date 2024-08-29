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

class ReactESP;

// ESP32 doesn't have the micros64 function defined
#ifdef ESP32
uint64_t ICACHE_RAM_ATTR micros64();
#endif

/**
 * @brief Reactions are code to be called when a given condition is fulfilled
 */
struct ReactionInterface {
  /**
   * @brief Default virtual destructor
   */
  virtual ~ReactionInterface() = default;

  virtual void add(ReactESP* app = nullptr) = 0;
  virtual void remove(ReactESP* app = nullptr) = 0;
  virtual void tick() = 0;
};

/**
 * @brief Reactions are code to be called when a given condition is fulfilled
 */
class Reaction : public ReactionInterface {
 protected:
  const react_callback callback;

 public:
  /**
   * @brief Construct a new Reaction object
   *
   * @param callback Function to be called when the reaction is triggered
   */
  Reaction(react_callback callback) : callback(callback) {}

  // Disabling copy and move semantics
  Reaction(const Reaction&) = delete;
  Reaction(Reaction&&) = delete;
  Reaction& operator=(const Reaction&) = delete;
  Reaction& operator=(Reaction&&) = delete;
};

/**
 * @brief TimedReactions are called based on elapsing of time.
 */
class TimedReaction : public Reaction {
 protected:
  const uint64_t interval;
  uint64_t last_trigger_time;
  bool enabled;
  // A repeat reaction needs to know which app it belongs to
  ReactESP* app_context = nullptr;

 public:
  /**
   * @brief Construct a new Timed Reaction object
   *
   * @param interval Interval or delay for the reaction, in milliseconds
   * @param callback Function to be called when the reaction is triggered
   */
  TimedReaction(uint32_t interval, react_callback callback)
      : Reaction(callback),
        interval((uint64_t)1000 * (uint64_t)interval),
        last_trigger_time(micros64()),
        enabled(true) {}
  /**
   * @brief Construct a new Timed Reaction object
   *
   * @param interval Interval, in microseconds
   * @param callback Function to be called when the reaction is triggered
   */
  TimedReaction(uint64_t interval, react_callback callback)
      : Reaction(callback),
        interval(interval),
        last_trigger_time(micros64()),
        enabled(true) {}

  bool operator<(const TimedReaction& other) const;
  void add(ReactESP* app = nullptr) override;
  void remove(ReactESP* app = nullptr) override;
  uint32_t getTriggerTime() const {
    return (last_trigger_time + interval) / 1000;
  }
  uint64_t getTriggerTimeMicros() const {
    return (last_trigger_time + interval);
  }
  bool isEnabled() const { return enabled; }
};

struct TriggerTimeCompare {
  bool operator()(TimedReaction* a, TimedReaction* b) { return *b < *a; }
};

/**
 * @brief Reaction that is triggered after a certain time delay
 */
class DelayReaction : public TimedReaction {
 public:
  /**
   * @brief Construct a new Delay Reaction object
   *
   * @param delay Delay, in milliseconds
   * @param callback Function to be called after the delay
   */
  DelayReaction(uint32_t delay, react_callback callback);
  /**
   * @brief Construct a new Delay Reaction object
   *
   * @param delay Delay, in microseconds
   * @param callback Function to be called after the delay
   */
  DelayReaction(uint64_t delay, react_callback callback);

  void tick() override;
};

/**
 * @brief Reaction that is triggered repeatedly
 */
class RepeatReaction : public TimedReaction {
 public:
  /**
   * @brief Construct a new Repeat Reaction object
   *
   * @param interval Repetition interval, in milliseconds
   * @param callback Function to be called at every repetition
   */
  RepeatReaction(uint32_t interval, react_callback callback)
      : TimedReaction(interval, callback) {}
  /**
   * @brief Construct a new Repeat Reaction object
   *
   * @param interval Repetition interval, in microseconds
   * @param callback Function to be called at every repetition
   */
  RepeatReaction(uint64_t interval, react_callback callback)
      : TimedReaction(interval, callback) {}

  void tick() override;
};

/**
 * @brief Reactions that are triggered based on something else than time
 */
class UntimedReaction : public Reaction {
 public:
  UntimedReaction(react_callback callback) : Reaction(callback) {}

  void add(ReactESP* app = nullptr) override;
  void remove(ReactESP* app = nullptr) override;
};

/**
 * @brief Reaction that is triggered when there is input available at the given
 *   Arduino Stream
 */
class StreamReaction : public UntimedReaction {
 private:
  Stream& stream;

 public:
  /**
   * @brief Construct a new Stream Reaction object
   *
   * @param stream Stream to monitor
   * @param callback Callback to call for new input
   */
  StreamReaction(Stream& stream, react_callback callback)
      : UntimedReaction(callback), stream(stream) {}

  void tick() override;
};

/**
 * @brief Reaction that is triggered unconditionally at each execution loop
 */
class TickReaction : public UntimedReaction {
 public:
  /**
   * @brief Construct a new Tick Reaction object
   *
   * @param callback Function to be called at each execution loop
   */
  TickReaction(react_callback callback) : UntimedReaction(callback) {}

  void tick() override;
};

/**
 * @brief Reaction that is triggered on an input pin change
 */
class ISRReaction : public Reaction {
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
   * @brief Construct a new ISRReaction object
   *
   * @param pin_number GPIO pin number to which the interrupt is attached
   * @param mode Interrupt mode. One of RISING, FALLING, CHANGE
   * @param callback Interrupt callback. Keep this function short and add the
   * ICACHE_RAM_ATTR attribute.
   */
  ISRReaction(uint8_t pin_number, int mode, react_callback callback)
      : Reaction(callback), pin_number(pin_number), mode(mode) {
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

  void add(ReactESP* app = nullptr) override;
  void remove(ReactESP* app = nullptr) override;
  void tick() override {}
};

///////////////////////////////////////
// ReactESP main class implementation

/**
 * @brief Main class of a ReactESP program
 */
class ReactESP {
  friend class Reaction;
  friend class TimedReaction;
  friend class RepeatReaction;
  friend class UntimedReaction;
  friend class ISRReaction;

 public:
  /**
   * @brief Construct a new ReactESP object.
   *
   * @param singleton If true, set the singleton instance to this object
   */
  ReactESP(bool singleton = true)
      : timed_queue(), untimed_list(), isr_reaction_list(), isr_pending_list() {
    if (singleton) {
      app = this;
    }
  }

  void tick();

  /// Static singleton reference to the instantiated ReactESP object
  static ReactESP* app;

  /**
   * @brief Create a new DelayReaction
   *
   * @param delay Delay, in milliseconds
   * @param callback Callback function
   * @return DelayReaction*
   */
  DelayReaction* onDelay(uint32_t delay, react_callback callback);
  /**
   * @brief Create a new DelayReaction
   *
   * @param delay Delay, in microseconds
   * @param callback Callback function
   * @return DelayReaction*
   */
  DelayReaction* onDelayMicros(uint64_t delay, react_callback callback);
  /**
   * @brief Create a new RepeatReaction
   *
   * @param delay Interval, in milliseconds
   * @param callback Callback function
   * @return RepeatReaction*
   */
  RepeatReaction* onRepeat(uint32_t interval, react_callback callback);
  /**
   * @brief Create a new RepeatReaction
   *
   * @param delay Interval, in microseconds
   * @param callback Callback function
   * @return RepeatReaction*
   */
  RepeatReaction* onRepeatMicros(uint64_t interval, react_callback callback);
  /**
   * @brief Create a new StreamReaction
   *
   * @param stream Arduino Stream object to monitor
   * @param callback Callback function
   * @return StreamReaction*
   */
  StreamReaction* onAvailable(Stream& stream, react_callback callback);
  /**
   * @brief Create a new ISRReaction (interrupt reaction)
   *
   * @param pin_number GPIO pin number
   * @param mode One of CHANGE, RISING, FALLING
   * @param callback Interrupt handler to call. This should be a very simple
   * function, ideally setting a flag variable or incrementing a counter. The
   * function should be defined with ICACHE_RAM_ATTR.
   * @return ISRReaction*
   */
  ISRReaction* onInterrupt(uint8_t pin_number, int mode,
                           react_callback callback);
  /**
   * @brief Create a new TickReaction
   *
   * @param callback Callback function to be called at every loop execution
   * @return TickReaction*
   */
  TickReaction* onTick(react_callback callback);

  /**
   * @brief Remove a reaction from the list of active reactions
   *
   * @param reaction Reaction to remove
   */
  void remove(Reaction* reaction);

 private:
  std::priority_queue<TimedReaction*, std::vector<TimedReaction*>,
                      TriggerTimeCompare>
      timed_queue;
  std::forward_list<UntimedReaction*> untimed_list;
  std::forward_list<ISRReaction*> isr_reaction_list;
  std::forward_list<ISRReaction*> isr_pending_list;

  void tickTimed();
  void tickUntimed();
  void tickISR();
  void add(Reaction* re);
};

}  // namespace reactesp

#endif
