#pragma once

#include <QPointF>
#include <vector>

struct MouseImpulse {
    QPointF pos;
    QPointF vel;
    float strength;
};

class MouseInput
{
public:
    void push(const QPointF &pos, const QPointF &vel, float strength = 1.0f)
    {
        m_events.push_back({pos, vel, strength});
    }

    std::vector<MouseImpulse> take()
    {
        auto out = m_events;
        m_events.clear();
        return out;
    }

private:
    std::vector<MouseImpulse> m_events;
};
