#ifndef REACTESP_H_
#define REACTESP_H_

#include <Arduino.h>

#include <functional>
#include <forward_list>
#include <queue>

typedef std::function<void()> react_callback;

// forward declarations

class ReactESP;

///////////////////////////////////////
// Reaction classes define the reaction behaviour

class Reaction {
protected:
    const react_callback callback;
public:
    Reaction(react_callback callback)
    : callback(callback) {}
    // FIXME: why do these have to be defined?
    virtual void add() = 0;
    virtual void remove() = 0;
    virtual void tick() = 0;
};

class TimedReaction : public Reaction {
protected:
    const uint32_t interval;
    uint32_t last_trigger_time;
    bool enabled;
public:
    TimedReaction(const uint32_t interval, const react_callback callback)
    : Reaction(callback), interval(interval) {
        last_trigger_time = millis();
        enabled = true;
    }
    virtual ~TimedReaction() {}
    bool operator<(const TimedReaction& other);
    void add();
    void remove();
    uint32_t getTriggerTime() { return last_trigger_time + interval; }
    bool isEnabled() { return enabled; }
    virtual void tick() = 0;
};

struct TriggerTimeCompare
{
    bool operator()(TimedReaction* a, TimedReaction* b)
    {
        return *b < *a;
    }
};


class DelayReaction : public TimedReaction {
public:
    DelayReaction(const uint32_t interval, const react_callback callback);
    virtual ~DelayReaction() {}
    void tick();
};

class RepeatReaction: public TimedReaction {
public:
    RepeatReaction(const uint32_t interval, const react_callback callback)
    : TimedReaction(interval, callback) {}
    void tick();
};

class UntimedReaction : public Reaction {
public:
    UntimedReaction(const react_callback callback)
    : Reaction(callback) {}
    virtual ~UntimedReaction() {}
    virtual void add();
    virtual void remove();
    virtual void tick() = 0;
};

class StreamReaction : public UntimedReaction {
private:
    Stream& stream;
public:
    StreamReaction(Stream& stream, const react_callback callback)
    : UntimedReaction(callback), stream(stream) {}
    void tick();
};

class TickReaction : public UntimedReaction {
public:
    TickReaction(const react_callback callback)
    : UntimedReaction(callback) {}
    void tick();
};

class ISRReaction : public Reaction {
private:
    const uint32_t pin_number;
    const int mode;
public:
    ISRReaction(uint32_t pin_number, int mode, const react_callback callback)
    : Reaction(callback), pin_number(pin_number), mode(mode) {}
    virtual ~ISRReaction() {}
    void add();
    void remove();
    void tick();
};


///////////////////////////////////////
// ReactESP main class implementation

class ReactESP
{
    friend class Reaction;
    friend class TimedReaction;
    friend class RepeatReaction;
    friend class UntimedReaction;
    friend class ISRReaction;
public:
    ReactESP(const react_callback cb) : _setup(cb) { app = this; }
    void setup(void) { _setup(); }
    void tick(void);

    // static singleton reference to the instantiated ReactESP object
    static ReactESP* app;

    // Public API
    DelayReaction* onDelay(const uint32_t t, const react_callback cb);
    RepeatReaction* onRepeat(const uint32_t t, const react_callback cb);
    StreamReaction* onAvailable(Stream& stream, const react_callback cb);
    ISRReaction* onInterrupt(const uint8_t pin_number, int mode, const react_callback cb);
    TickReaction* onTick(const react_callback cb);

private:
    const react_callback _setup;
    std::priority_queue<TimedReaction*, std::vector<TimedReaction*>, TriggerTimeCompare> timed_queue;
    std::forward_list<UntimedReaction*> untimed_list;
    std::forward_list<ISRReaction*> isr_reaction_list;
    std::forward_list<ISRReaction*> isr_pending_list;
    void tickTimed();
    void tickUntimed();
    void tickISR();
    void add(Reaction* re);
};

#endif
