from collections.abc import Iterator
from enum import IntEnum
from typing import Any, Callable, Optional, overload

import numpy as np
import numpy.typing as npt

from .. import async_  # pyright: ignore[reportAttributeAccessIssue]
from ..async_ import AsyncSystem, Future, IAssetAccessor, IAssetRequest, SharedFuture
from ..geometry import (
    BoundingCylinderRegion,
    BoundingSphere,
    OctreeTileID,
    OrientedBoundingBox,
    QuadtreeTileID,
    UpsampledQuadtreeNode,
)
from ..geospatial import (
    BoundingRegion,
    BoundingRegionWithLooseFittingHeights,
    Cartographic,
    Ellipsoid,
    GlobeRectangle,
    S2CellBoundingVolume,
)
from ..gltf import ImageAsset, Ktx2TranscodeTargets, Model
from ..gltf.reader import GltfSharedAssetSystem
from ..raster_overlays import (
    ActivatedRasterOverlay,
    RasterizedPolygonsOverlay,
    RasterOverlay,
    RasterOverlayDetails,
    RasterOverlayTile,
)
from ..utility import ErrorList, JsonValue
from . import Asset, GroupMetadata, MetadataEntity, Properties, Schema, Statistics

class RasterMappedTo3DTileAttachmentState(IntEnum):
    Unattached = 0
    TemporarilyAttached = 1
    Attached = 2

class RasterMappedTo3DTile:
    @property
    def loading_tile(self) -> RasterOverlayTile | None: ...
    @property
    def ready_tile(self) -> RasterOverlayTile | None: ...
    @property
    def texture_coordinate_id(self) -> int: ...
    @property
    def translation(self) -> npt.NDArray[np.float64]: ...
    @property
    def scale(self) -> npt.NDArray[np.float64]: ...
    @property
    def state(self) -> RasterMappedTo3DTileAttachmentState: ...
    def is_more_detail_available(self) -> bool: ...
    def load_throttled(self) -> bool: ...

class TileOcclusionRendererProxy: ...

class IPrepareRendererResources:
    """Base class for tile content preparation callbacks.

    Subclass from Python and override the ``prepare_*`` / ``free_*`` methods
    to implement custom render-resource management.  ``void*`` render
    resources on the C++ side are mapped to arbitrary Python objects.
    """

    def prepare_in_load_thread(
        self,
        async_system: async_.AsyncSystem,
        tile_load_result: TileLoadResult,
        transform: npt.NDArray[np.float64],
    ) -> Any:
        """Return render resources (arbitrary Python object), or None.

        ``tile_load_result`` is the full C++ ``TileLoadResult`` passed as a
        reference.  Access ``tile_load_result.model`` for the glTF Model
        (or ``None`` when the tile has no glTF content).  The reference is
        only valid for the duration of this callback — do not store it.
        """
        ...

    def prepare_in_main_thread(self, tile: Tile, load_thread_result: Any) -> Any:
        """Return main-thread render resources (arbitrary Python object)."""
        ...

    def free_tile(
        self,
        tile: Tile,
        load_thread_result: Any,
        main_thread_result: Any,
    ) -> None: ...
    def attach_raster_in_main_thread(
        self,
        tile: Tile,
        overlay_texture_coordinate_id: int,
        raster_tile: RasterOverlayTile,
        main_thread_renderer_resources: Any,
        translation: npt.NDArray[np.float64],
        scale: npt.NDArray[np.float64],
    ) -> None: ...
    def detach_raster_in_main_thread(
        self,
        tile: Tile,
        overlay_texture_coordinate_id: int,
        raster_tile: RasterOverlayTile,
        main_thread_renderer_resources: Any,
    ) -> None: ...
    def prepare_raster_in_load_thread(self, image: ImageAsset) -> Any: ...
    def prepare_raster_in_main_thread(
        self, raster_tile: RasterOverlayTile, load_thread_result: Any
    ) -> Any: ...
    def free_raster(
        self,
        raster_tile: RasterOverlayTile,
        load_thread_result: Any,
        main_thread_result: Any,
    ) -> None: ...

class TilesetExternals:
    """External interfaces needed to construct a ``Tileset``.

    Example — replicate what ``TilesetLayer.from_ellipsoid()`` does::

        from argus import TaskProcessor, PrepareRendererResources, TilesetLayer
        from cesium.async_ import AsyncSystem
        from cesium.curl import CurlAssetAccessor
        from cesium.tiles.selection import TilesetExternals, Tileset

        tp   = TaskProcessor()
        asys = AsyncSystem(tp)
        acc  = CurlAssetAccessor()
        # PrepareRendererResources needs a TilesetLayer for content extraction
        layer = TilesetLayer(...)
        prr  = PrepareRendererResources(layer)
        ext  = TilesetExternals(acc, prr, asys)
        ts   = Tileset(ext, "https://example.com/tileset.json")
    """
    def __init__(
        self,
        asset_accessor: async_.IAssetAccessor,
        prepare_renderer_resources: IPrepareRendererResources,
        async_system: async_.AsyncSystem,
        credit_system: CreditSystem | None = None,
    ) -> None: ...
    asset_accessor: async_.IAssetAccessor
    prepare_renderer_resources: IPrepareRendererResources
    async_system: async_.AsyncSystem
    credit_system: CreditSystem | None
    shared_asset_system: TilesetSharedAssetSystem | None
    @property
    def gltf_modifier(self) -> GltfModifier | None: ...
    @gltf_modifier.setter
    def gltf_modifier(self, value: GltfModifier | None) -> None: ...

class ITileExcluder:
    def __init__(self) -> None: ...
    def start_new_frame(self) -> None: ...
    def should_exclude(self, tile: Tile) -> bool: ...

class RasterizedPolygonsTileExcluder(ITileExcluder):
    """Excludes tiles entirely inside polygons from a RasterizedPolygonsOverlay."""
    def __init__(self, overlay: RasterizedPolygonsOverlay) -> None: ...
    @property
    def overlay(self) -> RasterizedPolygonsOverlay: ...

class TilesetContentLoaderFactory:
    def __init__(self) -> None: ...
    def create_loader(
        self,
        externals: TilesetExternals,
        tileset_options: TilesetOptions,
        header_change_listener: Callable[..., None],
    ) -> FutureTilesetContentLoaderResult: ...
    def is_valid(self) -> bool: ...

class TilesetContentLoader:
    def __init__(self) -> None: ...
    def load_tile_content(self, input: TileLoadInput) -> FutureTileLoadResult: ...
    def create_tile_children(
        self, tile: Tile, ellipsoid: Ellipsoid = ...
    ) -> TileChildrenResult: ...
    def has_height_sampler(self) -> bool: ...
    @property
    def height_sampler(self) -> ITilesetHeightSampler | None: ...
    def has_owner(self) -> bool: ...

class TileChildrenResult:
    """Result of TilesetContentLoader.create_tile_children().

    Use ``add_child(loader)`` to append new tiles.  The result owns all
    child ``Tile`` objects — references returned by ``add_child`` and
    ``children`` are valid for the lifetime of this object.
    """
    state: TileLoadResultState
    @property
    def children(self) -> list[Tile]: ...
    def add_child(self, loader: TilesetContentLoader) -> Tile:
        """Append a new child Tile and return a reference to it."""
        ...
    def __init__(self) -> None: ...

class Credit: ...

class TileOcclusionState(IntEnum):
    OcclusionUnavailable = ...
    NotOccluded = ...
    Occluded = ...

class TileLoadPriorityGroup(IntEnum):
    Preload = ...
    Normal = ...
    Urgent = ...

class TileLoadTask:
    def __init__(self) -> None: ...
    tile: Tile | None
    group: TileLoadPriorityGroup
    priority: float

class TileLoadInput:
    @property
    def tile(self) -> Tile: ...
    @property
    def content_options(self) -> TilesetContentOptions: ...
    @property
    def async_system(self) -> AsyncSystem: ...
    @property
    def asset_accessor(self) -> IAssetAccessor: ...
    @property
    def request_headers(self) -> list[tuple[str, str]]: ...
    @property
    def ellipsoid(self) -> Ellipsoid: ...
    @property
    def shared_asset_system(self) -> TilesetSharedAssetSystem | None: ...
    @property
    def has_logger(self) -> bool: ...

class ITilesetHeightSampler:
    def __init__(self) -> None: ...
    def sample_heights(
        self, async_system: AsyncSystem, positions: npt.NDArray[np.float64]
    ) -> FutureSampleHeightResult: ...

class CesiumIonTilesetContentLoaderFactory(TilesetContentLoaderFactory):
    def __init__(
        self,
        ion_asset_id: int,
        ion_access_token: str,
        ion_asset_endpoint_url: str = "https://api.cesium.com/",
    ) -> None: ...

class IModelMeshExportContentLoaderFactory(TilesetContentLoaderFactory):
    def __init__(
        self,
        imodel_id: str,
        export_id: Optional[str] = None,
        *,
        itwin_access_token: str,
    ) -> None: ...

class ITwinCesiumCuratedContentLoaderFactory(TilesetContentLoaderFactory):
    def __init__(
        self,
        itwin_cesium_content_id: int,
        itwin_access_token: str,
    ) -> None: ...

class ITwinRealityDataContentLoaderFactory(TilesetContentLoaderFactory):
    def __init__(
        self,
        reality_data_id: str,
        itwin_id: Optional[str] = None,
        *,
        itwin_access_token: str,
        async_system: AsyncSystem,
        token_refresh_callback: Callable[[str], str],
    ) -> None: ...

class TilesetSharedAssetSystem(GltfSharedAssetSystem):
    @staticmethod
    def default_system() -> TilesetSharedAssetSystem: ...

class CreditSystem:
    def should_be_shown_on_screen(self, credit: Credit) -> bool: ...
    def set_show_on_screen(self, credit: Credit, show_on_screen: bool) -> None: ...
    def html(self, credit: Credit) -> str: ...

class CreditSource:
    @property
    def credit_system(self) -> CreditSystem | None: ...

class TileRefine(IntEnum):
    Add = 0
    Replace = 1

class TileLoadResultState(IntEnum):
    Success = 0
    Failed = 1
    RetryLater = 2

class TileLoadState(IntEnum):
    Unloading = ...
    FailedTemporarily = ...
    Unloaded = ...
    ContentLoading = ...
    ContentLoaded = ...
    Done = ...
    Failed = ...

class TileSelectionResult(IntEnum):
    None_ = ...
    Culled = ...
    Rendered = ...
    Refined = ...
    RenderedAndKicked = ...
    RefinedAndKicked = ...

class TileSelectionState:
    def __init__(self, result: TileSelectionResult | None = None) -> None: ...
    @property
    def result(self) -> TileSelectionResult: ...
    @property
    def original_result(self) -> TileSelectionResult: ...
    def was_kicked(self) -> bool: ...
    def kick(self) -> None: ...

class BoundingVolume:
    @property
    def center(self) -> npt.NDArray[np.float64]: ...
    def as_oriented_bounding_box(
        self, ellipsoid: Ellipsoid = ...
    ) -> OrientedBoundingBox: ...
    def transform(self, transform: npt.NDArray[np.float64]) -> BoundingVolume:
        """Transform this bounding volume by the given 4x4 matrix."""
        ...
    def estimate_globe_rectangle(
        self, ellipsoid: Ellipsoid = ...
    ) -> GlobeRectangle | None:
        """Estimate the bounding GlobeRectangle. Returns None if not estimable."""
        ...
    @property
    def bounding_region(self) -> BoundingRegion | None:
        """Get the BoundingRegion if this volume is one, else None."""
        ...
    @staticmethod
    def from_obb(obb: OrientedBoundingBox) -> BoundingVolume:
        """Construct a BoundingVolume wrapping an OrientedBoundingBox."""
        ...
    @staticmethod
    def from_sphere(sphere: BoundingSphere) -> BoundingVolume:
        """Construct a BoundingVolume wrapping a BoundingSphere."""
        ...
    @staticmethod
    def from_region(region: BoundingRegion) -> BoundingVolume:
        """Construct a BoundingVolume wrapping a BoundingRegion."""
        ...
    @staticmethod
    def from_loose_region(
        region: BoundingRegionWithLooseFittingHeights,
    ) -> BoundingVolume:
        """Construct a BoundingVolume wrapping a BoundingRegionWithLooseFittingHeights."""
        ...
    @staticmethod
    def from_s2(s2: S2CellBoundingVolume) -> BoundingVolume:
        """Construct a BoundingVolume wrapping an S2CellBoundingVolume."""
        ...
    @staticmethod
    def from_cylinder(cylinder: BoundingCylinderRegion) -> BoundingVolume:
        """Construct a BoundingVolume wrapping a BoundingCylinderRegion."""
        ...
    def __repr__(self) -> str: ...

class TileExternalContentMetadata: ...
class TileUnknownContent: ...
class TileEmptyContent: ...

class TileExternalContent:
    metadata: TileExternalContentMetadata | None

class TileRenderContent:
    @property
    def model(self) -> Model: ...
    @property
    def render_resources(self) -> Any: ...
    @property
    def lod_transition_fade_percentage(self) -> float: ...

class TileContent:
    def __init__(self, content: TileEmptyContent | None = None) -> None: ...
    @property
    def content_kind(
        self,
    ) -> TileRenderContent | TileUnknownContent | TileEmptyContent | TileExternalContent | None:
        """Current content variant. Assign TileUnknownContent or TileEmptyContent to change."""
        ...
    @content_kind.setter
    def content_kind(
        self, value: TileUnknownContent | TileEmptyContent
    ) -> None: ...
    def is_unknown_content(self) -> bool: ...
    def is_empty_content(self) -> bool: ...
    def is_external_content(self) -> bool: ...
    def is_render_content(self) -> bool: ...
    @property
    def render_content(self) -> TileRenderContent | None: ...
    @property
    def content_kind_name(self) -> str: ...

class Tile:
    @property
    def parent(self) -> Tile | None: ...
    @property
    def children(self) -> list[Tile]: ...
    @property
    def child_count(self) -> int: ...
    @property
    def bounding_volume(self) -> BoundingVolume: ...
    @bounding_volume.setter
    def bounding_volume(self, value: BoundingVolume) -> None: ...
    @property
    def viewer_request_volume(self) -> BoundingVolume | None: ...
    geometric_error: float
    @property
    def non_zero_geometric_error(self) -> float: ...
    unconditionally_refine: bool
    refine: TileRefine
    transform: npt.NDArray[np.float64]
    @property
    def tile_id(self) -> str | QuadtreeTileID | OctreeTileID | UpsampledQuadtreeNode:
        """Get or set the typed tile ID.

        Returns the actual variant member — a ``QuadtreeTileID``,
        ``OctreeTileID``, ``UpsampledQuadtreeNode``, or ``str``.
        Setting accepts any of those types.
        """
        ...
    @tile_id.setter
    def tile_id(
        self,
        value: str | QuadtreeTileID | OctreeTileID | UpsampledQuadtreeNode,
    ) -> None: ...
    @property
    def tile_id_string(self) -> str: ...
    @tile_id_string.setter
    def tile_id_string(self, value: str) -> None: ...
    @property
    def content(self) -> TileContent: ...
    @property
    def state(self) -> TileLoadState: ...
    @property
    def reference_count(self) -> int: ...
    @property
    def content_bounding_volume(self) -> BoundingVolume | None: ...
    @property
    def mapped_raster_tiles(self) -> list[RasterMappedTo3DTile]: ...
    def compute_byte_size(self) -> int: ...
    def is_renderable(self) -> bool: ...
    def is_render_content(self) -> bool: ...
    def is_external_content(self) -> bool: ...
    def is_empty_content(self) -> bool: ...
    def add_reference(self, reason: str | None = None) -> None: ...
    def release_reference(self, reason: str | None = None) -> None: ...
    def has_referencing_content(self) -> bool: ...
    def __repr__(self) -> str: ...

class TileLoadResult:
    gltf_up_axis: int
    asset_accessor: IAssetAccessor | None
    completed_request: IAssetRequest | None
    state: TileLoadResultState
    ellipsoid: Ellipsoid
    content_kind_name: str
    raster_overlay_details: RasterOverlayDetails | None
    tile_initializer: Callable[[Tile], None] | None
    updated_bounding_volume: BoundingVolume | None
    updated_content_bounding_volume: BoundingVolume | None
    initial_bounding_volume: BoundingVolume | None
    initial_content_bounding_volume: BoundingVolume | None
    @property
    def model(self) -> Model | None: ...
    @model.setter
    def model(self, value: Model) -> None: ...
    @property
    def content_kind(
        self,
    ) -> Model | TileUnknownContent | TileEmptyContent | TileExternalContent:
        """Get or set the content variant.

        Assign a ``Model``, ``TileEmptyContent``, ``TileExternalContent``, or
        ``TileUnknownContent`` to change the tile's content type.
        """
        ...
    @content_kind.setter
    def content_kind(
        self,
        value: Model | TileUnknownContent | TileEmptyContent | TileExternalContent,
    ) -> None: ...
    @staticmethod
    def create_failed_result(
        asset_accessor: IAssetAccessor, completed_request: IAssetRequest
    ) -> TileLoadResult: ...
    @staticmethod
    def create_retry_later_result(
        asset_accessor: IAssetAccessor, completed_request: IAssetRequest
    ) -> TileLoadResult: ...
    def __repr__(self) -> str: ...

class TileLoadResultAndRenderResources:
    result: TileLoadResult
    render_resources: Any
    def __init__(
        self,
        result: TileLoadResult,
        render_resources: Any = None,
    ) -> None: ...

class LoaderCreditResult:
    credit_text: str
    show_on_screen: bool

class SampleHeightResult:
    @property
    def positions(self) -> npt.NDArray[np.float64]: ...
    @property
    def sample_success(self) -> npt.NDArray[np.bool_]: ...
    warnings: list[str]

class TilesetMetadata:
    asset: Asset
    properties: dict[str, Properties]
    schema: Schema | None
    schema_uri: str | None
    statistics: Statistics | None
    groups: list[GroupMetadata]
    metadata: MetadataEntity | None
    geometric_error: float | None
    extensions_used: list[str]
    extensions_required: list[str]
    def has_generic_extension(self, extension_name: str) -> bool: ...
    def get_generic_extension(self, extension_name: str) -> JsonValue: ...
    def load_schema_uri(
        self, async_system: AsyncSystem, asset_accessor: IAssetAccessor
    ) -> SharedFuture[None]: ...

class TilesetContentLoaderResult:
    credits: list[LoaderCreditResult]
    request_headers: list[tuple[str, str]]
    errors: ErrorList
    status_code: int
    def has_loader(self) -> bool: ...
    def has_root_tile(self) -> bool: ...
    @property
    def root_tile(self) -> Tile | None: ...
    @property
    def loader(self) -> TilesetContentLoader | None: ...

FutureSampleHeightResult = Future[SampleHeightResult]
SharedFutureSampleHeightResult = SharedFuture[SampleHeightResult]
FutureTilesetMetadata = Future[TilesetMetadata | None]
SharedFutureTilesetMetadata = SharedFuture[TilesetMetadata | None]
FutureTilesetContentLoaderResult = Future[TilesetContentLoaderResult]
FutureTileLoadResult = Future[TileLoadResult]

class ViewState:
    @overload
    def __init__(
        self,
        position: npt.NDArray[np.floating],
        direction: npt.NDArray[np.floating],
        up: npt.NDArray[np.floating],
        viewport_size: npt.NDArray[np.floating],
        horizontal_field_of_view: float,
        vertical_field_of_view: float,
    ) -> None: ...
    @overload
    def __init__(
        self,
        view_matrix: npt.NDArray[np.floating],
        projection_matrix: npt.NDArray[np.floating],
        viewport_size: npt.NDArray[np.floating],
    ) -> None: ...
    @overload
    def __init__(
        self,
        position: npt.NDArray[np.floating],
        direction: npt.NDArray[np.floating],
        up: npt.NDArray[np.floating],
        viewport_size: npt.NDArray[np.floating],
        left: float,
        right: float,
        bottom: float,
        top: float,
        ellipsoid: Ellipsoid = ...,
    ) -> None: ...
    position: npt.NDArray[np.float64]
    direction: npt.NDArray[np.float64]
    up: npt.NDArray[np.float64]
    viewport_size: npt.NDArray[np.float64]
    horizontal_field_of_view: float
    vertical_field_of_view: float
    view_matrix: npt.NDArray[np.float64]
    projection_matrix: npt.NDArray[np.float64]
    @overload
    def compute_screen_space_error(
        self, geometric_error: float, distance: float
    ) -> float: ...
    @overload
    def compute_screen_space_error(
        self,
        geometric_error: npt.NDArray[np.floating],
        distance: npt.NDArray[np.floating],
    ) -> npt.NDArray[np.float64]: ...
    def __repr__(self) -> str: ...
    def is_bounding_volume_visible(self, bounding_volume: BoundingVolume) -> bool: ...
    def compute_distance_squared_to_bounding_volume(
        self, bounding_volume: BoundingVolume
    ) -> float: ...
    @property
    def position_cartographic(self) -> Cartographic | None: ...

class GltfModifierState(IntEnum):
    Idle = ...
    WorkerRunning = ...
    WorkerDone = ...

class GltfModifierInput:
    @property
    def version(self) -> int: ...
    @property
    def previous_model(self) -> Model: ...
    @property
    def tile_transform(self) -> npt.NDArray[np.float64]: ...

class GltfModifierOutput:
    def __init__(self) -> None: ...
    modified_model: Model

class GltfModifier:
    def __init__(self) -> None: ...
    @property
    def current_version(self) -> int: ...
    def is_active(self) -> bool: ...
    def trigger(self) -> None: ...
    def apply(
        self, model: Model, tile_transform: npt.NDArray[np.float64]
    ) -> Model | None: ...

class FogDensityAtHeight:
    camera_height: float
    fog_density: float

class TilesetContentOptions:
    enable_water_mask: bool
    generate_missing_normals_smooth: bool
    ktx2_transcode_targets: Ktx2TranscodeTargets
    apply_texture_transform: bool

class TilesetOptions:
    credit: str | None
    show_credits_on_screen: bool
    maximum_screen_space_error: float
    maximum_simultaneous_tile_loads: int
    preload_ancestors: bool
    preload_siblings: bool
    loading_descendant_limit: int
    forbid_holes: bool
    enable_frustum_culling: bool
    enable_occlusion_culling: bool
    delay_refinement_for_occlusion: bool
    enable_fog_culling: bool
    enforce_culled_screen_space_error: bool
    culled_screen_space_error: float
    maximum_cached_bytes: int
    fog_density_table: list[FogDensityAtHeight]
    render_tiles_under_camera: bool
    enable_lod_transition_period: bool
    lod_transition_length: float
    kick_descendants_while_fading_in: bool
    main_thread_loading_time_limit: float
    tile_cache_unload_time_limit: float
    content_options: TilesetContentOptions
    ellipsoid: Ellipsoid
    request_headers: list[tuple[str, str]]
    excluders: list[ITileExcluder]
    load_error_callback: Callable[[TilesetLoadFailureDetails], None] | None
    def __repr__(self) -> str: ...

class RasterOverlayCollection:
    def size(self) -> int: ...
    def add(self, overlay: RasterOverlay) -> None:
        """Add a raster overlay to this tileset."""
        ...
    def remove(self, overlay: RasterOverlay) -> None:
        """Remove a raster overlay from this tileset."""
        ...
    @property
    def activated_overlays(self) -> list[ActivatedRasterOverlay]: ...
    def __len__(self) -> int: ...
    def __iter__(self) -> Iterator[RasterOverlay]: ...
    def __repr__(self) -> str: ...

class TilesetLoadType(IntEnum):
    Unknown = ...
    CesiumIon = ...
    TilesetJson = ...

class TilesetLoadFailureDetails:
    tileset: Tileset | None
    type: TilesetLoadType
    status_code: int
    message: str
    def has_tileset(self) -> bool: ...

class ViewUpdateResult:
    tiles_to_render: list[Tile]
    tiles_fading_out: list[Tile]
    worker_thread_tile_load_queue_length: int
    main_thread_tile_load_queue_length: int
    tiles_visited: int
    culled_tiles_visited: int
    tiles_culled: int
    tiles_occluded: int
    tiles_waiting_for_occlusion_results: int
    tiles_kicked: int
    max_depth_visited: int
    frame_number: int
    @property
    def tile_positions(self) -> npt.NDArray[np.float64]:
        """(N,3) float64 translation column of each rendered tile's transform."""
        ...
    @property
    def tile_bounding_volume_centers(self) -> npt.NDArray[np.float64]:
        """(N,3) float64 bounding-volume center for each rendered tile."""
        ...
    @property
    def tile_geometric_errors(self) -> npt.NDArray[np.float64]:
        """(N,) float64 geometric error for each rendered tile."""
        ...
    @property
    def tile_transforms(self) -> npt.NDArray[np.float64]:
        """(N,4,4) float64 full transform matrix for each rendered tile."""
        ...
    def __len__(self) -> int: ...
    def __iter__(self) -> Iterator[Tile]: ...

class TileLoadRequester:
    def unregister(self) -> None: ...
    def is_registered(self) -> bool: ...

class TilesetViewGroupLoadQueueCheckpoint: ...

class TilesetViewGroup(TileLoadRequester):
    view_update_result: ViewUpdateResult
    worker_thread_load_queue_length: int
    main_thread_load_queue_length: int
    previous_load_progress_percentage: float
    weight: float
    def save_tile_load_queue_checkpoint(
        self,
    ) -> TilesetViewGroupLoadQueueCheckpoint: ...
    def restore_tile_load_queue_checkpoint(
        self, checkpoint: TilesetViewGroupLoadQueueCheckpoint
    ) -> None: ...
    def has_more_tiles_to_load_in_worker_thread(self) -> bool: ...
    def next_tile_to_load_in_worker_thread(self) -> Tile | None: ...
    def has_more_tiles_to_load_in_main_thread(self) -> bool: ...
    def next_tile_to_load_in_main_thread(self) -> Tile | None: ...
    def is_credit_referenced(self, credit: Credit) -> bool: ...

class TilesetFrameState:
    view_group: TilesetViewGroup
    frustums: list[ViewState]
    fog_densities: list[float]

class LoadedConstTileEnumerator:
    def __init__(self, root_tile: Tile | None = None) -> None: ...
    def to_list(self) -> list[Tile]: ...
    def __iter__(self) -> Iterator[Tile]: ...

class LoadedTileEnumerator:
    def __init__(self, root_tile: Tile | None = None) -> None: ...
    def to_list(self) -> list[Tile]: ...
    def __iter__(self) -> Iterator[Tile]: ...

class DebugTileStateDatabase:
    def __init__(self, database_filename: str) -> None: ...
    def record_all_tile_states(
        self, frame_number: int, tileset: Tileset, view_group: TilesetViewGroup
    ) -> None: ...
    def record_tile_state(
        self, frame_number: int, view_group: TilesetViewGroup, tile: Tile
    ) -> None: ...

class EllipsoidTilesetLoader:
    @staticmethod
    def create_tileset(
        externals: TilesetExternals,
        options: TilesetOptions = ...,
    ) -> Tileset:
        """Create a tileset from a procedural ellipsoid."""

class Tileset:
    @overload
    def __init__(
        self,
        externals: TilesetExternals,
        url: str,
        options: TilesetOptions = ...,
    ) -> None: ...
    @overload
    def __init__(
        self,
        externals: TilesetExternals,
        ion_asset_id: int,
        ion_access_token: str,
        options: TilesetOptions = ...,
        ion_asset_endpoint_url: str = "https://api.cesium.com/",
    ) -> None: ...
    @overload
    def __init__(
        self,
        externals: TilesetExternals,
        loader_factory: TilesetContentLoaderFactory,
        options: TilesetOptions = ...,
    ) -> None: ...
    async_system: AsyncSystem
    options: TilesetOptions
    ellipsoid: Ellipsoid
    user_credit: Credit | None
    tileset_credits: list[Credit]
    credit_source: CreditSource
    overlays: RasterOverlayCollection
    root_tile: Tile | None
    root_tile_available_event: SharedFuture[None]
    number_of_tiles_loaded: int
    total_data_bytes: int
    default_view_group: TilesetViewGroup
    metadata: TilesetMetadata | None
    def show_credits_on_screen(self, show_credits_on_screen: bool) -> None: ...
    def compute_load_progress(self) -> float: ...
    def load_metadata(self) -> Future[TilesetMetadata | None]: ...
    def sample_height_most_detailed(
        self, positions: npt.NDArray[np.float64]
    ) -> Future[SampleHeightResult]: ...
    def load_tiles(self) -> None: ...
    def register_load_requester(self, requester: TileLoadRequester) -> None: ...
    def wait_for_all_loads_to_complete(
        self, maximum_wait_time_in_milliseconds: int
    ) -> bool: ...
    def update_view_group(
        self,
        view_group: TilesetViewGroup,
        frustums: list[ViewState],
        delta_time: float = 0.0,
    ) -> ViewUpdateResult: ...
    def update_view_group_offline(
        self, view_group: TilesetViewGroup, frustums: list[ViewState]
    ) -> ViewUpdateResult: ...
    def loaded_tiles(self) -> LoadedConstTileEnumerator: ...
    def for_each_loaded_tile(self, callback: Callable[[Tile], None]) -> None: ...
    def metadata_for_tile(self, tile: Tile) -> TilesetMetadata | None: ...
    def summarize_loaded_tiles(self) -> list[dict[str, Any]]: ...
    @property
    def shared_asset_system(self) -> TilesetSharedAssetSystem: ...
    @property
    def externals(self) -> TilesetExternals: ...
    @property
    def async_destruction_complete_event(self) -> SharedFuture[None]: ...
    def __enter__(self) -> Tileset: ...
    def __exit__(self, exc_type: Any, exc_val: Any, exc_tb: Any) -> None: ...

def create_resolved_future_tile_load_result(
    async_system: AsyncSystem, value: TileLoadResult
) -> Future[TileLoadResult]: ...
def create_resolved_future_sample_height_result(
    async_system: AsyncSystem, value: SampleHeightResult
) -> Future[SampleHeightResult]: ...
def create_tile_id_string(
    tile_id: str | QuadtreeTileID | OctreeTileID | UpsampledQuadtreeNode,
) -> str: ...
def is_loadable_tile_id(
    tile_id: str | QuadtreeTileID | OctreeTileID | UpsampledQuadtreeNode,
) -> bool: ...
def create_tileset_content_loader_factory_from_callbacks(
    create_loader: Callable[
        [TilesetExternals, TilesetOptions, Callable[..., None]],
        FutureTilesetContentLoaderResult,
    ],
    is_valid: Callable[[], bool] | None = None,
) -> TilesetContentLoaderFactory: ...
def bounding_volume_center(bounding_volume: BoundingVolume) -> list[float]: ...
