from setuptools import setup
from pybind11.setup_helpers import Pybind11Extension, build_ext

ext_modules = [
    Pybind11Extension(
        "cache_engine",
        ["src/bindings.cpp"],
        include_dirs=["include"],
        cxx_std=17,
        extra_compile_args=["-O3", "-pthread"],
    ),
]

setup(
    name="cache_engine",
    version="1.0",
    ext_modules=ext_modules,
    cmdclass={"build_ext": build_ext},
)
