"""Integration tests for the pybind11 cache extension.

Build the extension first with ``python setup.py build_ext --inplace``.
"""

import unittest

import cache_engine


class CacheEngineTests(unittest.TestCase):
    def test_lru_eviction_and_read_refresh(self):
        engine = cache_engine.LiveEngine(2, 1, 1)
        engine.put("first", "1")
        engine.put("second", "2")
        self.assertEqual(engine.get("first"), "1")  # Refresh "first".

        engine.put("third", "3")

        self.assertEqual(engine.get("first"), "1")
        self.assertIsNone(engine.get("second"))
        self.assertEqual(engine.get("third"), "3")

    def test_shards_preserve_total_capacity(self):
        # Capacity 3 over two shards used to allocate one entry to each shard,
        # silently reducing the usable capacity to 2. A broad key set exercises
        # both shards and must retain exactly the requested three entries.
        engine = cache_engine.LiveEngine(3, 2, 1)
        keys = [f"key-{index}" for index in range(100)]
        for index, key in enumerate(keys):
            engine.put(key, str(index))

        remaining = sum(engine.get(key) is not None for key in keys)
        self.assertEqual(remaining, 3)

    def test_invalid_worker_count_fails_without_hanging(self):
        with self.assertRaises(Exception):
            cache_engine.LiveEngine(1, 1, 0)


if __name__ == "__main__":
    unittest.main()
