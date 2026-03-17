from collections.abc import Iterable
from enum import IntEnum
from typing import Any, overload

import numpy as np
import numpy.typing as npt

class Ray:
    """A ray defined by an origin point and a normalized direction vector."""

    def __init__(
        self,
        origin: npt.NDArray[np.floating],
        direction: npt.NDArray[np.floating],
    ) -> None: ...
    @property
    def origin(self) -> npt.NDArray[np.float64]: ...
    @property
    def direction(self) -> npt.NDArray[np.float64]: ...
    def point_from_distance(self, distance: float) -> npt.NDArray[np.float64]:
        """Return the point at origin + distance * direction."""
        ...
    def transform(self, transformation: npt.NDArray[np.floating]) -> Ray:
        """Transform this ray using a 4x4 matrix."""
        ...
    def __neg__(self) -> Ray: ...
    def __repr__(self) -> str: ...

class Plane:
    """An infinite plane defined by a unit normal and a signed distance from the origin."""

    @overload
    def __init__(self, normal: npt.NDArray[np.floating], distance: float) -> None: ...
    @overload
    def __init__(
        self,
        point: npt.NDArray[np.floating],
        normal: npt.NDArray[np.floating],
    ) -> None: ...

    ORIGIN_XY_PLANE: Plane
    ORIGIN_YZ_PLANE: Plane
    ORIGIN_ZX_PLANE: Plane

    @property
    def normal(self) -> npt.NDArray[np.float64]: ...
    @property
    def distance(self) -> float: ...
    def get_point_distance(
        self, point: npt.NDArray[np.floating]
    ) -> float | npt.NDArray[np.float64]:
        """Signed distance from *point* to this plane. Accepts (3,) or (N, 3)."""
        ...

    def project_point_onto_plane(
        self, point: npt.NDArray[np.floating]
    ) -> npt.NDArray[np.float64]:
        """Project *point* onto the plane. Accepts (3,) or (N, 3)."""
        ...
    def __repr__(self) -> str: ...

class AxisAlignedBox:
    """An axis-aligned bounding box."""

    def __init__(
        self,
        minimum_x: float,
        minimum_y: float,
        minimum_z: float,
        maximum_x: float,
        maximum_y: float,
        maximum_z: float,
    ) -> None: ...

    minimum_x: float
    minimum_y: float
    minimum_z: float
    maximum_x: float
    maximum_y: float
    maximum_z: float
    length_x: float
    length_y: float
    length_z: float

    @property
    def center(self) -> npt.NDArray[np.float64]: ...
    def contains(
        self, position: npt.NDArray[np.floating]
    ) -> bool | npt.NDArray[np.bool_]: ...
    def __contains__(self, position: npt.NDArray[np.floating]) -> bool: ...
    def __repr__(self) -> str: ...
    @staticmethod
    def from_positions(
        positions: npt.NDArray[np.floating],
    ) -> AxisAlignedBox:
        """Compute the tightest AABB containing all given (N, 3) positions."""
        ...

class Rectangle:
    """A 2-D axis-aligned rectangle."""

    def __init__(
        self,
        minimum_x: float,
        minimum_y: float,
        maximum_x: float,
        maximum_y: float,
    ) -> None: ...

    minimum_x: float
    minimum_y: float
    maximum_x: float
    maximum_y: float

    @property
    def lower_left(self) -> npt.NDArray[np.float64]: ...
    @property
    def lower_right(self) -> npt.NDArray[np.float64]: ...
    @property
    def upper_left(self) -> npt.NDArray[np.float64]: ...
    @property
    def upper_right(self) -> npt.NDArray[np.float64]: ...
    @property
    def center(self) -> npt.NDArray[np.float64]: ...
    def contains(
        self, position: npt.NDArray[np.floating]
    ) -> bool | npt.NDArray[np.bool_]: ...
    def __contains__(self, position: npt.NDArray[np.floating]) -> bool: ...
    def __repr__(self) -> str: ...
    def overlaps(self, other: Rectangle) -> bool: ...
    def fully_contains(self, other: Rectangle) -> bool: ...
    def compute_signed_distance(
        self, position: npt.NDArray[np.floating]
    ) -> float | npt.NDArray[np.float64]: ...
    def compute_width(self) -> float: ...
    def compute_height(self) -> float: ...
    def compute_intersection(self, other: Rectangle) -> Rectangle: ...
    def compute_union(self, other: Rectangle) -> Rectangle: ...

class CullingResult(IntEnum):
    Outside = ...
    Intersecting = ...
    Inside = ...

class CullingVolume:
    """Six-plane frustum used for visibility culling."""

    left_plane: Plane
    right_plane: Plane
    top_plane: Plane
    bottom_plane: Plane

class Transforms:
    """Coordinate-system and projection matrix utilities."""

    Y_UP_TO_Z_UP: npt.NDArray[np.float64]
    Z_UP_TO_Y_UP: npt.NDArray[np.float64]
    X_UP_TO_Z_UP: npt.NDArray[np.float64]
    Z_UP_TO_X_UP: npt.NDArray[np.float64]
    X_UP_TO_Y_UP: npt.NDArray[np.float64]
    Y_UP_TO_X_UP: npt.NDArray[np.float64]

    @staticmethod
    def create_translation_rotation_scale_matrix(
        translation: npt.NDArray[np.floating],
        rotation: npt.NDArray[np.floating],
        scale: npt.NDArray[np.floating],
    ) -> npt.NDArray[np.float64]:
        """Build a 4x4 TRS matrix from translation (3,), quaternion (4,), scale (3,)."""
        ...

    @staticmethod
    def compute_translation_rotation_scale_from_matrix(
        matrix: npt.NDArray[np.floating],
    ) -> tuple[
        npt.NDArray[np.float64], npt.NDArray[np.float64], npt.NDArray[np.float64]
    ]:
        """Decompose a 4x4 matrix into (translation, quaternion, scale)."""
        ...

    @staticmethod
    def get_up_axis_transform(
        from_axis: Axis, to_axis: Axis
    ) -> npt.NDArray[np.float64]:
        """Return the 4x4 matrix that re-orients *from_axis* up to *to_axis* up."""
        ...

    @staticmethod
    def create_view_matrix(
        position: npt.NDArray[np.floating],
        direction: npt.NDArray[np.floating],
        up: npt.NDArray[np.floating],
    ) -> npt.NDArray[np.float64]:
        """Create a camera view (world-to-camera) 4x4 matrix."""
        ...

    @staticmethod
    def create_perspective_matrix(
        fovx: float,
        fovy: float,
        near: float,
        far: float,
    ) -> npt.NDArray[np.float64]: ...
    @staticmethod
    def create_perspective_matrix_from_extents(
        left: float,
        right: float,
        bottom: float,
        top: float,
        near: float,
        far: float,
    ) -> npt.NDArray[np.float64]: ...
    @staticmethod
    def create_orthographic_matrix(
        left: float,
        right: float,
        bottom: float,
        top: float,
        near: float,
        far: float,
    ) -> npt.NDArray[np.float64]: ...

class IntersectionTests:
    """Static ray-intersection tests against common geometric primitives."""

    @staticmethod
    def ray_plane(ray: Ray, plane: Plane) -> npt.NDArray[np.float64] | None:
        """Intersect *ray* with *plane*. Returns the hit point or None."""
        ...

    @staticmethod
    def ray_ellipsoid(
        ray: Ray, radii: npt.NDArray[np.floating]
    ) -> npt.NDArray[np.float64] | None:
        """Intersect *ray* with the ellipsoid of the given radii (3,).
        Returns [t_entry, t_exit] or None."""
        ...

    @staticmethod
    def ray_triangle(
        ray: Ray,
        p0: npt.NDArray[np.floating],
        p1: npt.NDArray[np.floating],
        p2: npt.NDArray[np.floating],
        cull_back_faces: bool = False,
    ) -> npt.NDArray[np.float64] | None:
        """Intersect *ray* with a triangle. Returns the hit point or None."""
        ...

    @overload
    @staticmethod
    def ray_triangle_parametric(
        ray: Ray,
        p0: npt.NDArray[np.floating],
        p1: npt.NDArray[np.floating],
        p2: npt.NDArray[np.floating],
        cull_back_faces: bool = False,
    ) -> float | None:
        """Like ray_triangle but returns the parametric t value."""
        ...

    @overload
    @staticmethod
    def ray_triangle_parametric(
        origins: npt.NDArray[np.floating],
        directions: npt.NDArray[np.floating],
        p0: npt.NDArray[np.floating],
        p1: npt.NDArray[np.floating],
        p2: npt.NDArray[np.floating],
        cull_back_faces: bool = False,
    ) -> npt.NDArray[np.float64]:
        """Batch ray-triangle: (N,3) origins + (N,3) directions vs one triangle. Returns (N,) t values (NaN=miss)."""
        ...

    @staticmethod
    def ray_aabb(ray: Ray, aabb: AxisAlignedBox) -> npt.NDArray[np.float64] | None: ...
    @staticmethod
    def ray_aabb_parametric(ray: Ray, aabb: AxisAlignedBox) -> float | None: ...
    @overload
    @staticmethod
    def ray_obb(
        ray: Ray, obb: OrientedBoundingBox
    ) -> npt.NDArray[np.float64] | None: ...
    @overload
    @staticmethod
    def ray_obb(
        origins: npt.NDArray[np.floating],
        directions: npt.NDArray[np.floating],
        obb: OrientedBoundingBox,
    ) -> npt.NDArray[np.float64]:
        """Vectorized: (N,3) origins, (N,3) directions, OBB -> (N,3) hit points (NaN=miss)."""
        ...
    @overload
    @staticmethod
    def ray_obb_parametric(ray: Ray, obb: OrientedBoundingBox) -> float | None: ...
    @overload
    @staticmethod
    def ray_obb_parametric(
        origins: npt.NDArray[np.floating],
        directions: npt.NDArray[np.floating],
        obb: OrientedBoundingBox,
    ) -> npt.NDArray[np.float64]:
        """Vectorized: (N,3) origins, (N,3) directions, OBB -> (N,) float64 t (NaN=miss)."""
        ...
    @staticmethod
    def ray_sphere(
        ray: Ray, sphere: BoundingSphere
    ) -> npt.NDArray[np.float64] | None: ...
    @staticmethod
    def ray_sphere_parametric(ray: Ray, sphere: BoundingSphere) -> float | None: ...
    @staticmethod
    def point_in_triangle_2d(
        point: npt.NDArray[np.floating],
        triangle_vert_a: npt.NDArray[np.floating],
        triangle_vert_b: npt.NDArray[np.floating],
        triangle_vert_c: npt.NDArray[np.floating],
    ) -> bool | npt.NDArray[np.bool_]:
        """Test if point(s) are inside a 2D triangle. Accepts (2,) or (N, 2)."""
        ...
    @staticmethod
    def point_in_triangle_3d(
        point: npt.NDArray[np.floating],
        triangle_vert_a: npt.NDArray[np.floating],
        triangle_vert_b: npt.NDArray[np.floating],
        triangle_vert_c: npt.NDArray[np.floating],
    ) -> bool | npt.NDArray[np.bool_]:
        """Test if point(s) are inside a 3D triangle. Accepts (3,) or (N, 3)."""
        ...
    @staticmethod
    def point_in_triangle_3d_with_barycentric(
        point: npt.NDArray[np.floating],
        triangle_vert_a: npt.NDArray[np.floating],
        triangle_vert_b: npt.NDArray[np.floating],
        triangle_vert_c: npt.NDArray[np.floating],
    ) -> tuple[bool, npt.NDArray[np.float64]]:
        """Returns (inside, barycentric_coords)."""
        ...

class OrientedBoundingBox:
    """An oriented bounding box defined by a center and a half-axes matrix."""

    def __init__(
        self,
        center: npt.NDArray[np.floating],
        half_axes: npt.NDArray[np.floating],
    ) -> None: ...
    @property
    def center(self) -> npt.NDArray[np.float64]:
        """Center of the box as a (3,) array."""
        ...

    @property
    def half_axes(self) -> npt.NDArray[np.float64]:
        """Half-axes as a (3, 3) column matrix: each column is a half-extent vector."""
        ...

    @property
    def inverse_half_axes(self) -> npt.NDArray[np.float64]:
        """Inverse of the half-axes matrix as a (3, 3) array."""
        ...

    @property
    def lengths(self) -> npt.NDArray[np.float64]:
        """Full edge lengths (3,) = 2 * |half-axis| for each axis."""
        ...

    @property
    def corners(self) -> npt.NDArray[np.float64]:
        """The 8 corners of the box as an (8, 3) float64 array."""
        ...

    def intersect_plane(self, plane: Plane) -> CullingResult: ...
    def compute_distance_squared_to_position(
        self, position: npt.NDArray[np.floating]
    ) -> float | npt.NDArray[np.float64]: ...
    def contains(
        self, position: npt.NDArray[np.floating]
    ) -> bool | npt.NDArray[np.bool_]: ...
    def __contains__(self, position: npt.NDArray[np.floating]) -> bool: ...
    def __repr__(self) -> str: ...
    def transform(
        self, transformation: npt.NDArray[np.floating]
    ) -> OrientedBoundingBox:
        """Apply a 4x4 transformation matrix and return a new OBB."""
        ...

    def to_axis_aligned(self) -> AxisAlignedBox: ...
    def to_sphere(self) -> BoundingSphere: ...
    @staticmethod
    def from_axis_aligned(axis_aligned: AxisAlignedBox) -> OrientedBoundingBox: ...
    @staticmethod
    def from_sphere(sphere: BoundingSphere) -> OrientedBoundingBox: ...

class BoundingSphere:
    """A bounding sphere defined by a center point and a radius."""

    def __init__(
        self,
        center: npt.NDArray[np.floating],
        radius: float,
    ) -> None: ...
    @property
    def center(self) -> npt.NDArray[np.float64]: ...
    @property
    def radius(self) -> float: ...
    def intersect_plane(self, plane: Plane) -> CullingResult: ...
    def compute_distance_squared_to_position(
        self, position: npt.NDArray[np.floating]
    ) -> float | npt.NDArray[np.float64]: ...
    def contains(
        self, position: npt.NDArray[np.floating]
    ) -> bool | npt.NDArray[np.bool_]: ...
    def __contains__(self, position: npt.NDArray[np.floating]) -> bool: ...
    def transform(self, transformation: npt.NDArray[np.floating]) -> BoundingSphere:
        """Transform this bounding sphere using a 4x4 matrix."""
        ...
    def __repr__(self) -> str: ...

class BoundingCylinderRegion:
    """A bounding cylinder region with translation, rotation, height, and radial/angular bounds."""

    @overload
    def __init__(
        self,
        translation: npt.NDArray[np.floating],
        rotation: npt.NDArray[np.floating],
        height: float,
        radial_bounds: npt.NDArray[np.floating],
    ) -> None: ...
    @overload
    def __init__(
        self,
        translation: npt.NDArray[np.floating],
        rotation: npt.NDArray[np.floating],
        height: float,
        radial_bounds: npt.NDArray[np.floating],
        angular_bounds: npt.NDArray[np.floating],
    ) -> None: ...
    @property
    def center(self) -> npt.NDArray[np.float64]: ...
    @property
    def rotation(self) -> npt.NDArray[np.float64]: ...
    @property
    def height(self) -> float: ...
    def intersect_plane(self, plane: Plane) -> CullingResult: ...
    def compute_distance_squared_to_position(
        self, position: npt.NDArray[np.floating]
    ) -> float | npt.NDArray[np.float64]: ...
    def contains(
        self, position: npt.NDArray[np.floating]
    ) -> bool | npt.NDArray[np.bool_]: ...
    def __contains__(self, position: npt.NDArray[np.floating]) -> bool: ...
    def __repr__(self) -> str: ...
    def transform(
        self, transformation: npt.NDArray[np.floating]
    ) -> BoundingCylinderRegion: ...
    def to_oriented_bounding_box(self) -> OrientedBoundingBox: ...

class QuadtreeTilingScheme:
    def __init__(
        self, rectangle: Rectangle, root_tiles_x: int, root_tiles_y: int
    ) -> None: ...
    @property
    def rectangle(self) -> Rectangle: ...
    @property
    def root_tiles_x(self) -> int: ...
    @property
    def root_tiles_y(self) -> int: ...
    def get_number_of_x_tiles_at_level(self, level: int) -> int: ...
    def get_number_of_y_tiles_at_level(self, level: int) -> int: ...
    def position_to_tile(
        self,
        position: npt.NDArray[np.floating],
        level: int,
    ) -> QuadtreeTileID: ...
    def tile_to_rectangle(self, tile_id: QuadtreeTileID) -> Rectangle: ...

class QuadtreeTileID:
    def __init__(self, level: int, x: int, y: int) -> None: ...

    level: int
    x: int
    y: int

    @property
    def parent(self) -> QuadtreeTileID: ...
    def __eq__(self, other: object) -> bool: ...
    def __ne__(self, other: object) -> bool: ...
    def __hash__(self) -> int: ...
    def __repr__(self) -> str: ...

class UpsampledQuadtreeNode:
    tile_id: QuadtreeTileID

class OctreeTileID:
    def __init__(self, level: int, x: int, y: int, z: int) -> None: ...

    level: int
    x: int
    y: int
    z: int

    def __eq__(self, other: object) -> bool: ...
    def __ne__(self, other: object) -> bool: ...
    def __hash__(self) -> int: ...
    def __repr__(self) -> str: ...

class Axis(IntEnum):
    X = ...
    Y = ...
    Z = ...

def create_culling_volume(
    position: npt.NDArray[np.floating],
    direction: npt.NDArray[np.floating],
    up: npt.NDArray[np.floating],
    fovx: float,
    fovy: float,
) -> CullingVolume:
    """Build a perspective frustum from camera pose and field of view."""
    ...

def create_culling_volume_from_extents(
    position: npt.NDArray[np.floating],
    direction: npt.NDArray[np.floating],
    up: npt.NDArray[np.floating],
    left: float,
    right: float,
    bottom: float,
    top: float,
    near: float,
) -> CullingVolume: ...
def create_orthographic_culling_volume(
    position: npt.NDArray[np.floating],
    direction: npt.NDArray[np.floating],
    up: npt.NDArray[np.floating],
    left: float,
    right: float,
    bottom: float,
    top: float,
    near: float,
) -> CullingVolume: ...
def create_culling_volume_from_matrix(
    clip_matrix: npt.NDArray[np.floating],
) -> CullingVolume: ...

class ConstantAvailability:
    def __init__(self, constant: bool = False) -> None: ...
    constant: bool

class SubtreeBufferView:
    def __init__(
        self, byte_offset: int = 0, byte_length: int = 0, buffer: int = 0
    ) -> None: ...
    byte_offset: int
    byte_length: int
    buffer: int

class AvailabilitySubtree:
    def __init__(self) -> None: ...
    tile_availability: ConstantAvailability | SubtreeBufferView
    content_availability: ConstantAvailability | SubtreeBufferView
    subtree_availability: ConstantAvailability | SubtreeBufferView
    def get_buffers(self) -> list[bytes]: ...
    def set_buffers(self, buffers: Iterable[bytes]) -> None: ...

class AvailabilityNode: ...
class AvailabilityTree: ...

class AvailabilityAccessor:
    def __init__(self, view: Any, subtree: AvailabilitySubtree) -> None: ...
    def is_buffer_view(self) -> bool: ...
    def is_constant(self) -> bool: ...
    @property
    def constant(self) -> bool: ...
    def size(self) -> int: ...
    def get_buffer_bytes(self) -> bytes: ...

class InterpolatedVertex:
    def __init__(self, first: int = 0, second: int = 0, t: float = 0.0) -> None: ...
    first: int
    second: int
    t: float
    def __eq__(self, other: object) -> bool: ...
    def __ne__(self, other: object) -> bool: ...

class TileAvailabilityFlags:
    TILE_AVAILABLE: int
    CONTENT_AVAILABLE: int
    SUBTREE_AVAILABLE: int
    SUBTREE_LOADED: int
    REACHABLE: int

class QuadtreeTileRectangularRange:
    def __init__(self) -> None: ...
    level: int
    minimum_x: int
    minimum_y: int
    maximum_x: int
    maximum_y: int

class OctreeTilingScheme:
    def __init__(
        self,
        box: AxisAlignedBox,
        root_tiles_x: int,
        root_tiles_y: int,
        root_tiles_z: int,
    ) -> None: ...
    @property
    def box(self) -> AxisAlignedBox: ...
    @property
    def root_tiles_x(self) -> int: ...
    @property
    def root_tiles_y(self) -> int: ...
    @property
    def root_tiles_z(self) -> int: ...
    def get_number_of_x_tiles_at_level(self, level: int) -> int: ...
    def get_number_of_y_tiles_at_level(self, level: int) -> int: ...
    def get_number_of_z_tiles_at_level(self, level: int) -> int: ...
    def position_to_tile(
        self, position: npt.NDArray[np.floating], level: int
    ) -> OctreeTileID: ...
    def tile_to_box(self, tile_id: OctreeTileID) -> AxisAlignedBox: ...

class OctreeAvailability:
    def __init__(self, subtree_levels: int, maximum_level: int) -> None: ...
    @property
    def subtree_levels(self) -> int: ...
    @property
    def maximum_level(self) -> int: ...
    def compute_availability(self, tile_id: OctreeTileID) -> TileAvailabilityFlags: ...

class QuadtreeAvailability:
    def __init__(self, subtree_levels: int, maximum_level: int) -> None: ...
    @property
    def subtree_levels(self) -> int: ...
    @property
    def maximum_level(self) -> int: ...
    def compute_availability(
        self, tile_id: QuadtreeTileID
    ) -> TileAvailabilityFlags: ...

class QuadtreeRectangleAvailability:
    def __init__(
        self, tiling_scheme: QuadtreeTilingScheme, maximum_level: int
    ) -> None: ...
    def add_available_tile_range(self, range: QuadtreeTileRectangularRange) -> None: ...
    def compute_maximum_level_at_position(
        self, position: npt.NDArray[np.floating]
    ) -> int | npt.NDArray[np.int32]:
        """Get max available level. Accepts (2,) or (N, 2)."""
        ...
    def is_tile_available(self, id: QuadtreeTileID) -> bool: ...

def count_ones_in_byte(value: int) -> int: ...
def count_ones_in_buffer(buffer: bytes) -> int: ...
def transform_points(
    points: npt.NDArray[np.floating],
    matrix: npt.NDArray[np.floating],
) -> npt.NDArray[np.float64]:
    """Transform (N, 3) points by a 4x4 matrix (GIL-free). Returns (N, 3)."""
    ...

def clip_triangle_at_axis_aligned_threshold(
    threshold: float,
    keep_above: bool,
    i0: int,
    i1: int,
    i2: int,
    u0: float,
    u1: float,
    u2: float,
) -> list[int | InterpolatedVertex]: ...
