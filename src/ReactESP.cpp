#include "ReactESP.h"

#include <Arduino.h>
#include <FunctionalInterrupt.h>
#include <string.h>

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

bool TimedReaction::operator<(const TimedReaction& other) {
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
}

DelayReaction::DelayReaction(uint32_t interval, const react_callback callback)
    : TimedReaction(interval, callback) {
  this->last_trigger_time = micros64();
}

DelayReaction::DelayReaction(uint64_t interval, const react_callback callback)
    : TimedReaction(interval, callback) {
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
  if (stream.available()) {
    this->callback();
  }
}

void TickReaction::tick() { this->callback(); }

#ifdef ESP32
bool ISRReaction::isr_service_installed = false;

void ISRReaction::isr(void* this_ptr) {
  auto* this_ = (ISRReaction*)this_ptr;
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
ReactESP* ReactESP::app = NULL;

void ReactESP::tickTimed() {
  uint64_t now = micros64();
  uint64_t trigger_t;
  TimedReaction* top;

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
    trigger_t = top->getTriggerTimeMicros();
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

DelayReaction* ReactESP::onDelay(const uint32_t t, const react_callback cb) {
  DelayReaction* dre = new DelayReaction(t, cb);
  dre->add(this);
  return dre;
}

DelayReaction* ReactESP::onDelayMicros(const uint64_t t,
                                       const react_callback cb) {
  DelayReaction* dre = new DelayReaction(t, cb);
  dre->add(this);
  return dre;
}

RepeatReaction* ReactESP::onRepeat(const uint32_t t, const react_callback cb) {
  RepeatReaction* rre = new RepeatReaction(t, cb);
  rre->add(this);
  return rre;
}

RepeatReaction* ReactESP::onRepeatMicros(const uint64_t t,
                                         const react_callback cb) {
  RepeatReaction* rre = new RepeatReaction(t, cb);
  rre->add(this);
  return rre;
}

StreamReaction* ReactESP::onAvailable(Stream& stream, const react_callback cb) {
  StreamReaction* sre = new StreamReaction(stream, cb);
  sre->add(this);
  return sre;
}

ISRReaction* ReactESP::onInterrupt(const uint8_t pin_number, int mode,
                                   const react_callback cb) {
  ISRReaction* isrre = new ISRReaction(pin_number, mode, cb);
  isrre->add(this);
  return isrre;
}

TickReaction* ReactESP::onTick(const react_callback cb) {
  TickReaction* tre = new TickReaction(cb);
  tre->add(this);
  return tre;
}

void ReactESP::remove(Reaction* reaction) {
  reaction->remove(this);
}

}  // namespace reactesp
