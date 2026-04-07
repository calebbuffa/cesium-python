#pragma once

/**
 * @file NumpyConversions.hpp
 * @brief Unified numpy <-> C++ conversion and batch-processing utilities.
 *
 * Provides:
 *   - GLM <-> numpy conversions (copy and zero-copy)
 *   - Numpy array validation and creation helpers
 *   - GIL-free batch operation templates for near-native throughput
 *   - Byte/vector interop (copy and zero-copy)
 */

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/mat3x3.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <pybind11/numpy.h>
#include <pybind11/pybind11.h>

#include <cstddef>
#include <cstring>
#include <limits>
#include <optional>
#include <span>
#include <string>
#include <type_traits>
#include <vector>

namespace py = pybind11;

namespace CesiumPython {

// numpy/sequence -> GLM  (always copies)

/// Convert a 1-D numpy array or sequence of length N to glm::dvec<N>.
template <glm::length_t N>
inline glm::vec<N, double, glm::defaultp> toDvec(const py::handle& value) {
  if (py::isinstance<py::array>(value)) {
    auto arr = py::reinterpret_borrow<
        py::array_t<double, py::array::c_style | py::array::forcecast>>(value);
    if (arr.ndim() != 1 || arr.shape(0) != static_cast<py::ssize_t>(N)) {
      throw py::value_error(
          "Expected numpy shape (" + std::to_string(N) + ",).\n");
    }
    auto a = arr.unchecked<1>();
    glm::vec<N, double, glm::defaultp> result;
    for (size_t i = 0; i < N; ++i) {
      result[static_cast<int>(i)] = a(static_cast<py::ssize_t>(i));
    }
    return result;
  }

  py::sequence seq = py::reinterpret_borrow<py::sequence>(value);
  if (py::len(seq) != static_cast<py::ssize_t>(N)) {
    throw py::value_error(
        "Expected sequence length " + std::to_string(N) + ".");
  }

  glm::vec<N, double, glm::defaultp> result;
  for (size_t i = 0; i < N; ++i) {
    result[static_cast<int>(i)] =
        py::cast<double>(seq[static_cast<py::ssize_t>(i)]);
  }
  return result;
}

/// Convert a 2-D (NxN) or 1-D (N*N) numpy array, or flat sequence, to
/// glm::dmat<N,N>. Expects column-major layout: arr[col, row] = mat[col][row].
template <glm::length_t N>
inline glm::mat<N, N, double, glm::defaultp> toDmat(const py::handle& value) {
  if (py::isinstance<py::array>(value)) {
    auto arr = py::reinterpret_borrow<
        py::array_t<double, py::array::c_style | py::array::forcecast>>(value);
    glm::mat<N, N, double, glm::defaultp> matrix(1.0);

    if (arr.ndim() == 1) {
      if (arr.shape(0) != static_cast<py::ssize_t>(N * N)) {
        throw py::value_error(
            "Expected numpy shape (" + std::to_string(N * N) + ",) or (" +
            std::to_string(N) + "," + std::to_string(N) + ").");
      }
      auto a = arr.unchecked<1>();
      for (size_t col = 0; col < N; ++col) {
        for (size_t row = 0; row < N; ++row) {
          matrix[static_cast<int>(col)][static_cast<int>(row)] =
              a(static_cast<py::ssize_t>(col * N + row));
        }
      }
      return matrix;
    }

    if (arr.ndim() == 2) {
      if (arr.shape(0) != static_cast<py::ssize_t>(N) ||
          arr.shape(1) != static_cast<py::ssize_t>(N)) {
        throw py::value_error(
            "Expected numpy shape (" + std::to_string(N * N) + ",) or (" +
            std::to_string(N) + "," + std::to_string(N) + ").");
      }
      auto a = arr.unchecked<2>();
      for (size_t row = 0; row < N; ++row) {
        for (size_t col = 0; col < N; ++col) {
          matrix[static_cast<int>(col)][static_cast<int>(row)] =
              a(static_cast<py::ssize_t>(col), static_cast<py::ssize_t>(row));
        }
      }
      return matrix;
    }

    throw py::value_error(
        "Expected numpy shape (" + std::to_string(N * N) + ",) or (" +
        std::to_string(N) + "," + std::to_string(N) + ").");
  }

  py::sequence seq = py::reinterpret_borrow<py::sequence>(value);
  if (py::len(seq) != static_cast<py::ssize_t>(N * N)) {
    throw py::value_error(
        "Expected sequence length " + std::to_string(N * N) + ".");
  }

  glm::mat<N, N, double, glm::defaultp> matrix(1.0);
  for (size_t col = 0; col < N; ++col) {
    for (size_t row = 0; row < N; ++row) {
      matrix[static_cast<int>(col)][static_cast<int>(row)] =
          py::cast<double>(seq[static_cast<py::ssize_t>(col * N + row)]);
    }
  }
  return matrix;
}

/// Convert a length-4 numpy array [x,y,z,w] or sequence to glm::dquat
/// (w,x,y,z).
inline glm::dquat toDquat(const py::handle& value) {
  if (py::isinstance<py::array>(value)) {
    auto arr = py::reinterpret_borrow<
        py::array_t<double, py::array::c_style | py::array::forcecast>>(value);
    if (arr.ndim() != 1 || arr.shape(0) != 4) {
      throw py::value_error("Expected numpy shape (4,).");
    }
    auto a = arr.unchecked<1>();
    return glm::dquat(a(3), a(0), a(1), a(2));
  }

  py::sequence seq = py::reinterpret_borrow<py::sequence>(value);
  if (py::len(seq) != 4) {
    throw py::value_error("Expected sequence length 4.");
  }
  return glm::dquat(
      py::cast<double>(seq[3]),
      py::cast<double>(seq[0]),
      py::cast<double>(seq[1]),
      py::cast<double>(seq[2]));
}

// GLM -> numpy  (copy)

/// Copy a glm::dvec<N> to a new (N,) float64 numpy array.
template <glm::length_t N>
inline py::array_t<double>
toNumpy(const glm::vec<N, double, glm::defaultp>& value) {
  py::array_t<double> out(N);
  auto outView = out.mutable_unchecked<1>();
  for (size_t i = 0; i < N; ++i) {
    outView(static_cast<py::ssize_t>(i)) = value[static_cast<int>(i)];
  }
  return out;
}

/// Copy a glm::dmat<N,N> to a new (N, N) float64 numpy array (column-major).
template <glm::length_t N>
inline py::array_t<double>
toNumpy(const glm::mat<N, N, double, glm::defaultp>& value) {
  py::array_t<double> out(
      {static_cast<py::ssize_t>(N), static_cast<py::ssize_t>(N)});
  auto outView = out.mutable_unchecked<2>();
  for (size_t col = 0; col < N; ++col) {
    for (size_t row = 0; row < N; ++row) {
      outView(static_cast<py::ssize_t>(col), static_cast<py::ssize_t>(row)) =
          value[static_cast<int>(col)][static_cast<int>(row)];
    }
  }
  return out;
}

/// Copy a glm::dquat to a new (4,) float64 numpy array in [x, y, z, w] order.
inline py::array_t<double> toNumpy(const glm::dquat& value) {
  py::array_t<double> out(4);
  auto outView = out.mutable_unchecked<1>();
  outView(0) = value.x;
  outView(1) = value.y;
  outView(2) = value.z;
  outView(3) = value.w;
  return out;
}

/// Copy a span of glm::dvec<N> to a new (M, N) float64 numpy array.
template <glm::length_t N>
inline py::array_t<double>
toNumpy(std::span<const glm::vec<N, double, glm::defaultp>> values) {
  const auto M = static_cast<py::ssize_t>(values.size());
  const auto cols = static_cast<py::ssize_t>(N);
  py::array_t<double> out({M, cols});
  auto outView = out.mutable_unchecked<2>();
  for (py::ssize_t i = 0; i < M; ++i) {
    for (size_t j = 0; j < N; ++j) {
      outView(i, static_cast<py::ssize_t>(j)) =
          values[static_cast<size_t>(i)][static_cast<int>(j)];
    }
  }
  return out;
}

/// Copy a vector of glm::dvec<N> to a new (M, N) float64 numpy array.
template <glm::length_t N>
inline py::array_t<double>
toNumpy(const std::vector<glm::vec<N, double, glm::defaultp>>& values) {
  return toNumpy<N>(std::span<const glm::vec<N, double, glm::defaultp>>(
      values.data(),
      values.size()));
}

// GLM/data -> numpy  (zero-copy read-only view)

/// Zero-copy view of a glm::dvec<N> as (N,) float64 numpy array.
template <glm::length_t N>
inline py::array_t<double>
toNumpyView(const glm::vec<N, double, glm::defaultp>& vec, py::handle base) {
  auto arr = py::array_t<double>(
      std::vector<size_t>{N},
      std::vector<size_t>{sizeof(double)},
      &vec[0],
      base);
  arr.attr("flags").attr("writeable") = false;
  return arr;
}

/// Zero-copy view of a glm::dmat<N,N> as (N, N) float64 numpy array.
template <glm::length_t N>
inline py::array_t<double>
toNumpyView(const glm::mat<N, N, double, glm::defaultp>& mat, py::handle base) {
  auto arr = py::array_t<double>(
      std::vector<size_t>{N, N},
      std::vector<size_t>{sizeof(double) * N, sizeof(double)},
      &mat[0][0],
      base);
  arr.attr("flags").attr("writeable") = false;
  return arr;
}

/// Zero-copy view into contiguous memory as 1-D numpy array (read-only).
template <typename T>
inline py::array_t<T> toNumpyView(std::span<const T> span, py::handle base) {
  if (span.empty()) {
    return py::array_t<T>(std::vector<size_t>{0});
  }
  const size_t n = span.size();
  auto arr = py::array_t<T>(
      std::vector<size_t>{n},
      std::vector<size_t>{sizeof(T)},
      span.data(),
      base);
  arr.attr("flags").attr("writeable") = false;
  return arr;
}

/// Zero-copy view into std::vector<T> memory as 1-D numpy array (read-only).
template <typename T>
inline py::array_t<T> toNumpyView(const std::vector<T>& vec, py::handle base) {
  return toNumpyView<T>(std::span<const T>(vec.data(), vec.size()), base);
}

// Validation / points array helpers

inline py::array_t<double, py::array::c_style | py::array::forcecast>
requirePointsArray(const py::handle& value, py::ssize_t columns) {
  auto arr = py::reinterpret_borrow<
      py::array_t<double, py::array::c_style | py::array::forcecast>>(value);
  if (arr.ndim() != 2 || arr.shape(1) != columns) {
    throw py::value_error("Expected numpy shape (N, columns).");
  }
  return arr;
}

inline bool isNumpyPointsArray(const py::handle& value, py::ssize_t columns) {
  if (!py::isinstance<py::array>(value)) {
    return false;
  }
  py::array arr = py::reinterpret_borrow<py::array>(value);
  return arr.ndim() == 2 && arr.shape(1) == columns;
}

// numpy -> C++ containers

/// Copy a 1-D numpy array into a std::vector<T>.
template <typename T>
void ndarrayToVector(std::vector<T>& vec, const py::array_t<T>& arr) {
  auto buf = arr.request();
  const size_t n = buf.size;
  vec.clear();
  vec.reserve(n);
  if (n > 0) {
    vec.resize(n);
    std::memcpy(vec.data(), buf.ptr, n * sizeof(T));
  }
}

/// Convert a column-major std::vector<double>(16) to glm::dmat4.
inline glm::dmat4 matrixFromVector(const std::vector<double>& v) {
  if (v.size() != 16) {
    throw std::invalid_argument("Matrix must have 16 elements");
  }
  return glm::dmat4(
      v[0],
      v[1],
      v[2],
      v[3],
      v[4],
      v[5],
      v[6],
      v[7],
      v[8],
      v[9],
      v[10],
      v[11],
      v[12],
      v[13],
      v[14],
      v[15]);
}

// Array creation / fill helpers  (used by glTF extraction)

template <typename T> py::array_t<T> create2dArray(size_t rows, size_t cols) {
  std::vector<size_t> shape = {rows, cols};
  return py::array_t<T>(shape);
}

template <typename T> py::array_t<T> create1dArray(size_t size) {
  return py::array_t<T>(size);
}

template <typename SrcT, typename DstT>
void fillPositionArray(
    py::array_t<DstT>& arr,
    const std::vector<SrcT>& src,
    const glm::dmat4& transform = glm::dmat4(1.0)) {
  auto data = arr.template mutable_unchecked<2>();
  for (size_t i = 0; i < src.size(); ++i) {
    glm::dvec4 pos(src[i].x, src[i].y, src[i].z, 1.0);
    glm::dvec4 transformed = transform * pos;
    data(i, 0) = static_cast<DstT>(transformed.x);
    data(i, 1) = static_cast<DstT>(transformed.y);
    data(i, 2) = static_cast<DstT>(transformed.z);
  }
}

template <typename T>
void fillNormalArray(
    py::array_t<T>& arr,
    const std::vector<T>& src,
    const glm::dmat4& nodeTransform) {
  glm::dmat3 rotScale(nodeTransform);
  glm::dmat3 normalTransform = glm::transpose(glm::inverse(rotScale));
  auto data = arr.template mutable_unchecked<2>();
  for (size_t i = 0; i < src.size(); ++i) {
    glm::dvec3 norm(src[i].x, src[i].y, src[i].z);
    glm::dvec3 transformed = normalTransform * norm;
    transformed = glm::normalize(transformed);
    data(i, 0) = static_cast<T>(transformed.x);
    data(i, 1) = static_cast<T>(transformed.y);
    data(i, 2) = static_cast<T>(transformed.z);
  }
}

template <typename T>
void fillUvArray(py::array_t<T>& arr, const std::vector<glm::vec<2, T>>& src) {
  auto data = arr.template mutable_unchecked<2>();
  for (size_t i = 0; i < src.size(); ++i) {
    data(i, 0) = src[i].x;
    data(i, 1) = src[i].y;
  }
}

template <typename SrcT, typename DstT>
void fillIndexArray(py::array_t<DstT>& arr, const std::vector<SrcT>& src) {
  auto data = arr.template mutable_unchecked<1>();
  for (size_t i = 0; i < src.size(); ++i) {
    data(i) = static_cast<DstT>(src[i]);
  }
}

// Raw pointer extraction (must be called while holding GIL)

/// Extract read-only double* + row count from a validated (N, cols) array.
struct NumpyPointsView {
  const double* data;
  py::ssize_t rows;
  py::ssize_t cols;
};

inline NumpyPointsView extractPoints(
    py::array_t<double, py::array::c_style | py::array::forcecast>& arr,
    py::ssize_t expectedCols) {
  if (arr.ndim() != 2 || arr.shape(1) != expectedCols) {
    throw py::value_error(
        "Expected numpy shape (N, " + std::to_string(expectedCols) + ").");
  }
  return {arr.data(), arr.shape(0), expectedCols};
}

/// Allocate output array and return mutable pointer + the array.
struct NumpyOutputBool {
  py::array_t<bool> array;
  bool* data;
};

inline NumpyOutputBool allocBool(py::ssize_t n) {
  py::array_t<bool> out(n);
  return {std::move(out), static_cast<bool*>(out.mutable_data())};
}

struct NumpyOutput1D {
  py::array_t<double> array;
  double* data;
};

inline NumpyOutput1D allocDouble1d(py::ssize_t n) {
  py::array_t<double> out(n);
  return {std::move(out), static_cast<double*>(out.mutable_data())};
}

struct NumpyOutput2D {
  py::array_t<double> array;
  double* data;
  py::ssize_t rows;
  py::ssize_t cols;
};

inline NumpyOutput2D allocDouble2d(py::ssize_t rows, py::ssize_t cols) {
  py::array_t<double> out({rows, cols});
  return {std::move(out), static_cast<double*>(out.mutable_data()), rows, cols};
}

// GIL-free batch operation templates

/**
 * Apply a unary predicate to N points, writing bool results.
 * The predicate receives (const double* row) with `cols` elements.
 * GIL is released for the entire loop.
 */
template <typename Predicate>
py::array_t<bool> batchPredicate3d(
    const py::handle& pointsHandle,
    py::ssize_t cols,
    Predicate&& pred) {
  auto arr = py::reinterpret_borrow<
      py::array_t<double, py::array::c_style | py::array::forcecast>>(
      pointsHandle);
  auto view = extractPoints(arr, cols);
  py::array_t<bool> result(view.rows);
  bool* out = static_cast<bool*>(result.mutable_data());
  const double* in = view.data;
  const py::ssize_t n = view.rows;
  const py::ssize_t c = view.cols;

  {
    py::gil_scoped_release release;
    for (py::ssize_t i = 0; i < n; ++i) {
      out[i] = pred(in + i * c);
    }
  }

  return result;
}

/**
 * Apply a unary function to N points, writing scalar double results.
 * GIL is released for the entire loop.
 */
template <typename Func>
py::array_t<double>
batchScalar3d(const py::handle& pointsHandle, py::ssize_t cols, Func&& func) {
  auto arr = py::reinterpret_borrow<
      py::array_t<double, py::array::c_style | py::array::forcecast>>(
      pointsHandle);
  auto view = extractPoints(arr, cols);
  py::array_t<double> result(view.rows);
  double* out = static_cast<double*>(result.mutable_data());
  const double* in = view.data;
  const py::ssize_t n = view.rows;
  const py::ssize_t c = view.cols;

  {
    py::gil_scoped_release release;
    for (py::ssize_t i = 0; i < n; ++i) {
      out[i] = func(in + i * c);
    }
  }

  return result;
}

/**
 * Apply a unary transform to N input points, writing M-component output points.
 * GIL is released for the entire loop.
 */
template <typename Func>
py::array_t<double> batchTransform(
    const py::handle& pointsHandle,
    py::ssize_t inCols,
    py::ssize_t outCols,
    Func&& func) {
  auto arr = py::reinterpret_borrow<
      py::array_t<double, py::array::c_style | py::array::forcecast>>(
      pointsHandle);
  auto view = extractPoints(arr, inCols);
  py::array_t<double> result({view.rows, outCols});
  double* out = static_cast<double*>(result.mutable_data());
  const double* in = view.data;
  const py::ssize_t n = view.rows;

  {
    py::gil_scoped_release release;
    for (py::ssize_t i = 0; i < n; ++i) {
      func(in + i * inCols, out + i * outCols);
    }
  }

  return result;
}

/**
 * Paired-input batch: two (N, cols) arrays -> (N,) scalar output.
 * GIL is released for the entire loop.
 */
template <typename Func>
py::array_t<double> batchPairedScalar(
    const py::handle& aHandle,
    const py::handle& bHandle,
    py::ssize_t cols,
    Func&& func) {
  auto aArr = py::reinterpret_borrow<
      py::array_t<double, py::array::c_style | py::array::forcecast>>(aHandle);
  auto bArr = py::reinterpret_borrow<
      py::array_t<double, py::array::c_style | py::array::forcecast>>(bHandle);
  auto a = extractPoints(aArr, cols);
  auto b = extractPoints(bArr, cols);
  if (a.rows != b.rows) {
    throw py::value_error("Input arrays must have the same number of rows.");
  }

  py::array_t<double> result(a.rows);
  double* out = static_cast<double*>(result.mutable_data());
  const double* aData = a.data;
  const double* bData = b.data;
  const py::ssize_t n = a.rows;

  {
    py::gil_scoped_release release;
    for (py::ssize_t i = 0; i < n; ++i) {
      out[i] = func(aData + i * cols, bData + i * cols);
    }
  }

  return result;
}

/**
 * Paired-input batch: two (N, cols) arrays -> (N, outCols) array.
 * GIL is released for the entire loop.
 */
template <typename Func>
py::array_t<double> batchPairedTransform(
    const py::handle& aHandle,
    const py::handle& bHandle,
    py::ssize_t inCols,
    py::ssize_t outCols,
    Func&& func) {
  auto aArr = py::reinterpret_borrow<
      py::array_t<double, py::array::c_style | py::array::forcecast>>(aHandle);
  auto bArr = py::reinterpret_borrow<
      py::array_t<double, py::array::c_style | py::array::forcecast>>(bHandle);
  auto a = extractPoints(aArr, inCols);
  auto b = extractPoints(bArr, inCols);
  if (a.rows != b.rows) {
    throw py::value_error("Input arrays must have the same number of rows.");
  }

  py::array_t<double> result({a.rows, outCols});
  double* out = static_cast<double*>(result.mutable_data());
  const double* aData = a.data;
  const double* bData = b.data;
  const py::ssize_t n = a.rows;

  {
    py::gil_scoped_release release;
    for (py::ssize_t i = 0; i < n; ++i) {
      func(aData + i * inCols, bData + i * inCols, out + i * outCols);
    }
  }

  return result;
}

// Matrix batch transform  (GIL-free)

/// Transform (N, 3) points by dmat4, returning (N, 3).
inline py::array_t<double> transformPointsByMatrix(
    const py::handle& pointsHandle,
    const glm::dmat4& matrix) {
  return batchTransform(
      pointsHandle,
      3,
      3,
      [&matrix](const double* in, double* out) {
        glm::dvec4 p(in[0], in[1], in[2], 1.0);
        glm::dvec4 r = matrix * p;
        out[0] = r.x;
        out[1] = r.y;
        out[2] = r.z;
      });
}

// Zero-copy byte helpers

/// Zero-copy numpy view of contiguous byte data. Keeps parent alive.
inline py::array_t<uint8_t>
bytesToNumpyView(const std::byte* data, size_t size, py::handle base) {
  if (size == 0) {
    return py::array_t<uint8_t>(std::vector<size_t>{0});
  }
  auto arr = py::array_t<uint8_t>(
      std::vector<size_t>{size},
      std::vector<size_t>{sizeof(uint8_t)},
      reinterpret_cast<const uint8_t*>(data),
      base);
  arr.attr("flags").attr("writeable") = false;
  return arr;
}

/// Copy Python bytes to std::vector<std::byte> via direct memcpy.
inline std::vector<std::byte> bytesToVector(const py::bytes& input) {
  char* buffer;
  Py_ssize_t length;
  PyBytes_AsStringAndSize(input.ptr(), &buffer, &length);
  std::vector<std::byte> out(static_cast<size_t>(length));
  if (length > 0) {
    std::memcpy(out.data(), buffer, static_cast<size_t>(length));
  }
  return out;
}

/// Copy numpy uint8 array to std::vector<std::byte>.
inline std::vector<std::byte>
numpyToByteVector(const py::array_t<uint8_t>& arr) {
  auto buf = arr.request();
  std::vector<std::byte> out(buf.size);
  if (buf.size > 0) {
    std::memcpy(out.data(), buf.ptr, static_cast<size_t>(buf.size));
  }
  return out;
}

/// Copy std::vector<std::byte> to Python bytes.
inline py::bytes vectorToBytes(const std::vector<std::byte>& input) {
  return py::bytes(
      reinterpret_cast<const char*>(input.data()),
      static_cast<py::ssize_t>(input.size()));
}

/// Convert a std::span<const std::byte> to Python bytes.
inline py::bytes spanToBytes(std::span<const std::byte> input) {
  return py::bytes(
      reinterpret_cast<const char*>(input.data()),
      static_cast<py::ssize_t>(input.size()));
}

/// Zero-copy writable numpy view of a mutable std::vector<std::byte>.
/// Keeps the parent Python object alive via pybind11's reference-counting.
inline py::array_t<uint8_t>
bytesVectorToNumpy(std::vector<std::byte>& vec, py::handle parent) {
  return py::array_t<uint8_t>(
      {static_cast<py::ssize_t>(vec.size())},
      {sizeof(uint8_t)},
      reinterpret_cast<uint8_t*>(vec.data()),
      parent);
}

/// Convert std::optional<T> to numpy via toNumpy(), or py::none().
template <typename T> py::object optionalToNumpy(const std::optional<T>& opt) {
  if (!opt)
    return py::none();
  return toNumpy(*opt);
}

/// Convert std::optional<double> to py::float_ or py::none().
inline py::object optionalToPy(const std::optional<double>& opt) {
  if (!opt)
    return py::none();
  return py::cast(*opt);
}

/// Convert a Python iterable of points to std::vector<glm::dvec<N>>.
template <int N>
std::vector<glm::vec<N, double>> iterableToVec(const py::iterable& items) {
  std::vector<glm::vec<N, double>> out;
  out.reserve(static_cast<size_t>(py::len(items)));
  for (py::handle h : items) {
    out.push_back(toDvec<N>(h));
  }
  return out;
}

} // namespace CesiumPython
