#pragma once

#include <amqpcpp/libuv.h>
#include <uv.h>
#include "iostream"

namespace AMQP{
    
    class LibUVHandler : public LibUvHandler
    {
        private:
            uv_loop_t* loop;
            uv_timer_t* timer;

            static void timer_cb(uv_timer_t* handle)
            {
                TcpConnection* conn = static_cast<TcpConnection*>(handle->data);
                std::cout << "onHeartbeat" << std::endl;
                conn->heartbeat();
            }

            uint16_t onNegotiate(TcpConnection *connection, uint16_t interval) override
            {
                if(interval < 60) interval = 60;
                timer = new uv_timer_t();
                uv_timer_init(loop, timer);
                timer->data = connection;
                uv_timer_start(timer, timer_cb, 0, interval*1000);
                return interval;
            }

            void onConnected(TcpConnection *connection)
            {
                std::cout << "connected" << std::endl;
            }

        public:
            LibUVHandler(uv_loop_t* _loop) : loop(_loop), LibUvHandler(_loop)
            {

            }
            ~LibUVHandler()
            {
                uv_timer_stop(timer);
                uv_close(reinterpret_cast<uv_handle_t*>(timer), [](uv_handle_t* handle){
                    delete reinterpret_cast<uv_timer_t*>(handle);
                });
            }

    };

}

