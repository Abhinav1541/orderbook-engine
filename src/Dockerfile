FROM gcc:12

WORKDIR /app

COPY src/ ./src/

RUN gcc -c src/sqlite3.c -o sqlite3.o
RUN g++ src/main.cpp sqlite3.o -o orderbook

EXPOSE 8080

CMD ["./orderbook"]