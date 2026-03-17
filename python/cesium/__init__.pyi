from typing import TypeAlias

from . import (
    geospatial,
    utility,
)
from .tiles import selection

TilesetOptions: TypeAlias = selection.TilesetOptions
ViewState: TypeAlias = selection.ViewState
Tile: TypeAlias = selection.Tile
TilesetMetadata: TypeAlias = selection.TilesetMetadata
TilesetViewGroup: TypeAlias = selection.TilesetViewGroup
RasterOverlayCollection: TypeAlias = selection.RasterOverlayCollection
Uri: TypeAlias = utility.Uri
Ellipsoid: TypeAlias = geospatial.Ellipsoid
Cartographic: TypeAlias = geospatial.Cartographic
LocalHorizontalCoordinateSystem: TypeAlias = geospatial.LocalHorizontalCoordinateSystem
