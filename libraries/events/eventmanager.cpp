#include "eventmanager.h"

using namespace EVENTSLIB;

idNum EventManager::m_iEventIdCounter(0);
idNum EventManager::m_iGroupIdCounter(0);

//=============================================================================================================

EventManager::EventManager()
    : m_pSharedMemManager(std::make_unique<EVENTSINTERNAL::EventSharedMemManager>(this))
{

}

//=============================================================================================================

std::optional<Event> EventManager::getEvent(idNum eventId) const
{
    auto eventInt = findEventINT(eventId);
    if(eventInt.has_value())
    {
        return Event(eventInt.value()->second);
    }
    return {};
}

//=============================================================================================================

std::optional<std::multimap<const int, EVENTSINTERNAL::EventINT>::const_iterator>
EventManager::findEventINT(idNum eventId) const
{
    int sample = m_MapIdToSample.at(eventId);
    auto eventsRange = m_EventsListBySample.equal_range(sample);
    for(auto e = eventsRange.first; e != eventsRange.second; ++e)
    {
        if( e->second.getId() == eventId)
        {
            return e;
        }
    }
    return {};
}

//=============================================================================================================

std::unique_ptr<std::vector<Event> >
EventManager::getEvents(const std::vector<idNum> eventIds) const
{
    auto pEventsList(allocateOutputContainer<Event>(eventIds.size()));
    for (const auto& id: eventIds)
    {
        auto event = getEvent(id);
        if(event.has_value())
        {
            pEventsList->push_back(event.value());
        }
    }
    return pEventsList;
}

//=============================================================================================================

std::unique_ptr<std::vector<Event> > EventManager::getAllEvents() const
{
    auto pEventsList(allocateOutputContainer<Event>(getNumEvents()));
    for(auto& e: m_EventsListBySample)
    {
        pEventsList->emplace_back(Event(e.second));
    }
    return pEventsList;
}

//=============================================================================================================

std::unique_ptr<std::vector<Event> > EventManager::getEventsInSample(int sample) const
{
    int numEventsInSample = m_EventsListBySample.count(sample);
    auto pEventsList(allocateOutputContainer<Event>(numEventsInSample));

    auto eventsRange = m_EventsListBySample.equal_range(sample);
    for(auto e = eventsRange.first; e != eventsRange.second; e++)
    {
        pEventsList->emplace_back(Event(e->second));
    }
    return pEventsList;
}

//=============================================================================================================

std::unique_ptr<std::vector<Event> >
EventManager::getEventsBetween(int sampleStart, int sampleEnd) const
{
    int memoryHint = ((sampleEnd-sampleStart)/1000)+2;
    auto pEventsList(allocateOutputContainer<Event>(memoryHint));

    auto eventStart = m_EventsListBySample.lower_bound(sampleStart);
    auto eventEnd = m_EventsListBySample.upper_bound(sampleEnd);

    for(auto e = eventStart; e != eventEnd; e++)
    {
        pEventsList->emplace_back(Event(e->second));
    }
    return pEventsList;
}

//=============================================================================================================

std::unique_ptr<std::vector<Event> >
EventManager::getEventsBetween(int sampleStart, int sampleEnd, idNum groupId) const
{
    int memoryHint = ((sampleEnd-sampleStart)/1000)+2;
    auto pEventsList(allocateOutputContainer<Event>(memoryHint));

    auto eventStart = m_EventsListBySample.lower_bound(sampleStart);
    auto eventEnd = m_EventsListBySample.upper_bound(sampleEnd);

    for(auto e = eventStart; e != eventEnd; e++)
    {
        if(e->second.getGroupId() == groupId)
        {
            pEventsList->emplace_back(Event(e->second));
        }
    }
    return pEventsList;
}

//=============================================================================================================

std::unique_ptr<std::vector<Event> >
EventManager::getEventsBetween(int sampleStart, int sampleEnd, const std::vector<idNum>& groupIdsList) const
{
    int memoryHint = (sampleEnd-sampleStart)/200;
    auto pEventsList(allocateOutputContainer<Event>(memoryHint));


    auto eventStart = m_EventsListBySample.lower_bound(sampleStart);
    auto eventEnd = m_EventsListBySample.upper_bound(sampleEnd);

    for(auto e = eventStart; e != eventEnd; e++)
    {
        for(auto& groupId: groupIdsList)
        {
            if(e->second.getGroupId() == groupId)
            {
                pEventsList->emplace_back(Event(e->second));
            }
        }
    }
    return pEventsList;
}

//=============================================================================================================

std::unique_ptr<std::vector<Event> >
EventManager::getEventsInGroup(const idNum groupId) const
{
    auto pEventsList(allocateOutputContainer<Event>());

    for(const auto& e: m_EventsListBySample)
    {
        if(e.second.getGroupId() == groupId)
        {
            pEventsList->emplace_back(Event(e.second));
        }
    }
    return pEventsList;
}

//=============================================================================================================

idNum EventManager::generateNewEventId() const
{
    return ++m_iEventIdCounter;
}

//=============================================================================================================

idNum EventManager::generateNewGroupId() const
{
    return ++m_iGroupIdCounter;
}

//=============================================================================================================

size_t EventManager::getNumEvents() const
{
    return m_EventsListBySample.size();
}

//=============================================================================================================

Event EventManager::addEvent(int sample, idNum groupId)
{
    EVENTSINTERNAL::EventINT newEvent(generateNewEventId(), sample, groupId);
    insertEvent(newEvent);

    if(m_pSharedMemManager->isInit())
    {
        m_pSharedMemManager->addEvent(sample);
    }

    return Event(newEvent);
}

//=============================================================================================================

bool EventManager::moveEvent(idNum eventId, int newSample)
{
    bool status(false);
    auto event = findEventINT(eventId);
    if(event.has_value())
    {
        EVENTSINTERNAL::EventINT newEvent(event.value()->second);
        newEvent.setSample(newSample);
        deleteEvent(eventId);
        insertEvent(newEvent);
        status = true;
    }
    return status;
}

//=============================================================================================================

bool EventManager::deleteEvent(idNum eventId) noexcept
{
    bool status(false);
    auto event = findEventINT(eventId);
    if(event.has_value())
    {
        m_EventsListBySample.erase(event.value());
        m_MapIdToSample.erase(eventId);

        if(m_pSharedMemManager->isInit())
        {
            m_pSharedMemManager->deleteEvent(event.value()->second.getSample());
        }

        status = true;
    }
    return status;
}

//=============================================================================================================

bool EventManager::deleteEvents(const std::vector<idNum>& eventIds)
{
    bool status(eventIds.size());
    for(const auto& id: eventIds)
    {
        status = status && deleteEvent(id);
    }
    return status;
}

//=============================================================================================================

bool EventManager::deleteEvents(std::unique_ptr<std::vector<Event> > eventIds)
{
    bool status(eventIds->size());
    for(auto e: *eventIds){
        status = status && deleteEvent(e.id);
    }
    return status;
}

//=============================================================================================================

bool EventManager::deleteEventsInGroup(idNum groupId)
{
    std::vector<idNum> idList;
    for(auto& e: m_EventsListBySample)
    {
        if(e.second.getGroupId() == groupId)
        {
            idList.emplace_back(e.second.getId());
        }
    }
    return deleteEvents(idList);
}

//=============================================================================================================

void EventManager::insertEvent(const EVENTSINTERNAL::EventINT& e)
{
    m_EventsListBySample.emplace(std::make_pair(e.getSample(),e));
    m_MapIdToSample[e.getId()] = e.getSample();
}

//=============================================================================================================

int EventManager::getNumGroups() const
{
    return m_GroupsList.size();
}

//=============================================================================================================

std::optional<EventGroup> EventManager::getGroup(const idNum groupId) const
{
    auto groupFound = m_GroupsList.find(groupId);
    if(groupFound != m_GroupsList.end())
    {
        return EventGroup(groupFound->second);
    } else
    {
        return {};
    }
}

//=============================================================================================================

std::unique_ptr<std::vector<EventGroup> > EventManager::getAllGroups() const
{
    size_t  numGroups(m_GroupsList.size());
    auto pGroupsList(allocateOutputContainer<EventGroup>(numGroups));
    for(const auto& g: m_GroupsList)
    {
        pGroupsList->emplace_back(EventGroup(g.second));
    }
    return pGroupsList;
}

//=============================================================================================================

std::unique_ptr<std::vector<EventGroup> >
EventManager::getGroups(const std::vector<idNum>& groupIds) const
{
    auto pGroupList(allocateOutputContainer<EventGroup>(groupIds.size()));
    for(const auto& id: groupIds)
    {
        auto group = getGroup(id);
        if (group.has_value())
        {
            pGroupList->push_back(group.value());
        }
    }
    return pGroupList;
}

//=============================================================================================================

EventGroup EventManager::addGroup(const std::string& sGroupName)
{
    EVENTSINTERNAL::EventGroupINT newGroup(generateNewGroupId(), sGroupName);
    m_GroupsList.emplace(newGroup.getId(), newGroup);
    return EventGroup(newGroup);
}

//=============================================================================================================

EventGroup EventManager::addGroup(const std::string& sGroupName, const RgbColor& color)
{
    EVENTSINTERNAL::EventGroupINT newGroup(generateNewGroupId(), sGroupName, color);
    m_GroupsList.emplace(newGroup.getId(), newGroup);
    return EventGroup(newGroup);
}

//=============================================================================================================

bool EventManager::deleteGroup(const idNum groupId)
{
    bool state(false);
    auto events = getEventsInGroup(groupId);
    if(events->empty())
    {
        auto groupToDeleteIter = m_GroupsList.find(groupId);
        if(groupToDeleteIter != m_GroupsList.end())
        {
            m_GroupsList.erase(groupToDeleteIter);
            state = true;
        }
    }
    return state;
}

//=============================================================================================================

bool EventManager::deleteGroups(const std::vector<idNum>& groupIds)
{
    bool state(groupIds.size());
    for(auto g: groupIds)
    {
        state = state && deleteGroup(g);
    }
    return state;
}

//=============================================================================================================

void EventManager::renameGroup(const idNum groupId, const std::string& newName)
{
    auto group = m_GroupsList.find(groupId);
    if(group != m_GroupsList.end())
    {
        group->second.setName(newName);
    }
}

//=============================================================================================================

void EventManager::setGroupColor(const idNum groupId, const RgbColor& color)
{
    auto group = m_GroupsList.find(groupId);
    if( group != m_GroupsList.end())
    {
        group->second.setColor(color);
    }
}

//=============================================================================================================

EventGroup EventManager::mergeGroups(const std::vector<idNum>& groupIds, const std::string& newName)
{
    EVENTSLIB::EventGroup newGroup = addGroup(newName);
    auto eventsAll = getAllEvents();
    bool state(true);
    for(const auto& ev: *eventsAll)
    {
        for(auto g: groupIds)
        {
            if(ev.groupId == g)
            {
                state = state && addEventToGroup(ev.id, newGroup.id);
            }
        }
    }
    deleteGroups(groupIds);
    return newGroup;
}

//=============================================================================================================

EventGroup EventManager::duplicateGroup(const idNum groupId, const std::string& newName)
{
    EVENTSLIB::EventGroup newGroup = addGroup(newName);
    auto eventsToDuplicate = getEventsInGroup(groupId);
    for( const auto& e: *eventsToDuplicate)
    {
        addEvent(e.sample, newGroup.id);
    }
    return newGroup;
}

//=============================================================================================================

bool EventManager::addEventToGroup(const idNum eventId, const idNum groupId)
{
    bool state(false);
    int sample(0);
    sample = m_MapIdToSample.at(eventId); //if not found throws but not sure excp are used.
    if(sample != 0)
    {
        auto eventsRange = m_EventsListBySample.equal_range(sample);
        std::multimap<int, EVENTSINTERNAL::EventINT>::iterator e = eventsRange.first;
        for(; e != eventsRange.second; ++e)
        {
            if( e->second.getId() == eventId)
            {
                e->second.setGroupId(groupId);
                state = true;
                break;
            }
        }
    }
    return state;
}

//=============================================================================================================

bool EventManager::addEventsToGroup(const std::vector<idNum>& eventIds, const idNum groupId)
{
    bool state(true);
    for( idNum id: eventIds)
    {
        state = state && addEventToGroup(id, groupId);
    }
    return state;
}

//=============================================================================================================

void EventManager::initSharedMemory()
{
    initSharedMemory(SharedMemoryMode::READ);
}

//=============================================================================================================

void EventManager::initSharedMemory(SharedMemoryMode mode)
{
    m_pSharedMemManager->init(mode);
}

//=============================================================================================================

void EventManager::stopSharedMemory()
{
    m_pSharedMemManager->stop();
}

//=============================================================================================================

bool EventManager::isSharedMemoryInit()
{
    return m_pSharedMemManager->isInit();
}
