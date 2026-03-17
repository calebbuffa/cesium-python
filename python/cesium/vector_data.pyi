from __future__ import annotations

from enum import IntEnum
from typing import Any, Callable, Iterator, overload

import numpy.typing as npt

from .async_ import AsyncSystem, IAssetAccessor
from .geometry import AxisAlignedBox
from .geospatial import CartographicPolygon, Ellipsoid, GlobeRectangle
from .gltf import ImageAsset
from .utility import Color

class ColorMode(IntEnum):
    NORMAL = ...
    RANDOM = ...

class LineWidthMode(IntEnum):
    PIXELS = ...
    METERS = ...

class ColorStyle:
    color: Color
    color_mode: ColorMode

    def color_for_seed(self, random_color_seed: int = 0) -> Color: ...

class LineStyle(ColorStyle):
    width: float
    width_mode: LineWidthMode

class PolygonStyle:
    fill: ColorStyle
    outline: LineStyle

class VectorStyle:
    line: LineStyle
    polygon: PolygonStyle

    def __init__(
        self,
        line_style: LineStyle | None = None,
        polygon_style: PolygonStyle | None = None,
    ) -> None: ...

class GeoJsonObjectType(IntEnum):
    POINT = ...
    MULTI_POINT = ...
    LINE_STRING = ...
    MULTI_LINE_STRING = ...
    POLYGON = ...
    MULTI_POLYGON = ...
    GEOMETRY_COLLECTION = ...
    FEATURE = ...
    FEATURE_COLLECTION = ...

class GeoJsonPoint:
    TYPE: GeoJsonObjectType
    coordinates: npt.NDArray  # dvec3
    bounding_box: AxisAlignedBox | None
    foreign_members: dict[str, Any]
    style: VectorStyle | None

    @overload
    def __init__(self) -> None: ...
    @overload
    def __init__(self, coordinates: npt.NDArray) -> None: ...

class GeoJsonMultiPoint:
    TYPE: GeoJsonObjectType
    coordinates: list[npt.NDArray]  # list[dvec3]
    bounding_box: AxisAlignedBox | None
    foreign_members: dict[str, Any]
    style: VectorStyle | None

    def __init__(self) -> None: ...

class GeoJsonLineString:
    TYPE: GeoJsonObjectType
    coordinates: list[npt.NDArray]  # list[dvec3]
    bounding_box: AxisAlignedBox | None
    foreign_members: dict[str, Any]
    style: VectorStyle | None

    def __init__(self) -> None: ...

class GeoJsonMultiLineString:
    TYPE: GeoJsonObjectType
    coordinates: list[list[npt.NDArray]]  # list[list[dvec3]]
    bounding_box: AxisAlignedBox | None
    foreign_members: dict[str, Any]
    style: VectorStyle | None

    def __init__(self) -> None: ...

class GeoJsonPolygon:
    TYPE: GeoJsonObjectType
    coordinates: list[list[npt.NDArray]]  # list[list[dvec3]]
    bounding_box: AxisAlignedBox | None
    foreign_members: dict[str, Any]
    style: VectorStyle | None

    def __init__(self) -> None: ...

class GeoJsonMultiPolygon:
    TYPE: GeoJsonObjectType
    coordinates: list[list[list[npt.NDArray]]]  # list[list[list[dvec3]]]
    bounding_box: AxisAlignedBox | None
    foreign_members: dict[str, Any]
    style: VectorStyle | None

    def __init__(self) -> None: ...

class GeoJsonGeometryCollection:
    TYPE: GeoJsonObjectType
    geometries: list[GeoJsonObject]
    bounding_box: AxisAlignedBox | None
    foreign_members: dict[str, Any]
    style: VectorStyle | None

    def __init__(self) -> None: ...

class GeoJsonFeature:
    TYPE: GeoJsonObjectType
    id: str | int | None
    geometry: GeoJsonObject | None
    properties: dict[str, Any] | None
    bounding_box: AxisAlignedBox | None
    foreign_members: dict[str, Any]
    style: VectorStyle | None

    def __init__(self) -> None: ...

class GeoJsonFeatureCollection:
    TYPE: GeoJsonObjectType
    features: list[GeoJsonObject]
    bounding_box: AxisAlignedBox | None
    foreign_members: dict[str, Any]
    style: VectorStyle | None

    def __init__(self) -> None: ...

class GeoJsonObject:
    type: GeoJsonObjectType
    bounding_box: AxisAlignedBox | None
    foreign_members: dict[str, Any]
    style: VectorStyle | None

    @overload
    def __init__(self, point: GeoJsonPoint) -> None: ...
    @overload
    def __init__(self, multi_point: GeoJsonMultiPoint) -> None: ...
    @overload
    def __init__(self, line_string: GeoJsonLineString) -> None: ...
    @overload
    def __init__(self, multi_line_string: GeoJsonMultiLineString) -> None: ...
    @overload
    def __init__(self, polygon: GeoJsonPolygon) -> None: ...
    @overload
    def __init__(self, multi_polygon: GeoJsonMultiPolygon) -> None: ...
    @overload
    def __init__(self, geometry_collection: GeoJsonGeometryCollection) -> None: ...
    @overload
    def __init__(self, feature: GeoJsonFeature) -> None: ...
    @overload
    def __init__(self, feature_collection: GeoJsonFeatureCollection) -> None: ...

    # Type checking
    def is_point(self) -> bool: ...
    def is_multi_point(self) -> bool: ...
    def is_line_string(self) -> bool: ...
    def is_multi_line_string(self) -> bool: ...
    def is_polygon(self) -> bool: ...
    def is_multi_polygon(self) -> bool: ...
    def is_geometry_collection(self) -> bool: ...
    def is_feature(self) -> bool: ...
    def is_feature_collection(self) -> bool: ...

    # Typed accessors (return None on type mismatch)
    def as_point(self) -> GeoJsonPoint | None: ...
    def as_multi_point(self) -> GeoJsonMultiPoint | None: ...
    def as_line_string(self) -> GeoJsonLineString | None: ...
    def as_multi_line_string(self) -> GeoJsonMultiLineString | None: ...
    def as_polygon(self) -> GeoJsonPolygon | None: ...
    def as_multi_polygon(self) -> GeoJsonMultiPolygon | None: ...
    def as_geometry_collection(self) -> GeoJsonGeometryCollection | None: ...
    def as_feature(self) -> GeoJsonFeature | None: ...
    def as_feature_collection(self) -> GeoJsonFeatureCollection | None: ...

    # Primitive iterators (materialised as lists)
    def points(self) -> list[npt.NDArray]: ...
    def lines(self) -> list[list[npt.NDArray]]: ...
    def polygons(self) -> list[list[list[npt.NDArray]]]: ...
    def __iter__(self) -> Iterator[GeoJsonObject]: ...
    def __repr__(self) -> str: ...

class ResultGeoJsonDocument:
    @property
    def value(self) -> GeoJsonDocument | None: ...
    @property
    def errors(self) -> list[str]: ...
    @property
    def warnings(self) -> list[str]: ...

class FutureResultGeoJsonDocument:
    def wait(self) -> ResultGeoJsonDocument: ...
    def wait_in_main_thread(self) -> ResultGeoJsonDocument: ...
    def is_ready(self) -> bool: ...
    def share(self) -> FutureResultGeoJsonDocument: ...
    def then_in_worker_thread(
        self,
        callback: Callable[[ResultGeoJsonDocument], None],
    ) -> FutureResultGeoJsonDocument: ...
    def then_in_main_thread(
        self,
        callback: Callable[[ResultGeoJsonDocument], None],
    ) -> FutureResultGeoJsonDocument: ...

class VectorDocumentAttribution:
    html: str
    show_on_screen: bool

def geojson_object_type_to_string(type: GeoJsonObjectType) -> str: ...
def parse_geojson_document_summary(
    geojson_bytes: bytes,
    attributions: list[VectorDocumentAttribution] | None = None,
) -> dict[str, Any]: ...

class GeoJsonDocument:
    attributions: list[VectorDocumentAttribution]
    root_object: GeoJsonObject

    def __len__(self) -> int: ...
    def __iter__(self) -> Iterator[GeoJsonObject]: ...
    def __getitem__(self, index: int) -> GeoJsonObject: ...
    def __repr__(self) -> str: ...
    @staticmethod
    def from_geojson(
        data: bytes,
        attributions: list[VectorDocumentAttribution] | None = None,
    ) -> GeoJsonDocument: ...
    @staticmethod
    def from_url(
        async_system: AsyncSystem,
        asset_accessor: IAssetAccessor,
        url: str,
        headers: dict[str, str] | None = None,
    ) -> FutureResultGeoJsonDocument: ...
    @staticmethod
    def from_cesium_ion_asset(
        async_system: AsyncSystem,
        asset_accessor: IAssetAccessor,
        ion_asset_id: int,
        ion_access_token: str,
        ion_asset_endpoint_url: str = "https://api.cesium.com/",
    ) -> FutureResultGeoJsonDocument: ...

class VectorRasterizer:
    def __init__(
        self,
        bounds: GlobeRectangle,
        image_asset: ImageAsset,
        mip_level: int = 0,
        ellipsoid: Ellipsoid = ...,
    ) -> None: ...
    def draw_polygon(
        self, polygon: CartographicPolygon, style: PolygonStyle
    ) -> None: ...
    def draw_polygon_rings(
        self,
        rings: list[list[npt.NDArray]],
        style: PolygonStyle,
    ) -> None:
        """Draw a polygon from linear rings (list of list of [lon, lat, height] in degrees)."""
    def draw_polyline(self, points: list[npt.NDArray], style: LineStyle) -> None: ...
    def draw_geojson_object(
        self, geojson_object: GeoJsonObject, style: VectorStyle
    ) -> None: ...
    def clear(self, clear_color: Color) -> None: ...
    def finalize(self) -> ImageAsset: ...
