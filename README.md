Verified working end-to-end
Started server: ./orderbook → prints "Order book server running on port 8080..."
Browser GET http://localhost:8080/orderbook → showed seeded order correctly
Separate terminal: curl.exe -X POST http://localhost:8080/order -d "orderId=2&price=105&quantity=5&side=SELL" → "Order added successfully."
Refreshed browser GET → both orders now visible correctly (BIDS: OrderId 1 @ 100; ASKS: OrderId 2 @ 105, correctly NOT matched since 105 > 100).md