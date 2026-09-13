#include <iostream>
#include <map>
#include <list>
#include <vector>
#include <string>
#include <unordered_map>
#include <cassert>

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

        cout << "TRADE: " << tradedQty << " shares @ " << bestAskPrice
             << " (Buy Order " << buyOrder.orderId << " / Sell Order " << restingOrder.orderId << ")" << endl;

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

        cout << "TRADE: " << tradedQty << " shares @ " << bestBidPrice
             << " (Sell Order " << sellOrder.orderId << " / Buy Order " << restingOrder.orderId << ")" << endl;

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
    // Test 1: full match at the same price
    Order buy = {1, 100, 10, Side::BUY, 0, OrderType::LIMIT};
    Order sell = {2, 100, 10, Side::SELL, 1, OrderType::LIMIT};

    addOrder(buy);
    addOrder(sell);

    assert(bids.empty());
    assert(asks.empty());

    cout << "Test 1 passed!" << endl;

    // Test 2: partial match, leftover rests in book
    bids.clear();
    asks.clear();
    orderLookup.clear();

    Order buy2 = {3, 100, 10, Side::BUY, 2, OrderType::LIMIT};
    Order sell2 = {4, 100, 4, Side::SELL, 3, OrderType::LIMIT};

    addOrder(buy2);
    addOrder(sell2);

    assert(asks.empty());
    assert(bids[100].front().quantity == 6);

    cout << "Test 2 passed!" << endl;

        // Test 3: cancellation removes the order correctly
    bids.clear();
    asks.clear();
    orderLookup.clear();

    Order buy3 = {5, 100, 10, Side::BUY, 4, OrderType::LIMIT};
    addOrder(buy3);

    cancelOrder(5);

    assert(bids.empty());
    assert(orderLookup.find(5) == orderLookup.end());

    cout << "Test 3 passed!" << endl;

    return 0;
}