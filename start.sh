#!/usr/bin/env bash
set -e

echo "[start.sh] Starting C++ cache server on 9090..."
./build/cache_server &

echo "[start.sh] Starting FastAPI on 8000..."
exec uvicorn api.main:app --host 0.0.0.0 --port 8000