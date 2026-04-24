FROM python:3.10-slim

# Install C++ build tools
RUN apt-get update && apt-get install -y --no-install-recommends \
    g++ \
    make \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

# Copy source
COPY . /app

# Build C++ cache server
RUN mkdir -p build && \
    g++ -std=c++11 -pthread src/lru_cache.cpp src/benchmarking.cpp src/distributed_cache.cpp src/cache_server.cpp -o build/cache_server

# Install python dependencies
RUN pip install --no-cache-dir -r api/requirements.txt

# Start script
RUN chmod +x /app/start.sh

EXPOSE 8000 9090

CMD ["/app/start.sh"]