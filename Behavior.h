// Behavior.h
// The current behavior of an actor: a std::variant of state structs and the
// dispatch around it. Generic on purpose: nothing in here names a state.
//
// Every state provides
//     void enter(Actor&);
//     Next update(Actor&, float dt);        Next = std::optional<StateVariant>
// and may provide
//     Next onHit(Actor&, const HitEvent&);  omitted = the state ignores hits
//
// A state never changes the variant it lives in (that would destroy the
// object whose member function is running). It returns the next state and
// Behavior swaps it in after the visit has returned.
#pragma once

#include <optional>
#include <utility>
#include <variant>

class Actor;
struct HitEvent;

template <typename StateVariant>
class Behavior
{
public:
    using Next = std::optional<StateVariant>;

    // Replace the current state with S built from args, then enter it.
    template <typename S, typename... Args>
    void start(Actor& actor, Args&&... args)
    {
        m_state.template emplace<S>(std::forward<Args>(args)...);
        std::get<S>(m_state).enter(actor);
    }

    // Replace the current state with a ready-made one, then enter it.
    void set(Actor& actor, StateVariant state)
    {
        m_state = std::move(state);
        std::visit([&](auto& s) { s.enter(actor); }, m_state);
    }

    void update(Actor& actor, float dt)
    {
        apply(actor, std::visit([&](auto& s) -> Next { return s.update(actor, dt); }, m_state));
    }

    void onHit(Actor& actor, const HitEvent& hit)
    {
        apply(actor, std::visit([&](auto& s) -> Next
            {
                if constexpr (requires { s.onHit(actor, hit); })
                    return s.onHit(actor, hit);
                else
                    return std::nullopt;
            }, m_state));
    }

    template <typename S>
    bool is() const { return std::holds_alternative<S>(m_state); }

    const char* name() const
    {
        return std::visit([](const auto& s) { return s.name; }, m_state);
    }

private:
    void apply(Actor& actor, Next next)
    {
        if (next)
            set(actor, std::move(*next));
    }

    StateVariant m_state{};   // starts as the first alternative
};
