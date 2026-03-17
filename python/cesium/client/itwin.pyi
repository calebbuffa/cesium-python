from __future__ import annotations

from enum import IntEnum
from typing import Callable, Generic, Iterator, Optional, TypeVar

from cesium.async_ import (
    AsyncSystem,
    Future,
    IAssetAccessor,
)
from cesium.client.common import OAuth2ClientOptions as OAuth2ClientOptions
from cesium.geometry import AxisAlignedBox
from cesium.geospatial import GlobeRectangle
from cesium.utility import ErrorList, Uri, UriQuery
from cesium.vector_data import GeoJsonFeature

_T = TypeVar("_T")

class ITwinStatus(IntEnum):
    Unknown = ...
    Active = ...
    Inactive = ...
    Trial = ...

class IModelState(IntEnum):
    Unknown = ...
    Initialized = ...
    NotInitialized = ...

class IModelMeshExportStatus(IntEnum):
    Unknown = ...
    NotStarted = ...
    InProgress = ...
    Complete = ...
    Invalid = ...

class IModelMeshExportType(IntEnum):
    Unknown = ...
    ITwin3DFT = ...
    IModel = ...
    Cesium = ...
    Cesium3DTiles = ...

class ITwinRealityDataClassification(IntEnum):
    Unknown = ...
    Terrain = ...
    Imagery = ...
    Pinned = ...
    Model = ...
    Undefined = ...

class CesiumCuratedContentType(IntEnum):
    Unknown = ...
    Cesium3DTiles = ...
    Gltf = ...
    Imagery = ...
    Terrain = ...
    Kml = ...
    Czml = ...
    GeoJson = ...

class CesiumCuratedContentStatus(IntEnum):
    Unknown = ...
    AwaitingFiles = ...
    NotStarted = ...
    InProgress = ...
    Complete = ...
    Error = ...
    DataError = ...

class Result(Generic[_T]):
    """Result of an operation, with optional value and error information."""

    value: Optional[_T]
    errors: ErrorList
    def __bool__(self) -> bool: ...

class PagedList(Generic[_T]):
    """A paginated list of items from the iTwin API."""
    def __getitem__(self, index: int) -> _T: ...
    def __len__(self) -> int: ...
    def __iter__(self) -> Iterator[_T]: ...
    def has_next_url(self) -> bool: ...
    def has_prev_url(self) -> bool: ...

class UserProfile:
    id: str
    display_name: str
    given_name: str
    surname: str
    email: str

class ITwin:
    id: str
    itwin_class: str
    sub_class: str
    type: str
    number: str
    display_name: str
    status: ITwinStatus

class IModel:
    id: str
    display_name: str
    name: str
    description: str
    state: IModelState
    extent: GlobeRectangle

class IModelMeshExport:
    id: str
    display_name: str
    status: IModelMeshExportStatus
    export_type: IModelMeshExportType

class ITwinRealityData:
    id: str
    display_name: str
    description: str
    classification: ITwinRealityDataClassification
    type: str
    extent: GlobeRectangle
    authoring: bool

class CesiumCuratedContentAsset:
    id: int
    type: CesiumCuratedContentType
    name: str
    description: str
    attribution: str
    status: CesiumCuratedContentStatus

class GeospatialFeatureCollectionExtents:
    spatial: list[AxisAlignedBox]
    coordinate_reference_system: str
    temporal: list[tuple[str, str]]
    temporal_reference_system: str

class GeospatialFeatureCollection:
    id: str
    title: str
    description: str
    extents: Optional[GeospatialFeatureCollectionExtents]
    crs: list[str]
    storage_crs: str
    storage_crs_coordinate_epoch: Optional[float]

class QueryParameters:
    search: Optional[str]
    order_by: Optional[str]
    top: Optional[int]
    skip: Optional[int]
    def add_to_query(self, uri_query: UriQuery) -> None: ...
    def add_to_uri(self, uri: Uri) -> None: ...

class AuthenticationToken:
    def __init__(
        self,
        token: str,
        name: str,
        user_name: str,
        scopes: list[str],
        not_valid_before: int,
        expires: int,
    ) -> None: ...
    def is_valid(self) -> bool: ...
    @property
    def expiration_time(self) -> int: ...
    @property
    def token(self) -> str: ...
    @property
    def token_header(self) -> str: ...
    @staticmethod
    def parse_summary(token_str: str) -> dict: ...

class ITwinConnection:
    def __init__(
        self,
        async_system: AsyncSystem,
        asset_accessor: IAssetAccessor,
        authentication_token: AuthenticationToken,
        refresh_token: Optional[str],
        client_options: OAuth2ClientOptions,
    ) -> None: ...
    @property
    def authentication_token(self) -> AuthenticationToken: ...
    @authentication_token.setter
    def authentication_token(self, value: AuthenticationToken) -> None: ...
    @property
    def refresh_token(self) -> Optional[str]: ...
    @refresh_token.setter
    def refresh_token(self, value: Optional[str]) -> None: ...
    # Static methods
    @staticmethod
    def authorize(
        async_system: AsyncSystem,
        asset_accessor: IAssetAccessor,
        friendly_application_name: str,
        client_id: str,
        redirect_path: str,
        redirect_port: Optional[int],
        scopes: list[str],
        open_url_callback: Callable[[str], None],
    ) -> Future[Result[ITwinConnection]]: ...
    # Instance async methods
    def me(self) -> Future[Result[UserProfile]]: ...
    def itwins(
        self,
        params: QueryParameters = ...,
    ) -> Future[Result[PagedList[ITwin]]]: ...
    def imodels(
        self,
        itwin_id: str,
        params: QueryParameters = ...,
    ) -> Future[Result[PagedList[IModel]]]: ...
    def mesh_exports(
        self,
        imodel_id: str,
        params: QueryParameters = ...,
    ) -> Future[Result[PagedList[IModelMeshExport]]]: ...
    def reality_data(
        self,
        itwin_id: str,
        params: QueryParameters = ...,
    ) -> Future[Result[PagedList[ITwinRealityData]]]: ...
    def cesium_curated_content(
        self,
    ) -> Future[Result[list[CesiumCuratedContentAsset]]]: ...
    def geospatial_feature_collections(
        self,
        itwin_id: str,
    ) -> Future[Result[list[GeospatialFeatureCollection]]]: ...
    def geospatial_features(
        self,
        itwin_id: str,
        collection_id: str,
        limit: int = 10000,
    ) -> Future[Result[PagedList[GeoJsonFeature]]]: ...

def itwin_status_from_string(s: str) -> ITwinStatus: ...
def imodel_state_from_string(s: str) -> IModelState: ...
def imodel_mesh_export_status_from_string(s: str) -> IModelMeshExportStatus: ...
def imodel_mesh_export_type_from_string(s: str) -> IModelMeshExportType: ...
def itwin_reality_data_classification_from_string(
    s: str,
) -> ITwinRealityDataClassification: ...
def cesium_curated_content_type_from_string(
    s: str,
) -> CesiumCuratedContentType: ...
def cesium_curated_content_status_from_string(
    s: str,
) -> CesiumCuratedContentStatus: ...
