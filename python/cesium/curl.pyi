from . import async_  # pyright: ignore[reportAttributeAccessIssue]

class CurlAssetAccessorOptions:
    user_agent: str
    request_headers: list[tuple[str, str]]
    allow_directory_creation: bool
    certificate_path: str
    certificate_file: str
    do_global_init: bool

class CurlAssetAccessor(async_.IAssetAccessor):
    @property
    def options(self) -> CurlAssetAccessorOptions: ...
    def __init__(self, options: CurlAssetAccessorOptions = ...) -> None: ...
