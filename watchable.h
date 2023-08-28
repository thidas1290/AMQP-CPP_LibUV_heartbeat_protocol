#pragma once

#include <stdio.h>
#include "amqpcpp.h"
#include <iostream>
#include "uv.h"

using namespace AMQP;

class WatchableObj
{
public:
    /**
     *  The method that is called when a filedescriptor becomes active
     *  @param  fd
     *  @param  events
     */
    virtual void onActive(int fd, int events) = 0;
    
    /**
     *  Method that is called when the timer expires
     */
    virtual void onExpired() = 0;
    
};