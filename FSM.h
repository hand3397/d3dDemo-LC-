#pragma once

template<typename Owner>
class State;

template<typename Owner>
class FSM
{
public:
    FSM(Owner* owner) : owner_(owner), current_(nullptr), previous_(nullptr) {}

    void Update()
    {
        if (current_) 
            current_->Update(owner_, *this);
    }

    void Change(State<Owner>* next)
    {
        if (current_ == next) return;

        previous_ = current_;

        if (current_) current_->Exit(owner_);
        current_ = next;
        if (current_) current_->Enter(owner_);
    }

private:
    Owner* owner_;
    State<Owner>* current_;
    State<Owner>* previous_;
};

template<typename Owner>
class State
{
public:
    virtual ~State() {}
    virtual void Enter(Owner* owner) {}
    virtual void Update(Owner* owner, FSM<Owner>& fsm) {}
    virtual void Exit(Owner* owner) {}
};

