from __future__ import annotations

from collections.abc import Generator
from typing import Any, Callable, Generic, TypeVar

import numpy as np
import numpy.typing as npt

_T = TypeVar("_T")

class ITaskProcessor:
    def __init__(self) -> None: ...
    def start_task(self, fn: Callable[[], None]) -> None: ...

class NativeTaskProcessor(ITaskProcessor):
    """Pure-C++ thread-pool ITaskProcessor with zero GIL overhead."""
    def __init__(self, num_threads: int = 0) -> None: ...
    def shutdown(self) -> None: ...

class ThreadPool:
    def __init__(self, number_of_threads: int) -> None: ...

class Future(Generic[_T]):
    """A future that resolves to a value of type ``T``.

    At runtime this is the ``Future<py::object>`` pybind11 class.
    Type-specific C++ futures (``FutureVoid``, ``FutureAssetRequest``, …)
    are also available but share the same interface.

    Supports subscript syntax: ``Future[int]``, ``Future[MyResult]``, etc.

    Create via ``AsyncSystem.create_promise().future`` or
    ``AsyncSystem.create_resolved_future(value)``.
    """
    def __class_getitem__(cls, item: _T) -> type[Future[_T]]: ...
    def wait(self) -> _T: ...
    def wait_in_main_thread(self) -> _T: ...
    def is_ready(self) -> bool: ...
    def share(self) -> SharedFuture[_T]: ...
    def then_in_worker_thread(self, callback: Callable[[_T], None]) -> Future[None]: ...
    def then_in_main_thread(self, callback: Callable[[_T], None]) -> Future[None]: ...
    def catch_in_main_thread(self, callback: Callable[[str], _T]) -> Future[_T]: ...
    def __await__(self) -> Generator[Any, None, _T]:
        """Enable ``result = await future`` in asyncio code."""
        ...

class SharedFuture(Generic[_T]):
    """A shared (copyable) future that resolves to a value of type ``T``.

    Supports subscript syntax: ``SharedFuture[int]``, etc.
    """
    def __class_getitem__(cls, item: _T) -> type[SharedFuture[_T]]: ...
    def wait(self) -> _T: ...
    def wait_in_main_thread(self) -> _T: ...
    def is_ready(self) -> bool: ...
    def then_in_worker_thread(self, callback: Callable[[_T], None]) -> Future[None]: ...
    def then_in_main_thread(self, callback: Callable[[_T], None]) -> Future[None]: ...
    def catch_in_main_thread(self, callback: Callable[[str], _T]) -> Future[_T]: ...
    def __await__(self) -> Generator[Any, None, _T]:
        """Enable ``result = await shared_future`` in asyncio code."""
        ...

# Backward-compatible aliases for void variants (no forward ref needed).
FutureVoid = Future[None]
SharedFutureVoid = SharedFuture[None]

class Promise(Generic[_T]):
    """A promise that can be resolved or rejected to complete a ``Future[T]``.

    Supports subscript syntax: ``Promise[int]``, etc.

    Create via ``AsyncSystem.create_promise()`` for a generic promise, or
    ``create_promise_void()`` / ``create_promise_bytes()`` for type-specific ones.
    """
    def __class_getitem__(cls, item: Any) -> type[Promise[Any]]: ...
    def resolve(self, value: _T) -> None: ...
    def reject(self, message: str) -> None: ...
    @property
    def future(self) -> Future[_T]: ...

# Backward-compatible aliases for void/bytes variants.
PromiseVoid = Promise[None]
PromiseBytes = Promise[bytes]

class IAssetResponse:
    def status_code(self) -> int: ...
    def content_type(self) -> str: ...
    def headers(self) -> dict[str, str]: ...
    def data(self) -> npt.NDArray[np.uint8]: ...

class AssetResponse(IAssetResponse):
    def __init__(
        self,
        status_code: int,
        content_type: str,
        headers: dict[str, str] = ...,
        data: bytes = ...,
    ) -> None: ...

class IAssetRequest:
    def method(self) -> str: ...
    def url(self) -> str: ...
    def headers(self) -> dict[str, str]: ...
    def response(self) -> IAssetResponse | None: ...

class AssetRequest(IAssetRequest):
    def __init__(
        self,
        method: str,
        url: str,
        headers: dict[str, str] = ...,
        response: IAssetResponse | None = ...,
    ) -> None: ...

class IAssetAccessor:
    """Abstract asset accessor (HTTP client interface).

    Subclass and override ``get``, ``request``, and ``tick`` to provide
    a custom HTTP backend.
    """

    def __init__(self) -> None: ...
    def get(
        self,
        async_system: AsyncSystem,
        url: str,
        headers: dict[str, str] = ...,
    ) -> Future[IAssetRequest | None]: ...
    def request(
        self,
        async_system: AsyncSystem,
        verb: str,
        url: str,
        headers: dict[str, str] = ...,
        content_payload: bytes = ...,
    ) -> Future[IAssetRequest | None]: ...
    def tick(self) -> None: ...

class CachingAssetAccessor(IAssetAccessor):
    """An ``IAssetAccessor`` backed by an on-disk HTTP cache."""

    def __init__(
        self,
        asset_accessor: IAssetAccessor,
        cache_directory: str,
        requests_per_cache_prune: int = 10000,
    ) -> None: ...

class GunzipAssetAccessor(IAssetAccessor):
    """An ``IAssetAccessor`` that transparently decompresses gzipped responses.

    Wraps another accessor. Useful when servers return gzipped content
    without a proper ``Content-Encoding`` header (e.g. ArcGIS).
    """

    def __init__(self, asset_accessor: IAssetAccessor) -> None: ...

class ArchiveAssetAccessor(IAssetAccessor):
    """An ``IAssetAccessor`` that reads entries from a local .zip/.3tz archive.

    Wraps another accessor.  URLs whose path includes the archive filename
    are served from the archive; all other requests are forwarded to the
    inner accessor.
    """

    def __init__(self, asset_accessor: IAssetAccessor, archive_path: str) -> None: ...

class AsyncSystem:
    def __init__(self, task_processor: ITaskProcessor) -> None: ...
    def dispatch_main_thread_tasks(self) -> None: ...
    def dispatch_one_main_thread_task(self) -> None: ...
    def create_thread_pool(self, number_of_threads: int) -> ThreadPool: ...
    def create_resolved_future_asset_request(
        self, request: IAssetRequest
    ) -> Future[IAssetRequest | None]: ...
    def create_promise_void(self) -> Promise[None]: ...
    def create_promise_asset_request(self) -> Promise[IAssetRequest | None]: ...
    def create_promise_bytes(self) -> Promise[bytes]: ...
    def create_promise(self) -> Promise[Any]:
        """Create a generic Promise that can be resolved with any Python object."""
        ...
    def create_resolved_future(self, value: Any) -> Future[Any]:
        """Create a Future already resolved with the given value."""
        ...
    def run_in_worker_thread(self, callback: Callable[[], None]) -> Future[None]: ...
    def run_in_main_thread(self, callback: Callable[[], None]) -> Future[None]: ...
    def run_in_thread_pool(
        self, thread_pool: ThreadPool, callback: Callable[[], None]
    ) -> Future[None]: ...
    def all_void(self, futures: list[Future[None]]) -> Future[None]: ...

class CaseInsensitiveCompare:
    def __init__(self) -> None: ...
    def __call__(self, a: str, b: str) -> bool: ...

class CacheResponse:
    def __init__(
        self, status_code: int, headers: dict[str, str], data: bytes
    ) -> None: ...
    status_code: int
    @property
    def headers(self) -> dict[str, str]: ...
    @headers.setter
    def headers(self, value: dict[str, str]) -> None: ...
    @property
    def data(self) -> bytes: ...
    @data.setter
    def data(self, value: bytes) -> None: ...

class CacheRequest:
    def __init__(self, headers: dict[str, str], method: str, url: str) -> None: ...
    @property
    def headers(self) -> dict[str, str]: ...
    @headers.setter
    def headers(self, value: dict[str, str]) -> None: ...
    method: str
    url: str

class CacheItem:
    def __init__(
        self,
        expiry_time: int,
        cache_request: CacheRequest,
        cache_response: CacheResponse,
    ) -> None: ...
    expiry_time: int
    cache_request: CacheRequest
    cache_response: CacheResponse

class ICacheDatabase:
    def get_entry(self, key: str) -> CacheItem | None: ...
    def store_entry(
        self,
        key: str,
        expiry_time: int,
        url: str,
        request_method: str,
        request_headers: dict[str, str],
        status_code: int,
        response_headers: dict[str, str],
        response_data: bytes,
    ) -> None: ...
    def prune(self) -> None: ...
    def clear_all(self) -> None: ...

class SqliteCache(ICacheDatabase):
    def __init__(self, database_name: str, max_items: int = 4096) -> None: ...

class UpdatedToken:
    def __init__(self) -> None: ...
    token: str
    authorization_header: str

class CesiumIonAssetAccessor(IAssetAccessor):
    def notify_owner_is_being_destroyed(self) -> None: ...

class NetworkAssetDescriptor:
    def __init__(self) -> None: ...
    url: str
    headers: list[tuple[str, str]]
    def load_from_network(
        self, async_system: AsyncSystem, asset_accessor: IAssetAccessor
    ) -> Future[IAssetRequest | None]: ...
    def load_bytes_from_network(
        self, async_system: AsyncSystem, asset_accessor: IAssetAccessor
    ) -> Future[bytes]: ...
    def __eq__(self, other: object) -> bool: ...

# Aliases that reference IAssetRequest (must come after the class definition).
FutureAssetRequest = Future[IAssetRequest | None]
SharedFutureAssetRequest = SharedFuture[IAssetRequest | None]
PromiseAssetRequest = Promise[IAssetRequest | None]

def read_file_bytes(path: str) -> bytes:
    """Read a file into bytes with the GIL released during I/O."""
    ...
