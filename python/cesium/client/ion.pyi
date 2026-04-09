from enum import IntEnum
from typing import Callable, Generic, Optional, TypeVar

from cesium.async_ import (
    AsyncSystem,
    Future,
    IAssetAccessor,
)
from cesium.geospatial import Cartographic, GlobeRectangle
from cesium.utility import ErrorList

_T = TypeVar("_T")

class AuthenticationMode(IntEnum):
    CesiumIon = ...
    Saml = ...
    SingleUser = ...

class SortOrder(IntEnum):
    Ascending = ...
    Descending = ...

class GeocoderRequestType(IntEnum):
    Search = ...
    Autocomplete = ...

class GeocoderProviderType(IntEnum):
    Google = ...
    Bing = ...
    Default = ...

class Response(Generic[_T]):
    """A response from Cesium ion.

    ``value`` is ``None`` when the response was unsuccessful.
    Check ``error_code`` / ``error_message`` for details.
    """

    value: Optional[_T]
    http_status_code: int
    error_code: str
    error_message: str
    next_page_url: Optional[str]
    previous_page_url: Optional[str]
    def __bool__(self) -> bool: ...

# Concrete aliases used at runtime
ResponseProfile = Response["Profile"]
ResponseDefaults = Response["Defaults"]
ResponseAssets = Response["Assets"]
ResponseAsset = Response["Asset"]
ResponseToken = Response["Token"]
ResponseTokenList = Response["TokenList"]
ResponseNoValue = Response["NoValue"]
ResponseGeocoderResult = Response["GeocoderResult"]
ResponseApplicationData = Response["ApplicationData"]

class ResultConnection:
    value: Optional[Connection]
    errors: ErrorList
    def __bool__(self) -> bool: ...

class NoValue: ...

class ApplicationData:
    authentication_mode: AuthenticationMode
    data_store_type: str
    attribution: str
    def needs_oauth_authentication(self) -> bool: ...

class Asset:
    id: int
    name: str
    description: str
    attribution: str
    type: str
    bytes: int
    date_added: str
    status: str
    percent_complete: int
    def __repr__(self) -> str: ...
    def __eq__(self, other: object) -> bool: ...
    def __ne__(self, other: object) -> bool: ...
    def __hash__(self) -> int: ...

class Assets:
    link: str
    items: list[Asset]

class Token:
    id: str
    name: str
    token: str
    date_added: str
    date_modified: str
    date_last_used: str
    asset_ids: Optional[list[int]]
    is_default: bool
    allowed_urls: Optional[list[str]]
    scopes: list[str]
    def __repr__(self) -> str: ...
    def __eq__(self, other: object) -> bool: ...
    def __ne__(self, other: object) -> bool: ...
    def __hash__(self) -> int: ...

class TokenList:
    items: list[Token]

class ListTokensOptions:
    limit: Optional[int]
    page: Optional[int]
    search: Optional[str]
    sort_by: Optional[str]
    sort_order: Optional[SortOrder]

class ProfileStorage:
    used: int
    available: int
    total: int

class Profile:
    id: int
    scopes: list[str]
    username: str
    email: str
    email_verified: bool
    avatar: str
    storage: ProfileStorage

class DefaultAssets:
    imagery: int
    terrain: int
    buildings: int

class QuickAddRasterOverlay:
    name: str
    asset_id: int
    subscribed: bool

class QuickAddAsset:
    name: str
    object_name: str
    description: str
    asset_id: int
    type: str
    subscribed: bool
    raster_overlays: list[QuickAddRasterOverlay]

class Defaults:
    default_assets: DefaultAssets
    quick_add_assets: list[QuickAddAsset]

class GeocoderAttribution:
    html: str
    show_on_screen: bool

class GeocoderFeature:
    def __init__(self) -> None: ...
    display_name: str
    @property
    def globe_rectangle(self) -> GlobeRectangle: ...
    @property
    def cartographic(self) -> Cartographic: ...

class GeocoderResult:
    attributions: list[GeocoderAttribution]
    features: list[GeocoderFeature]

class LoginToken:
    def __init__(self, token: str, expiration_time: Optional[int]) -> None: ...
    def is_valid(self) -> bool: ...
    @property
    def expiration_time(self) -> Optional[int]: ...
    @property
    def token(self) -> str: ...
    @staticmethod
    def parse_summary(token_string: str) -> dict: ...
    def __bool__(self) -> bool: ...

class Connection:
    def __init__(
        self,
        async_system: AsyncSystem,
        asset_accessor: IAssetAccessor,
        access_token: str,
        app_data: ApplicationData,
        api_url: str = "https://api.cesium.com",
    ) -> None: ...
    # Properties
    @property
    def access_token(self) -> str: ...
    @property
    def refresh_token(self) -> str: ...
    @property
    def api_url(self) -> str: ...
    @property
    def async_system(self) -> AsyncSystem: ...
    @property
    def asset_accessor(self) -> IAssetAccessor: ...
    # Static methods
    @staticmethod
    def id_from_token(token: str) -> Optional[str]: ...
    @staticmethod
    def authorize(
        async_system: AsyncSystem,
        asset_accessor: IAssetAccessor,
        friendly_application_name: str,
        client_id: int,
        redirect_path: str,
        scopes: list[str],
        open_url_callback: Callable[[str], None],
        app_data: ApplicationData,
        ion_api_url: str = "https://api.cesium.com/",
        ion_authorize_url: str = "https://ion.cesium.com/oauth",
    ) -> Future[ResultConnection]: ...
    @staticmethod
    def app_data(
        async_system: AsyncSystem,
        asset_accessor: IAssetAccessor,
        api_url: str = "https://api.cesium.com",
    ) -> Future[Response[ApplicationData]]: ...
    @staticmethod
    def api_url_from_server(
        async_system: AsyncSystem,
        asset_accessor: IAssetAccessor,
        ion_url: str,
    ) -> Future[Optional[str]]: ...
    # Instance async methods
    def me(self) -> Future[Response[Profile]]: ...
    def defaults(self) -> Future[Response[Defaults]]: ...
    def assets(self) -> Future[Response[Assets]]: ...
    def tokens(
        self,
        options: ListTokensOptions = ...,
    ) -> Future[Response[TokenList]]: ...
    def asset(self, asset_id: int) -> Future[Response[Asset]]: ...
    def token(self, token_id: str) -> Future[Response[Token]]: ...
    def next_page(
        self,
        current_page: Response[TokenList],
    ) -> Future[Response[TokenList]]: ...
    def previous_page(
        self,
        current_page: Response[TokenList],
    ) -> Future[Response[TokenList]]: ...
    def create_token(
        self,
        name: str,
        scopes: list[str],
        asset_ids: Optional[list[int]] = None,
        allowed_urls: Optional[list[str]] = None,
    ) -> Future[Response[Token]]: ...
    def modify_token(
        self,
        token_id: str,
        new_name: str,
        new_asset_ids: Optional[list[int]],
        new_scopes: list[str],
        new_allowed_urls: Optional[list[str]],
    ) -> Future[Response[NoValue]]: ...
    def geocode(
        self,
        provider: GeocoderProviderType,
        type: GeocoderRequestType,
        query: str,
    ) -> Future[Response[GeocoderResult]]: ...
