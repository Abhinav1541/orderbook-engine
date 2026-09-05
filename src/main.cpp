#include <iostream>
#include <map>
#include <list>
#include <vector>
#include <string>

using std::cout;
using std::endl;
using std::string;

enum class Side { BUY, SELL };

struct Order {
    int orderId;
    int price;
    int quantity;
    Side side;
    int timestamp;
};

std::map<int, std::list<Order>, std::greater<int>> bids;
std::map<int, std::list<Order>> asks;

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
        if (order.quantity > 0) {
            bids[order.price].push_back(order);
        }
    } else {
        matchSellOrder(order);
        if (order.quantity > 0) {
            asks[order.price].push_back(order);
        }
    }
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
    Order o1 = {1, 100, 10, Side::BUY, 0};
    Order o2 = {2, 105, 5, Side::SELL, 1};
    Order o3 = {3, 100, 8, Side::BUY, 2};
    Order o4 = {4, 103, 4, Side::SELL, 3};

    addOrder(o1);
    addOrder(o2);
    addOrder(o3);
    addOrder(o4);

    Order o5 = {5, 104, 6, Side::BUY, 4};
    addOrder(o5);

    Order o6 = {6, 99, 5, Side::SELL, 5};
    addOrder(o6);

    printBook();
    return 0;
}