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
struct Request {
    std::string method, path, body;
};

struct Response {
    int status;
    std::string reply;
    std::string content_type = "text/plain";
};

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
            res.set_content(response.reply, response.content_type);
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
    bool reply(const int status, const std::string& answer, const std::string content_type) {
        if (not nexted) return false;
        nexted = false;

        response = {.status = status, .reply = answer, .content_type = content_type};
        done.release();

        return true;
    }


    bool mount(const std::string& prefix, const std::string& folder) {
        if (thread.joinable()) return false; // util::error("Mount folders before the server starts listening");

        return server.set_mount_point(prefix, folder);
    }

};

} // namespace pie::http


