# Deribit API Client

This project involves an order execution and management system to trade on Deribit Test. It provides a C++ client for interacting with the Deribit cryptocurrency derivatives exchange API. It includes both REST API and WebSocket functionality for real-time order book data.

## Features

- REST API client for Deribit
  - Authentication
  - Place orders
  - Modify orders
  - Cancel Orders
  - Get positions
  - Get order book data
  
- WebSocket client for real-time order book updates
- Command-line interface for interacting with the API

## Prerequisites

- C++11 or later, libcurl, JsonCpp, Boost (Beast, Asio) and OpenSSL.

## Installation

1. Clone the repository:
   ```
   git clone https://github.com/yourusername/deribit-api-client.git
   cd deribit-api-client
   ```

2. Install the required dependencies. On Ubuntu, you can use:
   ```
   sudo apt-get install libcurl4-openssl-dev libjsoncpp-dev libboost-all-dev libssl-dev
   ```

3. Compile the project:
   ```
   g++ -std=c++11 main.cpp -o deribit_client -lcurl -ljsoncpp
   g++ -std=c++11 websocket.cpp -o websocket_client -lboost_system -lssl -lcrypto
   ```

## How to Use it?

### REST API Client

Run the compiled REST API client:

```
./main
```

Follow the on-screen menu to interact with the Deribit API.

### WebSocket Client

Run the compiled WebSocket client with an instrument name:

```
./websocket BTC-PERPETUAL
```

It will connect to the Deribit WebSocket API and stream real-time order book data for the specified instrument.

## File Structure

- `main.cpp`: Contains the REST API client implementation and command-line interface.
- `websocket.cpp`: Contains the WebSocket client implementation.


