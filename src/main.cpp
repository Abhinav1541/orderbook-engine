#define _WIN32_WINNT 0x0A00
#include <iostream>
#include <map>
#include <list>
#include <vector>
#include <string>
#include <unordered_map>
#include <cassert>
#include <chrono>
#include <random>
#include <algorithm>
#include "httplib.h"
#include "sqlite3.h"

using std::cout;
using std::endl;
using std::string;

enum class Side { BUY, SELL };
enum class OrderType { LIMIT, MARKET };

struct Order {
    int orderId;
    int price;
    int quantity;
    Side side;
    int timestamp;
    OrderType type;
};

std::map<int, std::list<Order>, std::greater<int>> bids;
std::map<int, std::list<Order>> asks;

struct OrderLocation {
    Side side;
    int price;
    std::list<Order>::iterator it;
};

std::unordered_map<int, OrderLocation> orderLookup;

sqlite3* db;

void initDatabase() {
    cout << "initDatabase() called" << endl;

    int result = sqlite3_open("orderbook.db", &db);

    if (result != SQLITE_OK) {
        cout << "Failed to open database." << endl;
        return;
    }

    cout << "Database opened, creating table..." << endl;

    const char* createTableSQL =
        "CREATE TABLE IF NOT EXISTS trades ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "buy_order_id INTEGER,"
        "sell_order_id INTEGER,"
        "price INTEGER,"
        "quantity INTEGER"
        ");";

    char* errMsg = nullptr;
    result = sqlite3_exec(db, createTableSQL, nullptr, nullptr, &errMsg);

    if (result != SQLITE_OK) {
        cout << "Failed to create table: " << errMsg << endl;
        sqlite3_free(errMsg);
    } else {
        cout << "Database initialized successfully." << endl;
    }
}

void logTrade(int buyOrderId, int sellOrderId, int price, int quantity) {
    const char* insertSQL = "INSERT INTO trades (buy_order_id, sell_order_id, price, quantity) VALUES (?, ?, ?, ?);";

    sqlite3_stmt* stmt;
    sqlite3_prepare_v2(db, insertSQL, -1, &stmt, nullptr);

    sqlite3_bind_int(stmt, 1, buyOrderId);
    sqlite3_bind_int(stmt, 2, sellOrderId);
    sqlite3_bind_int(stmt, 3, price);
    sqlite3_bind_int(stmt, 4, quantity);

    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
}

void matchBuyOrder(Order& buyOrder) {
    while (buyOrder.quantity > 0 && !asks.empty()) {
        auto bestAskLevel = asks.begin();
        int bestAskPrice = bestAskLevel->first;

        if (buyOrder.price < bestAskPrice) {
            break;
        }

        std::list<Order>& ordersAtBestAsk = bestAskLevel->second;
        Order& restingOrder = ordersAtBestAsk.front();

        int tradedQty = std::min(buyOrder.quantity, restingOrder.quantity);

        logTrade(buyOrder.orderId, restingOrder.orderId, bestAskPrice, tradedQty);

        buyOrder.quantity -= tradedQty;
        restingOrder.quantity -= tradedQty;

        if (restingOrder.quantity == 0) {
            orderLookup.erase(restingOrder.orderId);
            ordersAtBestAsk.pop_front();
        }

        if (ordersAtBestAsk.empty()) {
            asks.erase(bestAskLevel);
        }
    }
}

void matchSellOrder(Order& sellOrder) {
    while (sellOrder.quantity > 0 && !bids.empty()) {
        auto bestBidLevel = bids.begin();
        int bestBidPrice = bestBidLevel->first;

        if (sellOrder.price > bestBidPrice) {
            break;
        }

        std::list<Order>& ordersAtBestBid = bestBidLevel->second;
        Order& restingOrder = ordersAtBestBid.front();

        int tradedQty = std::min(sellOrder.quantity, restingOrder.quantity);

        logTrade(restingOrder.orderId, sellOrder.orderId, bestBidPrice, tradedQty);

        sellOrder.quantity -= tradedQty;
        restingOrder.quantity -= tradedQty;

        if (restingOrder.quantity == 0) {
            orderLookup.erase(restingOrder.orderId);
            ordersAtBestBid.pop_front();
        }

        if (ordersAtBestBid.empty()) {
            bids.erase(bestBidLevel);
        }
    }
}

void addOrder(Order order) {
    if (order.side == Side::BUY) {
        matchBuyOrder(order);
        if (order.quantity > 0 && order.type == OrderType::LIMIT) {
            bids[order.price].push_back(order);
            auto it = std::prev(bids[order.price].end());
            orderLookup[order.orderId] = {Side::BUY, order.price, it};
        }
    } else {
        matchSellOrder(order);
        if (order.quantity > 0 && order.type == OrderType::LIMIT) {
            asks[order.price].push_back(order);
            auto it = std::prev(asks[order.price].end());
            orderLookup[order.orderId] = {Side::SELL, order.price, it};
        }
    }
}

void cancelOrder(int orderId) {
    auto lookupIt = orderLookup.find(orderId);

    if (lookupIt == orderLookup.end()) {
        cout << "Cancel failed: Order " << orderId << " not found." << endl;
        return;
    }

    OrderLocation loc = lookupIt->second;

    if (loc.side == Side::BUY) {
        bids[loc.price].erase(loc.it);
        if (bids[loc.price].empty()) {
            bids.erase(loc.price);
        }
    } else {
        asks[loc.price].erase(loc.it);
        if (asks[loc.price].empty()) {
            asks.erase(loc.price);
        }
    }

    orderLookup.erase(lookupIt);
    cout << "Order " << orderId << " cancelled." << endl;
}

void printBook() {
    cout << "----- BIDS -----" << endl;
    for (const auto& [price, orderList] : bids) {
        cout << "Price: " << price << endl;
        for (const auto& order : orderList) {
            cout << "  OrderId: " << order.orderId << ", Qty: " << order.quantity << endl;
        }
    }

    cout << "----- ASKS -----" << endl;
    for (const auto& [price, orderList] : asks) {
        cout << "Price: " << price << endl;
        for (const auto& order : orderList) {
            cout << "  OrderId: " << order.orderId << ", Qty: " << order.quantity << endl;
        }
    }
}

int main() {
    initDatabase();

    httplib::Server svr;

    svr.Get("/orderbook", [](const httplib::Request&, httplib::Response& res) {
        std::string result = "";

        result += "----- BIDS -----\n";
        for (const auto& [price, orderList] : bids) {
            result += "Price: " + std::to_string(price) + "\n";
            for (const auto& order : orderList) {
                result += "  OrderId: " + std::to_string(order.orderId) + ", Qty: " + std::to_string(order.quantity) + "\n";
            }
        }

        result += "----- ASKS -----\n";
        for (const auto& [price, orderList] : asks) {
            result += "Price: " + std::to_string(price) + "\n";
            for (const auto& order : orderList) {
                result += "  OrderId: " + std::to_string(order.orderId) + ", Qty: " + std::to_string(order.quantity) + "\n";
            }
        }

        res.set_content(result, "text/plain");
    });

    svr.Get("/trades", [](const httplib::Request&, httplib::Response& res) {
    std::string result = "";

    const char* selectSQL = "SELECT id, buy_order_id, sell_order_id, price, quantity FROM trades;";

    sqlite3_stmt* stmt;
    sqlite3_prepare_v2(db, selectSQL, -1, &stmt, nullptr);

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        int id = sqlite3_column_int(stmt, 0);
        int buyOrderId = sqlite3_column_int(stmt, 1);
        int sellOrderId = sqlite3_column_int(stmt, 2);
        int price = sqlite3_column_int(stmt, 3);
        int quantity = sqlite3_column_int(stmt, 4);

        result += "Trade " + std::to_string(id) + ": Buy#" + std::to_string(buyOrderId) +
                  " / Sell#" + std::to_string(sellOrderId) + " - " + std::to_string(quantity) +
                  " @ " + std::to_string(price) + "\n";
    }

    sqlite3_finalize(stmt);
    res.set_content(result, "text/plain");
});

    svr.Post("/order", [](const httplib::Request& req, httplib::Response& res) {
        int orderId = std::stoi(req.get_param_value("orderId"));
        int price = std::stoi(req.get_param_value("price"));
        int quantity = std::stoi(req.get_param_value("quantity"));
        std::string sideStr = req.get_param_value("side");

        Side side = (sideStr == "BUY") ? Side::BUY : Side::SELL;

        Order o = {orderId, price, quantity, side, 0, OrderType::LIMIT};
        addOrder(o);

        res.set_content("Order added successfully.", "text/plain");
    });

    svr.Post("/cancel", [](const httplib::Request& req, httplib::Response& res) {
        int orderId = std::stoi(req.get_param_value("orderId"));
        cancelOrder(orderId);
        res.set_content("Cancel request processed.", "text/plain");
    });

    Order o1 = {1, 100, 10, Side::BUY, 0, OrderType::LIMIT};
    addOrder(o1);

    std::cout << "Order book server running on port 8080..." << std::endl;
    svr.listen("0.0.0.0", 8080);

    return 0;
}