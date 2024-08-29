#include "ReactESP.h"

#include <Arduino.h>
#include <FunctionalInterrupt.h>

#include <cstring>

namespace reactesp {

/**
 * @brief Return the current time since the device restart in microseconds
 *
 * Returns the time since the device restart. Even though the time
 * is in microseconds, a 64-bit integer is all but guaranteed not to
 * rewrap, ever.
 */
#ifdef ESP32
uint64_t ICACHE_RAM_ATTR micros64() { return esp_timer_get_time(); }
#endif

// Reaction classes define the behaviour of each particular
// Reaction

bool TimedReaction::operator<(const TimedReaction& other) const {
  return (this->last_trigger_time + this->interval) <
         (other.last_trigger_time + other.interval);
}

void TimedReaction::add(ReactESP* app) {
  if (app == nullptr) {
    Serial.println("Got a null pointer in TimedReaction::add");
    app = ReactESP::app;
  }
  app_context = app;
  app->timed_queue.push(this);
}

void TimedReaction::remove(ReactESP* app) {
  this->enabled = false;
  // the object will be deleted when it's popped out of the
  // timer queue
  (void)app; /* unused */
}

DelayReaction::DelayReaction(uint32_t delay, react_callback callback)
    : TimedReaction(delay, callback) {
  this->last_trigger_time = micros64();
}

DelayReaction::DelayReaction(uint64_t delay, react_callback callback)
    : TimedReaction(delay, callback) {
  this->last_trigger_time = micros64();
}

void DelayReaction::tick() {
  this->last_trigger_time = micros64();
  this->callback();
  delete this;
}

void RepeatReaction::tick() {
  auto now = micros64();
  this->last_trigger_time = this->last_trigger_time + this->interval;
  if (this->last_trigger_time + this->interval < now) {
    // we're lagging more than one full interval; reset the time
    this->last_trigger_time = now;
  }
  this->callback();
  app_context->timed_queue.push(this);
}

void UntimedReaction::add(ReactESP* app) {
  if (app == nullptr) {
    app = ReactESP::app;
  }
  app->untimed_list.push_front(this);
}

void UntimedReaction::remove(ReactESP* app) {
  if (app == nullptr) {
    app = ReactESP::app;
  }

  app->untimed_list.remove(this);
  delete this;
}

void StreamReaction::tick() {
  if (0 != stream.available()) {
    this->callback();
  }
}

void TickReaction::tick() { this->callback(); }

#ifdef ESP32
bool ISRReaction::isr_service_installed = false;

void ISRReaction::isr(void* this_ptr) {
  auto* this_ = static_cast<ISRReaction*>(this_ptr);
  this_->callback();
}
#endif

void ISRReaction::add(ReactESP* app) {
  if (app == nullptr) {
    app = ReactESP::app;
  }

#ifdef ESP32
  gpio_isr_handler_add((gpio_num_t)pin_number, ISRReaction::isr, (void*)this);
#elif defined(ESP8266)
  attachInterrupt(digitalPinToInterrupt(pin_number), callback, mode);
#endif
  app->isr_reaction_list.push_front(this);
}

void ISRReaction::remove(ReactESP* app) {
  if (app == nullptr) {
    app = ReactESP::app;
  }

  app->isr_reaction_list.remove(this);
#ifdef ESP32
  gpio_isr_handler_remove((gpio_num_t)pin_number);
#elif defined(ESP8266)
  detachInterrupt(digitalPinToInterrupt(this->pin_number));
#endif
  delete this;
}

// Need to define the static variable outside of the class
ReactESP* ReactESP::app = nullptr;

void ReactESP::tickTimed() {
  const uint64_t now = micros64();
  TimedReaction* top = nullptr;

  while (true) {
    if (timed_queue.empty()) {
      break;
    }
    top = timed_queue.top();
    if (!top->isEnabled()) {
      timed_queue.pop();
      delete top;
      continue;
    }
    const uint64_t trigger_t = top->getTriggerTimeMicros();
    if (now >= trigger_t) {
      timed_queue.pop();
      top->tick();
    } else {
      break;
    }
  }
}

void ReactESP::tickUntimed() {
  for (UntimedReaction* re : this->untimed_list) {
    re->tick();
  }
}

void ReactESP::tick() {
  tickUntimed();
  tickTimed();
}

DelayReaction* ReactESP::onDelay(uint32_t delay, react_callback callback) {
  auto* dre = new DelayReaction(delay, callback);
  dre->add(this);
  return dre;
}

DelayReaction* ReactESP::onDelayMicros(uint64_t delay,
                                       react_callback callback) {
  auto* dre = new DelayReaction(delay, callback);
  dre->add(this);
  return dre;
}

RepeatReaction* ReactESP::onRepeat(uint32_t interval, react_callback callback) {
  auto* rre = new RepeatReaction(interval, callback);
  rre->add(this);
  return rre;
}

RepeatReaction* ReactESP::onRepeatMicros(uint64_t interval,
                                         react_callback callback) {
  auto* rre = new RepeatReaction(interval, callback);
  rre->add(this);
  return rre;
}

StreamReaction* ReactESP::onAvailable(Stream& stream, react_callback callback) {
  auto* sre = new StreamReaction(stream, callback);
  sre->add(this);
  return sre;
}

ISRReaction* ReactESP::onInterrupt(uint8_t pin_number, int mode,
                                   react_callback callback) {
  auto* isrre = new ISRReaction(pin_number, mode, callback);
  isrre->add(this);
  return isrre;
}

TickReaction* ReactESP::onTick(react_callback callback) {
  auto* tre = new TickReaction(callback);
  tre->add(this);
  return tre;
}

void ReactESP::remove(Reaction* reaction) { reaction->remove(this); }

}  // namespace reactesp
