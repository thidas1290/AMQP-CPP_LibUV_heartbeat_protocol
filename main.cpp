#include <iostream>
#include <amqpcpp.h>
#include <uv.h>
#include "handler.h"

using namespace AMQP;


int main()
{
    uv_loop_t* loop = uv_default_loop();
    LibUVHandler* handler = new LibUVHandler(loop);
    TcpConnection* conn = new TcpConnection(handler, Address("amqp://guest:guest@localhost/"));

    uv_run(loop, UV_RUN_DEFAULT);
    
    return 0;
}