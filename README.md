# 💸 PIX — Distributed Payment System (INF01151 - Sistemas Operacionais II)

Implementation of the **Etapa 1** assignment from *INF01151 - Sistemas Operacionais II (UFRGS)*.  
This project simulates a distributed payment service similar to **PIX**, using **UDP sockets** and **multithreading** in C++.

The system is composed of two programs:
- **Server (`servidor`)** — handles client discovery, transaction processing, and state management.
- **Client (`cliente`)** — discovers the server, sends transfer requests, and displays results.

---

## 📘 Overview

Each client can send money transfers to other clients through the central server.  
The server processes all transactions concurrently (one thread per request), updating:
- individual client balances,  
- transaction count,  
- total transferred amount, and  
- total bank balance.  

All communication between processes uses **UDP**, and clients resend messages automatically in case of packet loss or timeout.

---

## 🧩 Features

### Server
- Receives and processes requests concurrently.
- Manages a client table with balances and last request IDs.
- Handles duplicate or out-of-order messages.
- Displays transactions in real time.
- Thread-safe access to shared data via reader/writer synchronization.

### Client
- Discovers the server automatically via broadcast (`DESCOBERTA` message).
- Sends transfer requests (`REQUISIÇÃO` messages).
- Handles retransmission on timeout.
- Displays confirmation and updated balance after each operation.
- Gracefully exits with `Ctrl+C` or `Ctrl+D`.

---

## ⚙️ Requirements

- **Operating System:** Linux (Ubuntu/Debian recommended)
- **Compiler:** `g++` with C++17 or later
- **Build tools:** `make`

You can install them using:
```bash
sudo apt update
sudo apt install build-essential
```

---

## 🏗️ Building

Clone the repository and build the binaries with:
```bash
git clone https://github.com/murilosterchile/PIX---Sistemas-operacionais-2.git
cd PIX---Sistemas-operacionais-2
make
```

This will generate the executables:
- `./servidor`
- `./cliente`

To clean build files:
```bash
make clean
```

---

## 🚀 Running the System

### 1. Start the Server

Run the server on a chosen UDP port (e.g., 4000):

```bash
./servidor 4000
```

Expected output (initial state):
```
2025-09-11 18:37:00 num transactions 0 total transferred 0 total balance 0
```

The server will now listen for **broadcast discovery messages** and process incoming requests from clients.

---

### 2. Start One or More Clients

Each client must be started in a separate terminal or machine within the same network segment:

```bash
./cliente 4000
```

Expected output (after discovery):
```
2025-09-11 18:37:01 server addr 10.1.1.20
```

Then you can type transactions manually in the format:

```
<DEST_IP> <VALUE>
```

Example:
```
10.1.1.3 10
```

This sends a transfer of **10 units** to the client with IP `10.1.1.3`.  
To simply query your balance:
```
10.1.1.3 0
```

After processing, the client prints:
```
2025-09-11 18:37:02 server 10.1.1.20 id req 1 dest 10.1.1.3 value 10 new balance 90
```

---

### 3. Example Workflow

**Terminal 1 (server):**
```
./servidor 4000
2025-09-11 18:37:00 num transactions 0 total transferred 0 total balance 0
2025-09-11 18:37:01 client 10.1.1.2 id req 1 dest 10.1.1.3 value 10
num transactions 1 total transferred 10 total balance 300
```

**Terminal 2 (client 1):**
```
./cliente 4000
2025-09-11 18:37:00 server addr 10.1.1.20
10.1.1.3 10
2025-09-11 18:37:01 server 10.1.1.20 id req 1 dest 10.1.1.3 value 10 new balance 90
```

**Terminal 3 (client 2):**
```
./cliente 4000
2025-09-11 18:37:00 server addr 10.1.1.20
```

---

## 🧠 Implementation Notes

The system is modularized into:
- **`common/`** — Shared message structures and utility functions.
- **`server/`** — Discovery, transaction processing, synchronization logic.
- **`client/`** — Discovery, request handling, input/output threads.

Key components:
- UDP sockets for communication.
- Mutexes and condition variables for synchronization.
- Threads for concurrent processing.

---

## 🧪 Testing Locally

You can simulate multiple clients on the same machine using `tmux` or multiple terminals:

```bash
# Terminal 1
./servidor 4000

# Terminal 2
./cliente 4000

# Terminal 3
./cliente 4000
```

If using `localhost`, each client must have its own IP alias to simulate multiple nodes:
```bash
sudo ip addr add 127.0.0.2/8 dev lo
sudo ip addr add 127.0.0.3/8 dev lo
```

Then you can send requests to these IPs to mimic different clients.

---

## 🧰 Cleaning Up (Linux)

To remove the temporary loopback IPs after testing:
```bash
sudo ip addr del 127.0.0.2/8 dev lo
sudo ip addr del 127.0.0.3/8 dev lo
```
