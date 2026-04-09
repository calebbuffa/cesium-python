from __future__ import annotations

from typing import Any, overload

import numpy as np
import numpy.typing as npt

from ..async_ import AsyncSystem, IAssetAccessor
from ..geometry import (
    Axis,
    BoundingCylinderRegion,
    BoundingSphere,
    OctreeTileID,
    OrientedBoundingBox,
    QuadtreeTileID,
)
from ..geospatial import BoundingRegion, Ellipsoid, S2CellBoundingVolume
from ..gltf import Model
from ..gltf.reader import GltfReaderOptions
from ..utility import ErrorList
from . import BoundingVolume, Subtree, Tile

class BinaryToGltfConverter:
    @staticmethod
    def convert(
        gltf_binary: bytes, options: GltfReaderOptions, asset_fetcher: AssetFetcher
    ) -> FutureGltfConverterResult: ...

class B3dmToGltfConverter:
    @staticmethod
    def convert(
        b3dm_binary: bytes, options: GltfReaderOptions, asset_fetcher: AssetFetcher
    ) -> FutureGltfConverterResult: ...

class I3dmToGltfConverter:
    @staticmethod
    def convert(
        instances_binary: bytes, options: GltfReaderOptions, asset_fetcher: AssetFetcher
    ) -> FutureGltfConverterResult: ...

class PntsToGltfConverter:
    @staticmethod
    def convert(
        pnts_binary: bytes, options: GltfReaderOptions, asset_fetcher: AssetFetcher
    ) -> FutureGltfConverterResult: ...

class CmptToGltfConverter:
    @staticmethod
    def convert(
        cmpt_binary: bytes, options: GltfReaderOptions, asset_fetcher: AssetFetcher
    ) -> FutureGltfConverterResult: ...

def register_all_tile_content_types() -> None:
    """Register all built-in tile content loaders (glTF, b3dm, pnts, etc.)."""
    ...

class FutureGltfConverterResult:
    def wait(self) -> GltfConverterResult: ...
    def wait_in_main_thread(self) -> GltfConverterResult: ...
    def is_ready(self) -> bool: ...
    def share(self) -> SharedFutureGltfConverterResult: ...

class SharedFutureGltfConverterResult:
    def wait(self) -> GltfConverterResult: ...
    def wait_in_main_thread(self) -> GltfConverterResult: ...
    def is_ready(self) -> bool: ...

class FutureAssetFetcherResult:
    def wait(self) -> AssetFetcherResult: ...
    def wait_in_main_thread(self) -> AssetFetcherResult: ...
    def is_ready(self) -> bool: ...
    def share(self) -> SharedFutureAssetFetcherResult: ...

class SharedFutureAssetFetcherResult:
    def wait(self) -> AssetFetcherResult: ...
    def wait_in_main_thread(self) -> AssetFetcherResult: ...
    def is_ready(self) -> bool: ...

class AssetFetcher:
    def __init__(
        self,
        async_system: AsyncSystem,
        asset_accessor: IAssetAccessor,
        base_url: str,
        tile_transform: npt.NDArray[np.float64] | list[float],
        request_headers: dict[str, str] = ...,
        up_axis: Axis = ...,
    ) -> None: ...
    def get(self, relative_url: str) -> FutureAssetFetcherResult: ...
    async_system: AsyncSystem
    asset_accessor: IAssetAccessor
    base_url: str
    @property
    def tile_transform(self) -> npt.NDArray[np.float64]: ...
    @tile_transform.setter
    def tile_transform(self, value: npt.NDArray[np.float64] | list[float]) -> None: ...
    @property
    def request_headers(self) -> dict[str, str]: ...
    @request_headers.setter
    def request_headers(self, value: dict[str, str]) -> None: ...
    up_axis: Axis

class AssetFetcherResult:
    def __init__(self) -> None: ...
    @property
    def bytes(self) -> npt.NDArray[np.uint8]: ...
    @bytes.setter
    def bytes(self, value: bytes) -> None: ...
    error_list: ErrorList

class GltfConverterResult:
    def __init__(self) -> None: ...
    model: Model | None
    errors: ErrorList

class GltfConverters:
    @staticmethod
    def register_magic(magic: str, converter_name: str) -> None: ...
    @staticmethod
    def register_file_extension(file_extension: str, converter_name: str) -> None: ...
    @staticmethod
    def converter_by_file_extension(file_path: str) -> str | None: ...
    @staticmethod
    def converter_by_magic(content: bytes) -> str | None: ...
    @overload
    @staticmethod
    def convert(
        file_path: str,
        content: bytes,
        options: GltfReaderOptions,
        asset_fetcher: AssetFetcher,
    ) -> FutureGltfConverterResult: ...
    @overload
    @staticmethod
    def convert(
        content: bytes, options: GltfReaderOptions, asset_fetcher: AssetFetcher
    ) -> FutureGltfConverterResult: ...

def create_buffer_in_gltf(gltf: Model, buffer: bytes = ...) -> int: ...
def create_buffer_view_in_gltf(
    gltf: Model, buffer_id: int, byte_length: int, byte_stride: int
) -> int: ...
def create_accessor_in_gltf(
    gltf: Model, buffer_view_id: int, component_type: int, count: int, type: str
) -> int: ...
def apply_rtc_to_nodes(
    gltf: Model, rtc: npt.NDArray[np.float64] | list[float]
) -> None: ...
def parse_offset_for_semantic(json: str, semantic: str) -> dict[str, Any]: ...
def parse_array_value_dvec3(json: str, name: str) -> list[float] | None: ...

class ImplicitTileSubdivisionScheme:
    Quadtree: int
    Octree: int

class SubtreeConstantAvailability:
    def __init__(self) -> None: ...
    constant: bool

class SubtreeAvailability:
    @staticmethod
    def from_subtree(
        subdivision_scheme: ImplicitTileSubdivisionScheme,
        levels_in_subtree: int,
        subtree: Subtree,
    ) -> SubtreeAvailability | None: ...
    @staticmethod
    def create_empty(
        subdivision_scheme: ImplicitTileSubdivisionScheme,
        levels_in_subtree: int,
        set_tiles_available: bool,
    ) -> SubtreeAvailability: ...
    @overload
    def is_tile_available(
        self, subtree_id: QuadtreeTileID, tile_id: QuadtreeTileID
    ) -> bool: ...
    @overload
    def is_tile_available(
        self, subtree_id: OctreeTileID, tile_id: OctreeTileID
    ) -> bool: ...
    @overload
    def is_tile_available(
        self, relative_tile_level: int, relative_tile_morton_id: int
    ) -> bool: ...
    @overload
    def set_tile_available(
        self, subtree_id: QuadtreeTileID, tile_id: QuadtreeTileID, is_available: bool
    ) -> None: ...
    @overload
    def set_tile_available(
        self, subtree_id: OctreeTileID, tile_id: OctreeTileID, is_available: bool
    ) -> None: ...
    @overload
    def set_tile_available(
        self, relative_tile_level: int, relative_tile_morton_id: int, is_available: bool
    ) -> None: ...
    @overload
    def is_content_available(
        self, subtree_id: QuadtreeTileID, tile_id: QuadtreeTileID, content_id: int
    ) -> bool: ...
    @overload
    def is_content_available(
        self, subtree_id: OctreeTileID, tile_id: OctreeTileID, content_id: int
    ) -> bool: ...
    @overload
    def is_content_available(
        self, relative_tile_level: int, relative_tile_morton_id: int, content_id: int
    ) -> bool: ...
    @overload
    def set_content_available(
        self,
        subtree_id: QuadtreeTileID,
        tile_id: QuadtreeTileID,
        content_id: int,
        is_available: bool,
    ) -> None: ...
    @overload
    def set_content_available(
        self,
        subtree_id: OctreeTileID,
        tile_id: OctreeTileID,
        content_id: int,
        is_available: bool,
    ) -> None: ...
    @overload
    def set_content_available(
        self,
        relative_tile_level: int,
        relative_tile_morton_id: int,
        content_id: int,
        is_available: bool,
    ) -> None: ...
    @overload
    def is_subtree_available(
        self, this_subtree_id: QuadtreeTileID, check_subtree_id: QuadtreeTileID
    ) -> bool: ...
    @overload
    def is_subtree_available(
        self, this_subtree_id: OctreeTileID, check_subtree_id: OctreeTileID
    ) -> bool: ...
    @overload
    def is_subtree_available(self, relative_subtree_morton_id: int) -> bool: ...
    @overload
    def set_subtree_available(
        self,
        this_subtree_id: QuadtreeTileID,
        set_subtree_id: QuadtreeTileID,
        is_available: bool,
    ) -> None: ...
    @overload
    def set_subtree_available(
        self,
        this_subtree_id: OctreeTileID,
        set_subtree_id: OctreeTileID,
        is_available: bool,
    ) -> None: ...
    @overload
    def set_subtree_available(
        self, relative_subtree_morton_id: int, is_available: bool
    ) -> None: ...
    def subtree(self) -> Subtree: ...
    @overload
    def is_tile_available(
        self,
        relative_tile_level: int,
        relative_tile_morton_ids: npt.NDArray[np.uint64],
    ) -> npt.NDArray[np.bool_]: ...
    @overload
    def is_content_available(
        self,
        relative_tile_level: int,
        relative_tile_morton_ids: npt.NDArray[np.uint64],
        content_id: int,
    ) -> npt.NDArray[np.bool_]: ...

class QuadtreeChildren:
    def __init__(self, tile_id: QuadtreeTileID) -> None: ...
    def size(self) -> int: ...
    def to_list(self) -> list[QuadtreeTileID]: ...

class OctreeChildren:
    def __init__(self, tile_id: OctreeTileID) -> None: ...
    def size(self) -> int: ...
    def to_list(self) -> list[OctreeTileID]: ...

class TileTransform:
    @staticmethod
    def transform(tile: Tile) -> list[float] | None: ...
    @staticmethod
    def set_transform(
        tile: Tile, values: npt.NDArray[np.float64] | list[float]
    ) -> None: ...

class TileBoundingVolumes:
    @staticmethod
    def oriented_bounding_box(
        bounding_volume: BoundingVolume,
    ) -> OrientedBoundingBox: ...
    @staticmethod
    def set_oriented_bounding_box(
        bounding_volume: BoundingVolume, obb: OrientedBoundingBox
    ) -> None: ...
    @staticmethod
    def bounding_region(
        bounding_volume: BoundingVolume, ellipsoid: Ellipsoid
    ) -> BoundingRegion: ...
    @staticmethod
    def set_bounding_region(
        bounding_volume: BoundingVolume, region: BoundingRegion
    ) -> None: ...
    @staticmethod
    def bounding_sphere(bounding_volume: BoundingVolume) -> BoundingSphere: ...
    @staticmethod
    def set_bounding_sphere(
        bounding_volume: BoundingVolume, sphere: BoundingSphere
    ) -> None: ...
    @staticmethod
    def s2_cell_bounding_volume(
        bounding_volume: BoundingVolume, ellipsoid: Ellipsoid
    ) -> S2CellBoundingVolume: ...
    @staticmethod
    def set_s2_cell_bounding_volume(
        bounding_volume: BoundingVolume, volume: S2CellBoundingVolume
    ) -> None: ...
    @staticmethod
    def bounding_cylinder_region(
        bounding_volume: BoundingVolume,
    ) -> BoundingCylinderRegion: ...
    @staticmethod
    def set_bounding_cylinder_region(
        bounding_volume: BoundingVolume, region: BoundingCylinderRegion
    ) -> None: ...

class ImplicitTilingUtilities:
    @overload
    @staticmethod
    def resolve_url(
        url_template: str, package_url: str, tile_id: QuadtreeTileID
    ) -> str: ...
    @overload
    @staticmethod
    def resolve_url(
        url_template: str, package_url: str, tile_id: OctreeTileID
    ) -> str: ...
    @staticmethod
    def compute_level_denominator(level: int) -> int: ...
    @overload
    @staticmethod
    def compute_morton_index(tile_id: QuadtreeTileID) -> int: ...
    @overload
    @staticmethod
    def compute_morton_index(tile_id: OctreeTileID) -> int: ...
    @overload
    @staticmethod
    def compute_relative_morton_index(
        parent_id: QuadtreeTileID, child_id: QuadtreeTileID
    ) -> int: ...
    @overload
    @staticmethod
    def compute_relative_morton_index(
        parent_id: OctreeTileID, child_id: OctreeTileID
    ) -> int: ...
    @overload
    @staticmethod
    def subtree_root_id(
        levels_in_subtree: int, tile_id: QuadtreeTileID
    ) -> QuadtreeTileID: ...
    @overload
    @staticmethod
    def subtree_root_id(
        levels_in_subtree: int, tile_id: OctreeTileID
    ) -> OctreeTileID: ...
    @overload
    @staticmethod
    def absolute_tile_id_to_relative(
        absolute_id: QuadtreeTileID, root_id: QuadtreeTileID
    ) -> QuadtreeTileID: ...
    @overload
    @staticmethod
    def absolute_tile_id_to_relative(
        absolute_id: OctreeTileID, root_id: OctreeTileID
    ) -> OctreeTileID: ...
    @overload
    @staticmethod
    def parent_id(tile_id: QuadtreeTileID) -> QuadtreeTileID: ...
    @overload
    @staticmethod
    def parent_id(tile_id: OctreeTileID) -> OctreeTileID: ...
    @overload
    @staticmethod
    def children(tile_id: QuadtreeTileID) -> QuadtreeChildren: ...
    @overload
    @staticmethod
    def children(tile_id: OctreeTileID) -> OctreeChildren: ...
    @overload
    @staticmethod
    def compute_bounding_volume(
        root_bounding_volume: BoundingVolume,
        tile_id: QuadtreeTileID,
        ellipsoid: Ellipsoid,
    ) -> BoundingVolume: ...
    @overload
    @staticmethod
    def compute_bounding_volume(
        root_bounding_volume: BoundingVolume,
        tile_id: OctreeTileID,
        ellipsoid: Ellipsoid,
    ) -> BoundingVolume: ...
    @overload
    @staticmethod
    def compute_bounding_volume(
        root_bounding_volume: BoundingRegion,
        tile_id: QuadtreeTileID,
        ellipsoid: Ellipsoid,
    ) -> BoundingVolume: ...
    @overload
    @staticmethod
    def compute_bounding_volume(
        root_bounding_volume: BoundingRegion,
        tile_id: OctreeTileID,
        ellipsoid: Ellipsoid,
    ) -> BoundingVolume: ...
    @overload
    @staticmethod
    def compute_bounding_volume(
        root_bounding_volume: OrientedBoundingBox, tile_id: QuadtreeTileID
    ) -> BoundingVolume: ...
    @overload
    @staticmethod
    def compute_bounding_volume(
        root_bounding_volume: OrientedBoundingBox, tile_id: OctreeTileID
    ) -> BoundingVolume: ...
    @overload
    @staticmethod
    def compute_bounding_volume(
        root_bounding_volume: S2CellBoundingVolume,
        tile_id: QuadtreeTileID,
        ellipsoid: Ellipsoid,
    ) -> BoundingVolume: ...
    @overload
    @staticmethod
    def compute_bounding_volume(
        root_bounding_volume: S2CellBoundingVolume,
        tile_id: OctreeTileID,
        ellipsoid: Ellipsoid,
    ) -> BoundingVolume: ...
    @overload
    @staticmethod
    def compute_bounding_volume(
        root_bounding_volume: BoundingCylinderRegion, tile_id: QuadtreeTileID
    ) -> BoundingVolume: ...
    @overload
    @staticmethod
    def compute_bounding_volume(
        root_bounding_volume: BoundingCylinderRegion, tile_id: OctreeTileID
    ) -> BoundingVolume: ...
    @overload
    @staticmethod
    def compute_bounding_region(
        root_bounding_region: BoundingRegion,
        tile_id: QuadtreeTileID,
        ellipsoid: Ellipsoid,
    ) -> BoundingVolume: ...
    @overload
    @staticmethod
    def compute_bounding_region(
        root_bounding_region: BoundingRegion,
        tile_id: OctreeTileID,
        ellipsoid: Ellipsoid,
    ) -> BoundingVolume: ...
    @overload
    @staticmethod
    def compute_morton_index(
        quadtree_tile_ids: npt.NDArray[np.uint32],
    ) -> npt.NDArray[np.uint64]:
        """Batch compute morton indices for an (N,3) array of (level, x, y)."""
