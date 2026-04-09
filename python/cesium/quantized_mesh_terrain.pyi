from typing import Any

from .geometry import QuadtreeTileID, QuadtreeTileRectangularRange, QuadtreeTilingScheme
from .geospatial import BoundingRegion, Ellipsoid, Projection
from .gltf import Model
from .json.writer import ExtensionWriterContext
from .utility import ErrorList

class AvailabilityRectangle:
    start_x: int
    start_y: int
    end_x: int
    end_y: int
    @property
    def size_bytes(self) -> int: ...

class Layer:
    attribution: str
    available: list[list[AvailabilityRectangle]]
    bounds: list[float]
    description: str | None
    extensions_property: str | None
    format: str
    maxzoom: int
    minzoom: int
    metadata_availability: int
    name: str | None
    parent_url: str | None
    projection: str
    scheme: str
    tiles: list[str]
    version: str
    @property
    def size_bytes(self) -> int: ...
    def get_projection(self, ellipsoid: Ellipsoid = ...) -> Projection | None: ...
    def get_tiling_scheme(
        self, ellipsoid: Ellipsoid = ...
    ) -> QuadtreeTilingScheme | None: ...
    def get_root_bounding_region(
        self, ellipsoid: Ellipsoid = ...
    ) -> BoundingRegion | None: ...

class LayerWriterOptions:
    def __init__(self) -> None: ...
    pretty_print: bool

class LayerWriterResult:
    def __init__(self) -> None: ...
    @property
    def bytes(self) -> bytes: ...
    errors: list[str]
    warnings: list[str]

class LayerWriter:
    def __init__(self) -> None: ...
    @property
    def extensions(self) -> ExtensionWriterContext: ...
    def write(
        self, layer: Layer, options: LayerWriterOptions = ...
    ) -> LayerWriterResult: ...

class LayerReader:
    def __init__(self) -> None: ...
    @property
    def options(self) -> Any: ...
    def read_from_json_summary(self, layer_json_bytes: bytes) -> dict[str, Any]: ...

def read_layer_json_summary(layer_json_bytes: bytes) -> dict[str, Any]: ...

class QuantizedMeshLoadResult:
    model: Model | None
    updated_bounding_volume: BoundingRegion | None
    available_tile_rectangles: list[QuadtreeTileRectangularRange]
    errors: ErrorList

    def __init__(self) -> None: ...

class QuantizedMeshMetadataResult:
    availability: list[QuadtreeTileRectangularRange]
    errors: ErrorList

    def __init__(self) -> None: ...

class QuantizedMeshLoader:
    @staticmethod
    def load(
        tile_id: QuadtreeTileID,
        tile_bounding_volume: BoundingRegion,
        url: str,
        data: bytes,
        enable_water_mask: bool = False,
        ellipsoid: Ellipsoid = ...,
    ) -> QuantizedMeshLoadResult: ...
    @staticmethod
    def load_metadata(
        data: bytes,
        tile_id: QuadtreeTileID,
    ) -> QuantizedMeshMetadataResult: ...
