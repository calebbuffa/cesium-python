from __future__ import annotations

from typing import overload

import numpy as np
import numpy.typing as npt

from ..geometry import Ray
from ..geospatial import BoundingRegion, Ellipsoid
from ..utility import ErrorList
from . import Buffer, ImageAsset, Model, Node

class GltfContent:
    model: Model

class GltfContentLoaderResult:
    content: GltfContent | None
    errors: ErrorList

class PixelRectangle:
    x: int
    y: int
    width: int
    height: int
    def __init__(self) -> None: ...

class ImageManipulation:
    @staticmethod
    def blit_image(
        target: ImageAsset,
        target_pixels: PixelRectangle,
        source: ImageAsset,
        source_pixels: PixelRectangle,
    ) -> None: ...
    @staticmethod
    def save_png(image: ImageAsset) -> bytes: ...

class RayGltfHit:
    """Data describing a hit from a ray / glTF intersection test."""
    @property
    def primitive_point(self) -> npt.NDArray[np.float64]: ...
    @property
    def primitive_to_world(self) -> npt.NDArray[np.float64]: ...
    @property
    def world_point(self) -> npt.NDArray[np.float64]: ...
    ray_to_world_point_distance_sq: float
    mesh_id: int
    primitive_id: int
    def __repr__(self) -> str: ...

class IntersectResult:
    """Hit result data for intersect_ray_gltf_model."""
    @property
    def hit(self) -> RayGltfHit | None: ...
    warnings: list[str]
    def __bool__(self) -> bool: ...
    def __repr__(self) -> str: ...

class GltfUtilities:
    @staticmethod
    def collapse_to_single_buffer(gltf: Model) -> None: ...
    @staticmethod
    def move_buffer_content(
        gltf: Model, destination: Buffer, source: Buffer
    ) -> None: ...
    @staticmethod
    def remove_unused_textures(
        gltf: Model, extra_used_texture_indices: list[int] | None = None
    ) -> None: ...
    @staticmethod
    def remove_unused_samplers(
        gltf: Model, extra_used_sampler_indices: list[int] | None = None
    ) -> None: ...
    @staticmethod
    def remove_unused_images(
        gltf: Model, extra_used_image_indices: list[int] | None = None
    ) -> None: ...
    @staticmethod
    def remove_unused_accessors(
        gltf: Model, extra_used_accessor_indices: list[int] | None = None
    ) -> None: ...
    @staticmethod
    def remove_unused_buffer_views(
        gltf: Model, extra_used_buffer_view_indices: list[int] | None = None
    ) -> None: ...
    @staticmethod
    def remove_unused_buffers(
        gltf: Model, extra_used_buffer_indices: list[int] | None = None
    ) -> None: ...
    @staticmethod
    def remove_unused_meshes(
        gltf: Model, extra_used_mesh_indices: list[int] | None = None
    ) -> None: ...
    @staticmethod
    def remove_unused_materials(
        gltf: Model, extra_used_material_indices: list[int] | None = None
    ) -> None: ...
    @staticmethod
    def compact_buffers(gltf: Model) -> None: ...
    @staticmethod
    def compact_buffer(gltf: Model, buffer_index: int) -> None: ...
    @staticmethod
    def get_node_transform(node: Node) -> npt.NDArray[np.float64] | None:
        """Get the local transform matrix for a node, or None if invalid."""
        ...
    @staticmethod
    def set_node_transform(node: Node, new_transform: npt.NDArray[np.floating]) -> None:
        """Set the local transform matrix for a node."""
        ...
    @staticmethod
    def apply_rtc_center(
        gltf: Model, root_transform: npt.NDArray[np.floating]
    ) -> npt.NDArray[np.float64]:
        """Apply the glTF's RTC_CENTER to the given transform."""
        ...
    @staticmethod
    def apply_gltf_up_axis_transform(
        model: Model, root_transform: npt.NDArray[np.floating]
    ) -> npt.NDArray[np.float64]:
        """Apply the glTF's gltfUpAxis transform."""
        ...
    @staticmethod
    def compute_bounding_region(
        gltf: Model,
        transform: npt.NDArray[np.floating],
        ellipsoid: Ellipsoid = ...,
    ) -> BoundingRegion:
        """Compute a bounding region from the vertex positions in a glTF model."""
        ...
    @staticmethod
    def parse_gltf_copyright(gltf: Model) -> list[str]:
        """Parse semicolon-separated copyright strings from a glTF model."""
        ...
    @overload
    @staticmethod
    def intersect_ray_gltf_model(
        ray: Ray,
        gltf: Model,
        cull_back_faces: bool = True,
        gltf_transform: npt.NDArray[np.floating] | None = None,
    ) -> IntersectResult:
        """Intersect a ray with a glTF model and return the first intersection."""
        ...
    @overload
    @staticmethod
    def intersect_ray_gltf_model(
        origins: npt.NDArray[np.floating],
        directions: npt.NDArray[np.floating],
        gltf: Model,
        cull_back_faces: bool = True,
        gltf_transform: npt.NDArray[np.floating] | None = None,
    ) -> list[IntersectResult]:
        """Vectorized: (N,3) origins, (N,3) directions -> list[IntersectResult]. GIL-free."""
        ...

class SkirtMeshMetadata:
    no_skirt_indices_begin: int
    no_skirt_indices_count: int
    no_skirt_vertices_begin: int
    no_skirt_vertices_count: int
    mesh_center: npt.NDArray[np.float64]
    skirt_west_height: float
    skirt_south_height: float
    skirt_east_height: float
    skirt_north_height: float

    def __init__(self) -> None: ...
    @staticmethod
    def parse_from_gltf_extras(extras: dict) -> SkirtMeshMetadata | None:
        """Parse SkirtMeshMetadata from a glTF mesh extras dict."""
        ...
    @staticmethod
    def create_gltf_extras(skirt: SkirtMeshMetadata) -> dict:
        """Create a glTF extras dict from SkirtMeshMetadata."""
        ...
