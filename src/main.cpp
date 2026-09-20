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

const std::string DASHBOARD_HTML = R"HTMLPAGE(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<title>Orderbook Engine — Dashboard</title>
<style>
  :root {
    --bg: #0d1117;
    --panel: #161b22;
    --border: #30363d;
    --text: #c9d1d9;
    --muted: #8b949e;
    --green: #3fb950;
    --red: #f85149;
    --accent: #58a6ff;
  }
  * { box-sizing: border-box; }
  body {
    background: var(--bg);
    color: var(--text);
    font-family: -apple-system, Segoe UI, Roboto, sans-serif;
    margin: 0;
    padding: 24px;
  }
  h1 { font-size: 20px; margin: 0 0 4px; }
  .sub { color: var(--muted); font-size: 13px; margin-bottom: 24px; }
  .grid {
    display: grid;
    grid-template-columns: 300px 1fr 1fr;
    gap: 16px;
    align-items: start;
  }
  @media (max-width: 900px) {
    .grid { grid-template-columns: 1fr; }
  }
  .panel {
    background: var(--panel);
    border: 1px solid var(--border);
    border-radius: 8px;
    padding: 16px;
  }
  .panel h2 {
    font-size: 14px;
    text-transform: uppercase;
    letter-spacing: 0.05em;
    color: var(--muted);
    margin: 0 0 12px;
  }
  label {
    display: block;
    font-size: 12px;
    color: var(--muted);
    margin: 10px 0 4px;
  }
  input, select {
    width: 100%;
    padding: 6px 8px;
    background: #0d1117;
    border: 1px solid var(--border);
    color: var(--text);
    border-radius: 6px;
    font-size: 13px;
  }
  button {
    margin-top: 14px;
    width: 100%;
    padding: 8px;
    background: var(--accent);
    border: none;
    border-radius: 6px;
    color: #0d1117;
    font-weight: 600;
    cursor: pointer;
    font-size: 13px;
  }
  button.cancel-btn { background: var(--red); color: white; }
  button:hover { opacity: 0.9; }
  table { width: 100%; border-collapse: collapse; font-size: 12px; font-family: monospace; }
  th, td { text-align: left; padding: 4px 6px; border-bottom: 1px solid var(--border); }
  th { color: var(--muted); font-weight: 500; }
  .bid-price { color: var(--green); }
  .ask-price { color: var(--red); }
  .status { font-size: 12px; margin-top: 8px; min-height: 16px; }
  .status.ok { color: var(--green); }
  .status.err { color: var(--red); }
  .empty { color: var(--muted); font-size: 12px; padding: 6px 0; }
</style>
</head>
<body>

<h1>Orderbook Engine</h1>
<div class="sub">Live order book, matching engine &amp; trade history — refreshes every 2s</div>

<div class="grid">

  <div class="panel">
    <h2>Place Order</h2>
    <form id="orderForm">
      <label>Order ID</label>
      <input type="number" id="orderId" required>

      <label>Side</label>
      <select id="side">
        <option value="BUY">BUY</option>
        <option value="SELL">SELL</option>
      </select>

      <label>Type</label>
      <select id="type">
        <option value="LIMIT">LIMIT</option>
        <option value="MARKET">MARKET</option>
      </select>

      <label>Price</label>
      <input type="number" id="price" required>

      <label>Quantity</label>
      <input type="number" id="quantity" required>

      <button type="submit">Submit Order</button>
      <div class="status" id="orderStatus"></div>
    </form>

    <h2 style="margin-top:24px;">Cancel Order</h2>
    <form id="cancelForm">
      <label>Order ID</label>
      <input type="number" id="cancelOrderId" required>
      <button type="submit" class="cancel-btn">Cancel Order</button>
      <div class="status" id="cancelStatus"></div>
    </form>
  </div>

  <div class="panel">
    <h2>Order Book</h2>
    <div id="bookContainer"><div class="empty">Loading…</div></div>
  </div>

  <div class="panel">
    <h2>Trade History</h2>
    <div id="tradesContainer"><div class="empty">Loading…</div></div>
  </div>

</div>

<script>
async function refreshBook() {
  try {
    const res = await fetch('/orderbook');
    const text = await res.text();
    renderBook(text);
  } catch (e) {
    document.getElementById('bookContainer').innerHTML = '<div class="empty">Unable to load order book.</div>';
  }
}

function renderBook(text) {
  const lines = text.split('\n');
  let side = null;
  const bids = [];
  const asks = [];
  let currentPrice = null;

  for (const line of lines) {
    if (line.includes('BIDS')) { side = 'bid'; continue; }
    if (line.includes('ASKS')) { side = 'ask'; continue; }
    const priceMatch = line.match(/^Price:\s*(\d+)/);
    if (priceMatch) { currentPrice = priceMatch[1]; continue; }
    const orderMatch = line.match(/OrderId:\s*(\d+),\s*Qty:\s*(\d+)/);
    if (orderMatch && currentPrice !== null) {
      const row = { price: currentPrice, orderId: orderMatch[1], qty: orderMatch[2] };
      if (side === 'bid') bids.push(row);
      else if (side === 'ask') asks.push(row);
    }
  }

  const container = document.getElementById('bookContainer');
  if (bids.length === 0 && asks.length === 0) {
    container.innerHTML = '<div class="empty">Book is empty.</div>';
    return;
  }

  const buildTable = (rows, cls) => {
    if (rows.length === 0) return '<div class="empty">None</div>';
    let html = '<table><tr><th>Price</th><th>Order ID</th><th>Qty</th></tr>';
    for (const r of rows) {
      html += `<tr><td class="${cls}">${r.price}</td><td>${r.orderId}</td><td>${r.qty}</td></tr>`;
    }
    return html + '</table>';
  };

  container.innerHTML =
    '<div style="margin-bottom:6px;color:var(--green);font-size:12px;">BIDS</div>' +
    buildTable(bids, 'bid-price') +
    '<div style="margin:12px 0 6px;color:var(--red);font-size:12px;">ASKS</div>' +
    buildTable(asks, 'ask-price');
}

async function refreshTrades() {
  try {
    const res = await fetch('/trades');
    const text = await res.text();
    renderTrades(text);
  } catch (e) {
    document.getElementById('tradesContainer').innerHTML = '<div class="empty">Unable to load trades.</div>';
  }
}

function renderTrades(text) {
  const lines = text.split('\n').filter(l => l.trim().length > 0);
  const container = document.getElementById('tradesContainer');
  if (lines.length === 0) {
    container.innerHTML = '<div class="empty">No trades yet.</div>';
    return;
  }

  let html = '<table><tr><th>ID</th><th>Buy</th><th>Sell</th><th>Qty</th><th>Price</th></tr>';
  for (const line of lines) {
    const m = line.match(/Trade (\d+): Buy#(\d+) \/ Sell#(\d+) - (\d+) @ (\d+)/);
    if (m) {
      html += `<tr><td>${m[1]}</td><td>${m[2]}</td><td>${m[3]}</td><td>${m[4]}</td><td>${m[5]}</td></tr>`;
    }
  }
  html += '</table>';
  container.innerHTML = html;
}

document.getElementById('orderForm').addEventListener('submit', async (e) => {
  e.preventDefault();
  const statusEl = document.getElementById('orderStatus');
  const body = new URLSearchParams({
    orderId: document.getElementById('orderId').value,
    price: document.getElementById('price').value,
    quantity: document.getElementById('quantity').value,
    side: document.getElementById('side').value,
    type: document.getElementById('type').value
  });
  try {
    const res = await fetch('/order', { method: 'POST', body });
    const text = await res.text();
    statusEl.textContent = text;
    statusEl.className = 'status ok';
    refreshBook();
    refreshTrades();
  } catch (err) {
    statusEl.textContent = 'Request failed.';
    statusEl.className = 'status err';
  }
});

document.getElementById('cancelForm').addEventListener('submit', async (e) => {
  e.preventDefault();
  const statusEl = document.getElementById('cancelStatus');
  const body = new URLSearchParams({
    orderId: document.getElementById('cancelOrderId').value
  });
  try {
    const res = await fetch('/cancel', { method: 'POST', body });
    const text = await res.text();
    statusEl.textContent = text;
    statusEl.className = 'status ok';
    refreshBook();
  } catch (err) {
    statusEl.textContent = 'Request failed.';
    statusEl.className = 'status err';
  }
});

refreshBook();
refreshTrades();
setInterval(refreshBook, 2000);
setInterval(refreshTrades, 2000);
</script>

</body>
</html>
)HTMLPAGE";

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

    svr.Get("/", [](const httplib::Request&, httplib::Response& res) {
        res.set_content(DASHBOARD_HTML, "text/html");
    });

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
        std::string typeStr = req.get_param_value("type");

        Side side = (sideStr == "BUY") ? Side::BUY : Side::SELL;
        OrderType type = (typeStr == "MARKET") ? OrderType::MARKET : OrderType::LIMIT;

        Order o = {orderId, price, quantity, side, 0, type};
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

    int port = 8080;
    if (const char* envPort = std::getenv("PORT")) {
        port = std::stoi(envPort);
    }
    svr.listen("0.0.0.0", port);

    return 0;
}