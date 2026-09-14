#include "httplib.h"
#include <iostream>

int main() {
    httplib::Server svr;

    svr.Get("/hello", [](const httplib::Request&, httplib::Response& res) {
        res.set_content("Hello from the order book server!", "text/plain");
    });

    std::cout << "Server starting on port 8080..." << std::endl;
    svr.listen("0.0.0.0", 8080);

    return 0;
}