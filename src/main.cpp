#include <iostream>
#include <map>
#include <list>
#include <vector>
#include <string>
#include <unordered_map>

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
    Order o1 = {1, 100, 10, Side::BUY, 0, OrderType::LIMIT};
    Order o2 = {2, 105, 5, Side::SELL, 1, OrderType::LIMIT};
    Order o3 = {3, 100, 8, Side::BUY, 2, OrderType::LIMIT};
    Order o4 = {4, 103, 4, Side::SELL, 3, OrderType::LIMIT};

    addOrder(o1);
    addOrder(o2);
    addOrder(o3);
    addOrder(o4);

    Order o5 = {5, 104, 6, Side::BUY, 4, OrderType::LIMIT};
    addOrder(o5);

    Order o6 = {6, 99, 5, Side::SELL, 5, OrderType::LIMIT};
    addOrder(o6);

    Order o7 = {7, 999999999, 10, Side::BUY, 6, OrderType::MARKET};
    addOrder(o7);

    cout << "----- BEFORE CANCEL -----" << endl;
    printBook();

    cancelOrder(3);  // cancel OrderId 3, resting at price 100

    cout << "----- AFTER CANCEL -----" << endl;
    printBook();

    return 0;
}