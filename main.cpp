#include <iostream>
#include <amqpcpp.h>
#include <uv.h>
#include "handler.h"
#include "amqpcpp/libuv.h"

using namespace AMQP;

void timer_callback(uv_timer_t* handle) {

    TcpChannel* channel = static_cast<TcpChannel*>(handle->data);
    channel->publish("", "demo", "ssdsdsdsdsd");
    
    // printf("Timer fired!\n");
    // std::cout << "now - " << uv_now(handle->loop);

}

int main()
{
    uv_loop_t* loop = uv_default_loop();

    // uv_timer_t timer;
    // uv_timer_init(loop, &timer);
    // uv_timer_start(&timer, timer_callback, 500, 0);
    // uv_unref((uv_handle_t*)loop);

    LibUVHandler* handler = new LibUVHandler(loop);
    TcpConnection* conn = new TcpConnection(handler, Address("amqp://guest:guest@localhost/"));
    TcpChannel* channel = new TcpChannel(conn);

    uv_timer_t* timer = new uv_timer_t();
    timer->data = channel;
    uv_timer_init(loop, timer);

    channel->declareQueue("demo" , exclusive).onSuccess([conn, timer](const std::string &name, uint32_t messagecount, uint32_t consumercount) {
        
        // report the name of the temporary queue
        std::cout << "declared queue " << name << std::endl;
        uv_timer_start(timer, timer_callback, 50, 50);
    });
    

    uv_run(loop, UV_RUN_DEFAULT);
    
    return 0;
}