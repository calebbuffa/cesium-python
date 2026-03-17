from __future__ import annotations

from collections.abc import Callable
from typing import Optional

from ..async_ import AsyncSystem, FutureVoid, IAssetAccessor, NetworkAssetDescriptor
from . import ImageAsset, Ktx2TranscodeTargets, Model

class GltfReaderOptions:
    def __init__(self) -> None: ...
    decode_data_urls: bool
    clear_decoded_data_urls: bool
    decode_embedded_images: bool
    resolve_external_images: bool
    decode_draco: bool
    decode_mesh_opt_data: bool
    decode_spz: bool
    dequantize_mesh_data: bool
    apply_texture_transform: bool
    resolve_external_structural_metadata: bool
    ktx2_transcode_targets: Ktx2TranscodeTargets

class GltfReaderResult:
    def __init__(self) -> None: ...
    @property
    def model(self) -> Optional[Model]: ...
    errors: list[str]
    warnings: list[str]
    @property
    def has_model(self) -> bool: ...

class FutureGltfReaderResult:
    def wait(self) -> GltfReaderResult: ...
    def wait_in_main_thread(self) -> GltfReaderResult: ...
    def is_ready(self) -> bool: ...
    def then_in_worker_thread(
        self, callback: Callable[[GltfReaderResult], None]
    ) -> FutureVoid: ...
    def then_in_main_thread(
        self, callback: Callable[[GltfReaderResult], None]
    ) -> FutureVoid: ...
    def catch_in_main_thread(
        self, callback: Callable[[str], GltfReaderResult]
    ) -> FutureGltfReaderResult: ...
    def __await__(self) -> GltfReaderResult: ...

class GltfReader:
    def __init__(self) -> None: ...
    @property
    def options(self) -> GltfReaderOptions: ...
    @property
    def extensions(self) -> object: ...
    def read_gltf(
        self, data: bytes, options: GltfReaderOptions = ...
    ) -> GltfReaderResult: ...
    def read_gltf_summary(
        self, data: bytes, options: GltfReaderOptions = ...
    ) -> dict: ...
    def load_gltf(
        self,
        async_system: AsyncSystem,
        url: str,
        asset_accessor: IAssetAccessor,
        headers: dict[str, str] = ...,
        options: GltfReaderOptions = ...,
    ) -> FutureGltfReaderResult: ...
    def postprocess_gltf(
        self, result: GltfReaderResult, options: GltfReaderOptions = ...
    ) -> None: ...
    def read_gltf_and_external_data(
        self,
        data: bytes,
        async_system: AsyncSystem,
        asset_accessor: IAssetAccessor,
        headers: dict[str, str] = ...,
        base_url: str = ...,
        options: GltfReaderOptions = ...,
    ) -> FutureGltfReaderResult: ...

class GltfSharedAssetSystem:
    @staticmethod
    def default_system() -> GltfSharedAssetSystem: ...

class ImageReaderResult:
    @property
    def image(self) -> ImageAsset | None: ...
    errors: list[str]
    warnings: list[str]

class ImageDecoder:
    @staticmethod
    def read_image(
        data: bytes,
        ktx2_transcode_targets: Ktx2TranscodeTargets | None = None,
    ) -> ImageReaderResult: ...
    @staticmethod
    def generate_mip_maps(image: ImageAsset) -> str | None: ...

class NetworkImageAssetDescriptor(NetworkAssetDescriptor):
    def __init__(self) -> None: ...
    def __eq__(self, other: object) -> bool: ...

class NetworkSchemaAssetDescriptor(NetworkAssetDescriptor):
    def __init__(self) -> None: ...
    def __eq__(self, other: object) -> bool: ...
