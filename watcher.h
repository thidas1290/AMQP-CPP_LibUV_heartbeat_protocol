#pragma once

#include<stdio.h>
#include "amqpcpp.h"
#include <iostream>
#include "uv.h"

#include "watchable.h"

using namespace AMQP;

class Watcher
{
private:
    /**
     *  The event loop to which it is attached
     *  @var uv_loop_t
     */
    uv_loop_t *_loop;

    /**
     *  The actual watcher structure
     *  @var uv_poll_t
     */
    uv_poll_t *_poll;


    /**
     * monitoring file descriptor
    */
    int _fd;

    /**
     *  Callback method that is called by libuv when a filedescriptor becomes active
     *  @param  handle     Internal poll handle
     *  @param  status     LibUV error code UV_*
     *  @param  events     Events triggered
     */
    static void callback(uv_poll_t *handle, int status, int events)
    {
        // retrieve the connection
        WatchableObj *obj = static_cast<WatchableObj*>(handle->data);

        // tell the connection that its filedescriptor is active
        int fd = -1;
        uv_fileno(reinterpret_cast<uv_handle_t*>(handle), &fd);
        obj->onActive(fd, uv_to_amqp_events(status, events));


    }


    /**
     *  Convert event flags from UV format to AMQP format
     */
    static int uv_to_amqp_events(int status, int events)
    {
        // if the socket is closed report both so we pick up the error
        if (status != 0)
            return AMQP::readable | AMQP::writable;

        // map read or write
        int amqp_events = 0;
        if (events & UV_READABLE)
            amqp_events |= AMQP::readable;
        if (events & UV_WRITABLE)
            amqp_events |= AMQP::writable;
        return amqp_events;
    }

    /**
     *  Convert event flags from AMQP format to UV format
     */
    static int amqp_to_uv_events(int events)
    {
        int uv_events = 0;
        if (events & AMQP::readable)
            uv_events |= UV_READABLE;
        if (events & AMQP::writable)
            uv_events |= UV_WRITABLE;
        return uv_events;
    }


public:
    /**
     *  Constructor
     *  @param  loop            The current event loop
     *  @param  obj             The watchable object being watched
     *  @param  fd              The filedescriptor being watched
     *  @param  events          The events that should be monitored
     */
    Watcher(uv_loop_t *loop, WatchableObj *obj, int fd, int events) : 
        _loop(loop),
        _fd(fd)
    {
        // create a new poll
        _poll = new uv_poll_t();

        // initialize the libev structure
        uv_poll_init(_loop, _poll, fd);

        // store the connection in the data "void*"
        _poll->data = obj;

        // start the watcher
        uv_poll_start(_poll, amqp_to_uv_events(events), callback);
    }

    /**
     *  Watchers cannot be copied or moved
     *
     *  @param  that    The object to not move or copy
     */
    Watcher(Watcher &&that) = delete;
    Watcher(const Watcher &that) = delete;

    /**
     *  Destructor
     */
    virtual ~Watcher()
    {
        // stop the watcher
        uv_poll_stop(_poll);

        // close the handle
        uv_close(reinterpret_cast<uv_handle_t*>(_poll), [](uv_handle_t* handle) {
            // delete memory once closed
            delete reinterpret_cast<uv_poll_t*>(handle);
        });
    }

    /**
     *  Change the events for which the filedescriptor is monitored
     *  @param  events
     */
    void events(int events)
    {
        uv_poll_stop(_poll);
        // update the events being watched for
        uv_poll_start(_poll, amqp_to_uv_events(events), callback);
    }

    bool contains (int fd) const {
        return fd == _fd;
    }
};