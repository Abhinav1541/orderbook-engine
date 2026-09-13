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

void runBenchmark(int numOrders) {
    bids.clear();
    asks.clear();
    orderLookup.clear();

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<int> priceDist(95, 105);
    std::uniform_int_distribution<int> qtyDist(1, 20);
    std::uniform_int_distribution<int> sideDist(0, 1);

    std::vector<long long> latencies;
    latencies.reserve(numOrders);

    auto overallStart = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < numOrders; i++) {
        Order o;
        o.orderId = i;
        o.price = priceDist(gen);
        o.quantity = qtyDist(gen);
        o.side = (sideDist(gen) == 0) ? Side::BUY : Side::SELL;
        o.timestamp = i;
        o.type = OrderType::LIMIT;

        auto orderStart = std::chrono::high_resolution_clock::now();
        addOrder(o);
        auto orderEnd = std::chrono::high_resolution_clock::now();

        auto orderDuration = std::chrono::duration_cast<std::chrono::nanoseconds>(orderEnd - orderStart);
        latencies.push_back(orderDuration.count());
    }

    auto overallEnd = std::chrono::high_resolution_clock::now();
    auto totalDuration = std::chrono::duration_cast<std::chrono::milliseconds>(overallEnd - overallStart);

    cout << "Processed " << numOrders << " orders in " << totalDuration.count() << " ms" << endl;
    double ordersPerSecond = (double)numOrders / ((double)totalDuration.count() / 1000.0);
    cout << "Throughput: " << ordersPerSecond << " orders/sec" << endl;

    std::sort(latencies.begin(), latencies.end());

    long long p50 = latencies[latencies.size() * 50 / 100];
    long long p99 = latencies[latencies.size() * 99 / 100];

    cout << "[Mixed] p50 latency: " << p50 << " ns" << endl;
    cout << "[Mixed] p99 latency: " << p99 << " ns" << endl;
}

void runInsertOnlyBenchmark(int numOrders) {
    bids.clear();
    asks.clear();
    orderLookup.clear();

    std::vector<long long> latencies;
    latencies.reserve(numOrders);

    for (int i = 0; i < numOrders; i++) {
        Order o;
        o.orderId = i;
        o.price = i;
        o.quantity = 10;
        o.side = Side::BUY;
        o.timestamp = i;
        o.type = OrderType::LIMIT;

        auto orderStart = std::chrono::high_resolution_clock::now();
        addOrder(o);
        auto orderEnd = std::chrono::high_resolution_clock::now();

        latencies.push_back(std::chrono::duration_cast<std::chrono::nanoseconds>(orderEnd - orderStart).count());
    }

    std::sort(latencies.begin(), latencies.end());
    cout << "[Insert-only] p50: " << latencies[latencies.size() * 50 / 100]
         << " ns, p99: " << latencies[latencies.size() * 99 / 100] << " ns" << endl;
}

void runMatchOnlyBenchmark(int numOrders) {
    bids.clear();
    asks.clear();
    orderLookup.clear();

    std::vector<long long> latencies;
    latencies.reserve(numOrders);

    for (int i = 0; i < numOrders; i++) {
        Order o;
        o.orderId = i;
        o.price = 100;
        o.quantity = 10;
        o.side = (i % 2 == 0) ? Side::BUY : Side::SELL;
        o.timestamp = i;
        o.type = OrderType::LIMIT;

        auto orderStart = std::chrono::high_resolution_clock::now();
        addOrder(o);
        auto orderEnd = std::chrono::high_resolution_clock::now();

        latencies.push_back(std::chrono::duration_cast<std::chrono::nanoseconds>(orderEnd - orderStart).count());
    }

    std::sort(latencies.begin(), latencies.end());
    cout << "[Match-only] p50: " << latencies[latencies.size() * 50 / 100]
         << " ns, p99: " << latencies[latencies.size() * 99 / 100] << " ns" << endl;
}

void runInsertOnlySmallPriceRangeBenchmark(int numOrders) {
    bids.clear();
    asks.clear();
    orderLookup.clear();

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<int> priceDist(1, 10);

    std::vector<long long> latencies;
    latencies.reserve(numOrders);

    for (int i = 0; i < numOrders; i++) {
        Order o;
        o.orderId = i;
        o.price = priceDist(gen);
        o.quantity = 10;
        o.side = Side::BUY;
        o.timestamp = i;
        o.type = OrderType::LIMIT;

        auto orderStart = std::chrono::high_resolution_clock::now();
        addOrder(o);
        auto orderEnd = std::chrono::high_resolution_clock::now();

        latencies.push_back(std::chrono::duration_cast<std::chrono::nanoseconds>(orderEnd - orderStart).count());
    }

    std::sort(latencies.begin(), latencies.end());
    cout << "[Insert-only, small price range] p50: " << latencies[latencies.size() * 50 / 100]
         << " ns, p99: " << latencies[latencies.size() * 99 / 100] << " ns" << endl;
}

int main() {
    runBenchmark(100000);
    runInsertOnlyBenchmark(100000);
    runMatchOnlyBenchmark(100000);
    runInsertOnlySmallPriceRangeBenchmark(100000);
    return 0;
}