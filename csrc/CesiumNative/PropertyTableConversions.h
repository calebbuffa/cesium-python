#pragma once

#include <CesiumGltf/Class.h>
#include <CesiumGltf/ClassProperty.h>
#include <CesiumGltf/FeatureIdTextureView.h>
#include <CesiumGltf/Model.h>
#include <CesiumGltf/PropertyArrayView.h>
#include <CesiumGltf/PropertyAttributeView.h>
#include <CesiumGltf/PropertyTableView.h>
#include <CesiumGltf/PropertyTextureView.h>
#include <CesiumGltf/PropertyType.h>

#include <pybind11/numpy.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <type_traits>

namespace py = pybind11;

namespace CesiumStructuralMetadata {

// Helper: determine numpy dtype string for a scalar component type
template <typename T> struct NumpyDtype;
template <> struct NumpyDtype<int8_t> {
  static constexpr const char* value = "int8";
};
template <> struct NumpyDtype<uint8_t> {
  static constexpr const char* value = "uint8";
};
template <> struct NumpyDtype<int16_t> {
  static constexpr const char* value = "int16";
};
template <> struct NumpyDtype<uint16_t> {
  static constexpr const char* value = "uint16";
};
template <> struct NumpyDtype<int32_t> {
  static constexpr const char* value = "int32";
};
template <> struct NumpyDtype<uint32_t> {
  static constexpr const char* value = "uint32";
};
template <> struct NumpyDtype<int64_t> {
  static constexpr const char* value = "int64";
};
template <> struct NumpyDtype<uint64_t> {
  static constexpr const char* value = "uint64";
};
template <> struct NumpyDtype<float> {
  static constexpr const char* value = "float32";
};
template <> struct NumpyDtype<double> {
  static constexpr const char* value = "float64";
};

// Type traits for glm vec/mat detection
template <typename T> struct is_glm_vec : std::false_type {};
template <glm::length_t N, typename T, glm::qualifier Q>
struct is_glm_vec<glm::vec<N, T, Q>> : std::true_type {};

template <typename T> struct is_glm_mat : std::false_type {};
template <glm::length_t C, glm::length_t R, typename T, glm::qualifier Q>
struct is_glm_mat<glm::mat<C, R, T, Q>> : std::true_type {};

// Extract full column as numpy array for scalar types
// GIL is released during the bulk copy loop for maximum throughput.
template <typename T, bool Normalized>
py::object columnToNumpy(
    const CesiumGltf::PropertyTablePropertyView<T, Normalized>& view) {
  const auto n = view.size();
  py::array_t<T> out(n);
  T* buf = static_cast<T*>(out.mutable_data());
  {
    py::gil_scoped_release release;
    for (int64_t i = 0; i < n; ++i) {
      auto val = view.get(i);
      buf[i] = val ? *val : T{};
    }
  }
  return out;
}

// Normalized integer -> float64 after transformation
template <typename T>
py::object columnToNumpyNormalized(
    const CesiumGltf::PropertyTablePropertyView<T, true>& view) {
  const auto n = view.size();
  py::array_t<double> out(n);
  double* buf = static_cast<double*>(out.mutable_data());
  {
    py::gil_scoped_release release;
    for (int64_t i = 0; i < n; ++i) {
      auto val = view.get(i);
      buf[i] = val ? *val : 0.0;
    }
  }
  return out;
}

// Extract full column for glm::vec<N, T> as (size, N) numpy array
template <glm::length_t N, typename T, bool Normalized>
py::object vecColumnToNumpy(
    const CesiumGltf::PropertyTablePropertyView<glm::vec<N, T>, Normalized>&
        view) {
  const auto rows = view.size();
  using OutT = std::conditional_t<Normalized, double, T>;
  py::array_t<OutT> out(
      {static_cast<py::ssize_t>(rows), static_cast<py::ssize_t>(N)});
  OutT* buf = static_cast<OutT*>(out.mutable_data());
  {
    py::gil_scoped_release release;
    for (int64_t i = 0; i < rows; ++i) {
      auto val = view.get(i);
      OutT* row = buf + i * N;
      for (glm::length_t j = 0; j < N; ++j) {
        row[j] = val ? static_cast<OutT>((*val)[j]) : OutT{};
      }
    }
  }
  return out;
}

// Extract full column for glm::mat<N, N, T> as (size, N, N) numpy array
template <glm::length_t N, typename T, bool Normalized>
py::object matColumnToNumpy(
    const CesiumGltf::PropertyTablePropertyView<glm::mat<N, N, T>, Normalized>&
        view) {
  const auto rows = view.size();
  using OutT = std::conditional_t<Normalized, double, T>;
  py::array_t<OutT> out(
      {static_cast<py::ssize_t>(rows),
       static_cast<py::ssize_t>(N),
       static_cast<py::ssize_t>(N)});
  OutT* buf = static_cast<OutT*>(out.mutable_data());
  {
    py::gil_scoped_release release;
    for (int64_t i = 0; i < rows; ++i) {
      auto val = view.get(i);
      OutT* mat = buf + i * N * N;
      for (glm::length_t c = 0; c < N; ++c) {
        for (glm::length_t r = 0; r < N; ++r) {
          mat[c * N + r] = val ? static_cast<OutT>((*val)[c][r]) : OutT{};
        }
      }
    }
  }
  return out;
}

// Extract full column for bool as numpy bool array
inline py::object boolColumnToNumpy(
    const CesiumGltf::PropertyTablePropertyView<bool, false>& view) {
  const auto n = view.size();
  py::array_t<bool> out(n);
  bool* buf = static_cast<bool*>(out.mutable_data());
  {
    py::gil_scoped_release release;
    for (int64_t i = 0; i < n; ++i) {
      auto val = view.get(i);
      buf[i] = val ? *val : false;
    }
  }
  return out;
}

// Extract full column for string as Python list
inline py::object stringColumnToList(
    const CesiumGltf::PropertyTablePropertyView<std::string_view, false>&
        view) {
  py::list out;
  for (int64_t i = 0; i < view.size(); ++i) {
    auto val = view.get(i);
    if (val) {
      out.append(py::str(std::string(*val)));
    } else {
      out.append(py::none());
    }
  }
  return out;
}

// Extract full column for array types as Python list of numpy arrays
// For numeric element types, the inner arrays are filled without the GIL.
template <typename T, bool Normalized>
py::object arrayColumnToList(
    const CesiumGltf::PropertyTablePropertyView<
        CesiumGltf::PropertyArrayView<T>,
        Normalized>& view) {
  py::list out;
  for (int64_t i = 0; i < view.size(); ++i) {
    auto val = view.get(i);
    if (val) {
      const auto& arr = *val;
      const auto len = arr.size();
      py::array_t<T> np(len);
      T* buf = static_cast<T*>(np.mutable_data());
      // Inner copy is pure C++ — no GIL needed but overhead of release/acquire
      // per row isn't worth it. The outer list.append needs the GIL anyway.
      for (int64_t j = 0; j < len; ++j) {
        buf[j] = arr[j];
      }
      out.append(np);
    } else {
      out.append(py::none());
    }
  }
  return out;
}

// Convert a single PropertyArrayView<T> or PropertyArrayCopy<T> element to a
// Python object.  Factored out so MSVC only instantiates it for actual array
// element types.
template <typename ArrayT> py::object propertyArrayToObject(const ArrayT& arr) {
  using T =
      std::remove_cv_t<std::remove_reference_t<decltype(arr[int64_t{0}])>>;
  const auto len = arr.size();
  if constexpr (std::is_same_v<T, bool>) {
    py::list result(len);
    for (int64_t j = 0; j < len; ++j)
      result[j] = py::bool_(arr[j]);
    return py::object(std::move(result));
  } else if constexpr (std::is_same_v<T, std::string_view>) {
    py::list result(len);
    for (int64_t j = 0; j < len; ++j)
      result[j] = py::str(std::string(arr[j]));
    return py::object(std::move(result));
  } else if constexpr (is_glm_vec<T>::value) {
    constexpr auto N = T::length();
    using Scalar = typename T::value_type;
    py::array_t<Scalar> np(
        {static_cast<py::ssize_t>(len), static_cast<py::ssize_t>(N)});
    Scalar* buf = static_cast<Scalar*>(np.mutable_data());
    for (int64_t j = 0; j < len; ++j) {
      const auto& v = arr[j];
      for (glm::length_t k = 0; k < N; ++k)
        buf[j * N + k] = v[k];
    }
    return py::object(std::move(np));
  } else if constexpr (is_glm_mat<T>::value) {
    constexpr auto C = T::length();
    constexpr auto R = static_cast<glm::length_t>(T::col_type::length());
    using Scalar = typename T::value_type;
    py::array_t<Scalar> np(
        {static_cast<py::ssize_t>(len),
         static_cast<py::ssize_t>(C),
         static_cast<py::ssize_t>(R)});
    Scalar* buf = static_cast<Scalar*>(np.mutable_data());
    for (int64_t j = 0; j < len; ++j) {
      const auto& m = arr[j];
      for (glm::length_t c = 0; c < C; ++c)
        for (glm::length_t r = 0; r < R; ++r)
          buf[j * C * R + c * R + r] = m[c][r];
    }
    return py::object(std::move(np));
  } else {
    py::array_t<T> np(len);
    T* buf = static_cast<T*>(np.mutable_data());
    for (int64_t j = 0; j < len; ++j)
      buf[j] = arr[j];
    return py::object(std::move(np));
  }
}

// Type-erased wrapper: stores lambdas for get(index) and as_numpy()

struct PropertyColumnAccessor {
  int32_t status;
  int64_t size;
  std::function<py::object(int64_t)> getFn;
  std::function<py::object()> asNumpyFn;
};

// Build a PropertyColumnAccessor from a typed view using the callback dispatch
inline PropertyColumnAccessor makeColumnAccessorFromView(
    const CesiumGltf::PropertyTableView& tableView,
    const std::string& propertyId) {
  PropertyColumnAccessor accessor{0, 0, nullptr, nullptr};

  tableView.getPropertyView(
      propertyId,
      [&accessor](const std::string& /*id*/, auto&& view) {
        using ViewType = std::decay_t<decltype(view)>;
        accessor.status = view.status();
        accessor.size = view.size();

        if (view.status() != 0) { // not Valid
          accessor.getFn = [](int64_t) -> py::object { return py::none(); };
          accessor.asNumpyFn = []() -> py::object { return py::none(); };
          return;
        }

        auto sharedView = std::make_shared<ViewType>(std::move(view));

        // get(index)
        accessor.getFn = [sharedView](int64_t index) -> py::object {
          if (index < 0 || index >= sharedView->size()) {
            throw py::index_error("Index out of range");
          }
          auto val = sharedView->get(index);
          if (!val)
            return py::none();

          using ElemType = typename decltype(sharedView->get(0))::value_type;
          if constexpr (std::is_same_v<ElemType, bool>) {
            return py::cast(*val);
          } else if constexpr (std::is_same_v<ElemType, std::string_view>) {
            return py::str(std::string(*val));
          } else if constexpr (CesiumGltf::IsMetadataArray<ElemType>::value) {
            return propertyArrayToObject(*val);
          } else {
            return py::cast(*val);
          }
        };

        // as_numpy() — column extraction
        accessor.asNumpyFn = [sharedView]() -> py::object {
          using ElemType = typename decltype(sharedView->get(0))::value_type;

          if constexpr (std::is_same_v<ElemType, bool>) {
            return boolColumnToNumpy(*sharedView);
          } else if constexpr (std::is_same_v<ElemType, std::string_view>) {
            return stringColumnToList(*sharedView);
          } else if constexpr (is_glm_vec<ElemType>::value) {
            return vecColumnToNumpy(*sharedView);
          } else if constexpr (is_glm_mat<ElemType>::value) {
            return matColumnToNumpy(*sharedView);
          } else if constexpr (CesiumGltf::IsMetadataScalar<ElemType>::value) {
            return columnToNumpy(*sharedView);
          } else if constexpr (CesiumGltf::IsMetadataArray<ElemType>::value) {
            return arrayColumnToList(*sharedView);
          } else {
            return py::none();
          }
        };
      });

  return accessor;
}

// Get all column data as a dict {property_id: numpy_or_list}
inline py::dict tableToDict(const CesiumGltf::PropertyTableView& tableView) {
  py::dict result;
  const auto* pClass = tableView.getClass();
  if (!pClass)
    return result;

  for (const auto& [propId, _] : pClass->properties) {
    auto accessor = makeColumnAccessorFromView(tableView, propId);
    if (accessor.status == 0 && accessor.asNumpyFn) {
      result[py::str(propId)] = accessor.asNumpyFn();
    }
  }
  return result;
}

} // namespace CesiumStructuralMetadata

// Type-erased wrappers for PropertyTexture / PropertyAttribute property views
namespace CesiumPropertyView {

struct TexturePropertyAccessor {
  int32_t status;
  std::function<py::object(double, double)> getFn;
  std::function<py::object(double, double)> getRawFn;
};

inline TexturePropertyAccessor makeTexturePropertyAccessor(
    const CesiumGltf::PropertyTextureView& textureView,
    const std::string& propertyId) {
  TexturePropertyAccessor accessor{0, nullptr, nullptr};

  textureView.getPropertyView(
      propertyId,
      [&accessor](const std::string& /*id*/, auto&& view) {
        using ViewType = std::decay_t<decltype(view)>;
        accessor.status = view.status();

        if (view.status() != 0) {
          accessor.getFn = [](double, double) -> py::object {
            return py::none();
          };
          accessor.getRawFn = [](double, double) -> py::object {
            return py::none();
          };
          return;
        }

        auto sharedView = std::make_shared<ViewType>(std::move(view));

        accessor.getFn = [sharedView](double u, double v) -> py::object {
          auto val = sharedView->get(u, v);
          if (!val)
            return py::none();
          return py::cast(*val);
        };

        accessor.getRawFn = [sharedView](double u, double v) -> py::object {
          return py::cast(sharedView->getRaw(u, v));
        };
      });

  return accessor;
}

struct AttributePropertyAccessor {
  int32_t status;
  int64_t size;
  std::function<py::object(int64_t)> getFn;
  std::function<py::object(int64_t)> getRawFn;
};

inline AttributePropertyAccessor makeAttributePropertyAccessor(
    const CesiumGltf::PropertyAttributeView& attributeView,
    const CesiumGltf::MeshPrimitive& primitive,
    const std::string& propertyId) {
  AttributePropertyAccessor accessor{0, 0, nullptr, nullptr};

  attributeView.getPropertyView(
      primitive,
      propertyId,
      [&accessor](const std::string& /*id*/, auto&& view) {
        using ViewType = std::decay_t<decltype(view)>;
        accessor.status = view.status();
        accessor.size = view.size();

        if (view.status() != 0) {
          accessor.getFn = [](int64_t) -> py::object { return py::none(); };
          accessor.getRawFn = [](int64_t) -> py::object { return py::none(); };
          return;
        }

        auto sharedView = std::make_shared<ViewType>(std::move(view));

        accessor.getFn = [sharedView](int64_t index) -> py::object {
          if (index < 0 || index >= sharedView->size()) {
            throw py::index_error("Index out of range");
          }
          auto val = sharedView->get(index);
          if (!val)
            return py::none();
          return py::cast(*val);
        };

        accessor.getRawFn = [sharedView](int64_t index) -> py::object {
          if (index < 0 || index >= sharedView->size()) {
            throw py::index_error("Index out of range");
          }
          return py::cast(sharedView->getRaw(index));
        };
      });

  return accessor;
}

} // namespace CesiumPropertyView
