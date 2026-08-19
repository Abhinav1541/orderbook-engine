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

int main() {
    Order order1;
    order1.orderId = 1;
    order1.price = 100;
    order1.quantity = 10;
    order1.side = Side::BUY;
    order1.timestamp = 0;

    cout << "OrderId: " << order1.orderId
         << ", Price: " << order1.price
         << ", Quantity: " << order1.quantity
         << ", Timestamp: " << order1.timestamp
         << endl;

    return 0;
}