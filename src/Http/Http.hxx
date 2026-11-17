#pragma once

#include <atomic>
#include <mutex>
#include <semaphore>
#include <string>
#include <thread>

#include <httplib.h>

#include "../Utils/utils.hxx"
#include "Diagnostic/Exceptions.hxx"



namespace pie::http {

// A simpler and lightweight representation of httplib's Req and Res types
struct Request { std::string method, path, body; };
struct Response { std::string reply; int status; };

class Server {
    httplib::Server server;
    Request  request ;
    Response response;


    std::thread thread;
    std::binary_semaphore arrived{0};
    std::counting_semaphore<2> done{0};
    std::atomic_flag destroyed;

    bool nexted{};
public:

    Server() {
        server.new_task_queue = [] { return new httplib::ThreadPool{1}; };

        const auto handle = [this] (const httplib::Request& req, httplib::Response& res) {
            request.method = req.method;
            request.path   = req.path;
            request.body   = req.body;

            arrived.release();
            done.acquire();

            if (destroyed.test()) {
                res.status = 503;
                return;
            }

            res.status = response.status;
            res.set_content(response.reply, "text/plain");
        };

        server.Put(".*", handle);
        server.Get(".*", handle);
        server.Post(".*", handle);
        server.Delete(".*", handle);
    }

    ~Server() {
        destroyed.test_and_set();
        done.release();

        server.stop();
        if (thread.joinable()) thread.join();
    }


    void listen(const std::string& host, const int port) {
        if (thread.joinable()) util::error<except::ThreadError>("Cannot listen to a server twice!");

        if (not server.bind_to_port(host, port))
            util::error("Couldn't bind to host " + host + ':' + std::to_string(port));


        thread = std::thread{ [this]{ server.listen_after_bind(); } };
    }

    const Request& getRequest() {
        arrived.acquire();
        nexted = true;
        return request;
    }


    // @answer is moved if true is returned
    bool reply(const int status, const std::string& answer) {
        if (not nexted) return false;
        nexted = false;

        response = {answer, status};
        done.release();

        return true;
    }

};

} // namespace pie::http


