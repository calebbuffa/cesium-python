from __future__ import annotations

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
    geometric_error: float
    refine: TileRefine
    children: list[Tile]
    content_uri: str

class Tileset:
    root: Tile
    geometric_error: float

class Subtree: ...
class SubtreeBuffer: ...
class Availability: ...
class ImplicitTileCoordinates: ...
class ImplicitTiling: ...

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
