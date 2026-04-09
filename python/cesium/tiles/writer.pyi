from ..json.writer import ExtensionWriterContext
from . import Schema, Subtree, Tileset

class TilesetWriterResult:
    @property
    def tileset_bytes(self) -> bytes: ...
    errors: list[str]
    warnings: list[str]

class TilesetWriterOptions:
    pretty_print: bool

class SubtreeWriterResult:
    @property
    def subtree_bytes(self) -> bytes: ...
    errors: list[str]
    warnings: list[str]

class SubtreeWriterOptions:
    pretty_print: bool

class SchemaWriterResult:
    @property
    def schema_bytes(self) -> bytes: ...
    errors: list[str]
    warnings: list[str]

class SchemaWriterOptions:
    pretty_print: bool

class TilesetWriter:
    @property
    def extensions(self) -> ExtensionWriterContext: ...
    def write_tileset(
        self, tileset: Tileset, options: TilesetWriterOptions = ...
    ) -> TilesetWriterResult: ...

class SubtreeWriter:
    @property
    def extensions(self) -> ExtensionWriterContext: ...
    def write_subtree_json(
        self, subtree: Subtree, options: SubtreeWriterOptions = ...
    ) -> SubtreeWriterResult: ...
    def write_subtree_binary(
        self,
        subtree: Subtree,
        buffer_data: bytes,
        options: SubtreeWriterOptions = ...,
    ) -> SubtreeWriterResult: ...

class SchemaWriter:
    @property
    def extensions(self) -> ExtensionWriterContext: ...
    def write_schema(
        self, schema: Schema, options: SchemaWriterOptions = ...
    ) -> SchemaWriterResult: ...

def write_tileset(
    tileset: Tileset, options: TilesetWriterOptions = ...
) -> TilesetWriterResult: ...
def write_subtree_json(
    subtree: Subtree, options: SubtreeWriterOptions = ...
) -> SubtreeWriterResult: ...
def write_schema(
    schema: Schema, options: SchemaWriterOptions = ...
) -> SchemaWriterResult: ...
