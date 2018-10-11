#include <Arduino.h>
#include <string.h>
#include <FunctionalInterrupt.h>

#include "Reactduino.h"
#include "ReactduinoISR.h"

// ReactionEntry classes define the behaviour of each particular
// Reaction

DelayReactionEntry::DelayReactionEntry(uint32_t interval, react_callback callback) 
        : TimedReactionEntry(interval, callback) {
    this->last_trigger_time = millis();
}

void DelayReactionEntry::tick(Reactduino *app, reaction r_pos) {
    uint32_t elapsed;
    uint32_t now = millis();
    elapsed = now - this->last_trigger_time;
    if (elapsed >= this->interval) {
        this->last_trigger_time = now;
        app->free(r_pos);
        this->callback();
        delete this;
    }
}

void RepeatReactionEntry::tick(Reactduino *app, reaction r_pos) {
    uint32_t elapsed;
    uint32_t now = millis();
    elapsed = now - this->last_trigger_time;
    if (elapsed >= this->interval) {
        this->last_trigger_time = now;
        this->callback();
    }
}

void StreamReactionEntry::tick(Reactduino *app, reaction r_pos) {
    if (stream->available()) {
        this->callback();
    }
}

void TickReactionEntry::tick(Reactduino *app, reaction r_pos) {
    this->callback();
}

void ISRReactionEntry::tick(Reactduino *app, reaction r_pos) {
    if (react_isr_check(this->pin_number)) {
        this->callback();
    }
}

void ISRReactionEntry::disable() {
    detachInterrupt(this->pin_number);
    react_isr_free(this->isr);
}

// Need to define the static variable outside of the class
Reactduino* Reactduino::app = NULL;

void setup(void)
{
    Reactduino::app->setup();
}

void loop(void)
{
    Reactduino::app->tick();
    yield();
}

Reactduino::Reactduino(react_callback cb) : _setup(cb)
{
    app = this;
}

void Reactduino::setup(void)
{
    _setup();
}

void Reactduino::tick(void)
{
    reaction r;

    for (r = 0; r < _top; r++) {
        ReactionEntry* r_entry = _table[r];

        if (r_entry==nullptr) {
            continue;
        }

        r_entry->tick(app, r);
    }
}

reaction Reactduino::onDelay(uint32_t t, react_callback cb) {
    return alloc(new DelayReactionEntry(t, cb));
}

reaction Reactduino::onRepeat(uint32_t t, react_callback cb) {
    return alloc(new RepeatReactionEntry(t, cb));
}

reaction Reactduino::onAvailable(Stream *stream, react_callback cb)
{
    return alloc(new StreamReactionEntry(stream, cb));
}

reaction Reactduino::onInterrupt(uint8_t number, react_callback cb, int mode)
{
    reaction r;
    int8_t isr;

    // Obtain and use ISR to handle the interrupt, see: ReactduinoISR.c/.h

    isr = react_isr_alloc();

    if (isr == INVALID_ISR) {
        return INVALID_REACTION;
    }

    r = alloc(new ISRReactionEntry(number, isr, cb));

    if (r == INVALID_REACTION) {
        react_isr_free(isr);
        return INVALID_REACTION;
    }

    attachInterrupt(number, react_isr_get(isr), mode);

    return r;
}

reaction Reactduino::onPinRising(uint8_t pin, react_callback cb)
{
    return onInterrupt(digitalPinToInterrupt(pin), cb, RISING);
}

reaction Reactduino::onPinFalling(uint8_t pin, react_callback cb)
{
    return onInterrupt(digitalPinToInterrupt(pin), cb, FALLING);
}

reaction Reactduino::onPinChange(uint8_t pin, react_callback cb)
{
    return onInterrupt(digitalPinToInterrupt(pin), cb, CHANGE);
}

reaction Reactduino::onTick(react_callback cb)
{
    return alloc(new TickReactionEntry(cb));
}

ReactionEntry* Reactduino::free(reaction r)
{
    if (r == INVALID_REACTION) {
        return nullptr;
    }

    ReactionEntry *re = _table[r];

    if (re==nullptr) {
        return nullptr;
    }

    re->disable();
    _table[r] = nullptr;

    // Move the top of the stack pointer down if we free from the top
    if (_top == r + 1) {
        _top--;
    }

    return re;
}

reaction Reactduino::alloc(ReactionEntry *re)
{
    reaction r;

    for (r = 0; r < REACTDUINO_MAX_REACTIONS; r++) {
        // If we're at the top of the stak or the allocated flag isn't set
        if (r >= _top || _table[r] == nullptr) {
            _table[r] = re;
            // Reaction is enabled
            //_table[r]->flags = REACTION_FLAG_ENABLED;

            // Move the stack pointer up if we add to the top
            if (r >= _top) {
                _top = r + 1;
            }

            return r;
        }
    }

    return INVALID_REACTION;
}
