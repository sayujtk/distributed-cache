from fastapi import FastAPI, HTTPException
from pydantic import BaseModel
import socket
import json
from typing import Any, Optional

CACHE_HOST = "127.0.0.1"
CACHE_PORT = 9090

ALLOWED_NODES = {"node1", "node2", "node3"}

app = FastAPI(title="Distributed LRU Cache API", version="1.0.0")


class PutBody(BaseModel):
    value: Any


def _encode_value(v: Any) -> str:
    # Convert non-string JSON types to compact JSON.
    # Strings remain plain strings.
    if isinstance(v, (dict, list, int, float, bool)) or v is None:
        return json.dumps(v, separators=(",", ":"))
    return str(v)


def tcp_request(line: str, timeout_sec: float = 2.0) -> str:
    """
    Send a single line command to the C++ cache server and read one response.
    The C++ server is line-based: each request ends with '\n'.
    """
    if not line.endswith("\n"):
        line += "\n"

    try:
        with socket.create_connection((CACHE_HOST, CACHE_PORT), timeout=timeout_sec) as s:
            s.sendall(line.encode("utf-8"))
            data = s.recv(4096)
            if not data:
                raise RuntimeError("Empty response from cache server")
            return data.decode("utf-8").strip()
    except OSError as e:
        raise HTTPException(status_code=503, detail=f"Cache server unavailable: {e}")


def parse_ok_value(resp: str) -> Optional[str]:
    # Expected: "OK VALUE <value...>" or "OK MISS"
    if resp == "OK MISS":
        return None
    if resp.startswith("OK VALUE "):
        return resp[len("OK VALUE ") :]
    raise HTTPException(status_code=500, detail=f"Unexpected cache response: {resp}")


@app.get("/health")
def health():
    resp = tcp_request("HEALTH")
    if resp != "OK":
        raise HTTPException(status_code=500, detail=f"Unhealthy: {resp}")
    return {"status": "ok"}


@app.get("/stats")
def stats():
    resp = tcp_request("STATS")
    # Example: OK SINGLE_SIZE 3 NODE1_SIZE 1 NODE2_SIZE 0 NODE3_SIZE 0
    if not resp.startswith("OK "):
        raise HTTPException(status_code=500, detail=f"Unexpected stats response: {resp}")

    parts = resp.split()
    out = {}
    for i in range(1, len(parts) - 1, 2):
        k = parts[i].lower()
        v = parts[i + 1]
        try:
            out[k] = int(v)
        except ValueError:
            out[k] = v
    return out


# -------- Single node endpoints --------
@app.put("/single/cache/{key}")
def single_put(key: str, body: PutBody):
    value = _encode_value(body.value)
    resp = tcp_request(f"SINGLE PUT {key} {value}")
    if resp != "OK":
        raise HTTPException(status_code=500, detail=resp)
    return {"ok": True, "mode": "single", "key": key}


@app.get("/single/cache/{key}")
def single_get(key: str):
    resp = tcp_request(f"SINGLE GET {key}")
    value = parse_ok_value(resp)
    if value is None:
        raise HTTPException(status_code=404, detail="Key not found")
    return {"mode": "single", "key": key, "value": value}


@app.delete("/single/cache/{key}")
def single_del(key: str):
    resp = tcp_request(f"SINGLE DEL {key}")
    if resp == "OK REMOVED":
        return {"ok": True, "removed": True}
    if resp == "OK NOT_FOUND":
        return {"ok": True, "removed": False}
    raise HTTPException(status_code=500, detail=resp)


# -------- Distributed node endpoints (explicit node targeting) --------
@app.put("/nodes/{node_id}/cache/{key}")
def node_put(node_id: str, key: str, body: PutBody):
    if node_id not in ALLOWED_NODES:
        raise HTTPException(status_code=400, detail="Invalid node_id. Use node1/node2/node3.")
    value = _encode_value(body.value)
    resp = tcp_request(f"NODE {node_id} PUT {key} {value}")
    if resp != "OK":
        raise HTTPException(status_code=500, detail=resp)
    return {"ok": True, "mode": "distributed", "node": node_id, "key": key}


@app.get("/nodes/{node_id}/cache/{key}")
def node_get(node_id: str, key: str):
    if node_id not in ALLOWED_NODES:
        raise HTTPException(status_code=400, detail="Invalid node_id. Use node1/node2/node3.")
    resp = tcp_request(f"NODE {node_id} GET {key}")
    value = parse_ok_value(resp)
    if value is None:
        raise HTTPException(status_code=404, detail="Key not found (maybe invalidated)")
    return {"mode": "distributed", "node": node_id, "key": key, "value": value}


@app.delete("/nodes/{node_id}/cache/{key}")
def node_del(node_id: str, key: str):
    if node_id not in ALLOWED_NODES:
        raise HTTPException(status_code=400, detail="Invalid node_id. Use node1/node2/node3.")
    resp = tcp_request(f"NODE {node_id} DEL {key}")
    if resp == "OK REMOVED":
        return {"ok": True, "removed": True}
    if resp == "OK NOT_FOUND":
        return {"ok": True, "removed": False}
    raise HTTPException(status_code=500, detail=resp)