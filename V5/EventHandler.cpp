#include "EventHandler.h"
#include "../V4/JobQueue.h"

/*
 * C Callback functions.
 * Simply decide the data into EventHandler and call the C++ functions.
 */
void eventCallback(evutil_socket_t fd, short eventType, void* data)
{
    EventHandler&    eventHandler = *reinterpret_cast<EventHandler*>(data);
    eventHandler.eventAction(fd, static_cast<EventType>(eventType));
}

/*
 * EventLib wrapper. Set up C-Function callbacks
 */
Event::Event(EventBase& eventBase, int fd, EventType type, EventHandler& eventHandler)
    : event{event_new(eventBase.eventBase, fd, static_cast<short>(type), &eventCallback, &eventHandler)}
{}

EventHandler::EventHandler(JobQueue& jobQueue)
    : jobQueue{jobQueue}
    , finished{false}
{}

void EventHandler::run()
{
    finished = false;
    eventBase.run();
}

void EventHandler::stop()
{
    finished = true;
}

void EventHandler::add(int fd, Handler&& h)
{
    std::cerr << "Adding Handler For: " << fd << "\n";
    handlerMap[fd] = {std::move(h),
                      Event{eventBase, fd, EventType::Read , *this},
                      Event{eventBase, fd, EventType::Write , *this}
                     };

    handlerMap[fd].read.add();
}

void EventHandler::restore(int fd, bool read)
{
    if (read) {
        handlerMap[fd].read.add();
    }
    else {
        handlerMap[fd].write.add();
    }
}

void EventHandler::eventAction(int fd, EventType)
{
    std::cerr << "Handler Event For: " << fd << "\n";
    Handler&   handler   = handlerMap[fd].handler;
    handler(fd);
}
