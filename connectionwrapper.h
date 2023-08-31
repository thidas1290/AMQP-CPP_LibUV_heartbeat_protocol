#pragma once

#include <stdio.h>
#include "amqpcpp.h"
#include "amqpcpp/linux_tcp.h"
#include <iostream>
#include "uv.h"
#include <list>
#include <algorithm>
#include "watcher.h"
#include "watchable.h"

namespace AMQP{

    class UvConnectionHandler : public WatchableObj
    {
    private:
        uv_loop_t* _loop;
        uv_timer_t _timer;
        AMQP::TcpConnection* _conn;
        std::list<Watcher> _watchers;
        uint64_t _timeout;
        uint64_t _expire;
        uint64_t _next;

        /**
         *  timer callback
         *  @param  handle  Internal timer handle
         */
        static void timer_cb(uv_timer_t* handle)
        {
            // retrieving the connection
            WatchableObj* obj = static_cast<WatchableObj*>(handle->data);
            std::cout << "onExpired::timerCB" << std::endl;

            // telliing the connection to send a heartbeat to the broker
            obj->onExpired();
        }

        bool timed() const
        {
            // if neither timers are set
            return _expire > 0.0 || _next > 0.0;
        }

        uint64_t sec_to_ms(uint64_t millis)
        {
            if(millis > 0) return millis * 1000; 
            return 0;
        }

        uint16_t ms_to_sec(uint64_t sec)
        {
            if(sec > 0) return sec / 1000;
            return 0;
        }

    public:
        UvConnectionHandler(uv_loop_t* loop, AMQP::TcpConnection* connection, uint64_t timeout) :
            _loop(loop),
            _conn(connection),
            _timeout(sec_to_ms(timeout)),
            _next(0),
            _expire(uv_now(_loop) + sec_to_ms(timeout))
        {
            _timer.data = this;
            uv_timer_init(_loop, &_timer);
            uv_timer_start(&_timer, timer_cb, _timeout, 0);
            // uv_unref((uv_handle_t*)_loop);
        }

        virtual ~UvConnectionHandler()
        {
                        // the timer was already stopped
            if (!timed()) return;

            // stop the timer
            uv_timer_stop(&_timer);

            // restore loop refcount
            // uv_ref((uv_handle_t*)_loop);
        }

        bool contains(AMQP::TcpConnection* connection) const 
        {
            return _conn == connection;
        }


        uint16_t start(uint16_t timeout)
        {
            // we now know for sure that the connection was set up
            _timeout = sec_to_ms(timeout);
            std::cout << "start" << std::endl;
            
            // if heartbeats are disabled we do not have to set it
            if (_timeout == 0) return 0;
            
            // calculate current time
            auto now = uv_now(_loop);
            
            // we also know when the next heartbeat should be sent
            _next = now + std::max((_timeout / 2), static_cast<u_int64_t>(1000));
            
            // because the server has just sent us some data, we will update the expire time too
            _expire = now + _timeout * 1.5;

            // stop the existing timer (we have to stop it and restart it, because ev_timer_set() 
            // on its own does not change the running timer) (note that we assume that the timer
            // is already running and keeps on running, so no calls to ev_ref()/en_unref() here)
            uv_timer_stop(&_timer);

            // find the earliest thing that expires
            uv_timer_start(&_timer, timer_cb, std::min(_next, _expire) - now, 0);

            // and start it again
            // ev_timer_start(_loop, &_timer);
            
            // expose the accepted interval
            return ms_to_sec(_timeout);
        }


        virtual void onExpired() override
        {
            // get the current time
            auto now = uv_now(_loop);
            
            // timer is no longer active, so the refcounter in the loop is restored
            // uv_ref((uv_handle_t*)_loop);

            // if the onNegotiate method was not yet called, and no heartbeat timeout was negotiated
            if (_timeout == 0)
            {
                // this can happen in three situations: 1. a connect-timeout, 2. user space has
                // told us that we're not interested in heartbeats, 3. rabbitmq does not want heartbeats,
                // in either case we're no longer going to run further timers.
                _next = _expire = 0.0;
                
                // if we have an initialized connection, user-space must have overridden the onNegotiate
                // method, so we keep using the connection
                if (_conn->initialized()) return;

                // this is a connection timeout, close the connection from our side too
                std::cout << "connectionClosed -- no negotations" << std::endl;
                return (void)_conn->close(true);
            }
            else if (now >= _expire)
            {
                // the server was inactive for a too long period of time, reset state
                _next = _expire = 0.0; _timeout = 0;
                
                // close the connection because server was inactive (we close it with immediate effect,
                // because it was inactive so we cannot trust it to respect the AMQP close handshake)
                std::cout << "connectionExpired -- inactive" << std::endl;
                return (void)_conn->close(true);
            }
            else if (now >= _next)
            {
                // it's time for the next heartbeat
                std::cout << "sending heartbeat" << std::endl;
                _conn->heartbeat();
                
                // remember when we should send out the next one, so the next one should be 
                // sent only after _timout/2 seconds again _from now_ (no catching up)
                _next = now + std::max(_timeout / 2, static_cast<uint64_t>(1000));
            }

            // reset the timer to trigger again later
            uv_timer_start(&_timer, timer_cb, std::min(_next, _expire) - now, 0.0);

            // and start it again
            // ev_timer_start(_loop, &_timer);
            
            // and because the timer is running again, we restore the refcounter
            // uv_unref((uv_handle_t*)_loop);
        }


        /**
         *  Monitor a filedescriptor
         *  @param  fd          specific file discriptor
         *  @param  events      updated events
         */
        void monitor(int fd, int events)
        {
            // should we remove?
            if (events == 0)
            {
                // remove the io-watcher
                _watchers.remove_if([fd](const Watcher &watcher) -> bool {
                    
                    // compare filedescriptors
                    return watcher.contains(fd);
                });
            }
            else
            {
                // look in the array for this filedescriptor
                for (auto &watcher : _watchers)
                {
                    // do we have a match?
                    if (watcher.contains(fd)) return watcher.events(events);
                }
                
                // if not found we need to register a new filedescriptor
                WatchableObj *watchable = this;
                
                // we should monitor a new filedescriptor
                _watchers.emplace_back(_loop, watchable, fd, events);
            }
        } 


        /**
         *  Method that is called when a filedescriptor becomes active
         *  @param  fd          the filedescriptor that is active
         *  @param  events      the events that are active (readable/writable)
         */
        virtual void onActive(int fd, int events) override
        {
            // if the server is readable, we have some extra time before it expires, the expire time 
            // is set to 1.5 * _timeout to close the connection when the third heartbeat is about to be sent
            if (_timeout != 0 && (events & UV_READABLE)) 
            {
                std::cout << "onActive::expire reset" << std::endl;
                _expire = uv_now(_loop) + _timeout * 1.5;
            }
            
            // pass on to the connection
            _conn->process(fd, events);
        }

    };

}

