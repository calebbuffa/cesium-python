from enum import IntEnum
from typing import Any, overload

import numpy as np
import numpy.typing as npt

from .geometry import (
    AxisAlignedBox,
    BoundingSphere,
    CullingResult,
    OrientedBoundingBox,
    Plane,
    QuadtreeTileID,
    Rectangle,
)

class Ellipsoid:
    radii: npt.NDArray[np.float64]

    def __init__(self, x: float, y: float, z: float) -> None: ...
    def __repr__(self) -> str: ...
    WGS84: Ellipsoid
    UNIT_SPHERE: Ellipsoid
    @overload
    def geodetic_surface_normal(
        self, position: Cartographic
    ) -> npt.NDArray[np.float64]: ...
    @overload
    def geodetic_surface_normal(
        self, position: npt.NDArray[np.float64]
    ) -> npt.NDArray[np.float64]: ...
    @overload
    def cartographic_to_cartesian(
        self, cartographic: Cartographic
    ) -> npt.NDArray[np.float64]: ...
    @overload
    def cartographic_to_cartesian(
        self, cartographic: npt.NDArray[np.float64]
    ) -> npt.NDArray[np.float64]: ...
    @overload
    def cartesian_to_cartographic(
        self,
        cartesian: npt.NDArray[np.float64],
    ) -> Cartographic | None: ...
    @overload
    def cartesian_to_cartographic(
        self,
        cartesian: npt.NDArray[np.float64],
    ) -> npt.NDArray[np.float64]: ...
    @overload
    def scale_to_geodetic_surface(
        self,
        cartesian: npt.NDArray[np.float64],
    ) -> npt.NDArray[np.float64] | None:
        """Single (3,) point. Returns None for a degenerate point at Earth center."""
        ...
    @overload
    def scale_to_geodetic_surface(
        self,
        cartesian: npt.NDArray[np.float64],
    ) -> npt.NDArray[np.float64]:
        """Batch (N, 3) array. Returns (N, 3) float64; rows for degenerate points contain NaN."""
        ...
    def scale_to_geocentric_surface(
        self,
        cartesian: npt.NDArray[np.float64],
    ) -> npt.NDArray[np.float64] | None: ...
    maximum_radius: float
    minimum_radius: float

class Cartographic:
    longitude: float
    latitude: float
    height: float

    def __init__(
        self, longitude: float = 0.0, latitude: float = 0.0, height: float = 0.0
    ) -> None: ...
    @overload
    @staticmethod
    def from_degrees(
        longitude_degrees: float, latitude_degrees: float, height: float = 0.0
    ) -> Cartographic: ...
    @overload
    @staticmethod
    def from_degrees(
        longitude_degrees: npt.NDArray[np.float64],
    ) -> npt.NDArray[np.float64]:
        """Batch: accepts (N,3) array of [lon_deg, lat_deg, height], returns (N,3) radians."""
        ...
    def __eq__(self, other: object) -> bool: ...
    def __hash__(self) -> int: ...
    def __repr__(self) -> str: ...

class CartographicPolygon:
    positions: list[Cartographic]
    holes: list[CartographicPolygon]

class GlobeRectangle:
    west: float
    south: float
    east: float
    north: float

    EMPTY: GlobeRectangle
    MAXIMUM: GlobeRectangle

    def __init__(
        self, west: float, south: float, east: float, north: float
    ) -> None: ...
    @staticmethod
    def from_degrees(
        west_degrees: float,
        south_degrees: float,
        east_degrees: float,
        north_degrees: float,
    ) -> GlobeRectangle: ...
    @staticmethod
    def from_rectangle_radians(rectangle: Rectangle) -> GlobeRectangle: ...
    def to_simple_rectangle(self) -> Rectangle: ...
    @property
    def southwest(self) -> Cartographic: ...
    @property
    def southeast(self) -> Cartographic: ...
    @property
    def northwest(self) -> Cartographic: ...
    @property
    def northeast(self) -> Cartographic: ...
    def compute_width(self) -> float: ...
    def compute_height(self) -> float: ...
    def compute_center(self) -> Cartographic: ...
    def contains(self, cartographic: Cartographic) -> bool: ...
    def is_empty(self) -> bool: ...
    def compute_intersection(self, other: GlobeRectangle) -> GlobeRectangle | None: ...
    def compute_union(self, other: GlobeRectangle) -> GlobeRectangle: ...
    def compute_normalized_coordinates(
        self, cartographic: Cartographic
    ) -> npt.NDArray[np.float64]: ...
    def split_at_anti_meridian(self) -> list[GlobeRectangle]: ...
    @staticmethod
    def equals(left: GlobeRectangle, right: GlobeRectangle) -> bool: ...
    @staticmethod
    def equals_epsilon(
        left: GlobeRectangle,
        right: GlobeRectangle,
        relative_epsilon: float,
    ) -> bool: ...
    def __contains__(self, cartographic: Cartographic) -> bool: ...
    def __repr__(self) -> str: ...

class BoundingRegion:
    rectangle: GlobeRectangle
    minimum_height: float
    maximum_height: float
    bounding_box: OrientedBoundingBox
    def __init__(
        self,
        rectangle: GlobeRectangle,
        minimum_height: float,
        maximum_height: float,
        ellipsoid: Ellipsoid = ...,
    ) -> None: ...
    def intersect_plane(self, plane: Plane) -> CullingResult: ...
    @overload
    def compute_distance_squared_to_position(
        self,
        position: npt.NDArray[np.floating],
        ellipsoid: Ellipsoid = ...,
    ) -> float: ...
    @overload
    def compute_distance_squared_to_position(
        self,
        positions: npt.NDArray[np.floating],
        ellipsoid: Ellipsoid = ...,
    ) -> npt.NDArray[np.float64]: ...
    def compute_distance_squared_to_cartographic(
        self, position: Cartographic, ellipsoid: Ellipsoid = ...
    ) -> float: ...
    def compute_distance_squared_to_known_position(
        self,
        cartographic_position: Cartographic,
        cartesian_position: npt.NDArray[np.floating],
    ) -> float: ...
    def compute_union(
        self, other: BoundingRegion, ellipsoid: Ellipsoid = ...
    ) -> BoundingRegion: ...
    @staticmethod
    def equals(left: BoundingRegion, right: BoundingRegion) -> bool: ...
    @staticmethod
    def equals_epsilon(
        left: BoundingRegion, right: BoundingRegion, relative_epsilon: float
    ) -> bool: ...
    def __repr__(self) -> str: ...

class S2CellID:
    id: int
    level: int
    face: int
    center: npt.NDArray[np.float64]
    vertices: list[npt.NDArray[np.float64]]
    parent: S2CellID

    def __init__(self, id: int) -> None: ...
    @staticmethod
    def from_token(token: str) -> S2CellID: ...
    @staticmethod
    def from_face_level_position(face: int, level: int, position: int) -> S2CellID: ...
    @staticmethod
    def from_quadtree_tile_id(
        face: int, quadtree_tile_id: QuadtreeTileID
    ) -> S2CellID: ...
    def is_valid(self) -> bool: ...
    def to_token(self) -> str: ...
    def child(self, index: int) -> S2CellID: ...
    def compute_bounding_rectangle(self) -> GlobeRectangle: ...
    def __eq__(self, other: object) -> bool: ...
    def __hash__(self) -> int: ...
    def __repr__(self) -> str: ...

class S2CellBoundingVolume:
    def __init__(
        self,
        cell_id: S2CellID,
        minimum_height: float,
        maximum_height: float,
        ellipsoid: Ellipsoid = ...,
    ) -> None: ...
    @property
    def cell_id(self) -> S2CellID: ...
    @property
    def minimum_height(self) -> float: ...
    @property
    def maximum_height(self) -> float: ...
    @property
    def center(self) -> npt.NDArray[np.float64]: ...
    @property
    def vertices(self) -> npt.NDArray[np.float64]: ...
    def intersect_plane(self, plane: Plane) -> CullingResult: ...
    @overload
    def compute_distance_squared_to_position(
        self, position: npt.NDArray[np.floating]
    ) -> float: ...
    @overload
    def compute_distance_squared_to_position(
        self, position: npt.NDArray[np.floating]
    ) -> npt.NDArray[np.float64]: ...
    @property
    def bounding_planes(self) -> list[Plane]: ...
    def compute_bounding_region(self, ellipsoid: Ellipsoid = ...) -> BoundingRegion: ...

class GeographicProjection:
    ellipsoid: Ellipsoid

    def __init__(self, ellipsoid: Ellipsoid | None = None) -> None: ...
    @overload
    def project(self, cartographic: Cartographic) -> npt.NDArray[np.float64]: ...
    @overload
    def project(
        self, cartographic: npt.NDArray[np.float64]
    ) -> npt.NDArray[np.float64]: ...
    @overload
    def unproject(self, projected: Cartographic) -> npt.NDArray[np.float64]: ...
    @overload
    def unproject(
        self, projected: npt.NDArray[np.float64]
    ) -> npt.NDArray[np.float64]: ...

class WebMercatorProjection:
    ellipsoid: Ellipsoid

    def __init__(self, ellipsoid: Ellipsoid | None = None) -> None: ...
    @overload
    def project(self, cartographic: Cartographic) -> npt.NDArray[np.float64]: ...
    @overload
    def project(
        self, cartographic: npt.NDArray[np.float64]
    ) -> npt.NDArray[np.float64]: ...
    @overload
    def unproject(self, projected: Cartographic) -> npt.NDArray[np.float64]: ...
    @overload
    def unproject(
        self, projected: npt.NDArray[np.float64]
    ) -> npt.NDArray[np.float64]: ...

class Projection:
    def kind_name(self) -> str: ...

GeodeticProjection = GeographicProjection

class LocalDirection(IntEnum):
    East = 0
    North = 1
    West = 2
    South = 3
    Up = 4
    Down = 5

class LocalHorizontalCoordinateSystem:
    ellipsoid: Ellipsoid
    origin_cartographic: Cartographic
    east: npt.NDArray[np.float64]
    north: npt.NDArray[np.float64]
    up: npt.NDArray[np.float64]
    west: npt.NDArray[np.float64]
    south: npt.NDArray[np.float64]
    down: npt.NDArray[np.float64]

    @overload
    def __init__(
        self,
        origin: Cartographic,
        x_axis_direction: LocalDirection = LocalDirection.East,
        y_axis_direction: LocalDirection = LocalDirection.North,
        z_axis_direction: LocalDirection = LocalDirection.Up,
        scale_to_meters: float = 1.0,
        ellipsoid: Ellipsoid | None = None,
    ) -> None: ...
    @overload
    def __init__(
        self,
        origin_ecef: npt.NDArray[np.float64],
        x_axis_direction: LocalDirection = LocalDirection.East,
        y_axis_direction: LocalDirection = LocalDirection.North,
        z_axis_direction: LocalDirection = LocalDirection.Up,
        scale_to_meters: float = 1.0,
        ellipsoid: Ellipsoid | None = None,
    ) -> None: ...
    @overload
    def __init__(self, local_to_ecef: npt.NDArray[np.float64]) -> None: ...
    @overload
    def __init__(
        self,
        local_to_ecef: npt.NDArray[np.floating[Any]],
        ecef_to_local: npt.NDArray[np.floating[Any]],
    ) -> None: ...
    def local_position_to_ecef(
        self, point: npt.NDArray[np.float64]
    ) -> npt.NDArray[np.float64]: ...
    def ecef_position_to_local(
        self, point: npt.NDArray[np.float64]
    ) -> npt.NDArray[np.float64]: ...
    def local_direction_to_ecef[T: Any](
        self, direction: npt.NDArray[np.floating[T]]
    ) -> npt.NDArray[np.floating[T]]: ...
    def ecef_direction_to_local[T: Any](
        self, direction: npt.NDArray[np.floating[T]]
    ) -> npt.NDArray[np.floating[T]]: ...
    def local_to_ecef_transformation(self) -> npt.NDArray[np.float64]: ...
    def ecef_to_local_transformation(self) -> npt.NDArray[np.float64]: ...

class GlobeTransforms:
    @staticmethod
    def east_north_up_to_fixed_frame(
        origin: npt.NDArray[np.float64], ellipsoid: Ellipsoid | None = None
    ) -> npt.NDArray[np.float64]: ...

class BoundingVolume:
    bounding_sphere: BoundingSphere | None
    oriented_bounding_box: OrientedBoundingBox | None
    bounding_region: BoundingRegion | None
    s2_cell_bounding_volume: S2CellBoundingVolume | None
    @property
    def center(self) -> npt.NDArray[np.float64]: ...
    def as_oriented_bounding_box(
        self, ellipsoid: Ellipsoid = ...
    ) -> OrientedBoundingBox: ...
    def transform(self, transform: npt.NDArray[np.float64]) -> BoundingVolume: ...
    def estimate_globe_rectangle(
        self, ellipsoid: Ellipsoid = ...
    ) -> GlobeRectangle | None: ...
    def get_bounding_region(self) -> BoundingRegion | None: ...

def project_position(
    projection: Projection,
    position: Cartographic | npt.NDArray[np.float64],
) -> npt.NDArray[np.float64]: ...
def unproject_position(
    projection: Projection,
    position: npt.NDArray[np.float64],
) -> npt.NDArray[np.float64]: ...
def projection_ellipsoid(projection: Projection) -> Ellipsoid: ...
def project_rectangle_simple(
    projection: Projection, rectangle: GlobeRectangle
) -> Rectangle: ...
def unproject_rectangle_simple(
    projection: Projection, rectangle: Rectangle
) -> GlobeRectangle: ...
def project_region_simple(
    projection: Projection, region: BoundingRegion
) -> AxisAlignedBox: ...
def unproject_region_simple(
    projection: Projection,
    box: AxisAlignedBox,
    ellipsoid: Ellipsoid = ...,
) -> BoundingRegion: ...
def compute_projected_rectangle_size(
    projection: Projection,
    rectangle: Rectangle,
    max_height: float,
    ellipsoid: Ellipsoid = ...,
) -> npt.NDArray[np.float64]: ...
def calc_quadtree_max_geometric_error(
    ellipsoid: Ellipsoid, tile_image_width: int, level_zero_tiles: int
) -> float: ...

class BoundingRegionBuilder:
    def __init__(self) -> None: ...
    @property
    def pole_tolerance(self) -> float: ...
    @pole_tolerance.setter
    def pole_tolerance(self, value: float) -> None: ...
    def to_region(self, ellipsoid: Ellipsoid = ...) -> BoundingRegion: ...
    def to_globe_rectangle(self) -> GlobeRectangle: ...
    def expand_to_include_position(self, position: npt.NDArray[np.float64]) -> None: ...
    def expand_to_include_globe_rectangle(self, rectangle: GlobeRectangle) -> None: ...

class BoundingRegionWithLooseFittingHeights:
    def __init__(self, bounding_region: BoundingRegion) -> None: ...
    @property
    def bounding_region(self) -> BoundingRegion: ...
    def compute_conservative_distance_squared_to_position(
        self, position: npt.NDArray[np.float64], ellipsoid: Ellipsoid = ...
    ) -> float: ...
    def compute_conservative_distance_squared_to_cartographic(
        self, position: Cartographic, ellipsoid: Ellipsoid = ...
    ) -> float: ...
    def compute_conservative_distance_squared_to_known_position(
        self,
        cartographic_position: Cartographic,
        cartesian_position: npt.NDArray[np.float64],
    ) -> float: ...

class EarthGravitationalModel1996Grid:
    @staticmethod
    def from_buffer(buffer: bytes) -> EarthGravitationalModel1996Grid | None: ...
    @overload
    def sample_height(self, position: Cartographic) -> float: ...
    @overload
    def sample_height(
        self, positions: npt.NDArray[np.float64]
    ) -> npt.NDArray[np.float64]: ...

class EllipsoidTangentPlane:
    def __init__(
        self,
        origin: npt.NDArray[np.float64] | None = None,
        ellipsoid: Ellipsoid = ...,
        east_north_up_to_fixed_frame: npt.NDArray[np.float64] | None = None,
    ) -> None: ...
    @property
    def ellipsoid(self) -> Ellipsoid: ...
    @property
    def origin(self) -> npt.NDArray[np.float64]: ...
    @property
    def x_axis(self) -> npt.NDArray[np.float64]: ...
    @property
    def y_axis(self) -> npt.NDArray[np.float64]: ...
    @property
    def z_axis(self) -> npt.NDArray[np.float64]: ...
    @property
    def plane(self) -> Plane: ...
    def project_point_to_nearest_on_plane(
        self, cartesian: npt.NDArray[np.float64]
    ) -> npt.NDArray[np.float64]: ...

class GlobeAnchor:
    def __init__(self, anchor_to_fixed: npt.NDArray[np.float64]) -> None: ...
    @staticmethod
    def from_anchor_to_fixed_transform(
        anchor_to_fixed: npt.NDArray[np.float64],
    ) -> GlobeAnchor: ...
    @staticmethod
    def from_anchor_to_local_transform(
        local_coordinate_system: LocalHorizontalCoordinateSystem,
        anchor_to_local: npt.NDArray[np.float64],
    ) -> GlobeAnchor: ...
    def anchor_to_fixed_transform(self) -> npt.NDArray[np.float64]: ...
    def set_anchor_to_fixed_transform(
        self,
        new_anchor_to_fixed: npt.NDArray[np.float64],
        adjust_orientation: bool,
        ellipsoid: Ellipsoid = ...,
    ) -> None: ...
    def anchor_to_local_transform(
        self, local_coordinate_system: LocalHorizontalCoordinateSystem
    ) -> npt.NDArray[np.float64]: ...
    def set_anchor_to_local_transform(
        self,
        local_coordinate_system: LocalHorizontalCoordinateSystem,
        new_anchor_to_local: npt.NDArray[np.float64],
        adjust_orientation: bool,
        ellipsoid: Ellipsoid = ...,
    ) -> None: ...

class SimplePlanarEllipsoidCurve:
    @staticmethod
    def from_earth_centered_earth_fixed_coordinates(
        ellipsoid: Ellipsoid,
        source_ecef: npt.NDArray[np.float64],
        destination_ecef: npt.NDArray[np.float64],
    ) -> SimplePlanarEllipsoidCurve | None: ...
    @staticmethod
    def from_longitude_latitude_height(
        ellipsoid: Ellipsoid,
        source: Cartographic,
        destination: Cartographic,
    ) -> SimplePlanarEllipsoidCurve | None: ...
    def position(
        self, percentage: float, additional_height: float = 0.0
    ) -> npt.NDArray[np.float64]: ...
