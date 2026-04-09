from collections.abc import Callable, Iterator
from typing import overload

import numpy as np
import numpy.typing as npt

from .async_ import AsyncSystem, Future, SharedFuture
from .geometry import QuadtreeTilingScheme, Rectangle, UpsampledQuadtreeNode
from .geospatial import (
    BoundingRegion,
    CartographicPolygon,
    Ellipsoid,
    GlobeRectangle,
    Projection,
)
from .gltf import Model
from .vector_data import GeoJsonDocument, VectorStyle

class RasterOverlayOptions:
    maximum_texture_size: int
    maximum_simultaneous_tile_loads: int
    sub_tile_cache_bytes: int
    maximum_screen_space_error: float
    ktx2_transcode_targets: int
    show_credits_on_screen: bool
    ellipsoid: Ellipsoid
    load_error_callback: Callable[[RasterOverlayLoadFailureDetails], None] | None

class RasterOverlay:
    """Base class for all raster overlays."""
    @property
    def name(self) -> str: ...
    @property
    def options(self) -> RasterOverlayOptions: ...

class UrlTemplateRasterOverlayOptions:
    """Options for URL template overlays."""

    credit: str | None
    projection: Projection | None
    tiling_scheme: QuadtreeTilingScheme | None
    minimum_level: int
    maximum_level: int
    tile_width: int
    tile_height: int
    coverage_rectangle: Rectangle | None
    def __init__(self) -> None: ...

class UrlTemplateRasterOverlay(RasterOverlay):
    """Raster overlay from a URL template with ``{z}/{x}/{y}`` placeholders.

    Examples::

            opts = UrlTemplateRasterOverlayOptions()
            opts.minimum_level = 0
            opts.maximum_level = 18
            overlay = UrlTemplateRasterOverlay(
                    "osm", "https://tile.openstreetmap.org/{z}/{x}/{y}.png",
                    url_template_options=opts,
            )
            layer.overlays.add(overlay)
    """
    def __init__(
        self,
        name: str,
        url: str,
        headers: list[tuple[str, str]] | None = None,
        url_template_options: UrlTemplateRasterOverlayOptions | None = None,
        overlay_options: RasterOverlayOptions | None = None,
    ) -> None: ...

class TileMapServiceRasterOverlayOptions:
    """Options for TMS (Tile Map Service) overlays."""

    file_extension: str | None
    credit: str | None
    minimum_level: int | None
    maximum_level: int | None
    coverage_rectangle: Rectangle | None
    projection: Projection | None
    tiling_scheme: QuadtreeTilingScheme | None
    tile_width: int | None
    tile_height: int | None
    flip_xy: bool | None
    def __init__(self) -> None: ...

class TileMapServiceRasterOverlay(RasterOverlay):
    """Raster overlay from a TMS (Tile Map Service) endpoint."""
    def __init__(
        self,
        name: str,
        url: str,
        headers: list[tuple[str, str]] | None = None,
        tms_options: TileMapServiceRasterOverlayOptions | None = None,
        overlay_options: RasterOverlayOptions | None = None,
    ) -> None: ...

class BingMapsRasterOverlay(RasterOverlay):
    """Raster overlay from Bing Maps (requires API key)."""
    def __init__(
        self,
        name: str,
        url: str,
        key: str,
        map_style: str = "Aerial",
        culture: str = "",
        overlay_options: RasterOverlayOptions | None = None,
    ) -> None: ...

class IonRasterOverlay(RasterOverlay):
    """Raster overlay from a Cesium ion asset."""
    def __init__(
        self,
        name: str,
        ion_asset_id: int,
        ion_access_token: str,
        overlay_options: RasterOverlayOptions | None = None,
        ion_asset_endpoint_url: str = "https://api.cesium.com/",
    ) -> None: ...
    asset_options: str | None

class DebugColorizeTilesRasterOverlay(RasterOverlay):
    """Debug overlay that colorizes tiles with unique colors."""
    def __init__(
        self,
        name: str,
        overlay_options: RasterOverlayOptions | None = None,
    ) -> None: ...

class WebMapServiceRasterOverlayOptions:
    """Options for Web Map Service (WMS) overlays."""

    version: str
    layers: str
    format: str
    credit: str | None
    minimum_level: int
    maximum_level: int
    tile_width: int
    tile_height: int
    def __init__(self) -> None: ...

class WebMapServiceRasterOverlay(RasterOverlay):
    """Raster overlay from a Web Map Service (WMS) server."""
    def __init__(
        self,
        name: str,
        url: str,
        headers: list[tuple[str, str]] | None = None,
        wms_options: WebMapServiceRasterOverlayOptions | None = None,
        overlay_options: RasterOverlayOptions | None = None,
    ) -> None: ...

class WebMapTileServiceRasterOverlayOptions:
    """Options for Web Map Tile Service (WMTS) overlays."""

    format: str | None
    subdomains: list[str]
    credit: str | None
    layer: str
    style: str
    tile_matrix_set_id: str
    tile_matrix_labels: list[str] | None
    minimum_level: int | None
    maximum_level: int | None
    coverage_rectangle: Rectangle | None
    projection: Projection | None
    tiling_scheme: QuadtreeTilingScheme | None
    dimensions: dict[str, str] | None
    tile_width: int | None
    tile_height: int | None
    def __init__(self) -> None: ...

class WebMapTileServiceRasterOverlay(RasterOverlay):
    """Raster overlay from a Web Map Tile Service (WMTS) server."""
    def __init__(
        self,
        name: str,
        url: str,
        headers: list[tuple[str, str]] | None = None,
        wmts_options: WebMapTileServiceRasterOverlayOptions | None = None,
        overlay_options: RasterOverlayOptions | None = None,
    ) -> None: ...

class RasterizedPolygonsOverlay(RasterOverlay):
    """Overlay that rasterizes CartographicPolygon objects (monochrome mask)."""
    def __init__(
        self,
        name: str,
        polygons: list[CartographicPolygon],
        invert_selection: bool,
        ellipsoid: Ellipsoid,
        projection: Projection,
        overlay_options: RasterOverlayOptions | None = None,
    ) -> None: ...
    @property
    def polygons(self) -> list[CartographicPolygon]: ...
    @property
    def invert_selection(self) -> bool: ...
    @property
    def ellipsoid(self) -> Ellipsoid: ...

class GoogleMapTilesNewSessionParameters:
    """Parameters for starting a new Google Map Tiles API session."""

    key: str
    map_type: str
    language: str
    region: str
    image_format: str | None
    scale: str | None
    high_dpi: bool | None
    layer_types: list[str] | None
    overlay: bool | None
    api_base_url: str
    def __init__(self) -> None: ...

class GoogleMapTilesExistingSession:
    """Parameters for an existing Google Map Tiles API session."""

    key: str
    session: str
    expiry: str
    tile_width: int
    tile_height: int
    image_format: str
    show_logo: bool
    api_base_url: str
    def __init__(self) -> None: ...

class GoogleMapTilesRasterOverlay(RasterOverlay):
    """Raster overlay from the Google Map Tiles API."""
    @overload
    def __init__(
        self,
        name: str,
        new_session_parameters: GoogleMapTilesNewSessionParameters,
        overlay_options: RasterOverlayOptions | None = None,
    ) -> None: ...
    @overload
    def __init__(
        self,
        name: str,
        existing_session: GoogleMapTilesExistingSession,
        overlay_options: RasterOverlayOptions | None = None,
    ) -> None: ...

class RasterOverlayLoadFailureDetails:
    """Details about a raster overlay load failure."""

    type: int
    request: object | None
    message: str
    def __init__(self) -> None: ...

class ActivatedRasterOverlay:
    @property
    def tile_data_bytes(self) -> int: ...
    @property
    def number_of_tiles_loading(self) -> int: ...
    def get_ready_event(self) -> SharedFuture[None]: ...
    @property
    def overlay(self) -> RasterOverlay: ...
    @property
    def tile_provider_ready(self) -> bool: ...
    @property
    def tile_provider(self) -> RasterOverlayTileProvider | None: ...
    def get_tile(
        self,
        rectangle: Rectangle,
        target_screen_pixels_x: float,
        target_screen_pixels_y: float,
    ) -> RasterOverlayTile: ...
    def load_tile(self, tile: RasterOverlayTile) -> FutureTileLoadResult: ...
    def load_tile_throttled(self, tile: RasterOverlayTile) -> bool: ...

class RasterOverlayMoreDetailAvailable:
    No: int
    Yes: int
    Unknown: int

class RasterOverlayTile:
    @property
    def state(self) -> int: ...
    def load_in_main_thread(self) -> None: ...
    def is_more_detail_available(self) -> int: ...
    @property
    def target_screen_pixels(self) -> list[float]: ...
    @property
    def renderer_resources(self) -> int: ...
    @renderer_resources.setter
    def renderer_resources(self, value: int) -> None: ...
    @property
    def rectangle(self) -> object: ...
    @property
    def image(self) -> object | None: ...
    @property
    def credits(self) -> list[object]: ...
    @property
    def activated_overlay(self) -> ActivatedRasterOverlay: ...
    @property
    def tile_provider(self) -> RasterOverlayTileProvider: ...
    @property
    def overlay(self) -> RasterOverlay: ...

class RasterOverlayTileLoadResult:
    activated: ActivatedRasterOverlay | None
    tile: RasterOverlayTile | None
    def __init__(
        self,
        activated: ActivatedRasterOverlay | None = None,
        tile: RasterOverlayTile | None = None,
    ) -> None: ...

FutureTileLoadResult = Future[RasterOverlayTileLoadResult]

class AzureMapsTilesetId:
    """Standard tileset ID constants for Azure Maps."""

    base_road: str
    base_dark_grey: str
    base_labels_road: str
    base_labels_dark_grey: str
    base_hybrid_road: str
    base_hybrid_dark_grey: str
    imagery: str
    terra: str
    weather_radar: str
    weather_infrared: str
    traffic_absolute: str
    traffic_relative_main: str
    traffic_relative_dark: str
    traffic_delay: str
    traffic_reduced: str

class AzureMapsSessionParameters:
    """Session parameters for Azure Maps overlays."""

    key: str
    api_version: str
    tileset_id: str
    language: str
    view: str
    show_logo: bool
    api_base_url: str
    def __init__(self) -> None: ...

class AzureMapsRasterOverlay(RasterOverlay):
    """Raster overlay from the Azure Maps API."""
    def __init__(
        self,
        name: str,
        session_parameters: AzureMapsSessionParameters,
        overlay_options: RasterOverlayOptions | None = None,
    ) -> None: ...

class GeoJsonDocumentRasterOverlayOptions:
    """Options for GeoJSON document raster overlays."""

    default_style: VectorStyle
    ellipsoid: Ellipsoid
    mip_levels: int
    def __init__(self) -> None: ...

class GeoJsonDocumentRasterOverlay(RasterOverlay):
    """Raster overlay from a rasterized GeoJSON document."""
    def __init__(
        self,
        async_system: AsyncSystem,
        name: str,
        document: GeoJsonDocument,
        vector_overlay_options: GeoJsonDocumentRasterOverlayOptions | None = None,
        overlay_options: RasterOverlayOptions | None = None,
    ) -> None: ...

class ITwinCesiumCuratedContentRasterOverlay(IonRasterOverlay):
    """Raster overlay from the iTwin Cesium Curated Content API."""
    def __init__(
        self,
        name: str,
        asset_id: int,
        itwin_access_token: str,
        overlay_options: RasterOverlayOptions | None = None,
    ) -> None: ...

class RasterOverlayDetails:
    raster_overlay_projections: list[Projection]
    raster_overlay_rectangles: list[Rectangle]
    bounding_region: BoundingRegion
    @overload
    def __init__(self) -> None: ...
    @overload
    def __init__(
        self,
        raster_overlay_projections: list[Projection],
        raster_overlay_rectangles: list[Rectangle],
        bounding_region: BoundingRegion,
    ) -> None: ...
    def find_rectangle_for_overlay_projection(
        self, projection: Projection
    ) -> Rectangle | None:
        """Find the rectangle for a given overlay projection, or None if not found."""
    def merge(self, other: RasterOverlayDetails, ellipsoid: Ellipsoid = ...) -> None:
        """Merge another RasterOverlayDetails into this one."""

class RasterOverlayUtilities:
    DEFAULT_TEXTURE_COORDINATE_BASE_NAME: str
    @staticmethod
    def create_raster_overlay_texture_coordinates(
        gltf: Model,
        model_to_ecef_transform: npt.NDArray[np.floating],
        globe_rectangle: GlobeRectangle | None,
        projections: list[Projection],
        invert_v_coordinate: bool = False,
        texture_coordinate_attribute_base_name: str = ...,
        first_texture_coordinate_id: int = 0,
    ) -> RasterOverlayDetails | None:
        """Create raster overlay texture coordinates for a glTF model."""
    @staticmethod
    def upsample_gltf_for_raster_overlays(
        parent_model: Model,
        child_id: UpsampledQuadtreeNode,
        has_inverted_v_coordinate: bool = False,
        texture_coordinate_attribute_base_name: str = ...,
        texture_coordinate_index: int = 0,
        ellipsoid: Ellipsoid = ...,
    ) -> Model | None:
        """Upsample a glTF model for raster overlays."""
    @staticmethod
    def compute_desired_screen_pixels(
        geometric_error: float,
        maximum_screen_space_error: float,
        projection: Projection,
        rectangle: Rectangle,
        ellipsoid: Ellipsoid = ...,
    ) -> npt.NDArray[np.floating]:
        """Compute the desired screen pixels for a raster overlay texture."""
    @staticmethod
    def compute_translation_and_scale(
        geometry_rectangle: Rectangle,
        overlay_rectangle: Rectangle,
    ) -> npt.NDArray[np.floating]:
        """Compute texture translation and scale to align an overlay with geometry."""

class RasterOverlayTileProvider:
    @property
    def projection(self) -> Projection: ...
    @property
    def coverage_rectangle(self) -> Rectangle: ...
    @property
    def owner(self) -> RasterOverlay: ...

class QuadtreeRasterOverlayTileProvider(RasterOverlayTileProvider):
    @property
    def minimum_level(self) -> int: ...
    @property
    def maximum_level(self) -> int: ...
    @property
    def width(self) -> int: ...
    @property
    def height(self) -> int: ...
    @property
    def tiling_scheme(self) -> QuadtreeTilingScheme: ...
    def compute_level_from_target_screen_pixels(
        self,
        rectangle: Rectangle,
        screen_pixels_x: float,
        screen_pixels_y: float,
    ) -> int: ...

class RasterMappedTo3DTile: ...

class RasterOverlayCollection:
    def size(self) -> int: ...
    def add(self, overlay: RasterOverlay) -> None:
        """Add a raster overlay to this tileset."""
        ...
    def remove(self, overlay: RasterOverlay) -> None:
        """Remove a raster overlay from this tileset."""
        ...
    def __len__(self) -> int: ...
    def __iter__(self) -> Iterator[RasterOverlay]: ...
    def __repr__(self) -> str: ...

class RasterOverlayLoadType:
    Unknown: int
    CesiumIon: int
    TileProvider: int

class RasterOverlayTileLoadState:
    Placeholder: int
    Failed: int
    Unloaded: int
    Loading: int
    Loaded: int
    Done: int
