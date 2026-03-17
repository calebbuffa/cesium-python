from __future__ import annotations

from collections.abc import Callable
from typing import Optional

from ..async_ import AsyncSystem, FutureVoid, IAssetAccessor
from ..utility import ErrorList

class OAuth2TokenResponse:
    def __init__(self) -> None: ...
    access_token: str
    refresh_token: Optional[str]

class OAuth2ClientOptions:
    def __init__(self) -> None: ...
    client_id: str
    redirect_path: str
    redirect_port: Optional[int]
    use_json_body: bool

class ResultOAuth2TokenResponse:
    value: Optional[OAuth2TokenResponse]
    errors: ErrorList
    def __bool__(self) -> bool: ...

class FutureResultOAuth2TokenResponse:
    def wait(self) -> ResultOAuth2TokenResponse: ...
    def wait_in_main_thread(self) -> ResultOAuth2TokenResponse: ...
    def is_ready(self) -> bool: ...
    def then_in_worker_thread(
        self, callback: Callable[[ResultOAuth2TokenResponse], None]
    ) -> FutureVoid: ...
    def then_in_main_thread(
        self, callback: Callable[[ResultOAuth2TokenResponse], None]
    ) -> FutureVoid: ...
    def __await__(self) -> ResultOAuth2TokenResponse: ...

class OAuth2PKCE:
    @staticmethod
    def authorize(
        async_system: AsyncSystem,
        asset_accessor: IAssetAccessor,
        friendly_app_name: str,
        client_options: OAuth2ClientOptions,
        scopes: list[str],
        open_url_callback: Callable[[str], None],
        token_endpoint_url: str,
        authorize_base_url: str,
    ) -> FutureResultOAuth2TokenResponse: ...
    @staticmethod
    def refresh(
        async_system: AsyncSystem,
        asset_accessor: IAssetAccessor,
        client_options: OAuth2ClientOptions,
        refresh_base_url: str,
        refresh_token: str,
    ) -> FutureResultOAuth2TokenResponse: ...

def parse_token_payload_summary(token: str) -> dict: ...
def parse_error_response(payload: bytes) -> dict: ...
def fill_with_random_bytes(length: int) -> bytes: ...
