# miniRedis

`miniRedis` is a small, high-performance in-memory key-value cache service. It combines a FastAPI HTTP interface with a C++17 cache engine exposed to Python through pybind11.

## Highlights

- **LRU eviction:** Least-recently-used entries are evicted when the cache reaches its capacity.
- **Sharded cache:** Keys are distributed across independently locked shards to reduce contention under concurrent access.
- **Custom thread pool:** Cache operations are dispatched through a C++ worker pool.
- **Benchmark endpoint:** Compares a single global-lock cache against the sharded implementation for configurable concurrent workloads.
- **Simple HTTP API:** Store and retrieve string key-value pairs with FastAPI.

## Tech stack

- Python / FastAPI
- C++17
- pybind11

## Getting started

Prerequisites: Python 3.10+ and a C++17-compatible compiler.

```bash
git clone https://github.com/<your-username>/miniRedis.git
cd miniRedis
python -m venv .venv
source .venv/bin/activate  # Windows: .venv\\Scripts\\activate
pip install -r requirements.txt
python setup.py build_ext --inplace
uvicorn app:app --reload
```

The API will be available at `http://127.0.0.1:8000`, with interactive documentation at `http://127.0.0.1:8000/docs`.

Run the extension tests after building it:

```bash
python -m unittest discover -s tests -v
```

## API

### Store a value

```bash
curl -X PUT http://127.0.0.1:8000/api/v1/cache \\
  -H "Content-Type: application/json" \\
  -d '{"key":"user_101","value":"Sanjith"}'
```

### Retrieve a value

```bash
curl http://127.0.0.1:8000/api/v1/cache/user_101
```

### Run a contention benchmark

```bash
curl -X POST http://127.0.0.1:8000/api/v1/benchmark \\
  -H "Content-Type: application/json" \\
  -d '{"num_threads":8,"total_operations":500000,"read_write_ratio":80,"num_shards":32,"cache_capacity":10000}'
```

The benchmark response reports execution time, throughput, and the measured improvement from lock striping.

## Project layout

```text
app.py                 FastAPI application and API routes
src/bindings.cpp       pybind11 bindings and benchmark implementation
include/lru_cache.hpp  Thread-safe LRU cache
include/sharded_cache.hpp  Lock-striped cache wrapper
include/thread_pool.hpp    C++ worker-thread pool
```

## Notes

This project is designed as a learning and benchmarking implementation. Cache contents are held only in memory and are lost when the service stops.
