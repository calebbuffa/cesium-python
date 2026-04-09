from __future__ import annotations

from collections.abc import Callable
from typing import Any

from ..utility import JsonValue

class TileRefine:
    Replace: int
    Add: int

class BoundingVolume:
    box: list[float] | None
    region: list[float] | None
    sphere: list[float] | None
    @property
    def size_bytes(self) -> int: ...
    def __init__(self) -> None: ...

class Tile:
    bounding_volume: BoundingVolume
    viewer_request_volume: BoundingVolume | None
    geometric_error: float
    refine: TileRefine
    transform: list[float]
    content: Content | None
    contents: list[Content]
    metadata: MetadataEntity | None
    implicit_tiling: ImplicitTiling | None
    children: list[Tile]
    @property
    def size_bytes(self) -> int: ...
    def __init__(self) -> None: ...

class Tileset:
    asset: Asset
    properties: dict[str, Properties]
    schema: Schema | None
    schema_uri: str | None
    statistics: Statistics | None
    groups: list[GroupMetadata]
    metadata: MetadataEntity | None
    geometric_error: float
    root: Tile
    extensions_used: list[str]
    extensions_required: list[str]
    def add_extension_used(self, name: str) -> None: ...
    def add_extension_required(self, name: str) -> None: ...
    def remove_extension_used(self, name: str) -> None: ...
    def remove_extension_required(self, name: str) -> None: ...
    def is_extension_used(self, name: str) -> bool: ...
    def is_extension_required(self, name: str) -> bool: ...
    @property
    def size_bytes(self) -> int: ...
    def for_each_tile(
        self,
        callback: Callable[[Tileset, Tile, Any], None],
    ) -> None: ...
    def for_each_content(
        self,
        callback: Callable[[Tileset, Tile, Content, Any], None],
    ) -> None: ...
    def __init__(self) -> None: ...

class Subtree:
    buffers: list[Buffer]
    buffer_views: list[BufferView]
    property_tables: list[PropertyTable]
    tile_availability: Availability
    content_availability: list[Availability]
    child_subtree_availability: Availability
    tile_metadata: MetadataEntity | None
    content_metadata: list[MetadataEntity]
    subtree_metadata: MetadataEntity | None
    @property
    def size_bytes(self) -> int: ...
    def __init__(self) -> None: ...

class SubtreeBuffer: ...

class Availability:
    bitstream: int | None
    available_count: int | None
    constant: int | None
    @property
    def size_bytes(self) -> int: ...
    def __init__(self) -> None: ...

class ImplicitTileCoordinates: ...

class ImplicitTiling:
    subdivision_scheme: str
    subtree_levels: int
    available_levels: int
    subtrees: Subtrees
    @property
    def size_bytes(self) -> int: ...
    def __init__(self) -> None: ...

class EnumValue:
    name: str
    description: str | None
    value: int
    @property
    def size_bytes(self) -> int: ...
    def __init__(self) -> None: ...

class ClassProperty:
    name: str | None
    description: str | None
    type: str
    component_type: str | None
    enum_type: str | None
    array: bool
    count: int | None
    normalized: bool
    offset: JsonValue | None
    scale: JsonValue | None
    max: JsonValue | None
    min: JsonValue | None
    required: bool
    no_data: JsonValue | None
    default_property: JsonValue | None
    semantic: str | None
    @property
    def size_bytes(self) -> int: ...
    def __init__(self) -> None: ...

class PropertyTableProperty:
    values: int
    array_offsets: int | None
    string_offsets: int | None
    array_offset_type: str
    string_offset_type: str
    offset: JsonValue | None
    scale: JsonValue | None
    max: JsonValue | None
    min: JsonValue | None
    @property
    def size_bytes(self) -> int: ...
    def __init__(self) -> None: ...

class PropertyTable:
    name: str | None
    class_property: str
    count: int
    properties: dict[str, PropertyTableProperty]
    @property
    def size_bytes(self) -> int: ...
    def __init__(self) -> None: ...

class Class:
    name: str | None
    description: str | None
    properties: dict[str, ClassProperty]
    parent: str | None
    @property
    def size_bytes(self) -> int: ...
    def __init__(self) -> None: ...

class Enum:
    name: str | None
    description: str | None
    value_type: str
    values: list[EnumValue]
    @property
    def size_bytes(self) -> int: ...
    def __init__(self) -> None: ...

class MetadataEntity:
    class_property: str
    properties: dict[str, JsonValue]
    @property
    def size_bytes(self) -> int: ...
    def __init__(self) -> None: ...

class GroupMetadata(MetadataEntity):
    @property
    def size_bytes(self) -> int: ...
    def __init__(self) -> None: ...

class PropertyStatistics:
    min: JsonValue | None
    max: JsonValue | None
    mean: JsonValue | None
    median: JsonValue | None
    standard_deviation: JsonValue | None
    variance: JsonValue | None
    sum: JsonValue | None
    occurrences: dict[str, JsonValue]
    @property
    def size_bytes(self) -> int: ...
    def __init__(self) -> None: ...

class ClassStatistics:
    count: int | None
    properties: dict[str, PropertyStatistics]
    @property
    def size_bytes(self) -> int: ...
    def __init__(self) -> None: ...

class Statistics:
    classes: dict[str, ClassStatistics]
    @property
    def size_bytes(self) -> int: ...
    def __init__(self) -> None: ...

class Properties:
    maximum: float
    minimum: float
    @property
    def size_bytes(self) -> int: ...
    def __init__(self) -> None: ...

class Asset:
    version: str
    tileset_version: str | None
    @property
    def size_bytes(self) -> int: ...
    def __init__(self) -> None: ...

class Schema:
    id: str
    name: str | None
    description: str | None
    version: str | None
    classes: dict[str, Class]
    enums: dict[str, Enum]
    @property
    def size_bytes(self) -> int: ...
    def __init__(self) -> None: ...

class BufferCesium:
    def __init__(self) -> None: ...
    @property
    def data(self) -> bytes: ...
    @data.setter
    def data(self, value: bytes) -> None: ...

class Buffer:
    def __init__(self) -> None: ...
    cesium: BufferCesium
    @property
    def size_bytes(self) -> int: ...

class BufferView:
    def __init__(self) -> None: ...
    buffer: int
    byte_offset: int
    byte_length: int
    name: str
    @property
    def size_bytes(self) -> int: ...

class Content:
    def __init__(self) -> None: ...
    bounding_volume: BoundingVolume
    uri: str
    metadata: int | None
    group: int | None
    @property
    def size_bytes(self) -> int: ...

class Extension3dTilesBoundingVolumeCylinder:
    def __init__(self) -> None: ...
    min_radius: float
    max_radius: float
    height: float
    min_angle: float
    max_angle: float
    translation: list[float]
    rotation: list[float]
    @property
    def size_bytes(self) -> int: ...

class Extension3dTilesBoundingVolumeS2:
    def __init__(self) -> None: ...
    token: str
    minimum_height: float
    maximum_height: float
    @property
    def size_bytes(self) -> int: ...

class Extension3dTilesEllipsoid:
    def __init__(self) -> None: ...
    body: str
    radii: list[float]
    @property
    def size_bytes(self) -> int: ...

class Padding:
    def __init__(self) -> None: ...
    before: int
    after: int
    @property
    def size_bytes(self) -> int: ...

class ExtensionContent3dTilesContentVoxels:
    def __init__(self) -> None: ...
    dimensions: list[int]
    padding: Padding
    class_property: str
    @property
    def size_bytes(self) -> int: ...

class Subtrees:
    def __init__(self) -> None: ...
    uri: str
    @property
    def size_bytes(self) -> int: ...

class FoundMetadataProperty:
    @property
    def class_identifier(self) -> str: ...
    @property
    def class_definition(self) -> Class: ...
    @property
    def property_identifier(self) -> str: ...
    @property
    def property_definition(self) -> ClassProperty: ...
    @property
    def property_value(self) -> JsonValue: ...

class MetadataQuery:
    @staticmethod
    def find_first_property_with_semantic(
        schema: Schema,
        entity: MetadataEntity,
        semantic: str,
    ) -> FoundMetadataProperty | None: ...
