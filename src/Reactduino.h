#ifndef REACTDUINO_H_
#define REACTDUINO_H_

#include <Arduino.h>
#include <stdint.h>

#include <functional>

#define INVALID_REACTION -1

#ifndef REACTDUINO_MAX_REACTIONS
#define REACTDUINO_MAX_REACTIONS 50
#endif

typedef std::function<void()> react_callback;
typedef int32_t reaction;

#define REACTION_FLAG_ENABLED 0x40
#define REACTION_TYPE_MASK 0x3F

#define REACTION_TYPE_DELAY 0
#define REACTION_TYPE_REPEAT 1
#define REACTION_TYPE_STREAM 2
#define REACTION_TYPE_INTERRUPT 3
#define REACTION_TYPE_TICK 4

#define REACTION_TYPE(x) ((x) & REACTION_TYPE_MASK)

#define INPUT_STATE_HIGH    HIGH
#define INPUT_STATE_LOW     LOW
#define INPUT_STATE_ANY     0xFF
#define INPUT_STATE_UNSET   0xFE

// typedef struct reaction_entry_t_ {
//     uint8_t flags;
//     void *ptr;
//     uint32_t param1, param2;
//     react_callback cb;
// } reaction_entry_t;

// forward declarations

class Reactduino;

///////////////////////////////////////
// ReactionEntry classes define the reaction behaviour

class ReactionEntry {
protected:
    react_callback callback;
public:
    ReactionEntry(react_callback callback)
    : callback(callback) {}
    virtual void disable() {}
    uint8_t flags;
    virtual void tick(Reactduino *app, reaction r_pos) {}
};

class TimedReactionEntry : public ReactionEntry {
protected:
    uint32_t interval;
    uint32_t last_trigger_time;
public:
    TimedReactionEntry(uint32_t interval, react_callback callback) 
    : interval(interval), ReactionEntry(callback) {}
};

class DelayReactionEntry : public TimedReactionEntry {
public:
    DelayReactionEntry(uint32_t interval, react_callback callback); 
    void tick(Reactduino *app, reaction r_pos);
};

class RepeatReactionEntry: public TimedReactionEntry {
public:
    RepeatReactionEntry(uint32_t interval, react_callback callback) 
    : TimedReactionEntry(interval, callback) {}
    void tick(Reactduino *app, reaction r_pos);
};

class UntimedReactionEntry : public ReactionEntry {
public:
    UntimedReactionEntry(react_callback callback)
    : ReactionEntry(callback) {}
};

class StreamReactionEntry : public UntimedReactionEntry {
private:
    Stream *stream;
public:
    StreamReactionEntry(Stream *stream, react_callback callback)
    : stream(stream), UntimedReactionEntry(callback) {}
    void tick(Reactduino *app, reaction r_pos);
};

class TickReactionEntry : public UntimedReactionEntry {
public:
    TickReactionEntry(react_callback callback)
    : UntimedReactionEntry(callback) {}
    void tick(Reactduino *app, reaction r_pos);
};

class ISRReactionEntry : public UntimedReactionEntry {
private:
    uint32_t pin_number;
public:
    ISRReactionEntry(uint32_t pin_number, int8_t isr, react_callback callback)
    : pin_number(pin_number), isr(isr), UntimedReactionEntry(callback) {}
    int8_t isr;
    void disable();
    void tick(Reactduino *app, reaction r_pos);
};


///////////////////////////////////////
// Reactduino main class implementation

class Reactduino
{
public:
    Reactduino(react_callback cb);
    void setup(void);
    void tick(void);

    // static singleton reference to the instantiated Reactduino object
    static Reactduino* app;

    // Public API
    reaction onDelay(uint32_t t, react_callback cb);
    reaction onRepeat(uint32_t t, react_callback cb);
    reaction onAvailable(Stream *stream, react_callback cb);
    reaction onInterrupt(uint8_t number, react_callback cb, int mode);
    reaction onPinRising(uint8_t pin, react_callback cb);
    reaction onPinFalling(uint8_t pin, react_callback cb);
    reaction onPinChange(uint8_t pin, react_callback cb);
    reaction onTick(react_callback cb);

    ReactionEntry* free(reaction r);

private:
    react_callback _setup;
    ReactionEntry *_table[REACTDUINO_MAX_REACTIONS];
    reaction _top = 0;

    reaction alloc(ReactionEntry *re);

};

#endif
