FROM gcc:13 AS build
WORKDIR /app
COPY src/ ./src/
RUN gcc -c src/sqlite3.c -o sqlite3.o -O2
RUN g++ src/main.cpp sqlite3.o -o orderbook -O2 -lpthread -static-libgcc -static-libstdc++

FROM debian:bookworm-slim
WORKDIR /app
COPY --from=build /app/orderbook .
EXPOSE 8080
CMD ["./orderbook"]
