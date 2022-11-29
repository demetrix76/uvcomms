#include <commlib/commlib.h>
#include <commlib/overloaded.h>

#include <iostream>
#include <future>
#include <thread>
#include <mutex>
#include <variant>
#include <vector>

#include <uvw.hpp>

class Client
{
public:
    struct CommandStop {};
    struct CommandMessage { std::string message; };
    using Command = std::variant<CommandStop, CommandMessage>;
    
    Client()
    {
        std::promise<std::shared_ptr<uvw::AsyncHandle>> initPromise;
        auto initFuture = initPromise.get_future();
        
        mThread = std::thread([this, pms = std::move(initPromise)]() mutable {
            run(std::move(pms));
        });
        
        mAsyncTrigger = initFuture.get();
    }
    
    ~Client()
    {
        // this won't interrupt the thread if it's currently waiting for connection availability;
        // on Windows, we can possibly use an Event object and WaitForMultipleObjects() to avoid that;
        // on macOS, a possible method is to interrupt the wait with a signal?
        
        postCommand(CommandStop{});
        mThread.join();
    }
    
    void postCommand(Command && aCommand)
    {
        {
            std::lock_guard lk(mMx);
            mPendingCommands.emplace_back(std::move(aCommand));
        }
        mAsyncTrigger->send();
    }
    
private:
    void run(std::promise<std::shared_ptr<uvw::AsyncHandle>> aInitPromise);
    
    void onAsync(uvw::AsyncHandle & aHandle);
    
private:
    std::shared_ptr<uvw::AsyncHandle> mAsyncTrigger;
    std::thread mThread;
    std::mutex  mMx;
    std::vector<Command> mPendingCommands;
    std::vector<Command> mCopiedCommands;
    
};

void Client::run(std::promise<std::shared_ptr<uvw::AsyncHandle>> aInitPromise)
{
    std::shared_ptr<uvw::Loop> theLoop = uvw::Loop::create();
    
    std::shared_ptr<uvw::AsyncHandle> asyncHandle = theLoop->resource<uvw::AsyncHandle>();
    
    asyncHandle->on<uvw::AsyncEvent>([this](auto const &, uvw::AsyncHandle & aHandle){
        onAsync(aHandle);
    });
    
    aInitPromise.set_value(asyncHandle);
    
    while(true)
    {
        std::cout << "Waiting for the server to become available...\n";
        
        commlib::ipc_lock lockfile;
        commlib::ipc_sem theSemaphore;
        
        if(lockfile.try_lock()) {
            while(theSemaphore.try_wait()) {}; // an exception will release the lock file should it happen
        }
        lockfile.unlock();
        
        theSemaphore.wait(0);
        theSemaphore.post();
        std::cout << "Connecting...\n";
    
        std::shared_ptr<uvw::PipeHandle> connection = theLoop->resource<uvw::PipeHandle>();
        asyncHandle->data(connection);
        
        connection->on<uvw::ErrorEvent>([](uvw::ErrorEvent const & aErrorEvent, uvw::PipeHandle & aConnection){
            std::cerr << "Client error: " << aErrorEvent.what() << std::endl;
            aConnection.close();
            aConnection.loop().stop();
        });
        
        connection->on<uvw::CloseEvent>([asyncHandle](uvw::CloseEvent const &, uvw::PipeHandle & aHandle){
            std::cout << "CloseEvent\n";
            asyncHandle->data(nullptr);
        });
        
        connection->on<uvw::ConnectEvent>([](uvw::ConnectEvent const &, uvw::PipeHandle & aConnection){
            std::cout << "Connected\n";
            aConnection.read();
        });
        
        connection->on<uvw::EndEvent>([theLoop](uvw::EndEvent const &, uvw::PipeHandle & aConnection){
            std::cout << "The server has closed the connection\n";
            aConnection.close();
            aConnection.loop().stop();
        });
        
        connection->on<uvw::DataEvent>([](uvw::DataEvent const & aDataEvent, uvw::PipeHandle & aConnection){
            std::cout << "Data from the server...\n";
        });
        
        connection->on<uvw::WriteEvent>([](uvw::WriteEvent const & aWriteEvent, uvw::PipeHandle & aConnection){
            std::cout << "WriteEvent\n";
        });
        
        connection->connect(commlib::pipeName(false));
        
        theLoop->run();
        std::cout << "Disconnected\n";
        
        std::cout << "Cooldown...\n";
        std::this_thread::sleep_for(std::chrono::seconds(5));
    }
}

void Client::onAsync(uvw::AsyncHandle & aHandle)
{
    std::shared_ptr<uvw::PipeHandle> connection = aHandle.data<uvw::PipeHandle>();
    mCopiedCommands.clear();
    {
        std::lock_guard lk(mMx);
        mPendingCommands.swap(mCopiedCommands);
    }
    for(Command const & cmd: mCopiedCommands)
        std::visit(overloaded{
            [&](CommandStop) {
                if(connection) {
                    connection->close();
                }
                aHandle.loop().stop();
            },
            [&](CommandMessage cmd){
                if(connection) {
                    std::unique_ptr<char[]> buffer(new char[cmd.message.size()]);
                    std::copy(cmd.message.begin(), cmd.message.end(), buffer.get());
                    connection->write(std::move(buffer), cmd.message.size());
                }
            }
        }, cmd);
    mCopiedCommands.clear();
}


int main(int, char*[])
{
    std::cout << "Hi from client\n";
    
    Client client;
    
    while(true)
    {
        std::this_thread::sleep_for(std::chrono::seconds(2));
        client.postCommand(Client::CommandMessage{"Message from client"});
        
    }
    
    return 0;
}
