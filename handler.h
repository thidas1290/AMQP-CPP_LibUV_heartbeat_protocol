#pragma once

#include "amqpcpp/linux_tcp.h"
#include <uv.h>
#include "iostream"
#include <list>
#include "watcher.h"
#include "connectionwrapper.h"

namespace AMQP{
    
    class LibUVHandler : public TcpHandler
    {
        private:
        /**
             *  The event loop
             *  @var uv_loop_t*
             */
            uv_loop_t *_loop;


            std::list<UvConnectionHandler> _wrappers;

            UvConnectionHandler &lookup(TcpConnection *connection)
            {
                // look for the appropriate connection
                for (auto &wrapper : _wrappers)
                {
                    // do we have a match?
                    if (wrapper.contains(connection)) return wrapper;
                }
                
                // add to the wrappers
                _wrappers.emplace_back(_loop, connection, 60);
                
                // done
                return _wrappers.back();
            }

        protected:
            /**
             *  Method that is called by AMQP-CPP to register a filedescriptor for readability or writability
             *  @param  connection  The TCP connection object that is reporting
             *  @param  fd          The filedescriptor to be monitored
             *  @param  flags       Should the object be monitored for readability or writability?
             */
            virtual void monitor(TcpConnection *connection, int fd, int flags) override
            {
                std::cout << "monitor" << std::endl;
                lookup(connection).monitor(fd, flags);
            }

            /**
             *  Method that is called when the server sends a heartbeat to the client
             *  @param  connection  The connection over which the heartbeat was received
             *  @param  interval    agreed interval by the broker  
             *  @see    ConnectionHandler::onHeartbeat
             */
            virtual uint16_t onNegotiate(TcpConnection *connection, uint16_t interval) override
            {
                std::cout << "onnegotiate - " << interval << std::endl;
                return lookup(connection).start(interval);

            }   

            virtual void onDetached(TcpConnection *connection) override
            {
                std::cout << "onDetached" << std::endl;
                _wrappers.remove_if([connection](const UvConnectionHandler &wrapper) -> bool {
                    return wrapper.contains(connection);
                    });
            }

        public:
            /**
             *  Constructor
             *  @param  loop    The event loop to wrap
             */
            LibUVHandler(uv_loop_t *loop) : _loop(loop) {}

            /**
             *  Destructor
             */
            ~LibUVHandler() = default;

    };

}

