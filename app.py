from fastapi import FastAPI, HTTPException
from pydantic import BaseModel, Field
import cache_engine

app = FastAPI(
    title="Concurrent In-Memory Key-Value Cache ",
    description="O(1) Sharded LRU Cache with Custom Thread Pool & Lock Striping",
    version="1.0.0",
)

# Initialize live engine: capacity=10000, shards=32, worker_threads=8
engine = cache_engine.LiveEngine(10000, 32, 8)


class PutRequest(BaseModel):
    key: str = Field(..., example="user_101")
    value: str = Field(..., example="Sanjith")


class BenchmarkRequest(BaseModel):
    num_threads: int = Field(8, ge=1, le=32, example=8)
    total_operations: int = Field(500000, ge=1000, le=5000000, example=500000)
    read_write_ratio: int = Field(80, ge=0, le=100, example=80)
    num_shards: int = Field(32, ge=2, le=256, example=32)
    cache_capacity: int = Field(10000, ge=100, le=1000000, example=10000)


@app.put("/api/v1/cache", tags=["Mode A: Live Cache CRUD"])
def put_key(req: PutRequest):
    engine.put(req.key, req.value)
    return {"status": "OK", "key": req.key, "value": req.value}


@app.get("/api/v1/cache/{key}", tags=["Mode A: Live Cache CRUD"])
def get_key(key: str):
    val = engine.get(key)
    if val is None:
        raise HTTPException(status_code=404, detail="Key not found in cache")
    return {"status": "HIT", "key": key, "value": val}


@app.post("/api/v1/benchmark", tags=["Mode B: Live Contention Benchmark"])
def run_benchmark(req: BenchmarkRequest):
    res = engine.benchmark(
        req.num_threads,
        req.total_operations,
        req.read_write_ratio,
        req.num_shards,
        req.cache_capacity,
    )
    return {
        "workload": {
            "threads": req.num_threads,
            "total_operations": req.total_operations,
            "read_ratio": f"{req.read_write_ratio}%",
            "shards": req.num_shards,
        },
        "global_lock_results": {
            "execution_time_ms": round(res["global_time_ms"], 2),
            "throughput_ops_per_sec": res["global_throughput_ops_sec"],
        },
        "sharded_lock_results": {
            "execution_time_ms": round(res["sharded_time_ms"], 2),
            "throughput_ops_per_sec": res["sharded_throughput_ops_sec"],
        },
        "improvement": {
            "speedup_factor": f"{res['speedup_factor']:.2f}x",
            "throughput_increase_percent": f"+{res['throughput_increase_pct']:.1f}%",
        },
    }
