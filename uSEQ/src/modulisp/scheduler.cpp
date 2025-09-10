#include "scheduler.h"
#include <algorithm>

void Scheduler::schedule(const String& id, const Value& ast, size_t period)
{
    // Remove existing item with same id if it exists
    unschedule(id);

    // Add new scheduled item
    ScheduledItem item;
    item.id       = id;
    item.ast      = ast;
    item.period   = period;
    item.last_run = 0;
    m_scheduled_items.push_back(item);
}

bool Scheduler::unschedule(const String& id)
{
    auto it =
        std::remove_if(m_scheduled_items.begin(), m_scheduled_items.end(),
                       [&id](const ScheduledItem& item) { return item.id == id; });

    if (it != m_scheduled_items.end())
    {
        m_scheduled_items.erase(it, m_scheduled_items.end());
        return true;
    }
    return false;
}

std::vector<ScheduledItem*> Scheduler::get_items_to_run(size_t current_time)
{
    std::vector<ScheduledItem*> items_to_run;

    for (auto& item : m_scheduled_items)
    {
        if (current_time - item.last_run >= item.period)
        {
            items_to_run.push_back(&item);
            item.last_run = current_time;
        }
    }

    return items_to_run;
}