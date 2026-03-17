from __future__ import annotations

from ..utility import ErrorList
from . import Model, Schema

class GltfWriterResult:
    gltf_bytes: bytes
    errors: ErrorList
    warnings: ErrorList
    def __init__(self) -> None: ...

class GltfWriterOptions:
    pretty_print: bool
    binary_chunk_byte_alignment: int
    def __init__(self) -> None: ...

class SchemaWriterResult:
    schema_bytes: bytes
    errors: ErrorList
    warnings: ErrorList
    def __init__(self) -> None: ...

class SchemaWriterOptions:
    pretty_print: bool
    def __init__(self) -> None: ...

class GltfWriter:
    def __init__(self) -> None: ...
    def write_gltf(
        self, model: Model, options: GltfWriterOptions = ...
    ) -> GltfWriterResult: ...
    def write_glb(
        self, model: Model, buffer_data: bytes, options: GltfWriterOptions = ...
    ) -> GltfWriterResult: ...

class SchemaWriter:
    def __init__(self) -> None: ...
    def write_schema(
        self, schema: Schema, options: SchemaWriterOptions = ...
    ) -> SchemaWriterResult: ...
