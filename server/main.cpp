#include <commlib/commlib.h>
#include <uvw.hpp>
#include <string>
#include <future>
#include <iostream>
#include <thread>

void serverFunction(std::promise<std::shared_ptr<uvw::AsyncHandle>> aInitPromise)
{
    std::shared_ptr<uvw::Loop> theLoop = uvw::Loop::create();

    try
    {
        std::shared_ptr<uvw::AsyncHandle> asyncHandle = theLoop->resource<uvw::AsyncHandle>();
        
        asyncHandle->on<uvw::AsyncEvent>([](uvw::AsyncEvent const &, uvw::AsyncHandle & aHandle ){
            // closing all handles will cause the loop to stop
            aHandle.loop().walk([](auto & aHandle) {
                aHandle.close();
            });
        });
        
        std::shared_ptr<uvw::PipeHandle> listeningPipe = theLoop->resource<uvw::PipeHandle>();
        listeningPipe->on<uvw::ErrorEvent>([asyncHandle](uvw::ErrorEvent const & err, uvw::PipeHandle & aHandle){
            std::cerr << "Listener error: " << err.what() << std::endl;
            asyncHandle->send();
        });
        
        listeningPipe->bind(commlib::pipeName(true));
        
        listeningPipe->on<uvw::ListenEvent>([](uvw::ListenEvent const &, uvw::PipeHandle & aListeningHandle){
            std::cout << "Incoming connection...\n";
            
            std::shared_ptr<uvw::PipeHandle> client = aListeningHandle.loop().resource<uvw::PipeHandle>();
            
            client->on<uvw::ErrorEvent>([](uvw::ErrorEvent const & aErrorEvent, uvw::PipeHandle & aClient){
                std::cerr << "Client connection error: " << aErrorEvent.what() << std::endl;
                aClient.close();
            });
            
            client->on<uvw::EndEvent>([](uvw::EndEvent const &, uvw::PipeHandle & aClient){
                std::cout << "Client disconnected\n";
                aClient.close();
            });
            
            client->on<uvw::DataEvent>([](uvw::DataEvent const & aDataEvent, uvw::PipeHandle & aClient){
                std::string message(aDataEvent.data.get(), aDataEvent.data.get() + aDataEvent.length);
                std::cout << "MESSAGE: " << message << std::endl;
            });
            
            aListeningHandle.accept(*client);
            client->read();

        });
        
        listeningPipe->listen();
        aInitPromise.set_value(asyncHandle);
    }
    catch(...)
    {
        aInitPromise.set_exception(std::current_exception());
        return;
    }
    
    commlib::ipc_lock lockfile;
    std::lock_guard lk(lockfile);
    
    commlib::ipc_sem theSemaphore;
    // 'normalize' the semaphore, draining all the potential sem_post's from the past
    while(theSemaphore.try_wait()) {}
    
    theSemaphore.post();
    try
    {
        std::cout << "Server loop running\n";
        theLoop->run();
        std::cout << "Server loop done\n";
        theSemaphore.try_wait();
    }
    catch(...)
    {
        theSemaphore.try_wait();
        throw;
    }
}

int main(int, char*[])
{
    std::promise<std::shared_ptr<uvw::AsyncHandle>> initPromise;
    auto initFuture = initPromise.get_future();
    
    std::thread server_thread([ pms = std::move(initPromise)] () mutable {
        serverFunction(std::move(pms));
    });
    
    auto asyncHandle = initFuture.get();
    std::cout << "Hi; press Enter to stop\n";
    
    std::string s;
    std::getline(std::cin, s, '\n');
    asyncHandle->send();
    
    server_thread.join();
    return 0;
}

