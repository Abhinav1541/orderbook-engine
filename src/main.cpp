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

// Tick-indexed flat order book, replacing the original std::map<price, list<Order>>.
// Price levels map directly to array indices (index = price - MIN_PRICE), giving
// O(1) access to any level and much better cache locality than a red-black tree,
// at the cost of assuming a bounded, known price range up front (1 to 10,000 here;
// a real instrument would size this from its actual tick size and price bounds).
const int MIN_PRICE = 1;
const int MAX_PRICE = 10000;
const int PRICE_RANGE = MAX_PRICE - MIN_PRICE + 1;

std::vector<std::list<Order>> bidLevels(PRICE_RANGE);
std::vector<std::list<Order>> askLevels(PRICE_RANGE);

// Best bid = highest occupied index; best ask = lowest occupied index.
// Tracked incrementally (O(1) on insert, amortized scan on removal) so matching
// never has to search the whole array to find the current best price.
int bestBidIndex = -1;          // -1 means no resting bids
int bestAskIndex = PRICE_RANGE; // PRICE_RANGE means no resting asks

inline int priceToIndex(int price) { return price - MIN_PRICE; }
inline int indexToPrice(int index) { return index + MIN_PRICE; }

struct OrderLocation {
    Side side;
    int index;
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
    while (buyOrder.quantity > 0 && bestAskIndex < PRICE_RANGE) {
        int bestAskPrice = indexToPrice(bestAskIndex);

        if (buyOrder.price < bestAskPrice) {
            break;
        }

        std::list<Order>& ordersAtBestAsk = askLevels[bestAskIndex];
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
            // Level just emptied — advance to the next occupied level above.
            // Worst case this scans the gap to the next resting order, but in
            // practice gaps are small, and this replaces what used to be an
            // O(log n) tree erase with amortized O(1) array access instead.
            bestAskIndex++;
            while (bestAskIndex < PRICE_RANGE && askLevels[bestAskIndex].empty()) {
                bestAskIndex++;
            }
        }
    }
}

void matchSellOrder(Order& sellOrder) {
    while (sellOrder.quantity > 0 && bestBidIndex >= 0) {
        int bestBidPrice = indexToPrice(bestBidIndex);

        if (sellOrder.price > bestBidPrice) {
            break;
        }

        std::list<Order>& ordersAtBestBid = bidLevels[bestBidIndex];
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
            bestBidIndex--;
            while (bestBidIndex >= 0 && bidLevels[bestBidIndex].empty()) {
                bestBidIndex--;
            }
        }
    }
}

void addOrder(Order order) {
    if (order.price < MIN_PRICE || order.price > MAX_PRICE) {
        cout << "Order rejected: price " << order.price << " outside supported range ["
             << MIN_PRICE << ", " << MAX_PRICE << "]" << endl;
        return;
    }

    int idx = priceToIndex(order.price);

    if (order.side == Side::BUY) {
        matchBuyOrder(order);
        if (order.quantity > 0 && order.type == OrderType::LIMIT) {
            bidLevels[idx].push_back(order);
            auto it = std::prev(bidLevels[idx].end());
            orderLookup[order.orderId] = {Side::BUY, idx, it};
            if (idx > bestBidIndex) bestBidIndex = idx;
        }
    } else {
        matchSellOrder(order);
        if (order.quantity > 0 && order.type == OrderType::LIMIT) {
            askLevels[idx].push_back(order);
            auto it = std::prev(askLevels[idx].end());
            orderLookup[order.orderId] = {Side::SELL, idx, it};
            if (idx < bestAskIndex) bestAskIndex = idx;
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
        bidLevels[loc.index].erase(loc.it);
        if (bidLevels[loc.index].empty() && loc.index == bestBidIndex) {
            bestBidIndex--;
            while (bestBidIndex >= 0 && bidLevels[bestBidIndex].empty()) {
                bestBidIndex--;
            }
        }
    } else {
        askLevels[loc.index].erase(loc.it);
        if (askLevels[loc.index].empty() && loc.index == bestAskIndex) {
            bestAskIndex++;
            while (bestAskIndex < PRICE_RANGE && askLevels[bestAskIndex].empty()) {
                bestAskIndex++;
            }
        }
    }

    orderLookup.erase(lookupIt);
    cout << "Order " << orderId << " cancelled." << endl;
}

// Shared by the /orderbook route and local debugging via printBook().
// Only touches indices between the current best price and the edge of the
// book on each side, not the full PRICE_RANGE, so a sparse book stays cheap
// to print even though the underlying array covers the whole price range.
std::string formatBook() {
    std::string result = "";

    result += "----- BIDS -----\n";
    for (int i = bestBidIndex; i >= 0; i--) {
        if (bidLevels[i].empty()) continue;
        result += "Price: " + std::to_string(indexToPrice(i)) + "\n";
        for (const auto& order : bidLevels[i]) {
            result += "  OrderId: " + std::to_string(order.orderId) + ", Qty: " + std::to_string(order.quantity) + "\n";
        }
    }

    result += "----- ASKS -----\n";
    for (int i = bestAskIndex; i < PRICE_RANGE; i++) {
        if (askLevels[i].empty()) continue;
        result += "Price: " + std::to_string(indexToPrice(i)) + "\n";
        for (const auto& order : askLevels[i]) {
            result += "  OrderId: " + std::to_string(order.orderId) + ", Qty: " + std::to_string(order.quantity) + "\n";
        }
    }

    return result;
}

void printBook() {
    cout << formatBook();
}

int main() {
    initDatabase();

    httplib::Server svr;

    svr.Get("/", [](const httplib::Request&, httplib::Response& res) {
        res.set_content(DASHBOARD_HTML, "text/html");
    });

    svr.Get("/orderbook", [](const httplib::Request&, httplib::Response& res) {
        std::string result = formatBook();

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