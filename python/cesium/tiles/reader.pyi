from typing import Any, Optional

from ..async_ import AsyncSystem, Future, IAssetAccessor
from ..json.reader import JsonReaderOptions
from . import Subtree

class SubtreeReader:
    options: JsonReaderOptions

    def __init__(self) -> None: ...
    def read_from_json_summary(self, subtree_bytes: bytes) -> dict[str, Any]: ...

class ReadSubtreeResult:
    value: Optional[Subtree]
    errors: list[str]
    warnings: list[str]
    def __bool__(self) -> bool: ...

FutureReadSubtreeResult = Future[ReadSubtreeResult]

class SubtreeFileReader:
    options: JsonReaderOptions

    def __init__(self) -> None: ...
    def load(
        self,
        async_system: AsyncSystem,
        asset_accessor: IAssetAccessor,
        url: str,
        headers: list[tuple[str, str]] = ...,
    ) -> FutureReadSubtreeResult: ...

def read_subtree_json_summary(subtree_bytes: bytes) -> dict[str, Any]: ...
