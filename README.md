# Distributed LRU Cache System (C++ + FastAPI + Docker)

A high-performance, thread-safe distributed cache project in C++ with a FastAPI interface and Dockerized runtime.

---

## 🚀 What this project includes

- **Single-node LRU cache (C++)**
  - O(1) `get` / `put` using hash map + doubly linked list
  - LRU eviction policy
  - Thread-safe with mutex

- **Distributed cache simulation**
  - Multi-node architecture
  - Invalidation-based consistency protocol
  - Peer-to-peer style invalidation flow

- **FastAPI integration (Python)**
  - HTTP endpoints for cache operations
  - Converts HTTP requests into TCP commands for C++ cache server

- **Docker support**
  - One container runs both:
    1. C++ TCP cache server
    2. FastAPI server (Uvicorn)

---

## 🧠 Architecture (simple)

```text
Client (Browser / Swagger / curl)
            |
            | HTTP (REST)
            v
     FastAPI (Python)      :8000
            |
            | TCP commands
            v
   C++ Cache Server         :9090
            |
            v
     In-memory cache (LRU)
```

---

## ⚙️ Why long-running C++ server is important

Cache data is stored in RAM.  
If the C++ process exits after each request, cache is lost every time.

So we run a **long-lived TCP server**:
- FastAPI sends `PUT/GET/DELETE` commands over TCP
- C++ server stays alive and retains cache state across requests

---

## 📁 Project Structure

```text
distributed-cache/
├── include/
│   ├── benchmarking.h
│   ├── distributed_cache.h
│   └── lru_cache.h
├── src/
│   ├── benchmarking.cpp
│   ├── cache_server.cpp
│   ├── distributed_cache.cpp
│   ├── lru_cache.cpp
│   └── main.cpp
├── api/
│   ├── main.py
│   └── requirements.txt
├── Dockerfile
├── .dockerignore
├── start.sh
├── README.md
└── .gitignore
```

---

## 🐳 Run with Docker (recommended)

### 1) Build image
```bash
docker build -t distributed-cache-api .
```

### 2) Run container
```bash
docker run --rm -p 8000:8000 -p 9090:9090 distributed-cache-api
```

### 3) Open Swagger UI
```text
http://127.0.0.1:8000/docs
```

---

## 🔌 API Endpoints (FastAPI)

### Health / stats
- `GET /health`
- `GET /stats`

### Single-node cache operations
- `PUT /single/cache/{key}`
- `GET /single/cache/{key}`
- `DELETE /single/cache/{key}`

### Distributed cache operations
- `PUT /distributed/cache/{key}`
- `GET /distributed/cache/{key}`
- `DELETE /distributed/cache/{key}`

> FastAPI talks to C++ cache server using TCP (port 9090).

---

## 🛠 Local C++ build (without Docker)

```bash
g++ -std=c++11 -pthread src/lru_cache.cpp src/benchmarking.cpp src/distributed_cache.cpp src/main.cpp -o build/lru_cache
./build/lru_cache
```

---

## 📊 Example Performance (from benchmarks)

### Single-node LRU
- Throughput: ~1.6M ops/sec
- Hit rate: ~98%
- Avg latency: ~0.0001 ms

### Multi-threaded
- 2 threads: ~1.05M ops/sec
- 4 threads: ~0.95M ops/sec
- Thread-safe under concurrent access

---

## 🔄 Consistency model (distributed simulation)

Uses **invalidation-based consistency**:

1. Node A updates key `X`
2. Node A broadcasts `INVALIDATE X` to peers
3. Peers remove stale `X`
4. Next read on peers becomes cache miss and refreshes

**Trade-off:** simple + consistent, but may increase misses and invalidation traffic.

---

## 💡 Key learnings

- O(1) LRU design with hash map + list
- Mutex-based thread safety
- Distributed consistency trade-offs
- API-to-TCP bridging pattern
- Dockerized multi-process startup via `start.sh`

---

## 📜 License

Educational use.