#ifndef THORSANVIL_NISSE_EVENT_HANDLER_H
#define THORSANVIL_NISSE_EVENT_HANDLER_H

/*
 * A thin wrapper on libEvent to C++ it.
 *
 * When an socket listener is first created via add() we store all data in the Store object.
 * When this has been created it adds the `ReadEvent` to libEvent to listen for any data.
 *
 * When (if) a socket event is triggered we save a lambda on the JobQueue addJob() that will be
 * executed by a thread. The lambda restarts the CoRoutine which will either yield one of
 * three values.
 *
 * When the code yields one of three situations happens:
 *      * TaskYieldState::RestoreRead    We restore the read listener waiting for more data.
 *      * TaskYieldState::RestoreWrite   We restore the write listener waiting for space to write.
 *      * TaskYieldState::Remove         We destroy the socket and all associated data.
 *
 * Note: This data is never destroyed immediately because the code may be executing on any thread.
 *       Instead a request is queued on the `Store` object. The main thread will then be used
 *       to clean up data (See Store for details).
 */

#include "EventHandlerLibEvent.h"
#include "ThorsSocket/Server.h"
#include "ThorsSocket/Socket.h"
#include "ThorsSocket/SocketStream.h"
#include <map>
#include <functional>

/*
 * C-Callback registered with LibEvent
 */
extern "C" void eventCallback(evutil_socket_t fd, short eventType, void* data);

namespace TASock   = ThorsAnvil::ThorsSocket;

namespace ThorsAnvil::Nisse::Server
{

class JobQueue;
struct StreamData;
struct ServerData;
struct OwnedFD;

class EventHandler
{
    using Handler   = std::function<void(int)>;
    struct EventInfo
    {
        Handler     handler;
        Event       read;
        Event       write;
    };
    using HandlerMap= std::map<int, EventInfo>;

    JobQueue&       jobQueue;
    EventBase       eventBase;
    bool            finished;
    HandlerMap      handlerMap;

    public:
        EventHandler(JobQueue& jobQueue);

        void run();
        void stop();
        void add(int fd, Handler&& h);
        void restore(int fd, bool read);

    private:
        friend void ::eventCallback(evutil_socket_t fd, short eventType, void* data);
        void eventAction(int fd, EventType type);
};

}

#endif
