#include "JsonConversions.h"
#include "NumpyConversions.h"

#include <Cesium3DTiles/Asset.h>
#include <Cesium3DTiles/Availability.h>
#include <Cesium3DTiles/BoundingVolume.h>
#include <Cesium3DTiles/Buffer.h>
#include <Cesium3DTiles/BufferCesium.h>
#include <Cesium3DTiles/BufferView.h>
#include <Cesium3DTiles/Class.h>
#include <Cesium3DTiles/ClassProperty.h>
#include <Cesium3DTiles/ClassStatistics.h>
#include <Cesium3DTiles/Content.h>
#include <Cesium3DTiles/Enum.h>
#include <Cesium3DTiles/EnumValue.h>
#include <Cesium3DTiles/Extension3dTilesBoundingVolumeCylinder.h>
#include <Cesium3DTiles/Extension3dTilesBoundingVolumeS2.h>
#include <Cesium3DTiles/Extension3dTilesEllipsoid.h>
#include <Cesium3DTiles/ExtensionContent3dTilesContentVoxels.h>
#include <Cesium3DTiles/GroupMetadata.h>
#include <Cesium3DTiles/ImplicitTiling.h>
#include <Cesium3DTiles/MetadataEntity.h>
#include <Cesium3DTiles/MetadataQuery.h>
#include <Cesium3DTiles/Padding.h>
#include <Cesium3DTiles/Properties.h>
#include <Cesium3DTiles/PropertyStatistics.h>
#include <Cesium3DTiles/PropertyTable.h>
#include <Cesium3DTiles/PropertyTableProperty.h>
#include <Cesium3DTiles/Schema.h>
#include <Cesium3DTiles/Statistics.h>
#include <Cesium3DTiles/Subtree.h>
#include <Cesium3DTiles/Subtrees.h>
#include <Cesium3DTiles/Tile.h>
#include <Cesium3DTiles/Tileset.h>

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include <cstddef>
#include <functional>
#include <string>
#include <vector>

namespace py = pybind11;

void initTiles3dBindings(py::module& m) {
  py::class_<Cesium3DTiles::BufferCesium>(m, "BufferCesium")
      .def(py::init<>())
      .def_property(
          "data",
          [](const Cesium3DTiles::BufferCesium& self) {
            return CesiumPython::vectorToBytes(self.data);
          },
          [](Cesium3DTiles::BufferCesium& self, const py::bytes& value) {
            self.data = CesiumPython::bytesToVector(value);
          });

  py::class_<Cesium3DTiles::Buffer>(m, "Buffer")
      .def(py::init<>())
      .def_readwrite("cesium", &Cesium3DTiles::Buffer::cesium)
      .def_property_readonly(
          "size_bytes",
          &Cesium3DTiles::Buffer::getSizeBytes);

  py::class_<Cesium3DTiles::BoundingVolume>(m, "BoundingVolume")
      .def(py::init<>())
      .def_readwrite("box", &Cesium3DTiles::BoundingVolume::box)
      .def_readwrite("region", &Cesium3DTiles::BoundingVolume::region)
      .def_readwrite("sphere", &Cesium3DTiles::BoundingVolume::sphere)
      .def_property_readonly(
          "size_bytes",
          &Cesium3DTiles::BoundingVolume::getSizeBytes);

  py::class_<Cesium3DTiles::EnumValue>(m, "EnumValue")
      .def(py::init<>())
      .def_readwrite("name", &Cesium3DTiles::EnumValue::name)
      .def_readwrite("description", &Cesium3DTiles::EnumValue::description)
      .def_readwrite("value", &Cesium3DTiles::EnumValue::value)
      .def_property_readonly(
          "size_bytes",
          &Cesium3DTiles::EnumValue::getSizeBytes);

  py::class_<Cesium3DTiles::ClassProperty>(m, "ClassProperty")
      .def(py::init<>())
      .def_readwrite("name", &Cesium3DTiles::ClassProperty::name)
      .def_readwrite("description", &Cesium3DTiles::ClassProperty::description)
      .def_readwrite("type", &Cesium3DTiles::ClassProperty::type)
      .def_readwrite(
          "component_type",
          &Cesium3DTiles::ClassProperty::componentType)
      .def_readwrite("enum_type", &Cesium3DTiles::ClassProperty::enumType)
      .def_readwrite("array", &Cesium3DTiles::ClassProperty::array)
      .def_readwrite("count", &Cesium3DTiles::ClassProperty::count)
      .def_readwrite("normalized", &Cesium3DTiles::ClassProperty::normalized)
      .def_readwrite("offset", &Cesium3DTiles::ClassProperty::offset)
      .def_readwrite("scale", &Cesium3DTiles::ClassProperty::scale)
      .def_readwrite("max", &Cesium3DTiles::ClassProperty::max)
      .def_readwrite("min", &Cesium3DTiles::ClassProperty::min)
      .def_readwrite("required", &Cesium3DTiles::ClassProperty::required)
      .def_readwrite("no_data", &Cesium3DTiles::ClassProperty::noData)
      .def_readwrite(
          "default_property",
          &Cesium3DTiles::ClassProperty::defaultProperty)
      .def_readwrite("semantic", &Cesium3DTiles::ClassProperty::semantic)
      .def_property_readonly(
          "size_bytes",
          &Cesium3DTiles::ClassProperty::getSizeBytes);

  py::class_<Cesium3DTiles::PropertyTableProperty>(m, "PropertyTableProperty")
      .def(py::init<>())
      .def_readwrite("values", &Cesium3DTiles::PropertyTableProperty::values)
      .def_readwrite(
          "array_offsets",
          &Cesium3DTiles::PropertyTableProperty::arrayOffsets)
      .def_readwrite(
          "string_offsets",
          &Cesium3DTiles::PropertyTableProperty::stringOffsets)
      .def_readwrite(
          "array_offset_type",
          &Cesium3DTiles::PropertyTableProperty::arrayOffsetType)
      .def_readwrite(
          "string_offset_type",
          &Cesium3DTiles::PropertyTableProperty::stringOffsetType)
      .def_readwrite("offset", &Cesium3DTiles::PropertyTableProperty::offset)
      .def_readwrite("scale", &Cesium3DTiles::PropertyTableProperty::scale)
      .def_readwrite("max", &Cesium3DTiles::PropertyTableProperty::max)
      .def_readwrite("min", &Cesium3DTiles::PropertyTableProperty::min)
      .def_property_readonly(
          "size_bytes",
          &Cesium3DTiles::PropertyTableProperty::getSizeBytes);

  py::class_<Cesium3DTiles::Class>(m, "Class")
      .def(py::init<>())
      .def_readwrite("name", &Cesium3DTiles::Class::name)
      .def_readwrite("description", &Cesium3DTiles::Class::description)
      .def_readwrite("properties", &Cesium3DTiles::Class::properties)
      .def_readwrite("parent", &Cesium3DTiles::Class::parent)
      .def_property_readonly("size_bytes", &Cesium3DTiles::Class::getSizeBytes);

  py::class_<Cesium3DTiles::Enum>(m, "Enum")
      .def(py::init<>())
      .def_readwrite("name", &Cesium3DTiles::Enum::name)
      .def_readwrite("description", &Cesium3DTiles::Enum::description)
      .def_readwrite("value_type", &Cesium3DTiles::Enum::valueType)
      .def_readwrite("values", &Cesium3DTiles::Enum::values)
      .def_property_readonly("size_bytes", &Cesium3DTiles::Enum::getSizeBytes);

  py::class_<Cesium3DTiles::MetadataEntity>(m, "MetadataEntity")
      .def(py::init<>())
      .def_readwrite(
          "class_property",
          &Cesium3DTiles::MetadataEntity::classProperty)
      .def_readwrite("properties", &Cesium3DTiles::MetadataEntity::properties)
      .def_property_readonly(
          "size_bytes",
          &Cesium3DTiles::MetadataEntity::getSizeBytes);

  py::class_<Cesium3DTiles::Availability>(m, "Availability")
      .def(py::init<>())
      .def_readwrite("bitstream", &Cesium3DTiles::Availability::bitstream)
      .def_readwrite(
          "available_count",
          &Cesium3DTiles::Availability::availableCount)
      .def_readwrite("constant", &Cesium3DTiles::Availability::constant)
      .def_property_readonly(
          "size_bytes",
          &Cesium3DTiles::Availability::getSizeBytes);

  py::class_<Cesium3DTiles::BufferView>(m, "BufferView")
      .def(py::init<>())
      .def_readwrite("buffer", &Cesium3DTiles::BufferView::buffer)
      .def_readwrite("byte_offset", &Cesium3DTiles::BufferView::byteOffset)
      .def_readwrite("byte_length", &Cesium3DTiles::BufferView::byteLength)
      .def_readwrite("name", &Cesium3DTiles::BufferView::name)
      .def_property_readonly(
          "size_bytes",
          &Cesium3DTiles::BufferView::getSizeBytes);

  py::class_<Cesium3DTiles::PropertyTable>(m, "PropertyTable")
      .def(py::init<>())
      .def_readwrite("name", &Cesium3DTiles::PropertyTable::name)
      .def_readwrite(
          "class_property",
          &Cesium3DTiles::PropertyTable::classProperty)
      .def_readwrite("count", &Cesium3DTiles::PropertyTable::count)
      .def_readwrite("properties", &Cesium3DTiles::PropertyTable::properties)
      .def_property_readonly(
          "size_bytes",
          &Cesium3DTiles::PropertyTable::getSizeBytes);

  py::class_<Cesium3DTiles::PropertyStatistics>(m, "PropertyStatistics")
      .def(py::init<>())
      .def_readwrite("min", &Cesium3DTiles::PropertyStatistics::min)
      .def_readwrite("max", &Cesium3DTiles::PropertyStatistics::max)
      .def_readwrite("mean", &Cesium3DTiles::PropertyStatistics::mean)
      .def_readwrite("median", &Cesium3DTiles::PropertyStatistics::median)
      .def_readwrite(
          "standard_deviation",
          &Cesium3DTiles::PropertyStatistics::standardDeviation)
      .def_readwrite("variance", &Cesium3DTiles::PropertyStatistics::variance)
      .def_readwrite("sum", &Cesium3DTiles::PropertyStatistics::sum)
      .def_readwrite(
          "occurrences",
          &Cesium3DTiles::PropertyStatistics::occurrences)
      .def_property_readonly(
          "size_bytes",
          &Cesium3DTiles::PropertyStatistics::getSizeBytes);

  py::class_<Cesium3DTiles::ClassStatistics>(m, "ClassStatistics")
      .def(py::init<>())
      .def_readwrite("count", &Cesium3DTiles::ClassStatistics::count)
      .def_readwrite("properties", &Cesium3DTiles::ClassStatistics::properties)
      .def_property_readonly(
          "size_bytes",
          &Cesium3DTiles::ClassStatistics::getSizeBytes);

  py::class_<Cesium3DTiles::Statistics>(m, "Statistics")
      .def(py::init<>())
      .def_readwrite("classes", &Cesium3DTiles::Statistics::classes)
      .def_property_readonly(
          "size_bytes",
          &Cesium3DTiles::Statistics::getSizeBytes);

  py::class_<Cesium3DTiles::Properties>(m, "Properties")
      .def(py::init<>())
      .def_readwrite("maximum", &Cesium3DTiles::Properties::maximum)
      .def_readwrite("minimum", &Cesium3DTiles::Properties::minimum)
      .def_property_readonly(
          "size_bytes",
          &Cesium3DTiles::Properties::getSizeBytes);

  py::class_<Cesium3DTiles::GroupMetadata, Cesium3DTiles::MetadataEntity>(
      m,
      "GroupMetadata")
      .def(py::init<>())
      .def_property_readonly(
          "size_bytes",
          &Cesium3DTiles::GroupMetadata::getSizeBytes);

  py::class_<Cesium3DTiles::Asset>(m, "Asset")
      .def(py::init<>())
      .def_readwrite("version", &Cesium3DTiles::Asset::version)
      .def_readwrite("tileset_version", &Cesium3DTiles::Asset::tilesetVersion)
      .def_property_readonly("size_bytes", &Cesium3DTiles::Asset::getSizeBytes);

  py::class_<Cesium3DTiles::Subtrees>(m, "Subtrees")
      .def(py::init<>())
      .def_readwrite("uri", &Cesium3DTiles::Subtrees::uri)
      .def_property_readonly(
          "size_bytes",
          &Cesium3DTiles::Subtrees::getSizeBytes);

  py::class_<Cesium3DTiles::ImplicitTiling>(m, "ImplicitTiling")
      .def(py::init<>())
      .def_readwrite(
          "subdivision_scheme",
          &Cesium3DTiles::ImplicitTiling::subdivisionScheme)
      .def_readwrite(
          "subtree_levels",
          &Cesium3DTiles::ImplicitTiling::subtreeLevels)
      .def_readwrite(
          "available_levels",
          &Cesium3DTiles::ImplicitTiling::availableLevels)
      .def_readwrite("subtrees", &Cesium3DTiles::ImplicitTiling::subtrees)
      .def_property_readonly(
          "size_bytes",
          &Cesium3DTiles::ImplicitTiling::getSizeBytes);

  py::class_<Cesium3DTiles::Content>(m, "Content")
      .def(py::init<>())
      .def_readwrite("bounding_volume", &Cesium3DTiles::Content::boundingVolume)
      .def_readwrite("uri", &Cesium3DTiles::Content::uri)
      .def_readwrite("metadata", &Cesium3DTiles::Content::metadata)
      .def_readwrite("group", &Cesium3DTiles::Content::group)
      .def_property_readonly(
          "size_bytes",
          &Cesium3DTiles::Content::getSizeBytes);

  py::class_<Cesium3DTiles::Extension3dTilesBoundingVolumeCylinder>(
      m,
      "Extension3dTilesBoundingVolumeCylinder")
      .def(py::init<>())
      .def_readwrite(
          "min_radius",
          &Cesium3DTiles::Extension3dTilesBoundingVolumeCylinder::minRadius)
      .def_readwrite(
          "max_radius",
          &Cesium3DTiles::Extension3dTilesBoundingVolumeCylinder::maxRadius)
      .def_readwrite(
          "height",
          &Cesium3DTiles::Extension3dTilesBoundingVolumeCylinder::height)
      .def_readwrite(
          "min_angle",
          &Cesium3DTiles::Extension3dTilesBoundingVolumeCylinder::minAngle)
      .def_readwrite(
          "max_angle",
          &Cesium3DTiles::Extension3dTilesBoundingVolumeCylinder::maxAngle)
      .def_readwrite(
          "translation",
          &Cesium3DTiles::Extension3dTilesBoundingVolumeCylinder::translation)
      .def_readwrite(
          "rotation",
          &Cesium3DTiles::Extension3dTilesBoundingVolumeCylinder::rotation)
      .def_property_readonly(
          "size_bytes",
          &Cesium3DTiles::Extension3dTilesBoundingVolumeCylinder::getSizeBytes);

  py::class_<Cesium3DTiles::Extension3dTilesBoundingVolumeS2>(
      m,
      "Extension3dTilesBoundingVolumeS2")
      .def(py::init<>())
      .def_readwrite(
          "token",
          &Cesium3DTiles::Extension3dTilesBoundingVolumeS2::token)
      .def_readwrite(
          "minimum_height",
          &Cesium3DTiles::Extension3dTilesBoundingVolumeS2::minimumHeight)
      .def_readwrite(
          "maximum_height",
          &Cesium3DTiles::Extension3dTilesBoundingVolumeS2::maximumHeight)
      .def_property_readonly(
          "size_bytes",
          &Cesium3DTiles::Extension3dTilesBoundingVolumeS2::getSizeBytes);

  py::class_<Cesium3DTiles::Extension3dTilesEllipsoid>(
      m,
      "Extension3dTilesEllipsoid")
      .def(py::init<>())
      .def_readwrite("body", &Cesium3DTiles::Extension3dTilesEllipsoid::body)
      .def_readwrite("radii", &Cesium3DTiles::Extension3dTilesEllipsoid::radii)
      .def_property_readonly(
          "size_bytes",
          &Cesium3DTiles::Extension3dTilesEllipsoid::getSizeBytes);

  py::class_<Cesium3DTiles::Padding>(m, "Padding")
      .def(py::init<>())
      .def_readwrite("before", &Cesium3DTiles::Padding::before)
      .def_readwrite("after", &Cesium3DTiles::Padding::after)
      .def_property_readonly(
          "size_bytes",
          &Cesium3DTiles::Padding::getSizeBytes);

  py::class_<Cesium3DTiles::ExtensionContent3dTilesContentVoxels>(
      m,
      "ExtensionContent3dTilesContentVoxels")
      .def(py::init<>())
      .def_readwrite(
          "dimensions",
          &Cesium3DTiles::ExtensionContent3dTilesContentVoxels::dimensions)
      .def_readwrite(
          "padding",
          &Cesium3DTiles::ExtensionContent3dTilesContentVoxels::padding)
      .def_readwrite(
          "class_property",
          &Cesium3DTiles::ExtensionContent3dTilesContentVoxels::classProperty)
      .def_property_readonly(
          "size_bytes",
          &Cesium3DTiles::ExtensionContent3dTilesContentVoxels::getSizeBytes);

  py::class_<Cesium3DTiles::Tile>(m, "Tile")
      .def(py::init<>())
      .def_readwrite("bounding_volume", &Cesium3DTiles::Tile::boundingVolume)
      .def_readwrite(
          "viewer_request_volume",
          &Cesium3DTiles::Tile::viewerRequestVolume)
      .def_readwrite("geometric_error", &Cesium3DTiles::Tile::geometricError)
      .def_readwrite("refine", &Cesium3DTiles::Tile::refine)
      .def_readwrite("transform", &Cesium3DTiles::Tile::transform)
      .def_readwrite("content", &Cesium3DTiles::Tile::content)
      .def_readwrite("contents", &Cesium3DTiles::Tile::contents)
      .def_readwrite("metadata", &Cesium3DTiles::Tile::metadata)
      .def_readwrite("implicit_tiling", &Cesium3DTiles::Tile::implicitTiling)
      .def_readwrite("children", &Cesium3DTiles::Tile::children)
      .def_property_readonly("size_bytes", &Cesium3DTiles::Tile::getSizeBytes);

  py::class_<Cesium3DTiles::Schema>(m, "Schema")
      .def(py::init<>())
      .def_readwrite("id", &Cesium3DTiles::Schema::id)
      .def_readwrite("name", &Cesium3DTiles::Schema::name)
      .def_readwrite("description", &Cesium3DTiles::Schema::description)
      .def_readwrite("version", &Cesium3DTiles::Schema::version)
      .def_readwrite("classes", &Cesium3DTiles::Schema::classes)
      .def_readwrite("enums", &Cesium3DTiles::Schema::enums)
      .def_property_readonly(
          "size_bytes",
          &Cesium3DTiles::Schema::getSizeBytes);

  py::class_<Cesium3DTiles::Subtree>(m, "Subtree")
      .def(py::init<>())
      .def_readwrite("buffers", &Cesium3DTiles::Subtree::buffers)
      .def_readwrite("buffer_views", &Cesium3DTiles::Subtree::bufferViews)
      .def_readwrite("property_tables", &Cesium3DTiles::Subtree::propertyTables)
      .def_readwrite(
          "tile_availability",
          &Cesium3DTiles::Subtree::tileAvailability)
      .def_readwrite(
          "content_availability",
          &Cesium3DTiles::Subtree::contentAvailability)
      .def_readwrite(
          "child_subtree_availability",
          &Cesium3DTiles::Subtree::childSubtreeAvailability)
      .def_readwrite("tile_metadata", &Cesium3DTiles::Subtree::tileMetadata)
      .def_readwrite(
          "content_metadata",
          &Cesium3DTiles::Subtree::contentMetadata)
      .def_readwrite(
          "subtree_metadata",
          &Cesium3DTiles::Subtree::subtreeMetadata)
      .def_property_readonly(
          "size_bytes",
          &Cesium3DTiles::Subtree::getSizeBytes);

  py::class_<Cesium3DTiles::Tileset>(m, "Tileset")
      .def(py::init<>())
      .def_readwrite("asset", &Cesium3DTiles::Tileset::asset)
      .def_readwrite("properties", &Cesium3DTiles::Tileset::properties)
      .def_readwrite("schema", &Cesium3DTiles::Tileset::schema)
      .def_readwrite("schema_uri", &Cesium3DTiles::Tileset::schemaUri)
      .def_readwrite("statistics", &Cesium3DTiles::Tileset::statistics)
      .def_readwrite("groups", &Cesium3DTiles::Tileset::groups)
      .def_readwrite("metadata", &Cesium3DTiles::Tileset::metadata)
      .def_readwrite("geometric_error", &Cesium3DTiles::Tileset::geometricError)
      .def_readwrite("root", &Cesium3DTiles::Tileset::root)
      .def_readwrite("extensions_used", &Cesium3DTiles::Tileset::extensionsUsed)
      .def_readwrite(
          "extensions_required",
          &Cesium3DTiles::Tileset::extensionsRequired)
      .def("add_extension_used", &Cesium3DTiles::Tileset::addExtensionUsed)
      .def(
          "add_extension_required",
          &Cesium3DTiles::Tileset::addExtensionRequired)
      .def(
          "remove_extension_used",
          &Cesium3DTiles::Tileset::removeExtensionUsed)
      .def(
          "remove_extension_required",
          &Cesium3DTiles::Tileset::removeExtensionRequired)
      .def("is_extension_used", &Cesium3DTiles::Tileset::isExtensionUsed)
      .def(
          "is_extension_required",
          &Cesium3DTiles::Tileset::isExtensionRequired)
      .def(
          "for_each_tile",
          [](const Cesium3DTiles::Tileset& self, const py::function& callback) {
            py::gil_scoped_release release;
            self.forEachTile([&callback](
                                 const Cesium3DTiles::Tileset& tileset,
                                 const Cesium3DTiles::Tile& tile,
                                 const glm::dmat4& transform) {
              py::gil_scoped_acquire acquire;
              callback(tileset, tile, CesiumPython::toNumpy(transform));
            });
          },
          py::arg("callback"),
          "Call callback(tileset, tile, transform) for every explicit tile.")
      .def(
          "for_each_content",
          [](const Cesium3DTiles::Tileset& self, const py::function& callback) {
            py::gil_scoped_release release;
            self.forEachContent([&callback](
                                    const Cesium3DTiles::Tileset& tileset,
                                    const Cesium3DTiles::Tile& tile,
                                    const Cesium3DTiles::Content& content,
                                    const glm::dmat4& transform) {
              py::gil_scoped_acquire acquire;
              callback(
                  tileset,
                  tile,
                  content,
                  CesiumPython::toNumpy(transform));
            });
          },
          py::arg("callback"),
          "Call callback(tileset, tile, content, transform) for every explicit "
          "tile content.")
      .def_property_readonly(
          "size_bytes",
          &Cesium3DTiles::Tileset::getSizeBytes);

  // ---- FoundMetadataProperty -----------------------------------------------
  py::class_<Cesium3DTiles::FoundMetadataProperty>(m, "FoundMetadataProperty")
      .def_property_readonly(
          "class_identifier",
          [](const Cesium3DTiles::FoundMetadataProperty& self)
              -> const std::string& { return self.classIdentifier; })
      .def_property_readonly(
          "class_definition",
          [](const Cesium3DTiles::FoundMetadataProperty& self)
              -> const Cesium3DTiles::Class& { return self.classDefinition; },
          py::return_value_policy::reference_internal)
      .def_property_readonly(
          "property_identifier",
          [](const Cesium3DTiles::FoundMetadataProperty& self)
              -> const std::string& { return self.propertyIdentifier; })
      .def_property_readonly(
          "property_definition",
          [](const Cesium3DTiles::FoundMetadataProperty& self)
              -> const Cesium3DTiles::ClassProperty& {
            return self.propertyDefinition;
          },
          py::return_value_policy::reference_internal)
      .def_property_readonly(
          "property_value",
          [](const Cesium3DTiles::FoundMetadataProperty& self) -> py::object {
            return CesiumPython::jsonValueToPy(self.propertyValue);
          });

  // ---- MetadataQuery -------------------------------------------------------
  py::class_<Cesium3DTiles::MetadataQuery>(m, "MetadataQuery")
      .def_static(
          "find_first_property_with_semantic",
          &Cesium3DTiles::MetadataQuery::findFirstPropertyWithSemantic,
          py::arg("schema"),
          py::arg("entity"),
          py::arg("semantic"));
}
