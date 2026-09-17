Verified working end-to-end
Started server: ./orderbook → prints "Order book server running on port 8080..."
Browser GET http://localhost:8080/orderbook → showed seeded order correctly
Separate terminal: curl.exe -X POST http://localhost:8080/order -d "orderId=2&price=105&quantity=5&side=SELL" → "Order added successfully."
Refreshed browser GET → both orders now visible correctly (BIDS: OrderId 1 @ 100; ASKS: OrderId 2 @ 105, correctly NOT matched since 105 > 100).md

THE BIG DEBUGGING STORY: C vs C++ compilation mismatch

This is genuinely worth retelling in an interview as a real debugging narrative:

First attempt: compiled with g++ src/main.cpp src/sqlite3.c -o orderbook -lws2_32 → hit cpp-httplib doesn't support Windows 8 or lower (unrelated leftover issue, already fixed via _WIN32_WINNT positioning from Session 10).
Real SQLite errors surfaced: invalid conversion from 'const void*' to 'const char*' — a genuine compile error inside sqlite3.c itself.
First fix attempted: added -fpermissive flag, which downgraded that specific error to a warning — compile "succeeded," but this was treating a symptom, not the actual cause.
Ran the program — nothing worked. initDatabase()'s print statements never appeared, and orderbook.db never got created. Added explicit debug-checkpoint prints (cout << "initDatabase() called") to narrow down where execution was actually failing.
Scrolled back through terminal output and discovered the real problem: hundreds of genuine compile errors (not just warnings) buried in the scrollback, e.g. invalid use of incomplete type 'struct SrcList_item' — meaning the program that "ran" was actually a stale, previously-successful executable, not a fresh build (same class of mistake as earlier sessions — always verify a fresh compile actually succeeded before trusting its output).
Root cause identified: sqlite3.c is C source code, but g++ (the C++ compiler) was being used to compile it — and g++ forces .c files to be compiled under strict C++ rules regardless of extension. C++ is stricter than C about certain patterns (like referencing a struct before its full definition), which is exactly why "incomplete type" errors appeared — this is legal, working C code being rejected by C++'s stricter type system.
Real fix: compile sqlite3.c separately, using the actual C compiler (gcc, not g++), producing a compiled object file — then link that finished object file into the C++ build, rather than asking g++ to compile raw C source directly: