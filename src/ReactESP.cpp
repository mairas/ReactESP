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

// Event classes define the behaviour of each particular
// Event

bool TimedEvent::operator<(const TimedEvent& other) const {
  return (this->last_trigger_time + this->interval) <
         (other.last_trigger_time + other.interval);
}

void TimedEvent::add(EventLoop* event_loop) {
  xSemaphoreTakeRecursive(event_loop->timed_queue_mutex_, portMAX_DELAY);
  event_loop->timed_queue.push(this);
  xSemaphoreGiveRecursive(event_loop->timed_queue_mutex_);
}

void TimedEvent::remove(EventLoop* event_loop) {
  this->enabled = false;
  // the object will be deleted when it's popped out of the
  // timer queue
}

DelayEvent::DelayEvent(uint32_t delay, react_callback callback)
    : TimedEvent(delay, callback) {
  this->last_trigger_time = micros64();
}

DelayEvent::DelayEvent(uint64_t delay, react_callback callback)
    : TimedEvent(delay, callback) {
  this->last_trigger_time = micros64();
}

void DelayEvent::tick(EventLoop* event_loop) {
  this->last_trigger_time = micros64();
  this->callback();
  delete this;
}

void RepeatEvent::tick(EventLoop* event_loop) {
  xSemaphoreTakeRecursive(event_loop->timed_queue_mutex_, portMAX_DELAY);
  auto now = micros64();
  this->last_trigger_time = this->last_trigger_time + this->interval;
  if (this->last_trigger_time + this->interval < now) {
    // we're lagging more than one full interval; reset the time
    this->last_trigger_time = now;
  }
  this->callback();
  event_loop->timed_queue.push(this);
  xSemaphoreGiveRecursive(event_loop->timed_queue_mutex_);
}

void UntimedEvent::add(EventLoop* event_loop) {
  xSemaphoreTakeRecursive(event_loop->untimed_list_mutex_, portMAX_DELAY);
  event_loop->untimed_list.push_front(this);
  xSemaphoreGiveRecursive(event_loop->untimed_list_mutex_);
}

void UntimedEvent::remove(EventLoop* event_loop) {
  xSemaphoreTakeRecursive(event_loop->untimed_list_mutex_, portMAX_DELAY);
  event_loop->untimed_list.remove(this);
  delete this;
  xSemaphoreGiveRecursive(event_loop->untimed_list_mutex_);
}

void StreamEvent::tick(EventLoop* event_loop) {
  if (0 != stream.available()) {
    this->callback();
  }
}

void TickEvent::tick(EventLoop* event_loop) { this->callback(); }

#ifdef ESP32
bool ISREvent::isr_service_installed = false;

void ISREvent::isr(void* this_ptr) {
  auto* this_ = static_cast<ISREvent*>(this_ptr);
  this_->callback();
}
#endif

void ISREvent::add(EventLoop* event_loop) {
  xSemaphoreTakeRecursive(event_loop->isr_event_list_mutex_, portMAX_DELAY);
#ifdef ESP32
  gpio_isr_handler_add((gpio_num_t)pin_number, ISREvent::isr, (void*)this);
#elif defined(ESP8266)
  attachInterrupt(digitalPinToInterrupt(pin_number), callback, mode);
#endif
  event_loop->isr_event_list.push_front(this);
  xSemaphoreGiveRecursive(event_loop->isr_event_list_mutex_);
}

void ISREvent::remove(EventLoop* event_loop) {
  xSemaphoreTakeRecursive(event_loop->isr_event_list_mutex_, portMAX_DELAY);
  event_loop->isr_event_list.remove(this);
#ifdef ESP32
  gpio_isr_handler_remove((gpio_num_t)pin_number);
#elif defined(ESP8266)
  detachInterrupt(digitalPinToInterrupt(this->pin_number));
#endif
  delete this;
  xSemaphoreGiveRecursive(event_loop->isr_event_list_mutex_);
}

void EventLoop::tickTimed() {
  xSemaphoreTakeRecursive(timed_queue_mutex_, portMAX_DELAY);
  const uint64_t now = micros64();
  TimedEvent* top = nullptr;

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
      top->tick(this);
    } else {
      break;
    }
  }
  xSemaphoreGiveRecursive(timed_queue_mutex_);
}

void EventLoop::tickUntimed() {
  xSemaphoreTakeRecursive(untimed_list_mutex_, portMAX_DELAY);
  for (UntimedEvent* re : this->untimed_list) {
    re->tick(this);
  }
  xSemaphoreGiveRecursive(untimed_list_mutex_);
}

void EventLoop::tick() {
  tickUntimed();
  tickTimed();
}

DelayEvent* EventLoop::onDelay(uint32_t delay, react_callback callback) {
  auto* dre = new DelayEvent(delay, callback);
  dre->add(this);
  return dre;
}

DelayEvent* EventLoop::onDelayMicros(uint64_t delay,
                                       react_callback callback) {
  auto* dre = new DelayEvent(delay, callback);
  dre->add(this);
  return dre;
}

RepeatEvent* EventLoop::onRepeat(uint32_t interval, react_callback callback) {
  auto* rre = new RepeatEvent(interval, callback);
  rre->add(this);
  return rre;
}

RepeatEvent* EventLoop::onRepeatMicros(uint64_t interval,
                                         react_callback callback) {
  auto* rre = new RepeatEvent(interval, callback);
  rre->add(this);
  return rre;
}

StreamEvent* EventLoop::onAvailable(Stream& stream, react_callback callback) {
  auto* sre = new StreamEvent(stream, callback);
  sre->add(this);
  return sre;
}

ISREvent* EventLoop::onInterrupt(uint8_t pin_number, int mode,
                                   react_callback callback) {
  auto* isrre = new ISREvent(pin_number, mode, callback);
  isrre->add(this);
  return isrre;
}

TickEvent* EventLoop::onTick(react_callback callback) {
  auto* tre = new TickEvent(callback);
  tre->add(this);
  return tre;
}

void EventLoop::remove(Event* event) { event->remove(this); }

}  // namespace reactesp
