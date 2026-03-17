#include "JsonConversions.h"
#include "NumpyConversions.h"
#include "PropertyTableConversions.h"

#include <CesiumGltf/Accessor.h>
#include <CesiumGltf/AccessorSparse.h>
#include <CesiumGltf/AccessorSparseIndices.h>
#include <CesiumGltf/AccessorSparseValues.h>
#include <CesiumGltf/AccessorUtility.h>
#include <CesiumGltf/AccessorView.h>
#include <CesiumGltf/AccessorWriter.h>
#include <CesiumGltf/Animation.h>
#include <CesiumGltf/AnimationChannel.h>
#include <CesiumGltf/AnimationChannelTarget.h>
#include <CesiumGltf/AnimationSampler.h>
#include <CesiumGltf/Asset.h>
#include <CesiumGltf/Buffer.h>
#include <CesiumGltf/BufferView.h>
#include <CesiumGltf/Camera.h>
#include <CesiumGltf/CameraOrthographic.h>
#include <CesiumGltf/CameraPerspective.h>
#include <CesiumGltf/Class.h>
#include <CesiumGltf/ClassProperty.h>
#include <CesiumGltf/Enum.h>
#include <CesiumGltf/EnumValue.h>
#include <CesiumGltf/ExtensionExtInstanceFeatures.h>
#include <CesiumGltf/ExtensionExtInstanceFeaturesFeatureId.h>
#include <CesiumGltf/ExtensionExtMeshFeatures.h>
#include <CesiumGltf/ExtensionExtMeshGpuInstancing.h>
#include <CesiumGltf/ExtensionMeshPrimitiveExtStructuralMetadata.h>
#include <CesiumGltf/ExtensionModelExtStructuralMetadata.h>
#include <CesiumGltf/ExtensionCesiumRTC.h>
#include <CesiumGltf/ExtensionKhrGaussianSplatting.h>
#include <CesiumGltf/ExtensionKhrGaussianSplattingCompressionSpz2.h>
#include <CesiumGltf/ExtensionKhrGaussianSplattingHintsValue.h>
#include <CesiumGltf/ExtensionKhrMaterialsUnlit.h>
#include <CesiumGltf/FeatureId.h>
#include <CesiumGltf/FeatureIdTexture.h>
#include <CesiumGltf/Image.h>
#include <CesiumGltf/ImageAsset.h>
#include <CesiumGltf/InstanceAttributeSemantics.h>
#include <CesiumGltf/KhrTextureTransform.h>
#include <CesiumGltf/Ktx2TranscodeTargets.h>
#include <CesiumGltf/Material.h>
#include <CesiumGltf/MaterialNormalTextureInfo.h>
#include <CesiumGltf/MaterialOcclusionTextureInfo.h>
#include <CesiumGltf/MaterialPBRMetallicRoughness.h>
#include <CesiumGltf/Mesh.h>
#include <CesiumGltf/MeshPrimitive.h>
#include <CesiumGltf/Model.h>
#include <CesiumGltf/Node.h>
#include <CesiumGltf/PropertyAttribute.h>
#include <CesiumGltf/PropertyAttributeProperty.h>
#include <CesiumGltf/PropertyTable.h>
#include <CesiumGltf/PropertyTableProperty.h>
#include <CesiumGltf/PropertyTexture.h>
#include <CesiumGltf/PropertyTextureProperty.h>
#include <CesiumGltf/Sampler.h>
#include <CesiumGltf/Scene.h>
#include <CesiumGltf/Schema.h>
#include <CesiumGltf/Skin.h>
#include <CesiumGltf/Texture.h>
#include <CesiumGltf/TextureInfo.h>
#include <CesiumGltf/TextureView.h>
#include <CesiumGltf/VertexAttributeSemantics.h>
#include <CesiumUtility/ExtensibleObject.h>

#include <pybind11/numpy.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace py = pybind11;

// ── AccessorView helpers (namespace scope for MSVC compat) ──────────────

// Number of scalar components for a glTF accessor element type.
template <typename T>
struct NumComponents {
  static constexpr py::ssize_t value = [] {
    if constexpr (std::is_arithmetic_v<T>)
      return 1;
    else if constexpr (
        std::is_same_v<T, glm::vec2> || std::is_same_v<T, glm::dvec2>)
      return 2;
    else if constexpr (
        std::is_same_v<T, glm::vec3> || std::is_same_v<T, glm::dvec3>)
      return 3;
    else if constexpr (
        std::is_same_v<T, glm::vec4> || std::is_same_v<T, glm::dvec4>)
      return 4;
    else if constexpr (std::is_same_v<T, glm::mat4>)
      return 16;
    else
      return 0;
  }();
};

// Scalar component type: T itself for arithmetic, T::value_type for glm.
template <typename T, typename = void>
struct ScalarOfT {
  using type = T;
};
template <typename T>
struct ScalarOfT<T, std::void_t<typename T::value_type>> {
  using type = typename T::value_type;
};

void initGltfBindings(py::module& m) {
  // ---- Enums & GPU formats ------------------------------------------------
  py::enum_<CesiumGltf::GpuCompressedPixelFormat>(m, "GpuCompressedPixelFormat")
      .value("NONE", CesiumGltf::GpuCompressedPixelFormat::NONE)
      .value("ETC1_RGB", CesiumGltf::GpuCompressedPixelFormat::ETC1_RGB)
      .value("ETC2_RGBA", CesiumGltf::GpuCompressedPixelFormat::ETC2_RGBA)
      .value("BC1_RGB", CesiumGltf::GpuCompressedPixelFormat::BC1_RGB)
      .value("BC3_RGBA", CesiumGltf::GpuCompressedPixelFormat::BC3_RGBA)
      .value("BC4_R", CesiumGltf::GpuCompressedPixelFormat::BC4_R)
      .value("BC5_RG", CesiumGltf::GpuCompressedPixelFormat::BC5_RG)
      .value("BC7_RGBA", CesiumGltf::GpuCompressedPixelFormat::BC7_RGBA)
      .value("PVRTC1_4_RGB", CesiumGltf::GpuCompressedPixelFormat::PVRTC1_4_RGB)
      .value(
          "PVRTC1_4_RGBA",
          CesiumGltf::GpuCompressedPixelFormat::PVRTC1_4_RGBA)
      .value(
          "ASTC_4x4_RGBA",
          CesiumGltf::GpuCompressedPixelFormat::ASTC_4x4_RGBA)
      .value("PVRTC2_4_RGB", CesiumGltf::GpuCompressedPixelFormat::PVRTC2_4_RGB)
      .value(
          "PVRTC2_4_RGBA",
          CesiumGltf::GpuCompressedPixelFormat::PVRTC2_4_RGBA)
      .value("ETC2_EAC_R11", CesiumGltf::GpuCompressedPixelFormat::ETC2_EAC_R11)
      .value(
          "ETC2_EAC_RG11",
          CesiumGltf::GpuCompressedPixelFormat::ETC2_EAC_RG11)
      .export_values();

  py::class_<CesiumGltf::SupportedGpuCompressedPixelFormats>(
      m,
      "SupportedGpuCompressedPixelFormats")
      .def(py::init<>())
      .def_readwrite(
          "ETC1_RGB",
          &CesiumGltf::SupportedGpuCompressedPixelFormats::ETC1_RGB)
      .def_readwrite(
          "ETC2_RGBA",
          &CesiumGltf::SupportedGpuCompressedPixelFormats::ETC2_RGBA)
      .def_readwrite(
          "BC1_RGB",
          &CesiumGltf::SupportedGpuCompressedPixelFormats::BC1_RGB)
      .def_readwrite(
          "BC3_RGBA",
          &CesiumGltf::SupportedGpuCompressedPixelFormats::BC3_RGBA)
      .def_readwrite(
          "BC4_R",
          &CesiumGltf::SupportedGpuCompressedPixelFormats::BC4_R)
      .def_readwrite(
          "BC5_RG",
          &CesiumGltf::SupportedGpuCompressedPixelFormats::BC5_RG)
      .def_readwrite(
          "BC7_RGBA",
          &CesiumGltf::SupportedGpuCompressedPixelFormats::BC7_RGBA)
      .def_readwrite(
          "PVRTC1_4_RGB",
          &CesiumGltf::SupportedGpuCompressedPixelFormats::PVRTC1_4_RGB)
      .def_readwrite(
          "PVRTC1_4_RGBA",
          &CesiumGltf::SupportedGpuCompressedPixelFormats::PVRTC1_4_RGBA)
      .def_readwrite(
          "ASTC_4x4_RGBA",
          &CesiumGltf::SupportedGpuCompressedPixelFormats::ASTC_4x4_RGBA)
      .def_readwrite(
          "PVRTC2_4_RGB",
          &CesiumGltf::SupportedGpuCompressedPixelFormats::PVRTC2_4_RGB)
      .def_readwrite(
          "PVRTC2_4_RGBA",
          &CesiumGltf::SupportedGpuCompressedPixelFormats::PVRTC2_4_RGBA)
      .def_readwrite(
          "ETC2_EAC_R11",
          &CesiumGltf::SupportedGpuCompressedPixelFormats::ETC2_EAC_R11)
      .def_readwrite(
          "ETC2_EAC_RG11",
          &CesiumGltf::SupportedGpuCompressedPixelFormats::ETC2_EAC_RG11);

  py::class_<CesiumGltf::Ktx2TranscodeTargets>(m, "Ktx2TranscodeTargets")
      .def(py::init<>())
      .def(
          py::init<
              const CesiumGltf::SupportedGpuCompressedPixelFormats&,
              bool>(),
          py::arg("supported_formats"),
          py::arg("preserve_high_quality"))
      .def_readwrite("ETC1S_R", &CesiumGltf::Ktx2TranscodeTargets::ETC1S_R)
      .def_readwrite("ETC1S_RG", &CesiumGltf::Ktx2TranscodeTargets::ETC1S_RG)
      .def_readwrite("ETC1S_RGB", &CesiumGltf::Ktx2TranscodeTargets::ETC1S_RGB)
      .def_readwrite(
          "ETC1S_RGBA",
          &CesiumGltf::Ktx2TranscodeTargets::ETC1S_RGBA)
      .def_readwrite("UASTC_R", &CesiumGltf::Ktx2TranscodeTargets::UASTC_R)
      .def_readwrite("UASTC_RG", &CesiumGltf::Ktx2TranscodeTargets::UASTC_RG)
      .def_readwrite("UASTC_RGB", &CesiumGltf::Ktx2TranscodeTargets::UASTC_RGB)
      .def_readwrite(
          "UASTC_RGBA",
          &CesiumGltf::Ktx2TranscodeTargets::UASTC_RGBA);

  py::class_<CesiumGltf::ImageAssetMipPosition>(m, "ImageAssetMipPosition")
      .def(py::init<>())
      .def_readwrite(
          "byte_offset",
          &CesiumGltf::ImageAssetMipPosition::byteOffset)
      .def_readwrite("byte_size", &CesiumGltf::ImageAssetMipPosition::byteSize);

  py::class_<
      CesiumGltf::ImageAsset,
      CesiumUtility::IntrusivePointer<CesiumGltf::ImageAsset>>(m, "ImageAsset")
      .def(py::init<>())
      .def_readwrite("width", &CesiumGltf::ImageAsset::width)
      .def_readwrite("height", &CesiumGltf::ImageAsset::height)
      .def_readwrite("channels", &CesiumGltf::ImageAsset::channels)
      .def_readwrite(
          "bytes_per_channel",
          &CesiumGltf::ImageAsset::bytesPerChannel)
      .def_readwrite(
          "compressed_pixel_format",
          &CesiumGltf::ImageAsset::compressedPixelFormat)
      .def_readwrite("mip_positions", &CesiumGltf::ImageAsset::mipPositions)
      .def_property(
          "pixel_data",
          [](CesiumGltf::ImageAsset& self) {
            return CesiumPython::bytesVectorToNumpy(
                self.pixelData,
                py::cast(self));
          },
          [](CesiumGltf::ImageAsset& self, py::array_t<uint8_t> value) {
            auto buf = value.request();
            auto* ptr = static_cast<uint8_t*>(buf.ptr);
            self.pixelData.resize(buf.size);
            std::memcpy(self.pixelData.data(), ptr, buf.size);
          })
      .def_readwrite("size_bytes", &CesiumGltf::ImageAsset::sizeBytes)
      .def(
          "change_number_of_channels",
          [](CesiumGltf::ImageAsset& self,
             int32_t newChannels,
             uint32_t defaultValue) {
            self.changeNumberOfChannels(
                newChannels,
                static_cast<std::byte>(
                    static_cast<uint8_t>(defaultValue & 0xFFu)));
          },
          py::arg("new_channels"),
          py::arg("default_value") = 0u)
      .def_property_readonly(
          "computed_size_bytes",
          &CesiumGltf::ImageAsset::getSizeBytes)
      .def("__repr__", [](const CesiumGltf::ImageAsset& self) {
        return "ImageAsset(" + std::to_string(self.width) + "x" +
               std::to_string(self.height) +
               ", channels=" + std::to_string(self.channels) + ")";
      });

  py::class_<CesiumGltf::BufferCesium>(m, "BufferCesium")
      .def(py::init<>())
      .def_property(
          "data",
          [](CesiumGltf::BufferCesium& self) {
            return CesiumPython::bytesVectorToNumpy(self.data, py::cast(self));
          },
          [](CesiumGltf::BufferCesium& self, py::array_t<uint8_t> value) {
            auto buf = value.request();
            auto* ptr = static_cast<uint8_t*>(buf.ptr);
            self.data.resize(buf.size);
            std::memcpy(self.data.data(), ptr, buf.size);
          });

  py::class_<CesiumGltf::Buffer>(m, "Buffer")
      .def(py::init<>())
      .def_readwrite("name", &CesiumGltf::Buffer::name)
      .def_readwrite("uri", &CesiumGltf::Buffer::uri)
      .def_readwrite("byte_length", &CesiumGltf::Buffer::byteLength)
      .def_readwrite("cesium", &CesiumGltf::Buffer::cesium)
      .def_property_readonly("size_bytes", &CesiumGltf::Buffer::getSizeBytes)
      .def("__repr__", [](const CesiumGltf::Buffer& self) {
        return "Buffer(name='" + self.name +
               "', byte_length=" + std::to_string(self.byteLength) + ")";
      });

  py::class_<CesiumGltf::BufferView> bufferViewCls(m, "BufferView");
  bufferViewCls.def(py::init<>())
      .def_readwrite("name", &CesiumGltf::BufferView::name)
      .def_readwrite("buffer", &CesiumGltf::BufferView::buffer)
      .def_readwrite("byte_offset", &CesiumGltf::BufferView::byteOffset)
      .def_readwrite("byte_length", &CesiumGltf::BufferView::byteLength)
      .def_readwrite("byte_stride", &CesiumGltf::BufferView::byteStride)
      .def_readwrite("target", &CesiumGltf::BufferView::target)
      .def("__repr__", [](const CesiumGltf::BufferView& self) {
        return "BufferView(name='" + self.name +
               "', buffer=" + std::to_string(self.buffer) +
               ", byte_length=" + std::to_string(self.byteLength) + ")";
      });

  py::class_<CesiumGltf::BufferView::Target>(bufferViewCls, "Target")
      .def_property_readonly_static(
          "ARRAY_BUFFER",
          [](py::object) {
            return CesiumGltf::BufferView::Target::ARRAY_BUFFER;
          })
      .def_property_readonly_static("ELEMENT_ARRAY_BUFFER", [](py::object) {
        return CesiumGltf::BufferView::Target::ELEMENT_ARRAY_BUFFER;
      });

  py::class_<CesiumGltf::AccessorSparseValues>(m, "AccessorSparseValues")
      .def(py::init<>())
      .def_readwrite(
          "buffer_view",
          &CesiumGltf::AccessorSparseValues::bufferView)
      .def_readwrite(
          "byte_offset",
          &CesiumGltf::AccessorSparseValues::byteOffset);

  py::class_<CesiumGltf::AccessorSparseIndices> sparseIndicesCls(
      m,
      "AccessorSparseIndices");
  sparseIndicesCls.def(py::init<>())
      .def_readwrite(
          "buffer_view",
          &CesiumGltf::AccessorSparseIndices::bufferView)
      .def_readwrite(
          "byte_offset",
          &CesiumGltf::AccessorSparseIndices::byteOffset)
      .def_readwrite(
          "component_type",
          &CesiumGltf::AccessorSparseIndices::componentType);

  py::class_<CesiumGltf::AccessorSparseIndices::ComponentType>(
      sparseIndicesCls,
      "ComponentType")
      .def_property_readonly_static(
          "UNSIGNED_BYTE",
          [](py::object) {
            return CesiumGltf::AccessorSparseIndices::ComponentType::
                UNSIGNED_BYTE;
          })
      .def_property_readonly_static(
          "UNSIGNED_SHORT",
          [](py::object) {
            return CesiumGltf::AccessorSparseIndices::ComponentType::
                UNSIGNED_SHORT;
          })
      .def_property_readonly_static("UNSIGNED_INT", [](py::object) {
        return CesiumGltf::AccessorSparseIndices::ComponentType::UNSIGNED_INT;
      });

  py::class_<CesiumGltf::AccessorSparse>(m, "AccessorSparse")
      .def(py::init<>())
      .def_readwrite("count", &CesiumGltf::AccessorSparse::count)
      .def_readwrite("indices", &CesiumGltf::AccessorSparse::indices)
      .def_readwrite("values", &CesiumGltf::AccessorSparse::values);

  py::class_<CesiumGltf::Accessor> accessorCls(m, "Accessor");
  accessorCls.def(py::init<>())
      .def_readwrite("name", &CesiumGltf::Accessor::name)
      .def_readwrite("buffer_view", &CesiumGltf::Accessor::bufferView)
      .def_readwrite("byte_offset", &CesiumGltf::Accessor::byteOffset)
      .def_readwrite("component_type", &CesiumGltf::Accessor::componentType)
      .def_readwrite("normalized", &CesiumGltf::Accessor::normalized)
      .def_readwrite("count", &CesiumGltf::Accessor::count)
      .def_readwrite("type", &CesiumGltf::Accessor::type)
      .def_readwrite("max", &CesiumGltf::Accessor::max)
      .def_readwrite("min", &CesiumGltf::Accessor::min)
      .def_readwrite("sparse", &CesiumGltf::Accessor::sparse)
      .def(
          "compute_number_of_components",
          py::overload_cast<>(
              &CesiumGltf::Accessor::computeNumberOfComponents,
              py::const_))
      .def(
          "compute_byte_size_of_component",
          py::overload_cast<>(
              &CesiumGltf::Accessor::computeByteSizeOfComponent,
              py::const_))
      .def(
          "compute_bytes_per_vertex",
          &CesiumGltf::Accessor::computeBytesPerVertex)
      .def(
          "compute_byte_stride",
          &CesiumGltf::Accessor::computeByteStride,
          py::arg("model"))
      .def("__repr__", [](const CesiumGltf::Accessor& self) {
        return "Accessor(name='" + self.name +
               "', count=" + std::to_string(self.count) +
               ", type='" + self.type + "')";
      });

  py::class_<CesiumGltf::Accessor::ComponentType>(accessorCls, "ComponentType")
      .def_property_readonly_static(
          "BYTE",
          [](py::object) { return CesiumGltf::Accessor::ComponentType::BYTE; })
      .def_property_readonly_static(
          "UNSIGNED_BYTE",
          [](py::object) {
            return CesiumGltf::Accessor::ComponentType::UNSIGNED_BYTE;
          })
      .def_property_readonly_static(
          "SHORT",
          [](py::object) { return CesiumGltf::Accessor::ComponentType::SHORT; })
      .def_property_readonly_static(
          "UNSIGNED_SHORT",
          [](py::object) {
            return CesiumGltf::Accessor::ComponentType::UNSIGNED_SHORT;
          })
      .def_property_readonly_static(
          "UNSIGNED_INT",
          [](py::object) {
            return CesiumGltf::Accessor::ComponentType::UNSIGNED_INT;
          })
      .def_property_readonly_static("FLOAT", [](py::object) {
        return CesiumGltf::Accessor::ComponentType::FLOAT;
      })
      .def_property_readonly_static(
          "INT",
          [](py::object) {
            return CesiumGltf::Accessor::ComponentType::INT;
          })
      .def_property_readonly_static(
          "INT64",
          [](py::object) {
            return CesiumGltf::Accessor::ComponentType::INT64;
          })
      .def_property_readonly_static(
          "UNSIGNED_INT64",
          [](py::object) {
            return CesiumGltf::Accessor::ComponentType::UNSIGNED_INT64;
          })
      .def_property_readonly_static("DOUBLE", [](py::object) {
        return CesiumGltf::Accessor::ComponentType::DOUBLE;
      });

  py::class_<CesiumGltf::Accessor::Type>(accessorCls, "Type")
      .def_property_readonly_static(
          "SCALAR",
          [](py::object) { return CesiumGltf::Accessor::Type::SCALAR; })
      .def_property_readonly_static(
          "VEC2",
          [](py::object) { return CesiumGltf::Accessor::Type::VEC2; })
      .def_property_readonly_static(
          "VEC3",
          [](py::object) { return CesiumGltf::Accessor::Type::VEC3; })
      .def_property_readonly_static(
          "VEC4",
          [](py::object) { return CesiumGltf::Accessor::Type::VEC4; })
      .def_property_readonly_static(
          "MAT2",
          [](py::object) { return CesiumGltf::Accessor::Type::MAT2; })
      .def_property_readonly_static(
          "MAT3",
          [](py::object) { return CesiumGltf::Accessor::Type::MAT3; })
      .def_property_readonly_static("MAT4", [](py::object) {
        return CesiumGltf::Accessor::Type::MAT4;
      });

  py::class_<CesiumGltf::Asset>(m, "Asset")
      .def(py::init<>())
      .def_readwrite("copyright", &CesiumGltf::Asset::copyright)
      .def_readwrite("generator", &CesiumGltf::Asset::generator)
      .def_readwrite("version", &CesiumGltf::Asset::version)
      .def_readwrite("min_version", &CesiumGltf::Asset::minVersion)
      .def("__repr__", [](const CesiumGltf::Asset& self) {
        return "Asset(version='" + self.version +
               "', generator='" + self.generator.value_or("") + "')";
      });

  py::class_<CesiumGltf::TextureInfo>(m, "TextureInfo")
      .def(py::init<>())
      .def_readwrite("index", &CesiumGltf::TextureInfo::index)
      .def_readwrite("tex_coord", &CesiumGltf::TextureInfo::texCoord);

  py::class_<CesiumGltf::MaterialNormalTextureInfo>(
      m,
      "MaterialNormalTextureInfo")
      .def(py::init<>())
      .def_readwrite("index", &CesiumGltf::MaterialNormalTextureInfo::index)
      .def_readwrite(
          "tex_coord",
          &CesiumGltf::MaterialNormalTextureInfo::texCoord)
      .def_readwrite("scale", &CesiumGltf::MaterialNormalTextureInfo::scale);

  py::class_<CesiumGltf::MaterialOcclusionTextureInfo>(
      m,
      "MaterialOcclusionTextureInfo")
      .def(py::init<>())
      .def_readwrite("index", &CesiumGltf::MaterialOcclusionTextureInfo::index)
      .def_readwrite(
          "tex_coord",
          &CesiumGltf::MaterialOcclusionTextureInfo::texCoord)
      .def_readwrite(
          "strength",
          &CesiumGltf::MaterialOcclusionTextureInfo::strength);

  py::class_<CesiumGltf::MaterialPBRMetallicRoughness>(
      m,
      "MaterialPBRMetallicRoughness")
      .def(py::init<>())
      .def_readwrite(
          "base_color_factor",
          &CesiumGltf::MaterialPBRMetallicRoughness::baseColorFactor)
      .def_readwrite(
          "base_color_texture",
          &CesiumGltf::MaterialPBRMetallicRoughness::baseColorTexture)
      .def_readwrite(
          "metallic_factor",
          &CesiumGltf::MaterialPBRMetallicRoughness::metallicFactor)
      .def_readwrite(
          "roughness_factor",
          &CesiumGltf::MaterialPBRMetallicRoughness::roughnessFactor)
      .def_readwrite(
          "metallic_roughness_texture",
          &CesiumGltf::MaterialPBRMetallicRoughness::metallicRoughnessTexture);

  py::class_<CesiumGltf::Material> materialCls(m, "Material");
  materialCls.def(py::init<>())
      .def_readwrite("name", &CesiumGltf::Material::name)
      .def_readwrite(
          "pbr_metallic_roughness",
          &CesiumGltf::Material::pbrMetallicRoughness)
      .def_readwrite("normal_texture", &CesiumGltf::Material::normalTexture)
      .def_readwrite(
          "occlusion_texture",
          &CesiumGltf::Material::occlusionTexture)
      .def_readwrite("emissive_texture", &CesiumGltf::Material::emissiveTexture)
      .def_readwrite("emissive_factor", &CesiumGltf::Material::emissiveFactor)
      .def_readwrite("alpha_mode", &CesiumGltf::Material::alphaMode)
      .def_readwrite("alpha_cutoff", &CesiumGltf::Material::alphaCutoff)
      .def_readwrite("double_sided", &CesiumGltf::Material::doubleSided)
      .def_property_readonly(
          "ext_materials_unlit",
          [](CesiumGltf::Material& self) -> py::object {
            auto* ext =
                self.getExtension<CesiumGltf::ExtensionKhrMaterialsUnlit>();
            if (!ext)
              return py::none();
            return py::cast(*ext, py::return_value_policy::copy);
          })
      .def("__repr__", [](const CesiumGltf::Material& self) {
        return "Material(name='" + self.name + "', double_sided=" +
               std::string(self.doubleSided ? "True" : "False") + ")";
      });

  py::class_<CesiumGltf::Material::AlphaMode>(materialCls, "AlphaMode")
      .def_property_readonly_static(
          "OPAQUE",
          [](py::object) { return CesiumGltf::Material::AlphaMode::OPAQUE; })
      .def_property_readonly_static(
          "MASK",
          [](py::object) { return CesiumGltf::Material::AlphaMode::MASK; })
      .def_property_readonly_static("BLEND", [](py::object) {
        return CesiumGltf::Material::AlphaMode::BLEND;
      });

  py::class_<CesiumGltf::MeshPrimitive> meshPrimCls(m, "MeshPrimitive");
  meshPrimCls.def(py::init<>())
      .def_readwrite("attributes", &CesiumGltf::MeshPrimitive::attributes)
      .def_readwrite("indices", &CesiumGltf::MeshPrimitive::indices)
      .def_readwrite("material", &CesiumGltf::MeshPrimitive::material)
      .def_readwrite("mode", &CesiumGltf::MeshPrimitive::mode)
      .def_readwrite("targets", &CesiumGltf::MeshPrimitive::targets)
      .def_property_readonly(
          "ext_mesh_features",
          [](CesiumGltf::MeshPrimitive& self) -> py::object {
            auto* ext =
                self.getExtension<CesiumGltf::ExtensionExtMeshFeatures>();
            if (!ext)
              return py::none();
            return py::cast(*ext, py::return_value_policy::copy);
          })
      .def_property_readonly(
          "ext_structural_metadata",
          [](CesiumGltf::MeshPrimitive& self) -> py::object {
            auto* ext = self.getExtension<
                CesiumGltf::ExtensionMeshPrimitiveExtStructuralMetadata>();
            if (!ext)
              return py::none();
            return py::cast(*ext, py::return_value_policy::copy);
          })
      .def_property_readonly(
          "ext_khr_gaussian_splatting",
          [](CesiumGltf::MeshPrimitive& self) -> py::object {
            auto* ext = self.getExtension<
                CesiumGltf::ExtensionKhrGaussianSplatting>();
            if (!ext)
              return py::none();
            return py::cast(*ext, py::return_value_policy::copy);
          })
      .def("__repr__", [](const CesiumGltf::MeshPrimitive& self) {
        return "MeshPrimitive(attributes=" +
               std::to_string(self.attributes.size()) +
               ", material=" + std::to_string(self.material) + ")";
      });

  py::class_<CesiumGltf::MeshPrimitive::Mode>(meshPrimCls, "Mode")
      .def_property_readonly_static(
          "POINTS",
          [](py::object) { return CesiumGltf::MeshPrimitive::Mode::POINTS; })
      .def_property_readonly_static(
          "LINES",
          [](py::object) { return CesiumGltf::MeshPrimitive::Mode::LINES; })
      .def_property_readonly_static(
          "LINE_LOOP",
          [](py::object) { return CesiumGltf::MeshPrimitive::Mode::LINE_LOOP; })
      .def_property_readonly_static(
          "LINE_STRIP",
          [](py::object) {
            return CesiumGltf::MeshPrimitive::Mode::LINE_STRIP;
          })
      .def_property_readonly_static(
          "TRIANGLES",
          [](py::object) { return CesiumGltf::MeshPrimitive::Mode::TRIANGLES; })
      .def_property_readonly_static(
          "TRIANGLE_STRIP",
          [](py::object) {
            return CesiumGltf::MeshPrimitive::Mode::TRIANGLE_STRIP;
          })
      .def_property_readonly_static("TRIANGLE_FAN", [](py::object) {
        return CesiumGltf::MeshPrimitive::Mode::TRIANGLE_FAN;
      });

  py::class_<CesiumGltf::Mesh>(m, "Mesh")
      .def(py::init<>())
      .def_readwrite("name", &CesiumGltf::Mesh::name)
      .def_readwrite("primitives", &CesiumGltf::Mesh::primitives)
      .def_readwrite("weights", &CesiumGltf::Mesh::weights)
      .def("__repr__", [](const CesiumGltf::Mesh& self) {
        return "Mesh(name='" + self.name +
               "', primitives=" + std::to_string(self.primitives.size()) + ")";
      });

  py::class_<CesiumGltf::Texture>(m, "Texture")
      .def(py::init<>())
      .def_readwrite("name", &CesiumGltf::Texture::name)
      .def_readwrite("sampler", &CesiumGltf::Texture::sampler)
      .def_readwrite("source", &CesiumGltf::Texture::source)
      .def("__repr__", [](const CesiumGltf::Texture& self) {
        return "Texture(name='" + self.name +
               "', sampler=" + std::to_string(self.sampler) +
               ", source=" + std::to_string(self.source) + ")";
      });

  py::class_<CesiumGltf::Image>(m, "Image")
      .def(py::init<>())
      .def_readwrite("name", &CesiumGltf::Image::name)
      .def_readwrite("uri", &CesiumGltf::Image::uri)
      .def_readwrite("mime_type", &CesiumGltf::Image::mimeType)
      .def_readwrite("buffer_view", &CesiumGltf::Image::bufferView)
      .def_property_readonly(
          "asset",
          [](CesiumGltf::Image& self)
              -> CesiumUtility::IntrusivePointer<CesiumGltf::ImageAsset> {
            return self.pAsset;
          });

  py::class_<CesiumGltf::Sampler> samplerCls(m, "Sampler");
  samplerCls.def(py::init<>())
      .def_readwrite("name", &CesiumGltf::Sampler::name)
      .def_readwrite("mag_filter", &CesiumGltf::Sampler::magFilter)
      .def_readwrite("min_filter", &CesiumGltf::Sampler::minFilter)
      .def_readwrite("wrap_s", &CesiumGltf::Sampler::wrapS)
      .def_readwrite("wrap_t", &CesiumGltf::Sampler::wrapT)
      .def("__repr__", [](const CesiumGltf::Sampler& self) {
        return "Sampler(name='" + self.name + "')";
      });

  py::class_<CesiumGltf::Sampler::MagFilter>(samplerCls, "MagFilter")
      .def_property_readonly_static(
          "NEAREST",
          [](py::object) {
            return CesiumGltf::Sampler::MagFilter::NEAREST;
          })
      .def_property_readonly_static("LINEAR", [](py::object) {
        return CesiumGltf::Sampler::MagFilter::LINEAR;
      });

  py::class_<CesiumGltf::Sampler::MinFilter>(samplerCls, "MinFilter")
      .def_property_readonly_static(
          "NEAREST",
          [](py::object) {
            return CesiumGltf::Sampler::MinFilter::NEAREST;
          })
      .def_property_readonly_static(
          "LINEAR",
          [](py::object) {
            return CesiumGltf::Sampler::MinFilter::LINEAR;
          })
      .def_property_readonly_static(
          "NEAREST_MIPMAP_NEAREST",
          [](py::object) {
            return CesiumGltf::Sampler::MinFilter::NEAREST_MIPMAP_NEAREST;
          })
      .def_property_readonly_static(
          "LINEAR_MIPMAP_NEAREST",
          [](py::object) {
            return CesiumGltf::Sampler::MinFilter::LINEAR_MIPMAP_NEAREST;
          })
      .def_property_readonly_static(
          "NEAREST_MIPMAP_LINEAR",
          [](py::object) {
            return CesiumGltf::Sampler::MinFilter::NEAREST_MIPMAP_LINEAR;
          })
      .def_property_readonly_static(
          "LINEAR_MIPMAP_LINEAR",
          [](py::object) {
            return CesiumGltf::Sampler::MinFilter::LINEAR_MIPMAP_LINEAR;
          });

  py::class_<CesiumGltf::Sampler::WrapS>(samplerCls, "WrapS")
      .def_property_readonly_static(
          "CLAMP_TO_EDGE",
          [](py::object) {
            return CesiumGltf::Sampler::WrapS::CLAMP_TO_EDGE;
          })
      .def_property_readonly_static(
          "MIRRORED_REPEAT",
          [](py::object) {
            return CesiumGltf::Sampler::WrapS::MIRRORED_REPEAT;
          })
      .def_property_readonly_static("REPEAT", [](py::object) {
        return CesiumGltf::Sampler::WrapS::REPEAT;
      });

  py::class_<CesiumGltf::Sampler::WrapT>(samplerCls, "WrapT")
      .def_property_readonly_static(
          "CLAMP_TO_EDGE",
          [](py::object) {
            return CesiumGltf::Sampler::WrapT::CLAMP_TO_EDGE;
          })
      .def_property_readonly_static(
          "MIRRORED_REPEAT",
          [](py::object) {
            return CesiumGltf::Sampler::WrapT::MIRRORED_REPEAT;
          })
      .def_property_readonly_static("REPEAT", [](py::object) {
        return CesiumGltf::Sampler::WrapT::REPEAT;
      });

  py::class_<CesiumGltf::Node>(m, "Node")
      .def(py::init<>())
      .def_readwrite("name", &CesiumGltf::Node::name)
      .def_readwrite("camera", &CesiumGltf::Node::camera)
      .def_readwrite("children", &CesiumGltf::Node::children)
      .def_readwrite("skin", &CesiumGltf::Node::skin)
      .def_readwrite("matrix", &CesiumGltf::Node::matrix)
      .def_readwrite("mesh", &CesiumGltf::Node::mesh)
      .def_readwrite("rotation", &CesiumGltf::Node::rotation)
      .def_readwrite("scale", &CesiumGltf::Node::scale)
      .def_readwrite("translation", &CesiumGltf::Node::translation)
      .def_readwrite("weights", &CesiumGltf::Node::weights)
      .def_property_readonly(
          "ext_mesh_gpu_instancing",
          [](CesiumGltf::Node& self) -> py::object {
            auto* ext = self.getExtension<
                CesiumGltf::ExtensionExtMeshGpuInstancing>();
            if (!ext)
              return py::none();
            return py::cast(*ext, py::return_value_policy::copy);
          })
      .def("__repr__", [](const CesiumGltf::Node& self) {
        return "Node(name='" + self.name +
               "', children=" + std::to_string(self.children.size()) +
               ", mesh=" + std::to_string(self.mesh) + ")";
      });

  py::class_<CesiumGltf::Scene>(m, "Scene")
      .def(py::init<>())
      .def_readwrite("name", &CesiumGltf::Scene::name)
      .def_readwrite("nodes", &CesiumGltf::Scene::nodes)
      .def("__repr__", [](const CesiumGltf::Scene& self) {
        return "Scene(name='" + self.name +
               "', nodes=" + std::to_string(self.nodes.size()) + ")";
      });

  py::class_<CesiumGltf::Skin>(m, "Skin")
      .def(py::init<>())
      .def_readwrite("name", &CesiumGltf::Skin::name)
      .def_readwrite(
          "inverse_bind_matrices",
          &CesiumGltf::Skin::inverseBindMatrices)
      .def_readwrite("skeleton", &CesiumGltf::Skin::skeleton)
      .def_readwrite("joints", &CesiumGltf::Skin::joints)
      .def("__repr__", [](const CesiumGltf::Skin& self) {
        return "Skin(name='" + self.name +
               "', joints=" + std::to_string(self.joints.size()) + ")";
      });

  py::class_<CesiumGltf::CameraOrthographic>(m, "CameraOrthographic")
      .def(py::init<>())
      .def_readwrite("xmag", &CesiumGltf::CameraOrthographic::xmag)
      .def_readwrite("ymag", &CesiumGltf::CameraOrthographic::ymag)
      .def_readwrite("zfar", &CesiumGltf::CameraOrthographic::zfar)
      .def_readwrite("znear", &CesiumGltf::CameraOrthographic::znear);

  py::class_<CesiumGltf::CameraPerspective>(m, "CameraPerspective")
      .def(py::init<>())
      .def_readwrite(
          "aspect_ratio",
          &CesiumGltf::CameraPerspective::aspectRatio)
      .def_readwrite("yfov", &CesiumGltf::CameraPerspective::yfov)
      .def_readwrite("zfar", &CesiumGltf::CameraPerspective::zfar)
      .def_readwrite("znear", &CesiumGltf::CameraPerspective::znear);

  py::class_<CesiumGltf::Camera> cameraCls(m, "GltfCamera");
  cameraCls.def(py::init<>())
      .def_readwrite("name", &CesiumGltf::Camera::name)
      .def_readwrite("orthographic", &CesiumGltf::Camera::orthographic)
      .def_readwrite("perspective", &CesiumGltf::Camera::perspective)
      .def_readwrite("type", &CesiumGltf::Camera::type)
      .def("__repr__", [](const CesiumGltf::Camera& self) {
        return "GltfCamera(name='" + self.name +
               "', type='" + self.type + "')";
      });

  py::class_<CesiumGltf::Camera::Type>(cameraCls, "Type")
      .def_property_readonly_static(
          "perspective",
          [](py::object) { return CesiumGltf::Camera::Type::perspective; })
      .def_property_readonly_static("orthographic", [](py::object) {
        return CesiumGltf::Camera::Type::orthographic;
      });

  py::class_<CesiumGltf::AnimationChannelTarget> animChannelTargetCls(
      m,
      "AnimationChannelTarget");
  animChannelTargetCls.def(py::init<>())
      .def_readwrite("node", &CesiumGltf::AnimationChannelTarget::node)
      .def_readwrite("path", &CesiumGltf::AnimationChannelTarget::path);

  py::class_<CesiumGltf::AnimationChannelTarget::Path>(
      animChannelTargetCls,
      "Path")
      .def_property_readonly_static(
          "translation",
          [](py::object) {
            return CesiumGltf::AnimationChannelTarget::Path::translation;
          })
      .def_property_readonly_static(
          "rotation",
          [](py::object) {
            return CesiumGltf::AnimationChannelTarget::Path::rotation;
          })
      .def_property_readonly_static(
          "scale",
          [](py::object) {
            return CesiumGltf::AnimationChannelTarget::Path::scale;
          })
      .def_property_readonly_static("weights", [](py::object) {
        return CesiumGltf::AnimationChannelTarget::Path::weights;
      });

  py::class_<CesiumGltf::AnimationChannel>(m, "AnimationChannel")
      .def(py::init<>())
      .def_readwrite("sampler", &CesiumGltf::AnimationChannel::sampler)
      .def_readwrite("target", &CesiumGltf::AnimationChannel::target)
      .def("__repr__", [](const CesiumGltf::AnimationChannel& self) {
        return "AnimationChannel(sampler=" + std::to_string(self.sampler) + ")";
      });

  py::class_<CesiumGltf::AnimationSampler> animSamplerCls(
      m,
      "AnimationSampler");
  animSamplerCls.def(py::init<>())
      .def_readwrite("input", &CesiumGltf::AnimationSampler::input)
      .def_readwrite(
          "interpolation",
          &CesiumGltf::AnimationSampler::interpolation)
      .def_readwrite("output", &CesiumGltf::AnimationSampler::output)
      .def("__repr__", [](const CesiumGltf::AnimationSampler& self) {
        return "AnimationSampler(interpolation='" + self.interpolation +
               "', input=" + std::to_string(self.input) +
               ", output=" + std::to_string(self.output) + ")";
      });

  py::class_<CesiumGltf::AnimationSampler::Interpolation>(
      animSamplerCls,
      "Interpolation")
      .def_property_readonly_static(
          "LINEAR",
          [](py::object) {
            return CesiumGltf::AnimationSampler::Interpolation::LINEAR;
          })
      .def_property_readonly_static(
          "STEP",
          [](py::object) {
            return CesiumGltf::AnimationSampler::Interpolation::STEP;
          })
      .def_property_readonly_static("CUBICSPLINE", [](py::object) {
        return CesiumGltf::AnimationSampler::Interpolation::CUBICSPLINE;
      });

  py::class_<CesiumGltf::Animation>(m, "Animation")
      .def(py::init<>())
      .def_readwrite("name", &CesiumGltf::Animation::name)
      .def_readwrite("channels", &CesiumGltf::Animation::channels)
      .def_readwrite("samplers", &CesiumGltf::Animation::samplers)
      .def("__repr__", [](const CesiumGltf::Animation& self) {
        return "Animation(name='" + self.name +
               "', channels=" + std::to_string(self.channels.size()) +
               ", samplers=" + std::to_string(self.samplers.size()) + ")";
      });

  py::class_<CesiumGltf::EnumValue, CesiumUtility::ExtensibleObject>(
      m,
      "EnumValue")
      .def(py::init<>())
      .def_readwrite("name", &CesiumGltf::EnumValue::name)
      .def_readwrite("description", &CesiumGltf::EnumValue::description)
      .def_readwrite("value", &CesiumGltf::EnumValue::value)
      .def("__repr__", [](const CesiumGltf::EnumValue& self) {
        return "EnumValue(name='" + self.name +
               "', value=" + std::to_string(self.value) + ")";
      });

  py::class_<CesiumGltf::Enum, CesiumUtility::ExtensibleObject>(m, "Enum")
      .def(py::init<>())
      .def_readwrite("name", &CesiumGltf::Enum::name)
      .def_readwrite("description", &CesiumGltf::Enum::description)
      .def_readwrite("value_type", &CesiumGltf::Enum::valueType)
      .def_readwrite("values", &CesiumGltf::Enum::values)
      .def(
          "get_name",
          [](const CesiumGltf::Enum& self,
             int64_t enumValue) -> std::optional<std::string> {
            auto result = self.getName(enumValue);
            if (result.has_value()) {
              return std::string(*result);
            }
            return std::nullopt;
          },
          py::arg("enum_value"))
      .def("__repr__", [](const CesiumGltf::Enum& self) {
        std::string n = self.name.value_or("<unnamed>");
        return "Enum(name='" + n +
               "', values=" + std::to_string(self.values.size()) + ")";
      });

  auto clsProp =
      py::class_<CesiumGltf::ClassProperty, CesiumUtility::ExtensibleObject>(
          m,
          "ClassProperty")
          .def(py::init<>())
          .def_readwrite("name", &CesiumGltf::ClassProperty::name)
          .def_readwrite(
              "description",
              &CesiumGltf::ClassProperty::description)
          .def_readwrite("type", &CesiumGltf::ClassProperty::type)
          .def_readwrite(
              "component_type",
              &CesiumGltf::ClassProperty::componentType)
          .def_readwrite(
              "enum_type",
              &CesiumGltf::ClassProperty::enumType)
          .def_readwrite("array", &CesiumGltf::ClassProperty::array)
          .def_readwrite("count", &CesiumGltf::ClassProperty::count)
          .def_readwrite(
              "normalized",
              &CesiumGltf::ClassProperty::normalized)
          .def_readwrite("required", &CesiumGltf::ClassProperty::required)
          .def_readwrite("semantic", &CesiumGltf::ClassProperty::semantic);

  auto jsonProp = [&clsProp](
                      const char* pyName,
                      std::optional<CesiumUtility::JsonValue>
                          CesiumGltf::ClassProperty::*member) {
    clsProp.def_property(
        pyName,
        [member](const CesiumGltf::ClassProperty& self) -> py::object {
          const auto& val = self.*member;
          if (!val.has_value())
            return py::none();
          return CesiumPython::jsonValueToPy(*val);
        },
        [member](CesiumGltf::ClassProperty& self, py::object value) {
          if (value.is_none()) {
            self.*member = std::nullopt;
          } else {
            self.*member = CesiumPython::pyToJsonValue(value);
          }
        });
  };
  jsonProp("offset", &CesiumGltf::ClassProperty::offset);
  jsonProp("scale", &CesiumGltf::ClassProperty::scale);
  jsonProp("max", &CesiumGltf::ClassProperty::max);
  jsonProp("min", &CesiumGltf::ClassProperty::min);
  jsonProp("no_data", &CesiumGltf::ClassProperty::noData);
  jsonProp("default_property", &CesiumGltf::ClassProperty::defaultProperty);

  clsProp.def("__repr__", [](const CesiumGltf::ClassProperty& self) {
    std::string n = self.name.value_or("<unnamed>");
    return "ClassProperty(name='" + n + "', type='" + self.type + "')";
  });

  py::class_<CesiumGltf::ClassProperty::Type>(clsProp, "Type")
      .def_property_readonly_static(
          "SCALAR",
          [](py::object) { return CesiumGltf::ClassProperty::Type::SCALAR; })
      .def_property_readonly_static(
          "VEC2",
          [](py::object) { return CesiumGltf::ClassProperty::Type::VEC2; })
      .def_property_readonly_static(
          "VEC3",
          [](py::object) { return CesiumGltf::ClassProperty::Type::VEC3; })
      .def_property_readonly_static(
          "VEC4",
          [](py::object) { return CesiumGltf::ClassProperty::Type::VEC4; })
      .def_property_readonly_static(
          "MAT2",
          [](py::object) { return CesiumGltf::ClassProperty::Type::MAT2; })
      .def_property_readonly_static(
          "MAT3",
          [](py::object) { return CesiumGltf::ClassProperty::Type::MAT3; })
      .def_property_readonly_static(
          "MAT4",
          [](py::object) { return CesiumGltf::ClassProperty::Type::MAT4; })
      .def_property_readonly_static(
          "STRING",
          [](py::object) { return CesiumGltf::ClassProperty::Type::STRING; })
      .def_property_readonly_static(
          "BOOLEAN",
          [](py::object) { return CesiumGltf::ClassProperty::Type::BOOLEAN; })
      .def_property_readonly_static(
          "ENUM",
          [](py::object) { return CesiumGltf::ClassProperty::Type::ENUM; });

  py::class_<CesiumGltf::ClassProperty::ComponentType>(
      clsProp,
      "ComponentType")
      .def_property_readonly_static(
          "INT8",
          [](py::object) {
            return CesiumGltf::ClassProperty::ComponentType::INT8;
          })
      .def_property_readonly_static(
          "UINT8",
          [](py::object) {
            return CesiumGltf::ClassProperty::ComponentType::UINT8;
          })
      .def_property_readonly_static(
          "INT16",
          [](py::object) {
            return CesiumGltf::ClassProperty::ComponentType::INT16;
          })
      .def_property_readonly_static(
          "UINT16",
          [](py::object) {
            return CesiumGltf::ClassProperty::ComponentType::UINT16;
          })
      .def_property_readonly_static(
          "INT32",
          [](py::object) {
            return CesiumGltf::ClassProperty::ComponentType::INT32;
          })
      .def_property_readonly_static(
          "UINT32",
          [](py::object) {
            return CesiumGltf::ClassProperty::ComponentType::UINT32;
          })
      .def_property_readonly_static(
          "INT64",
          [](py::object) {
            return CesiumGltf::ClassProperty::ComponentType::INT64;
          })
      .def_property_readonly_static(
          "UINT64",
          [](py::object) {
            return CesiumGltf::ClassProperty::ComponentType::UINT64;
          })
      .def_property_readonly_static(
          "FLOAT32",
          [](py::object) {
            return CesiumGltf::ClassProperty::ComponentType::FLOAT32;
          })
      .def_property_readonly_static(
          "FLOAT64",
          [](py::object) {
            return CesiumGltf::ClassProperty::ComponentType::FLOAT64;
          });

  py::class_<CesiumGltf::Class, CesiumUtility::ExtensibleObject>(
      m,
      "MetadataClass")
      .def(py::init<>())
      .def_readwrite("name", &CesiumGltf::Class::name)
      .def_readwrite("description", &CesiumGltf::Class::description)
      .def_readwrite("properties", &CesiumGltf::Class::properties)
      .def_readwrite("parent", &CesiumGltf::Class::parent)
      .def("__repr__", [](const CesiumGltf::Class& self) {
        std::string n = self.name.value_or("<unnamed>");
        return "MetadataClass(name='" + n +
               "', properties=" + std::to_string(self.properties.size()) + ")";
      });

  py::class_<
      CesiumGltf::Schema,
      CesiumUtility::IntrusivePointer<CesiumGltf::Schema>>(m, "Schema")
      .def(py::init<>())
      .def_readwrite("id", &CesiumGltf::Schema::id)
      .def_readwrite("name", &CesiumGltf::Schema::name)
      .def_readwrite("description", &CesiumGltf::Schema::description)
      .def_readwrite("version", &CesiumGltf::Schema::version)
      .def_readwrite("classes", &CesiumGltf::Schema::classes)
      .def_readwrite("enums", &CesiumGltf::Schema::enums)
      .def_property_readonly("size_bytes", &CesiumGltf::Schema::getSizeBytes);

  py::class_<CesiumGltf::Model, CesiumUtility::ExtensibleObject>(m, "Model")
      .def(py::init<>())
      .def_readwrite("accessors", &CesiumGltf::Model::accessors)
      .def_readwrite("animations", &CesiumGltf::Model::animations)
      .def_readwrite("asset", &CesiumGltf::Model::asset)
      .def_readwrite("buffers", &CesiumGltf::Model::buffers)
      .def_readwrite("buffer_views", &CesiumGltf::Model::bufferViews)
      .def_readwrite("cameras", &CesiumGltf::Model::cameras)
      .def_readwrite("images", &CesiumGltf::Model::images)
      .def_readwrite("materials", &CesiumGltf::Model::materials)
      .def_readwrite("meshes", &CesiumGltf::Model::meshes)
      .def_readwrite("nodes", &CesiumGltf::Model::nodes)
      .def_readwrite("samplers", &CesiumGltf::Model::samplers)
      .def_readwrite("scene", &CesiumGltf::Model::scene)
      .def_readwrite("scenes", &CesiumGltf::Model::scenes)
      .def_readwrite("skins", &CesiumGltf::Model::skins)
      .def_readwrite("textures", &CesiumGltf::Model::textures)
      .def_readwrite("extensions_used", &CesiumGltf::Model::extensionsUsed)
      .def_readwrite(
          "extensions_required",
          &CesiumGltf::Model::extensionsRequired)
      .def_property_readonly("size_bytes", &CesiumGltf::Model::getSizeBytes)
      .def("add_extension_used", &CesiumGltf::Model::addExtensionUsed)
      .def("add_extension_required", &CesiumGltf::Model::addExtensionRequired)
      .def("remove_extension_used", &CesiumGltf::Model::removeExtensionUsed)
      .def(
          "remove_extension_required",
          &CesiumGltf::Model::removeExtensionRequired)
      .def("is_extension_used", &CesiumGltf::Model::isExtensionUsed)
      .def("is_extension_required", &CesiumGltf::Model::isExtensionRequired)
      .def(
          "generate_missing_normals_smooth",
          &CesiumGltf::Model::generateMissingNormalsSmooth)
      .def(
          "merge",
          [](CesiumGltf::Model& self, CesiumGltf::Model other) {
            return self.merge(std::move(other));
          },
          py::arg("other"))
      .def(
          "for_each_root_node_in_scene",
          [](CesiumGltf::Model& self, int32_t sceneID, py::function callback) {
            self.forEachRootNodeInScene(
                sceneID,
                [callback](CesiumGltf::Model& gltf, CesiumGltf::Node& node) {
                  py::gil_scoped_acquire gil;
                  callback(
                      py::cast(gltf, py::return_value_policy::reference),
                      py::cast(node, py::return_value_policy::reference));
                });
          },
          py::arg("scene_id"),
          py::arg("callback"))
      .def(
          "for_each_node_in_scene",
          [](CesiumGltf::Model& self, int32_t sceneID, py::function callback) {
            self.forEachNodeInScene(
                sceneID,
                [callback](
                    CesiumGltf::Model& gltf,
                    CesiumGltf::Node& node,
                    const glm::dmat4& transform) {
                  py::gil_scoped_acquire gil;
                  callback(
                      py::cast(gltf, py::return_value_policy::reference),
                      py::cast(node, py::return_value_policy::reference),
                      py::cast(transform));
                });
          },
          py::arg("scene_id"),
          py::arg("callback"))
      .def(
          "for_each_primitive_in_scene",
          [](CesiumGltf::Model& self, int32_t sceneID, py::function callback) {
            self.forEachPrimitiveInScene(
                sceneID,
                [callback](
                    CesiumGltf::Model& gltf,
                    CesiumGltf::Node& node,
                    CesiumGltf::Mesh& mesh,
                    CesiumGltf::MeshPrimitive& primitive,
                    const glm::dmat4& transform) {
                  py::gil_scoped_acquire gil;
                  callback(
                      py::cast(gltf, py::return_value_policy::reference),
                      py::cast(node, py::return_value_policy::reference),
                      py::cast(mesh, py::return_value_policy::reference),
                      py::cast(primitive, py::return_value_policy::reference),
                      py::cast(transform));
                });
          },
          py::arg("scene_id"),
          py::arg("callback"))
      .def("__repr__", [](const CesiumGltf::Model& self) {
        return "Model(meshes=" + std::to_string(self.meshes.size()) +
               ", materials=" + std::to_string(self.materials.size()) +
               ", nodes=" + std::to_string(self.nodes.size()) + ")";
      })
      .def_property_readonly(
          "ext_structural_metadata",
          [](CesiumGltf::Model& self) -> py::object {
            auto* ext = self.getExtension<
                CesiumGltf::ExtensionModelExtStructuralMetadata>();
            if (!ext)
              return py::none();
            return py::cast(*ext, py::return_value_policy::copy);
          })
      .def(
          "get_or_create_ext_structural_metadata",
          [](CesiumGltf::Model& self)
              -> CesiumGltf::ExtensionModelExtStructuralMetadata& {
            return self
                .addExtension<
                    CesiumGltf::ExtensionModelExtStructuralMetadata>();
          },
          py::return_value_policy::reference_internal)
      .def_property_readonly(
          "ext_cesium_rtc",
          [](CesiumGltf::Model& self) -> py::object {
            auto* ext =
                self.getExtension<CesiumGltf::ExtensionCesiumRTC>();
            if (!ext)
              return py::none();
            return py::cast(*ext, py::return_value_policy::copy);
          });

  {
    auto vas = py::class_<CesiumGltf::VertexAttributeSemantics>(
                   m, "VertexAttributeSemantics")
                   .def_property_readonly_static(
                       "POSITION",
                       [](py::object) {
                         return CesiumGltf::VertexAttributeSemantics::POSITION;
                       })
                   .def_property_readonly_static(
                       "NORMAL",
                       [](py::object) {
                         return CesiumGltf::VertexAttributeSemantics::NORMAL;
                       })
                   .def_property_readonly_static(
                       "TANGENT",
                       [](py::object) {
                         return CesiumGltf::VertexAttributeSemantics::TANGENT;
                       });

    // Bind the indexed attributes: TEXCOORD_0..7, COLOR_0..7, etc.
    auto bindIndexed =
        [&](const char* name, const std::array<std::string, 8>& arr) {
          vas.def_static(
              name,
              [arr](int index) -> std::string {
                if (index < 0 || index >= 8)
                  throw py::index_error("index must be 0..7");
                return arr[index];
              },
              py::arg("index") = 0);
        };
    bindIndexed(
        "TEXCOORD", CesiumGltf::VertexAttributeSemantics::TEXCOORD_n);
    bindIndexed("COLOR", CesiumGltf::VertexAttributeSemantics::COLOR_n);
    bindIndexed("JOINTS", CesiumGltf::VertexAttributeSemantics::JOINTS_n);
    bindIndexed(
        "WEIGHTS", CesiumGltf::VertexAttributeSemantics::WEIGHTS_n);
    bindIndexed(
        "FEATURE_ID",
        CesiumGltf::VertexAttributeSemantics::FEATURE_ID_n);
  }


  py::class_<CesiumGltf::ExtensionKhrMaterialsUnlit,
             CesiumUtility::ExtensibleObject>(m, "ExtKhrMaterialsUnlit")
      .def(py::init<>())
      .def_property_readonly_static(
          "EXTENSION_NAME",
          [](py::object) {
            return CesiumGltf::ExtensionKhrMaterialsUnlit::ExtensionName;
          });

  py::class_<CesiumGltf::ExtensionKhrGaussianSplatting,
             CesiumUtility::ExtensibleObject>
      extGsCls(m, "ExtKhrGaussianSplatting");
  extGsCls.def(py::init<>())
      .def_readwrite("kernel", &CesiumGltf::ExtensionKhrGaussianSplatting::kernel)
      .def_readwrite(
          "color_space",
          &CesiumGltf::ExtensionKhrGaussianSplatting::colorSpace)
      .def_readwrite(
          "projection",
          &CesiumGltf::ExtensionKhrGaussianSplatting::projection)
      .def_readwrite(
          "sorting_method",
          &CesiumGltf::ExtensionKhrGaussianSplatting::sortingMethod)
      .def_property_readonly_static(
          "EXTENSION_NAME",
          [](py::object) {
            return CesiumGltf::ExtensionKhrGaussianSplatting::ExtensionName;
          });

  py::class_<CesiumGltf::ExtensionKhrGaussianSplatting::Kernel>(
      extGsCls, "Kernel")
      .def_property_readonly_static(
          "ellipse",
          [](py::object) {
            return CesiumGltf::ExtensionKhrGaussianSplatting::Kernel::ellipse;
          });

  py::class_<CesiumGltf::ExtensionKhrGaussianSplatting::ColorSpace>(
      extGsCls, "ColorSpace")
      .def_property_readonly_static(
          "srgb_rec709_display",
          [](py::object) {
            return CesiumGltf::ExtensionKhrGaussianSplatting::ColorSpace::
                srgb_rec709_display;
          })
      .def_property_readonly_static(
          "lin_rec709_display",
          [](py::object) {
            return CesiumGltf::ExtensionKhrGaussianSplatting::ColorSpace::
                lin_rec709_display;
          });

  py::class_<CesiumGltf::ExtensionKhrGaussianSplatting::Projection>(
      extGsCls, "Projection")
      .def_property_readonly_static(
          "perspective",
          [](py::object) {
            return CesiumGltf::ExtensionKhrGaussianSplatting::Projection::
                perspective;
          });

  py::class_<CesiumGltf::ExtensionKhrGaussianSplatting::SortingMethod>(
      extGsCls, "SortingMethod")
      .def_property_readonly_static(
          "cameraDistance",
          [](py::object) {
            return CesiumGltf::ExtensionKhrGaussianSplatting::SortingMethod::
                cameraDistance;
          });

  py::class_<CesiumGltf::ExtensionKhrGaussianSplattingHintsValue,
             CesiumUtility::ExtensibleObject>(
      m, "ExtKhrGaussianSplattingHintsValue")
      .def(py::init<>())
      .def_readwrite(
          "projection",
          &CesiumGltf::ExtensionKhrGaussianSplattingHintsValue::projection)
      .def_readwrite(
          "sorting_method",
          &CesiumGltf::ExtensionKhrGaussianSplattingHintsValue::sortingMethod);

  py::class_<CesiumGltf::ExtensionKhrGaussianSplattingCompressionSpz2,
             CesiumUtility::ExtensibleObject>(
      m, "ExtKhrGaussianSplattingCompressionSpz2")
      .def(py::init<>())
      .def_readwrite(
          "buffer_view",
          &CesiumGltf::ExtensionKhrGaussianSplattingCompressionSpz2::
              bufferView)
      .def_property_readonly_static(
          "EXTENSION_NAME",
          [](py::object) {
            return CesiumGltf::ExtensionKhrGaussianSplattingCompressionSpz2::
                ExtensionName;
          });

  py::class_<CesiumGltf::ExtensionCesiumRTC,
             CesiumUtility::ExtensibleObject>(m, "ExtCesiumRTC")
      .def(py::init<>())
      .def_readwrite("center", &CesiumGltf::ExtensionCesiumRTC::center)
      .def_property_readonly_static(
          "EXTENSION_NAME",
          [](py::object) {
            return CesiumGltf::ExtensionCesiumRTC::ExtensionName;
          });

  auto propTableProp =
      py::class_<CesiumGltf::PropertyTableProperty,
                 CesiumUtility::ExtensibleObject>(m, "PropertyTableProperty")
          .def(py::init<>())
          .def_readwrite(
              "values",
              &CesiumGltf::PropertyTableProperty::values)
          .def_readwrite(
              "array_offsets",
              &CesiumGltf::PropertyTableProperty::arrayOffsets)
          .def_readwrite(
              "string_offsets",
              &CesiumGltf::PropertyTableProperty::stringOffsets)
          .def_readwrite(
              "array_offset_type",
              &CesiumGltf::PropertyTableProperty::arrayOffsetType)
          .def_readwrite(
              "string_offset_type",
              &CesiumGltf::PropertyTableProperty::stringOffsetType)
          .def_property(
              "offset",
              [](const CesiumGltf::PropertyTableProperty& self)
                  -> py::object {
                if (!self.offset)
                  return py::none();
                return CesiumPython::jsonValueToPy(*self.offset);
              },
              [](CesiumGltf::PropertyTableProperty& self, py::object val) {
                if (val.is_none())
                  self.offset = std::nullopt;
                else
                  self.offset = CesiumPython::pyToJsonValue(val);
              })
          .def_property(
              "scale",
              [](const CesiumGltf::PropertyTableProperty& self)
                  -> py::object {
                if (!self.scale)
                  return py::none();
                return CesiumPython::jsonValueToPy(*self.scale);
              },
              [](CesiumGltf::PropertyTableProperty& self, py::object val) {
                if (val.is_none())
                  self.scale = std::nullopt;
                else
                  self.scale = CesiumPython::pyToJsonValue(val);
              })
          .def_property(
              "max",
              [](const CesiumGltf::PropertyTableProperty& self)
                  -> py::object {
                if (!self.max)
                  return py::none();
                return CesiumPython::jsonValueToPy(*self.max);
              },
              [](CesiumGltf::PropertyTableProperty& self, py::object val) {
                if (val.is_none())
                  self.max = std::nullopt;
                else
                  self.max = CesiumPython::pyToJsonValue(val);
              })
          .def_property(
              "min",
              [](const CesiumGltf::PropertyTableProperty& self)
                  -> py::object {
                if (!self.min)
                  return py::none();
                return CesiumPython::jsonValueToPy(*self.min);
              },
              [](CesiumGltf::PropertyTableProperty& self, py::object val) {
                if (val.is_none())
                  self.min = std::nullopt;
                else
                  self.min = CesiumPython::pyToJsonValue(val);
              });

  py::class_<CesiumGltf::PropertyTableProperty::ArrayOffsetType>(
      propTableProp, "ArrayOffsetType")
      .def_property_readonly_static(
          "UINT8",
          [](py::object) {
            return CesiumGltf::PropertyTableProperty::ArrayOffsetType::UINT8;
          })
      .def_property_readonly_static(
          "UINT16",
          [](py::object) {
            return CesiumGltf::PropertyTableProperty::ArrayOffsetType::UINT16;
          })
      .def_property_readonly_static(
          "UINT32",
          [](py::object) {
            return CesiumGltf::PropertyTableProperty::ArrayOffsetType::UINT32;
          })
      .def_property_readonly_static("UINT64", [](py::object) {
        return CesiumGltf::PropertyTableProperty::ArrayOffsetType::UINT64;
      });

  py::class_<CesiumGltf::PropertyTableProperty::StringOffsetType>(
      propTableProp, "StringOffsetType")
      .def_property_readonly_static(
          "UINT8",
          [](py::object) {
            return CesiumGltf::PropertyTableProperty::StringOffsetType::UINT8;
          })
      .def_property_readonly_static(
          "UINT16",
          [](py::object) {
            return CesiumGltf::PropertyTableProperty::StringOffsetType::UINT16;
          })
      .def_property_readonly_static(
          "UINT32",
          [](py::object) {
            return CesiumGltf::PropertyTableProperty::StringOffsetType::UINT32;
          })
      .def_property_readonly_static("UINT64", [](py::object) {
        return CesiumGltf::PropertyTableProperty::StringOffsetType::UINT64;
      });

  py::class_<CesiumGltf::PropertyTable, CesiumUtility::ExtensibleObject>(
      m, "PropertyTableSpec")
      .def(py::init<>())
      .def_readwrite("name", &CesiumGltf::PropertyTable::name)
      .def_readwrite(
          "class_property",
          &CesiumGltf::PropertyTable::classProperty)
      .def_readwrite("count", &CesiumGltf::PropertyTable::count)
      .def_readwrite("properties", &CesiumGltf::PropertyTable::properties);

  py::class_<CesiumGltf::PropertyTextureProperty, CesiumGltf::TextureInfo>(
      m, "PropertyTextureProperty")
      .def(py::init<>())
      .def_readwrite(
          "channels",
          &CesiumGltf::PropertyTextureProperty::channels)
      .def_property(
          "offset",
          [](const CesiumGltf::PropertyTextureProperty& self) -> py::object {
            if (!self.offset)
              return py::none();
            return CesiumPython::jsonValueToPy(*self.offset);
          },
          [](CesiumGltf::PropertyTextureProperty& self, py::object val) {
            if (val.is_none())
              self.offset = std::nullopt;
            else
              self.offset = CesiumPython::pyToJsonValue(val);
          })
      .def_property(
          "scale",
          [](const CesiumGltf::PropertyTextureProperty& self) -> py::object {
            if (!self.scale)
              return py::none();
            return CesiumPython::jsonValueToPy(*self.scale);
          },
          [](CesiumGltf::PropertyTextureProperty& self, py::object val) {
            if (val.is_none())
              self.scale = std::nullopt;
            else
              self.scale = CesiumPython::pyToJsonValue(val);
          })
      .def_property(
          "max",
          [](const CesiumGltf::PropertyTextureProperty& self) -> py::object {
            if (!self.max)
              return py::none();
            return CesiumPython::jsonValueToPy(*self.max);
          },
          [](CesiumGltf::PropertyTextureProperty& self, py::object val) {
            if (val.is_none())
              self.max = std::nullopt;
            else
              self.max = CesiumPython::pyToJsonValue(val);
          })
      .def_property(
          "min",
          [](const CesiumGltf::PropertyTextureProperty& self) -> py::object {
            if (!self.min)
              return py::none();
            return CesiumPython::jsonValueToPy(*self.min);
          },
          [](CesiumGltf::PropertyTextureProperty& self, py::object val) {
            if (val.is_none())
              self.min = std::nullopt;
            else
              self.min = CesiumPython::pyToJsonValue(val);
          });

  py::class_<CesiumGltf::PropertyTexture, CesiumUtility::ExtensibleObject>(
      m, "PropertyTextureSpec")
      .def(py::init<>())
      .def_readwrite("name", &CesiumGltf::PropertyTexture::name)
      .def_readwrite(
          "class_property",
          &CesiumGltf::PropertyTexture::classProperty)
      .def_readwrite("properties", &CesiumGltf::PropertyTexture::properties);

  py::class_<CesiumGltf::PropertyAttributeProperty,
             CesiumUtility::ExtensibleObject>(
      m, "PropertyAttributeProperty")
      .def(py::init<>())
      .def_readwrite(
          "attribute",
          &CesiumGltf::PropertyAttributeProperty::attribute)
      .def_property(
          "offset",
          [](const CesiumGltf::PropertyAttributeProperty& self) -> py::object {
            if (!self.offset)
              return py::none();
            return CesiumPython::jsonValueToPy(*self.offset);
          },
          [](CesiumGltf::PropertyAttributeProperty& self, py::object val) {
            if (val.is_none())
              self.offset = std::nullopt;
            else
              self.offset = CesiumPython::pyToJsonValue(val);
          })
      .def_property(
          "scale",
          [](const CesiumGltf::PropertyAttributeProperty& self) -> py::object {
            if (!self.scale)
              return py::none();
            return CesiumPython::jsonValueToPy(*self.scale);
          },
          [](CesiumGltf::PropertyAttributeProperty& self, py::object val) {
            if (val.is_none())
              self.scale = std::nullopt;
            else
              self.scale = CesiumPython::pyToJsonValue(val);
          })
      .def_property(
          "max",
          [](const CesiumGltf::PropertyAttributeProperty& self) -> py::object {
            if (!self.max)
              return py::none();
            return CesiumPython::jsonValueToPy(*self.max);
          },
          [](CesiumGltf::PropertyAttributeProperty& self, py::object val) {
            if (val.is_none())
              self.max = std::nullopt;
            else
              self.max = CesiumPython::pyToJsonValue(val);
          })
      .def_property(
          "min",
          [](const CesiumGltf::PropertyAttributeProperty& self) -> py::object {
            if (!self.min)
              return py::none();
            return CesiumPython::jsonValueToPy(*self.min);
          },
          [](CesiumGltf::PropertyAttributeProperty& self, py::object val) {
            if (val.is_none())
              self.min = std::nullopt;
            else
              self.min = CesiumPython::pyToJsonValue(val);
          });

  py::class_<CesiumGltf::PropertyAttribute, CesiumUtility::ExtensibleObject>(
      m, "PropertyAttributeSpec")
      .def(py::init<>())
      .def_readwrite("name", &CesiumGltf::PropertyAttribute::name)
      .def_readwrite(
          "class_property",
          &CesiumGltf::PropertyAttribute::classProperty)
      .def_readwrite(
          "properties",
          &CesiumGltf::PropertyAttribute::properties);

  py::class_<CesiumGltf::FeatureIdTexture, CesiumGltf::TextureInfo>(
      m, "FeatureIdTexture")
      .def(py::init<>())
      .def_readwrite("channels", &CesiumGltf::FeatureIdTexture::channels);

  py::class_<CesiumGltf::FeatureId, CesiumUtility::ExtensibleObject>(
      m, "FeatureId")
      .def(py::init<>())
      .def_readwrite("feature_count", &CesiumGltf::FeatureId::featureCount)
      .def_readwrite(
          "null_feature_id",
          &CesiumGltf::FeatureId::nullFeatureId)
      .def_readwrite("label", &CesiumGltf::FeatureId::label)
      .def_readwrite("attribute", &CesiumGltf::FeatureId::attribute)
      .def_readwrite("texture", &CesiumGltf::FeatureId::texture)
      .def_readwrite(
          "property_table",
          &CesiumGltf::FeatureId::propertyTable);

  py::class_<CesiumGltf::ExtensionExtMeshFeatures,
             CesiumUtility::ExtensibleObject>(m, "ExtMeshFeatures")
      .def(py::init<>())
      .def_readwrite(
          "feature_ids",
          &CesiumGltf::ExtensionExtMeshFeatures::featureIds);

  py::class_<CesiumGltf::ExtensionMeshPrimitiveExtStructuralMetadata,
             CesiumUtility::ExtensibleObject>(
      m, "MeshPrimitiveExtStructuralMetadata")
      .def(py::init<>())
      .def_readwrite(
          "property_textures",
          &CesiumGltf::ExtensionMeshPrimitiveExtStructuralMetadata::
              propertyTextures)
      .def_readwrite(
          "property_attributes",
          &CesiumGltf::ExtensionMeshPrimitiveExtStructuralMetadata::
              propertyAttributes);

  py::class_<CesiumGltf::ExtensionModelExtStructuralMetadata,
             CesiumUtility::ExtensibleObject>(
      m, "ModelExtStructuralMetadata")
      .def(py::init<>())
      .def_readwrite(
          "schema",
          &CesiumGltf::ExtensionModelExtStructuralMetadata::schema)
      .def_readwrite(
          "schema_uri",
          &CesiumGltf::ExtensionModelExtStructuralMetadata::schemaUri)
      .def_readwrite(
          "property_tables",
          &CesiumGltf::ExtensionModelExtStructuralMetadata::propertyTables)
      .def_readwrite(
          "property_textures",
          &CesiumGltf::ExtensionModelExtStructuralMetadata::propertyTextures)
      .def_readwrite(
          "property_attributes",
          &CesiumGltf::ExtensionModelExtStructuralMetadata::
              propertyAttributes);

  py::class_<CesiumGltf::ExtensionExtMeshGpuInstancing,
             CesiumUtility::ExtensibleObject>(
      m, "ExtMeshGpuInstancing")
      .def(py::init<>())
      .def_readwrite(
          "attributes",
          &CesiumGltf::ExtensionExtMeshGpuInstancing::attributes);

  py::class_<CesiumGltf::ExtensionExtInstanceFeaturesFeatureId,
             CesiumUtility::ExtensibleObject>(
      m, "ExtInstanceFeaturesFeatureId")
      .def(py::init<>())
      .def_readwrite(
          "feature_count",
          &CesiumGltf::ExtensionExtInstanceFeaturesFeatureId::featureCount)
      .def_readwrite(
          "null_feature_id",
          &CesiumGltf::ExtensionExtInstanceFeaturesFeatureId::nullFeatureId)
      .def_readwrite(
          "label",
          &CesiumGltf::ExtensionExtInstanceFeaturesFeatureId::label)
      .def_readwrite(
          "attribute",
          &CesiumGltf::ExtensionExtInstanceFeaturesFeatureId::attribute)
      .def_readwrite(
          "property_table",
          &CesiumGltf::ExtensionExtInstanceFeaturesFeatureId::propertyTable);

  py::class_<CesiumGltf::ExtensionExtInstanceFeatures,
             CesiumUtility::ExtensibleObject>(
      m, "ExtInstanceFeatures")
      .def(py::init<>())
      .def_readwrite(
          "feature_ids",
          &CesiumGltf::ExtensionExtInstanceFeatures::featureIds);

  using namespace CesiumStructuralMetadata;

  py::enum_<CesiumGltf::PropertyType>(m, "PropertyType")
      .value("Invalid", CesiumGltf::PropertyType::Invalid)
      .value("Scalar", CesiumGltf::PropertyType::Scalar)
      .value("Vec2", CesiumGltf::PropertyType::Vec2)
      .value("Vec3", CesiumGltf::PropertyType::Vec3)
      .value("Vec4", CesiumGltf::PropertyType::Vec4)
      .value("Mat2", CesiumGltf::PropertyType::Mat2)
      .value("Mat3", CesiumGltf::PropertyType::Mat3)
      .value("Mat4", CesiumGltf::PropertyType::Mat4)
      .value("String", CesiumGltf::PropertyType::String)
      .value("Boolean", CesiumGltf::PropertyType::Boolean)
      .value("Enum", CesiumGltf::PropertyType::Enum);

  py::enum_<CesiumGltf::PropertyComponentType>(m, "PropertyComponentType")
      .value("NONE", CesiumGltf::PropertyComponentType::None)
      .value("Int8", CesiumGltf::PropertyComponentType::Int8)
      .value("Uint8", CesiumGltf::PropertyComponentType::Uint8)
      .value("Int16", CesiumGltf::PropertyComponentType::Int16)
      .value("Uint16", CesiumGltf::PropertyComponentType::Uint16)
      .value("Int32", CesiumGltf::PropertyComponentType::Int32)
      .value("Uint32", CesiumGltf::PropertyComponentType::Uint32)
      .value("Int64", CesiumGltf::PropertyComponentType::Int64)
      .value("Uint64", CesiumGltf::PropertyComponentType::Uint64)
      .value("Float32", CesiumGltf::PropertyComponentType::Float32)
      .value("Float64", CesiumGltf::PropertyComponentType::Float64);

  py::enum_<CesiumGltf::PropertyTableViewStatus>(m, "PropertyTableViewStatus")
      .value("Valid", CesiumGltf::PropertyTableViewStatus::Valid)
      .value(
          "ErrorMissingMetadataExtension",
          CesiumGltf::PropertyTableViewStatus::ErrorMissingMetadataExtension)
      .value(
          "ErrorMissingSchema",
          CesiumGltf::PropertyTableViewStatus::ErrorMissingSchema)
      .value(
          "ErrorClassNotFound",
          CesiumGltf::PropertyTableViewStatus::ErrorClassNotFound);

  py::class_<PropertyColumnAccessor>(m, "PropertyColumnAccessor")
      .def_readonly(
          "status",
          &PropertyColumnAccessor::status,
          "Property view status (0 = Valid)")
      .def_readonly("size", &PropertyColumnAccessor::size)
      .def(
          "__array__",
          [](const PropertyColumnAccessor& self,
             py::object /*dtype*/,
             bool /*copy*/) {
            if (!self.asNumpyFn)
              throw py::value_error("Property accessor is not valid");
            return self.asNumpyFn();
          },
          py::arg("dtype") = py::none(),
          py::arg("copy") = false)
      .def(
          "__len__",
          [](const PropertyColumnAccessor& self) { return self.size; })
      .def(
          "__getitem__",
          [](const PropertyColumnAccessor& self, int64_t index) {
            if (!self.getFn)
              throw py::value_error("Property accessor is not valid");
            if (index < 0)
              index += self.size;
            if (index < 0 || index >= self.size)
              throw py::index_error("Index out of range");
            return self.getFn(index);
          })
      .def(
          "__iter__",
          [](const PropertyColumnAccessor& self) {
            if (!self.getFn)
              throw py::value_error("Property accessor is not valid");
            py::list items;
            for (int64_t i = 0; i < self.size; ++i) {
              items.append(self.getFn(i));
            }
            return py::iter(items);
          })
      .def(
          "__bool__",
          [](const PropertyColumnAccessor& self) { return self.status == 0; })
      .def("__repr__", [](const PropertyColumnAccessor& self) {
        return "<PropertyColumnAccessor status=" + std::to_string(self.status) +
               " size=" + std::to_string(self.size) + ">";
      });

  py::class_<CesiumGltf::PropertyTableView>(m, "PropertyTableView")
      .def(
          py::init<
              const CesiumGltf::Model&,
              const CesiumGltf::PropertyTable&>(),
          py::arg("model"),
          py::arg("property_table"),
          py::keep_alive<1, 2>(),
          py::keep_alive<1, 3>())
      .def_property_readonly("status", &CesiumGltf::PropertyTableView::status)
      .def_property_readonly("name", &CesiumGltf::PropertyTableView::name)
      .def_property_readonly("size", &CesiumGltf::PropertyTableView::size)
      .def(
          "get_class_property",
          &CesiumGltf::PropertyTableView::getClassProperty,
          py::arg("property_id"),
          py::return_value_policy::reference_internal)
      .def("__len__", &CesiumGltf::PropertyTableView::size)
      .def(
          "__getitem__",
          [](const CesiumGltf::PropertyTableView& self,
             const std::string& propertyId) {
            auto accessor = makeColumnAccessorFromView(self, propertyId);
            if (accessor.status != 0)
              throw py::key_error(propertyId);
            return accessor;
          },
          py::arg("property_id"))
      .def(
          "__contains__",
          [](const CesiumGltf::PropertyTableView& self,
             const std::string& propertyId) {
            const auto* pClass = self.getClass();
            if (!pClass)
              return false;
            return pClass->properties.count(propertyId) > 0;
          },
          py::arg("property_id"))
      .def(
          "__iter__",
          [](const CesiumGltf::PropertyTableView& self) {
            py::list ids;
            const auto* pClass = self.getClass();
            if (pClass) {
              for (const auto& [id, _] : pClass->properties) {
                ids.append(py::str(id));
              }
            }
            return py::iter(ids);
          })
      .def(
          "keys",
          [](const CesiumGltf::PropertyTableView& self) {
            py::list ids;
            const auto* pClass = self.getClass();
            if (pClass) {
              for (const auto& [id, _] : pClass->properties) {
                ids.append(py::str(id));
              }
            }
            return ids;
          })
      .def(
          "values",
          [](const CesiumGltf::PropertyTableView& self) {
            py::list vals;
            const auto* pClass = self.getClass();
            if (pClass) {
              for (const auto& [id, _] : pClass->properties) {
                auto accessor = makeColumnAccessorFromView(self, id);
                if (accessor.status == 0 && accessor.asNumpyFn) {
                  vals.append(accessor.asNumpyFn());
                }
              }
            }
            return vals;
          })
      .def(
          "items",
          [](const CesiumGltf::PropertyTableView& self) {
            py::list pairs;
            const auto* pClass = self.getClass();
            if (pClass) {
              for (const auto& [id, _] : pClass->properties) {
                auto accessor = makeColumnAccessorFromView(self, id);
                if (accessor.status == 0 && accessor.asNumpyFn) {
                  pairs.append(
                      py::make_tuple(py::str(id), accessor.asNumpyFn()));
                }
              }
            }
            return pairs;
          })
      .def(
          "__bool__",
          [](const CesiumGltf::PropertyTableView& self) {
            return self.status() == CesiumGltf::PropertyTableViewStatus::Valid;
          })
      .def("__repr__", [](const CesiumGltf::PropertyTableView& self) {
        auto name = self.name();
        return "<PropertyTableView name='" +
               (name ? *name : std::string("(unnamed)")) +
               "' size=" + std::to_string(self.size()) + ">";
      });

  py::enum_<CesiumGltf::PropertyTextureViewStatus>(
      m,
      "PropertyTextureViewStatus")
      .value("Valid", CesiumGltf::PropertyTextureViewStatus::Valid)
      .value(
          "ErrorMissingMetadataExtension",
          CesiumGltf::PropertyTextureViewStatus::ErrorMissingMetadataExtension)
      .value(
          "ErrorMissingSchema",
          CesiumGltf::PropertyTextureViewStatus::ErrorMissingSchema)
      .value(
          "ErrorClassNotFound",
          CesiumGltf::PropertyTextureViewStatus::ErrorClassNotFound);

  using TexturePropertyAccessor = CesiumPropertyView::TexturePropertyAccessor;
  py::class_<TexturePropertyAccessor>(m, "TexturePropertyAccessor")
      .def_readonly("status", &TexturePropertyAccessor::status)
      .def(
          "get",
          [](const TexturePropertyAccessor& self, double u, double v) {
            if (!self.getFn)
              return py::object(py::none());
            return self.getFn(u, v);
          },
          py::arg("u"),
          py::arg("v"))
      .def(
          "get_raw",
          [](const TexturePropertyAccessor& self, double u, double v) {
            if (!self.getRawFn)
              return py::object(py::none());
            return self.getRawFn(u, v);
          },
          py::arg("u"),
          py::arg("v"))
      .def(
          "__bool__",
          [](const TexturePropertyAccessor& self) { return self.status == 0; })
      .def("__repr__", [](const TexturePropertyAccessor& self) {
        return "<TexturePropertyAccessor status=" +
               std::to_string(self.status) + ">";
      });

  py::class_<CesiumGltf::PropertyTextureView>(m, "PropertyTextureView")
      .def(
          py::init<
              const CesiumGltf::Model&,
              const CesiumGltf::PropertyTexture&>(),
          py::arg("model"),
          py::arg("property_texture"),
          py::keep_alive<1, 2>(),
          py::keep_alive<1, 3>())
      .def_property_readonly("status", &CesiumGltf::PropertyTextureView::status)
      .def_property_readonly("name", &CesiumGltf::PropertyTextureView::name)
      .def(
          "get_class_property",
          &CesiumGltf::PropertyTextureView::getClassProperty,
          py::arg("property_id"),
          py::return_value_policy::reference_internal)
      .def(
          "__contains__",
          [](const CesiumGltf::PropertyTextureView& self,
             const std::string& propertyId) {
            const auto* pClass = self.getClass();
            if (!pClass)
              return false;
            return pClass->properties.count(propertyId) > 0;
          },
          py::arg("property_id"))
      .def(
          "__iter__",
          [](const CesiumGltf::PropertyTextureView& self) {
            py::list ids;
            const auto* pClass = self.getClass();
            if (pClass) {
              for (const auto& [id, _] : pClass->properties) {
                ids.append(py::str(id));
              }
            }
            return py::iter(ids);
          })
      .def(
          "__bool__",
          [](const CesiumGltf::PropertyTextureView& self) {
            return self.status() ==
                   CesiumGltf::PropertyTextureViewStatus::Valid;
          })
      .def(
          "__repr__",
          [](const CesiumGltf::PropertyTextureView& self) {
            auto name = self.name();
            return "<PropertyTextureView name='" +
                   (name ? *name : std::string("(unnamed)")) + "'>";
          })
      .def(
          "get_property_view",
          [](const CesiumGltf::PropertyTextureView& self,
             const std::string& propertyId) {
            return CesiumPropertyView::makeTexturePropertyAccessor(
                self,
                propertyId);
          },
          py::arg("property_id"));

  py::enum_<CesiumGltf::PropertyAttributeViewStatus>(
      m,
      "PropertyAttributeViewStatus")
      .value("Valid", CesiumGltf::PropertyAttributeViewStatus::Valid)
      .value(
          "ErrorMissingMetadataExtension",
          CesiumGltf::PropertyAttributeViewStatus::
              ErrorMissingMetadataExtension)
      .value(
          "ErrorMissingSchema",
          CesiumGltf::PropertyAttributeViewStatus::ErrorMissingSchema)
      .value(
          "ErrorClassNotFound",
          CesiumGltf::PropertyAttributeViewStatus::ErrorClassNotFound);

  using AttributePropertyAccessor =
      CesiumPropertyView::AttributePropertyAccessor;
  py::class_<AttributePropertyAccessor>(m, "AttributePropertyAccessor")
      .def_readonly("status", &AttributePropertyAccessor::status)
      .def_readonly("size", &AttributePropertyAccessor::size)
      .def(
          "get",
          [](const AttributePropertyAccessor& self, int64_t index) {
            if (!self.getFn)
              return py::object(py::none());
            return self.getFn(index);
          },
          py::arg("index"))
      .def(
          "get_raw",
          [](const AttributePropertyAccessor& self, int64_t index) {
            if (!self.getRawFn)
              return py::object(py::none());
            return self.getRawFn(index);
          },
          py::arg("index"))
      .def(
          "__len__",
          [](const AttributePropertyAccessor& self) { return self.size; })
      .def(
          "__bool__",
          [](const AttributePropertyAccessor& self) {
            return self.status == 0;
          })
      .def("__repr__", [](const AttributePropertyAccessor& self) {
        return "<AttributePropertyAccessor status=" +
               std::to_string(self.status) +
               " size=" + std::to_string(self.size) + ">";
      });

  py::class_<CesiumGltf::PropertyAttributeView>(m, "PropertyAttributeView")
      .def(
          py::init<
              const CesiumGltf::Model&,
              const CesiumGltf::PropertyAttribute&>(),
          py::arg("model"),
          py::arg("property_attribute"),
          py::keep_alive<1, 2>(),
          py::keep_alive<1, 3>())
      .def_property_readonly(
          "status",
          &CesiumGltf::PropertyAttributeView::status)
      .def_property_readonly("name", &CesiumGltf::PropertyAttributeView::name)
      .def(
          "get_class_property",
          &CesiumGltf::PropertyAttributeView::getClassProperty,
          py::arg("property_id"),
          py::return_value_policy::reference_internal)
      .def(
          "__contains__",
          [](const CesiumGltf::PropertyAttributeView& self,
             const std::string& propertyId) {
            const auto* pClass = self.getClass();
            if (!pClass)
              return false;
            return pClass->properties.count(propertyId) > 0;
          },
          py::arg("property_id"))
      .def(
          "__iter__",
          [](const CesiumGltf::PropertyAttributeView& self) {
            py::list ids;
            const auto* pClass = self.getClass();
            if (pClass) {
              for (const auto& [id, _] : pClass->properties) {
                ids.append(py::str(id));
              }
            }
            return py::iter(ids);
          })
      .def(
          "__bool__",
          [](const CesiumGltf::PropertyAttributeView& self) {
            return self.status() ==
                   CesiumGltf::PropertyAttributeViewStatus::Valid;
          })
      .def(
          "__repr__",
          [](const CesiumGltf::PropertyAttributeView& self) {
            auto name = self.name();
            return "<PropertyAttributeView name='" +
                   (name ? *name : std::string("(unnamed)")) + "'>";
          })
      .def(
          "get_property_view",
          [](const CesiumGltf::PropertyAttributeView& self,
             const CesiumGltf::MeshPrimitive& primitive,
             const std::string& propertyId) {
            return CesiumPropertyView::makeAttributePropertyAccessor(
                self,
                primitive,
                propertyId);
          },
          py::arg("primitive"),
          py::arg("property_id"));

  py::enum_<CesiumGltf::FeatureIdTextureViewStatus>(
      m,
      "FeatureIdTextureViewStatus")
      .value("Valid", CesiumGltf::FeatureIdTextureViewStatus::Valid)
      .value(
          "ErrorUninitialized",
          CesiumGltf::FeatureIdTextureViewStatus::ErrorUninitialized)
      .value(
          "ErrorInvalidTexture",
          CesiumGltf::FeatureIdTextureViewStatus::ErrorInvalidTexture)
      .value(
          "ErrorInvalidImage",
          CesiumGltf::FeatureIdTextureViewStatus::ErrorInvalidImage)
      .value(
          "ErrorInvalidSampler",
          CesiumGltf::FeatureIdTextureViewStatus::ErrorInvalidSampler)
      .value(
          "ErrorEmptyImage",
          CesiumGltf::FeatureIdTextureViewStatus::ErrorEmptyImage)
      .value(
          "ErrorInvalidImageBytesPerChannel",
          CesiumGltf::FeatureIdTextureViewStatus::
              ErrorInvalidImageBytesPerChannel)
      .value(
          "ErrorInvalidChannels",
          CesiumGltf::FeatureIdTextureViewStatus::ErrorInvalidChannels);

  py::class_<CesiumGltf::FeatureIdTextureView>(m, "FeatureIdTextureView")
      .def(
          py::init<
              const CesiumGltf::Model&,
              const CesiumGltf::FeatureIdTexture&>(),
          py::arg("model"),
          py::arg("feature_id_texture"),
          py::keep_alive<1, 2>(),
          py::keep_alive<1, 3>())
      .def_property_readonly(
          "status",
          &CesiumGltf::FeatureIdTextureView::status)
      .def_property_readonly(
          "channels",
          &CesiumGltf::FeatureIdTextureView::getChannels)
      .def(
          "__call__",
          &CesiumGltf::FeatureIdTextureView::getFeatureID,
          py::arg("u"),
          py::arg("v"),
          "Get the feature ID at the given texture coordinates.")
      .def(
          "__bool__",
          [](const CesiumGltf::FeatureIdTextureView& self) {
            return self.status() ==
                   CesiumGltf::FeatureIdTextureViewStatus::Valid;
          })
      .def("__repr__", [](const CesiumGltf::FeatureIdTextureView& self) {
        return "<FeatureIdTextureView status=" +
               std::to_string(static_cast<int>(self.status())) + ">";
      });

  py::class_<CesiumGltf::InstanceAttributeSemantics>(
      m,
      "InstanceAttributeSemantics")
      .def_property_readonly_static(
          "TRANSLATION",
          [](py::object) {
            return CesiumGltf::InstanceAttributeSemantics::TRANSLATION;
          })
      .def_property_readonly_static(
          "ROTATION",
          [](py::object) {
            return CesiumGltf::InstanceAttributeSemantics::ROTATION;
          })
      .def_property_readonly_static(
          "SCALE",
          [](py::object) {
            return CesiumGltf::InstanceAttributeSemantics::SCALE;
          })
      .def_property_readonly_static(
          "FEATURE_ID_n",
          [](py::object) -> std::array<std::string, 8> {
            return CesiumGltf::InstanceAttributeSemantics::FEATURE_ID_n;
          });

  py::enum_<CesiumGltf::KhrTextureTransformStatus>(
      m,
      "KhrTextureTransformStatus")
      .value("VALID", CesiumGltf::KhrTextureTransformStatus::Valid)
      .value(
          "ERROR_INVALID_OFFSET",
          CesiumGltf::KhrTextureTransformStatus::ErrorInvalidOffset)
      .value(
          "ERROR_INVALID_SCALE",
          CesiumGltf::KhrTextureTransformStatus::ErrorInvalidScale);

  py::class_<CesiumGltf::KhrTextureTransform>(m, "KhrTextureTransform")
      .def(py::init<>())
      .def_property_readonly("status", &CesiumGltf::KhrTextureTransform::status)
      .def_property_readonly(
          "offset",
          [](const CesiumGltf::KhrTextureTransform& self) {
            return std::array<double, 2>{self.offset().x, self.offset().y};
          })
      .def_property_readonly(
          "rotation",
          &CesiumGltf::KhrTextureTransform::rotation)
      .def_property_readonly(
          "rotation_sine_cosine",
          [](const CesiumGltf::KhrTextureTransform& self) {
            return std::array<double, 2>{
                self.rotationSineCosine().x,
                self.rotationSineCosine().y};
          })
      .def_property_readonly(
          "scale",
          [](const CesiumGltf::KhrTextureTransform& self) {
            return std::array<double, 2>{self.scale().x, self.scale().y};
          })
      .def(
          "apply_transform",
          [](const CesiumGltf::KhrTextureTransform& self, double u, double v) {
            auto r = self.applyTransform(u, v);
            return std::array<double, 2>{r.x, r.y};
          },
          py::arg("u"),
          py::arg("v"))
      .def_property_readonly(
          "tex_coord_set_index",
          &CesiumGltf::KhrTextureTransform::getTexCoordSetIndex)
      .def(
          "__bool__",
          [](const CesiumGltf::KhrTextureTransform& self) {
            return self.status() ==
                   CesiumGltf::KhrTextureTransformStatus::Valid;
          })
      .def("__repr__", [](const CesiumGltf::KhrTextureTransform& self) {
        return "<KhrTextureTransform status=" +
               std::to_string(static_cast<int>(self.status())) + ">";
      });

  py::enum_<CesiumGltf::TextureViewStatus>(m, "TextureViewStatus")
      .value("VALID", CesiumGltf::TextureViewStatus::Valid)
      .value(
          "ERROR_UNINITIALIZED",
          CesiumGltf::TextureViewStatus::ErrorUninitialized)
      .value(
          "ERROR_INVALID_TEXTURE",
          CesiumGltf::TextureViewStatus::ErrorInvalidTexture)
      .value(
          "ERROR_INVALID_SAMPLER",
          CesiumGltf::TextureViewStatus::ErrorInvalidSampler)
      .value(
          "ERROR_INVALID_IMAGE",
          CesiumGltf::TextureViewStatus::ErrorInvalidImage)
      .value(
          "ERROR_EMPTY_IMAGE",
          CesiumGltf::TextureViewStatus::ErrorEmptyImage)
      .value(
          "ERROR_INVALID_BYTES_PER_CHANNEL",
          CesiumGltf::TextureViewStatus::ErrorInvalidBytesPerChannel);

  py::class_<CesiumGltf::TextureViewOptions>(m, "TextureViewOptions")
      .def(py::init<>())
      .def_readwrite(
          "apply_khr_texture_transform_extension",
          &CesiumGltf::TextureViewOptions::applyKhrTextureTransformExtension)
      .def_readwrite(
          "make_image_copy",
          &CesiumGltf::TextureViewOptions::makeImageCopy);

  py::class_<CesiumGltf::TextureView>(m, "TextureView")
      .def(
          py::init<
              const CesiumGltf::Model&,
              const CesiumGltf::TextureInfo&,
              const CesiumGltf::TextureViewOptions&>(),
          py::arg("model"),
          py::arg("texture_info"),
          py::arg("options") = CesiumGltf::TextureViewOptions{},
          py::keep_alive<1, 2>())
      .def_property_readonly(
          "status",
          &CesiumGltf::TextureView::getTextureViewStatus)
      .def_property_readonly(
          "tex_coord_set_index",
          &CesiumGltf::TextureView::getTexCoordSetIndex)
      .def_property_readonly(
          "image",
          &CesiumGltf::TextureView::getImage,
          py::return_value_policy::reference_internal)
      .def_property_readonly(
          "texture_transform",
          &CesiumGltf::TextureView::getTextureTransform)
      .def(
          "sample_nearest_pixel",
          &CesiumGltf::TextureView::sampleNearestPixel,
          py::arg("u"),
          py::arg("v"),
          py::arg("channels"),
          "Sample the texture at (u,v) using nearest-pixel filtering.\n"
          "channels specifies which image channels to return and in what order.")
      .def(
          "__bool__",
          [](const CesiumGltf::TextureView& self) {
            return self.getTextureViewStatus() ==
                   CesiumGltf::TextureViewStatus::Valid;
          })
      .def("__repr__", [](const CesiumGltf::TextureView& self) {
        return "<TextureView status=" +
               std::to_string(
                   static_cast<int>(self.getTextureViewStatus())) +
               ">";
      });

  py::enum_<CesiumGltf::AccessorViewStatus>(m, "AccessorViewStatus")
      .value("VALID", CesiumGltf::AccessorViewStatus::Valid)
      .value(
          "INVALID_ACCESSOR_INDEX",
          CesiumGltf::AccessorViewStatus::InvalidAccessorIndex)
      .value(
          "INVALID_BUFFER_VIEW_INDEX",
          CesiumGltf::AccessorViewStatus::InvalidBufferViewIndex)
      .value(
          "INVALID_BUFFER_INDEX",
          CesiumGltf::AccessorViewStatus::InvalidBufferIndex)
      .value(
          "BUFFER_VIEW_TOO_SMALL",
          CesiumGltf::AccessorViewStatus::BufferViewTooSmall)
      .value("BUFFER_TOO_SMALL", CesiumGltf::AccessorViewStatus::BufferTooSmall)
      .value("WRONG_SIZE_T", CesiumGltf::AccessorViewStatus::WrongSizeT)
      .value("INVALID_TYPE", CesiumGltf::AccessorViewStatus::InvalidType)
      .value(
          "INVALID_COMPONENT_TYPE",
          CesiumGltf::AccessorViewStatus::InvalidComponentType)
      .value(
          "INVALID_BYTE_STRIDE",
          CesiumGltf::AccessorViewStatus::InvalidByteStride);


  auto bindAccessorView = [&]<typename T>(const char* name) {
    using Scalar = typename ScalarOfT<T>::type;
    constexpr py::ssize_t NC = NumComponents<T>::value;
    static_assert(NC > 0, "Unsupported AccessorView element type");

    py::class_<CesiumGltf::AccessorView<T>>(m, name)
        .def(
            py::init<const CesiumGltf::Model&, int32_t>(),
            py::arg("model"),
            py::arg("accessor_index"),
            py::keep_alive<1, 2>())
        .def_property_readonly("status", &CesiumGltf::AccessorView<T>::status)
        .def("__len__", &CesiumGltf::AccessorView<T>::size)
        .def(
            "__getitem__",
            [](const CesiumGltf::AccessorView<T>& self, int64_t i) -> T {
              if (i < 0)
                i += self.size();
              if (i < 0 || i >= self.size())
                throw py::index_error();
              return self[i];
            },
            py::arg("index"))
        .def(
            "__array__",
            [](py::object selfObj,
               py::object dtype,
               bool copy) -> py::object {
              constexpr py::ssize_t nc = NumComponents<T>::value;
              auto& self =
                  selfObj.cast<const CesiumGltf::AccessorView<T>&>();
              const auto n = static_cast<py::ssize_t>(self.size());
              if (n == 0) {
                if constexpr (nc == 1)
                  return py::object(py::array_t<Scalar>(0));
                else if constexpr (std::is_same_v<T, glm::mat4>)
                  return py::object(py::array_t<Scalar>(
                      {py::ssize_t{0}, py::ssize_t{4}, py::ssize_t{4}}));
                else
                  return py::object(
                      py::array_t<Scalar>({py::ssize_t{0}, nc}));
              }

              // Zero-copy path: build a numpy array that views
              // directly into the glTF buffer memory.
              const auto* ptr =
                  reinterpret_cast<const Scalar*>(self.data());
              const auto byteStride =
                  static_cast<py::ssize_t>(self.stride());
              const auto compBytes =
                  static_cast<py::ssize_t>(sizeof(Scalar));

              py::object view;
              if constexpr (nc == 1) {
                // Scalar: shape (N,), stride = byteStride
                view = py::object(py::array_t<Scalar>(
                    {n},             // shape
                    {byteStride},    // strides (bytes)
                    ptr,             // data pointer
                    selfObj));       // base object (prevent GC)
              } else if constexpr (std::is_same_v<T, glm::mat4>) {
                // Mat4: shape (N, 4, 4) — column-major, matching glm
                // layout: arr[i][col][row].
                view = py::object(py::array_t<Scalar>(
                    {n, py::ssize_t{4}, py::ssize_t{4}},
                    {byteStride, py::ssize_t{4} * compBytes, compBytes},
                    ptr,
                    selfObj));
              } else {
                // Vector: shape (N, nc), strides = (byteStride,
                // sizeof(Scalar)) — components within one element
                // are always contiguous per glTF spec.
                view = py::object(py::array_t<Scalar>(
                    {n, nc},                  // shape
                    {byteStride, compBytes},  // strides
                    ptr,                      // data pointer
                    selfObj));                // base object
              }

              // If dtype requested and differs, cast (implies copy).
              if (!dtype.is_none()) {
                auto np = py::module_::import("numpy");
                auto target = np.attr("dtype")(dtype);
                if (!target.equal(view.attr("dtype")))
                  return view.attr("astype")(target);
              }

              if (copy) {
                return view.attr("copy")();
              }
              return view;
            },
            py::arg("dtype") = py::none(),
            py::arg("copy") = false,
            "Convert to numpy array (zero-copy view into glTF buffer). "
            "Enables np.asarray(accessor_view).")
        .def(
            "__bool__",
            [](const CesiumGltf::AccessorView<T>& self) {
              return self.status() == CesiumGltf::AccessorViewStatus::Valid;
            })
        .def("__repr__", [name](const CesiumGltf::AccessorView<T>& self) {
          return std::string("<") + name +
                 " size=" + std::to_string(self.size()) +
                 " status=" + std::to_string(static_cast<int>(self.status())) +
                 ">";
        });
  };

  bindAccessorView.operator()<float>("AccessorViewFloat");
  bindAccessorView.operator()<double>("AccessorViewDouble");
  bindAccessorView.operator()<int8_t>("AccessorViewInt8");
  bindAccessorView.operator()<uint8_t>("AccessorViewUInt8");
  bindAccessorView.operator()<int16_t>("AccessorViewInt16");
  bindAccessorView.operator()<uint16_t>("AccessorViewUInt16");
  bindAccessorView.operator()<int32_t>("AccessorViewInt32");
  bindAccessorView.operator()<uint32_t>("AccessorViewUInt32");
  bindAccessorView.operator()<glm::vec2>("AccessorViewVec2");
  bindAccessorView.operator()<glm::vec3>("AccessorViewVec3");
  bindAccessorView.operator()<glm::vec4>("AccessorViewVec4");
  bindAccessorView.operator()<glm::dvec2>("AccessorViewDVec2");
  bindAccessorView.operator()<glm::dvec3>("AccessorViewDVec3");
  bindAccessorView.operator()<glm::dvec4>("AccessorViewDVec4");
  bindAccessorView.operator()<glm::mat4>("AccessorViewMat4");

  // ── AccessorView factory ──────────────────────────────────────────────
  // ``AccessorView(model, accessor_index)`` auto-detects componentType +
  // type and returns the correctly-typed AccessorView<T>.
  m.def(
      "AccessorView",
      [](const CesiumGltf::Model& model,
         int32_t accessorIndex) -> py::object {
        using CT = CesiumGltf::Accessor::ComponentType;
        using AT = CesiumGltf::Accessor::Type;

        const auto* acc =
            CesiumGltf::Model::getSafe(&model.accessors, accessorIndex);
        if (!acc)
          throw py::value_error("invalid accessor index");

        const auto& type = acc->type;
        const int32_t ct = acc->componentType;

        // Macro to reduce boilerplate: construct the right AccessorView<T>,
        // validate its status, and return it as a py::object.
#define RETURN_VIEW(T)                                                    \
  do {                                                                    \
    auto view = CesiumGltf::AccessorView<T>(model, accessorIndex);        \
    if (view.status() != CesiumGltf::AccessorViewStatus::Valid) {         \
      throw py::value_error(                                              \
          "AccessorView failed for accessor " +                           \
          std::to_string(accessorIndex) + ": status=" +                   \
          std::to_string(static_cast<int>(view.status())));               \
    }                                                                     \
    return py::cast(std::move(view), py::return_value_policy::move);      \
  } while (0)

        if (type == AT::SCALAR) {
          switch (ct) {
          case CT::BYTE:
            RETURN_VIEW(int8_t);
          case CT::UNSIGNED_BYTE:
            RETURN_VIEW(uint8_t);
          case CT::SHORT:
            RETURN_VIEW(int16_t);
          case CT::UNSIGNED_SHORT:
            RETURN_VIEW(uint16_t);
          case CT::INT:
            RETURN_VIEW(int32_t);
          case CT::UNSIGNED_INT:
            RETURN_VIEW(uint32_t);
          case CT::FLOAT:
            RETURN_VIEW(float);
          case CT::DOUBLE:
            RETURN_VIEW(double);
          default:
            break;
          }
        } else if (type == AT::VEC2) {
          if (ct == CT::FLOAT) RETURN_VIEW(glm::vec2);
          if (ct == CT::DOUBLE) RETURN_VIEW(glm::dvec2);
        } else if (type == AT::VEC3) {
          if (ct == CT::FLOAT) RETURN_VIEW(glm::vec3);
          if (ct == CT::DOUBLE) RETURN_VIEW(glm::dvec3);
        } else if (type == AT::VEC4) {
          if (ct == CT::FLOAT) RETURN_VIEW(glm::vec4);
          if (ct == CT::DOUBLE) RETURN_VIEW(glm::dvec4);
        } else if (type == AT::MAT4) {
          if (ct == CT::FLOAT) RETURN_VIEW(glm::mat4);
        }

#undef RETURN_VIEW

        throw py::type_error(
            "Unsupported accessor type/componentType combination: type=" +
            type + " componentType=" + std::to_string(ct));
      },
      py::arg("model"),
      py::arg("accessor_index"),
      py::keep_alive<0, 1>(),
      "Create a typed AccessorView from a model and accessor index.\n\n"
      "Auto-detects the accessor's type and componentType and returns\n"
      "the matching AccessorView (e.g. AccessorViewVec3 for VEC3/FLOAT).\n"
      "Use ``np.array(view)`` for a zero-copy numpy array.");

  auto bindAccessorWriter = [&]<typename T>(const char* name) {
    py::class_<CesiumGltf::AccessorWriter<T>>(m, name)
        .def(
            py::init<CesiumGltf::Model&, int32_t>(),
            py::arg("model"),
            py::arg("accessor_index"),
            py::keep_alive<1, 2>())
        .def_property_readonly("status", &CesiumGltf::AccessorWriter<T>::status)
        .def("__len__", &CesiumGltf::AccessorWriter<T>::size)
        .def(
            "__getitem__",
            [](const CesiumGltf::AccessorWriter<T>& self, int64_t i) -> T {
              if (i < 0)
                i += self.size();
              if (i < 0 || i >= self.size())
                throw py::index_error();
              return self[i];
            },
            py::arg("index"))
        .def(
            "__setitem__",
            [](CesiumGltf::AccessorWriter<T>& self, int64_t i, const T& value) {
              if (i < 0)
                i += self.size();
              if (i < 0 || i >= self.size())
                throw py::index_error();
              self[i] = value;
            },
            py::arg("index"),
            py::arg("value"))
        .def(
            "set_from_array",
            [](CesiumGltf::AccessorWriter<T>& self, py::array arr) {
              const auto n = self.size();
              if constexpr (std::is_arithmetic_v<T>) {
                auto in = arr.cast<py::array_t<T>>().unchecked<1>();
                if (in.shape(0) != n)
                  throw py::value_error(
                      "Array length " + std::to_string(in.shape(0)) +
                      " != accessor size " + std::to_string(n));
                for (int64_t i = 0; i < n; ++i)
                  self[i] = in(i);
              } else if constexpr (
                  std::is_same_v<T, glm::vec2> ||
                  std::is_same_v<T, glm::dvec2>) {
                using S = typename T::value_type;
                auto in = arr.cast<py::array_t<S>>().unchecked<2>();
                if (in.shape(0) != n || in.shape(1) != 2)
                  throw py::value_error(
                      "Expected (" + std::to_string(n) + ", 2) array");
                for (int64_t i = 0; i < n; ++i)
                  self[i] = T(in(i, 0), in(i, 1));
              } else if constexpr (
                  std::is_same_v<T, glm::vec3> ||
                  std::is_same_v<T, glm::dvec3>) {
                using S = typename T::value_type;
                auto in = arr.cast<py::array_t<S>>().unchecked<2>();
                if (in.shape(0) != n || in.shape(1) != 3)
                  throw py::value_error(
                      "Expected (" + std::to_string(n) + ", 3) array");
                for (int64_t i = 0; i < n; ++i)
                  self[i] = T(in(i, 0), in(i, 1), in(i, 2));
              } else if constexpr (
                  std::is_same_v<T, glm::vec4> ||
                  std::is_same_v<T, glm::dvec4>) {
                using S = typename T::value_type;
                auto in = arr.cast<py::array_t<S>>().unchecked<2>();
                if (in.shape(0) != n || in.shape(1) != 4)
                  throw py::value_error(
                      "Expected (" + std::to_string(n) + ", 4) array");
                for (int64_t i = 0; i < n; ++i)
                  self[i] = T(in(i, 0), in(i, 1), in(i, 2), in(i, 3));
              } else if constexpr (std::is_same_v<T, glm::mat4>) {
                auto in = arr.cast<py::array_t<float>>().unchecked<3>();
                if (in.shape(0) != n || in.shape(1) != 4 || in.shape(2) != 4)
                  throw py::value_error(
                      "Expected (" + std::to_string(n) + ", 4, 4) array");
                for (int64_t i = 0; i < n; ++i) {
                  glm::mat4 mat;
                  for (int c = 0; c < 4; ++c)
                    for (int r = 0; r < 4; ++r)
                      mat[c][r] = in(i, r, c);
                  self[i] = mat;
                }
              } else {
                throw py::type_error(
                    "Unsupported element type for set_from_array");
              }
            },
            py::arg("array"),
            "Bulk-write from a numpy array matching this accessor's shape.")
        .def(
            "__bool__",
            [](const CesiumGltf::AccessorWriter<T>& self) {
              return self.status() == CesiumGltf::AccessorViewStatus::Valid;
            })
        .def("__repr__", [name](const CesiumGltf::AccessorWriter<T>& self) {
          return std::string("<") + name +
                 " size=" + std::to_string(self.size()) +
                 " status=" + std::to_string(static_cast<int>(self.status())) +
                 ">";
        });
  };

  bindAccessorWriter.operator()<float>("AccessorWriterFloat");
  bindAccessorWriter.operator()<double>("AccessorWriterDouble");
  bindAccessorWriter.operator()<int8_t>("AccessorWriterInt8");
  bindAccessorWriter.operator()<uint8_t>("AccessorWriterUInt8");
  bindAccessorWriter.operator()<int16_t>("AccessorWriterInt16");
  bindAccessorWriter.operator()<uint16_t>("AccessorWriterUInt16");
  bindAccessorWriter.operator()<int32_t>("AccessorWriterInt32");
  bindAccessorWriter.operator()<uint32_t>("AccessorWriterUInt32");
  bindAccessorWriter.operator()<glm::vec2>("AccessorWriterVec2");
  bindAccessorWriter.operator()<glm::vec3>("AccessorWriterVec3");
  bindAccessorWriter.operator()<glm::vec4>("AccessorWriterVec4");
  bindAccessorWriter.operator()<glm::dvec2>("AccessorWriterDVec2");
  bindAccessorWriter.operator()<glm::dvec3>("AccessorWriterDVec3");
  bindAccessorWriter.operator()<glm::dvec4>("AccessorWriterDVec4");
  bindAccessorWriter.operator()<glm::mat4>("AccessorWriterMat4");


  // Helper: convert AccessorView<AccessorTypes::VEC<N><T>> to numpy
  // (N_elements, N_components)
  auto vecAccessorToNumpy = [](auto&& view, int components) -> py::object {
    const auto n = static_cast<py::ssize_t>(view.size());
    if (view.status() != CesiumGltf::AccessorViewStatus::Valid)
      return py::none();
    using ElemT = std::decay_t<decltype(view[0])>;
    using CompT = std::decay_t<decltype(view[0].value[0])>;
    py::array_t<CompT> result({n, static_cast<py::ssize_t>(components)});
    auto out = result.mutable_unchecked<2>();
    for (py::ssize_t i = 0; i < n; ++i)
      for (int c = 0; c < components; ++c)
        out(i, c) = view[i].value[c];
    return py::object(std::move(result));
  };

  m.def(
      "get_position_accessor_view",
      [vecAccessorToNumpy](
          const CesiumGltf::Model& model,
          const CesiumGltf::MeshPrimitive& primitive) {
        auto view = CesiumGltf::getPositionAccessorView(model, primitive);
        return vecAccessorToNumpy(view, 3);
      },
      py::arg("model"),
      py::arg("primitive"),
      "Get the position accessor as an (N,3) float32 numpy array, or None if "
      "invalid.");

  m.def(
      "get_normal_accessor_view",
      [vecAccessorToNumpy](
          const CesiumGltf::Model& model,
          const CesiumGltf::MeshPrimitive& primitive) {
        auto view = CesiumGltf::getNormalAccessorView(model, primitive);
        return vecAccessorToNumpy(view, 3);
      },
      py::arg("model"),
      py::arg("primitive"),
      "Get the normal accessor as an (N,3) float32 numpy array, or None if "
      "invalid.");

  m.def(
      "get_index_accessor_view",
      [](const CesiumGltf::Model& model,
         const CesiumGltf::MeshPrimitive& primitive) -> py::object {
        auto variant = CesiumGltf::getIndexAccessorView(model, primitive);
        return std::visit(
            [](auto&& v) -> py::object {
              using V = std::decay_t<decltype(v)>;
              if constexpr (std::is_same_v<V, std::monostate>)
                return py::none();
              else
                return py::cast(v);
            },
            variant);
      },
      py::arg("model"),
      py::arg("primitive"),
      "Get the index accessor view, or None if the primitive has no indices.");

  m.def(
      "get_feature_id_accessor_view",
      [](const CesiumGltf::Model& model,
         const CesiumGltf::MeshPrimitive& primitive,
         int32_t featureIdAttributeIndex) -> py::object {
        auto variant = CesiumGltf::getFeatureIdAccessorView(
            model,
            primitive,
            featureIdAttributeIndex);
        return std::visit(
            [](auto&& v) -> py::object { return py::cast(v); },
            variant);
      },
      py::arg("model"),
      py::arg("primitive"),
      py::arg("feature_id_attribute_index"),
      "Get a feature ID accessor view for the given attribute index.");

  m.def(
      "get_tex_coord_accessor_view",
      [vecAccessorToNumpy](
          const CesiumGltf::Model& model,
          const CesiumGltf::MeshPrimitive& primitive,
          int32_t textureCoordinateSetIndex) -> py::object {
        auto variant = CesiumGltf::getTexCoordAccessorView(
            model,
            primitive,
            textureCoordinateSetIndex);
        return std::visit(
            [&vecAccessorToNumpy](auto&& v) -> py::object {
              return vecAccessorToNumpy(v, 2);
            },
            variant);
      },
      py::arg("model"),
      py::arg("primitive"),
      py::arg("texture_coordinate_set_index"),
      "Get texture coordinates as an (N,2) numpy array.");

  m.def(
      "get_quaternion_accessor_view",
      [vecAccessorToNumpy](
          const CesiumGltf::Model& model,
          int32_t accessorIndex) -> py::object {
        auto variant =
            CesiumGltf::getQuaternionAccessorView(model, accessorIndex);
        return std::visit(
            [&vecAccessorToNumpy](auto&& v) -> py::object {
              return vecAccessorToNumpy(v, 4);
            },
            variant);
      },
      py::arg("model"),
      py::arg("accessor_index"),
      "Get quaternion accessor as an (N,4) numpy array.");
}
