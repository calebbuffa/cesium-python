#include "FutureTemplates.h"
#include "HttpHeaderConversions.h"
#include "JsonConversions.h"
#include "NumpyConversions.h"
#include "ZipArchive.h"

#include <CesiumAsync/AsyncSystem.h>
#include <CesiumAsync/CacheItem.h>
#include <CesiumAsync/CachingAssetAccessor.h>
#include <CesiumAsync/CesiumIonAssetAccessor.h>
#include <CesiumAsync/GunzipAssetAccessor.h>
#include <CesiumAsync/IAssetAccessor.h>
#include <CesiumAsync/IAssetRequest.h>
#include <CesiumAsync/IAssetResponse.h>
#include <CesiumAsync/ICacheDatabase.h>
#include <CesiumAsync/ITaskProcessor.h>
#include <CesiumAsync/NetworkAssetDescriptor.h>
#include <CesiumAsync/Promise.h>
#include <CesiumAsync/SharedFuture.h>
#include <CesiumAsync/SqliteCache.h>
#include <CesiumAsync/ThreadPool.h>
#include <CesiumUtility/Result.h>

#include <pybind11/functional.h>
#include <pybind11/numpy.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <mutex>
#include <optional>
#include <queue>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace py = pybind11;

namespace {

using FutureVoid = CesiumAsync::Future<void>;
using SharedFutureVoid = CesiumAsync::SharedFuture<void>;
using PromiseVoid = CesiumAsync::Promise<void>;

using AssetRequestPtr = std::shared_ptr<CesiumAsync::IAssetRequest>;
using FutureAssetRequest = CesiumAsync::Future<AssetRequestPtr>;
using SharedFutureAssetRequest = CesiumAsync::SharedFuture<AssetRequestPtr>;
using PromiseAssetRequest = CesiumAsync::Promise<AssetRequestPtr>;

using ByteVector = std::vector<std::byte>;
using FutureBytes = CesiumAsync::Future<ByteVector>;
using SharedFutureBytes = CesiumAsync::SharedFuture<ByteVector>;
using PromiseBytes = CesiumAsync::Promise<ByteVector>;

// Generic py::object specialization — lets Python users create arbitrary
// Future/Promise without needing a C++ type-specific binding.
using FutureObject = CesiumAsync::Future<py::object>;
using SharedFutureObject = CesiumAsync::SharedFuture<py::object>;
using PromiseObject = CesiumAsync::Promise<py::object>;

class PyITaskProcessor : public CesiumAsync::ITaskProcessor {
public:
  using CesiumAsync::ITaskProcessor::ITaskProcessor;

  void startTask(std::function<void()> f) override {
    // Acquire the GIL FIRST — py::cpp_function and py::get_overload both
    // allocate Python objects, so the GIL must already be held.  cesium-native
    // may call startTask from any C++ thread.
    py::gil_scoped_acquire gil;

    // Wrap the C++ callback so the GIL is released during execution.
    // Without this, every cesium-native async task (tile decompression,
    // content parsing, etc.) serialises on the GIL — killing throughput.
    auto wrapped = py::cpp_function(
        [fn = std::make_shared<std::function<void()>>(std::move(f))]() {
          py::gil_scoped_release release;
          (*fn)();
        });
    py::function overload = py::get_overload(
        static_cast<const CesiumAsync::ITaskProcessor*>(this),
        "start_task");
    if (overload) {
      overload(wrapped);
    }
  }
};

// ---------------------------------------------------------------------------
// NativeTaskProcessor — pure C++ thread pool, zero GIL overhead
// ---------------------------------------------------------------------------
// Unlike PyITaskProcessor (which bounces through Python for every task),
// NativeTaskProcessor dispatches work on native threads.  cesium-native's
// internal task callbacks are pure C++ and never need the GIL, so going
// through the Python trampoline just to release the GIL again is wasteful.
//
// Use this from Python:
//   proc = cesium.async_.NativeTaskProcessor()    # or (num_threads=8)
//   async_system = cesium.async_.AsyncSystem(proc)
class NativeTaskProcessor final : public CesiumAsync::ITaskProcessor {
public:
  explicit NativeTaskProcessor(int numThreads = 0) : _shutdown(false) {
    if (numThreads <= 0) {
      numThreads = static_cast<int>(
          std::min<unsigned>(std::thread::hardware_concurrency(), 16u));
      if (numThreads < 1)
        numThreads = 4;
    }
    _threads.reserve(static_cast<size_t>(numThreads));
    for (int i = 0; i < numThreads; ++i) {
      _threads.emplace_back([this]() { workerLoop(); });
    }
  }

  ~NativeTaskProcessor() override { shutdown(); }

  void shutdown() {
    {
      std::lock_guard<std::mutex> lock(_mutex);
      if (_shutdown)
        return;
      _shutdown = true;
    }
    _cv.notify_all();
    for (auto& t : _threads) {
      if (t.joinable())
        t.join();
    }
  }

  void startTask(std::function<void()> f) override {
    {
      std::lock_guard<std::mutex> lock(_mutex);
      _tasks.push(std::move(f));
    }
    _cv.notify_one();
  }

private:
  void workerLoop() {
    while (true) {
      std::function<void()> task;
      {
        std::unique_lock<std::mutex> lock(_mutex);
        _cv.wait(lock, [&] { return _shutdown || !_tasks.empty(); });
        if (_shutdown && _tasks.empty())
          return;
        task = std::move(_tasks.front());
        _tasks.pop();
      }
      task();
    }
  }

  bool _shutdown;
  std::mutex _mutex;
  std::condition_variable _cv;
  std::queue<std::function<void()>> _tasks;
  std::vector<std::thread> _threads;
};

// ---------------------------------------------------------------------------
// Concrete Python-constructible IAssetResponse
// ---------------------------------------------------------------------------
class PyAssetResponse : public CesiumAsync::IAssetResponse {
public:
  PyAssetResponse(
      uint16_t statusCode,
      std::string contentType,
      CesiumAsync::HttpHeaders headers,
      std::vector<std::byte> data)
      : statusCode_(statusCode),
        contentType_(std::move(contentType)),
        headers_(std::move(headers)),
        data_(std::move(data)) {}

  uint16_t statusCode() const override { return statusCode_; }
  std::string contentType() const override { return contentType_; }
  const CesiumAsync::HttpHeaders& headers() const override { return headers_; }
  std::span<const std::byte> data() const override {
    return std::span<const std::byte>(data_.data(), data_.size());
  }

private:
  uint16_t statusCode_;
  std::string contentType_;
  CesiumAsync::HttpHeaders headers_;
  std::vector<std::byte> data_;
};

// ---------------------------------------------------------------------------
// Concrete Python-constructible IAssetRequest
// ---------------------------------------------------------------------------
class PyAssetRequest : public CesiumAsync::IAssetRequest {
public:
  PyAssetRequest(
      std::string method,
      std::string url,
      CesiumAsync::HttpHeaders headers,
      std::shared_ptr<CesiumAsync::IAssetResponse> response)
      : method_(std::move(method)),
        url_(std::move(url)),
        headers_(std::move(headers)),
        response_(std::move(response)) {}

  const std::string& method() const override { return method_; }
  const std::string& url() const override { return url_; }
  const CesiumAsync::HttpHeaders& headers() const override { return headers_; }
  const CesiumAsync::IAssetResponse* response() const override {
    return response_.get();
  }

private:
  std::string method_;
  std::string url_;
  CesiumAsync::HttpHeaders headers_;
  std::shared_ptr<CesiumAsync::IAssetResponse> response_;
};

// ---------------------------------------------------------------------------
// ArchiveAssetAccessor — decorator that reads .3tz/.zip archive entries,
// falling through to an inner IAssetAccessor for non-archive URLs.
//
// Follows the same wrapper pattern as CachingAssetAccessor and
// GunzipAssetAccessor: wraps a shared_ptr<IAssetAccessor>, intercepts
// URLs that point into the archive, and delegates everything else.
// ---------------------------------------------------------------------------

std::string percentDecode(const std::string& input) {
  std::string out;
  out.reserve(input.size());
  for (size_t i = 0; i < input.size(); ++i) {
    if (input[i] == '%' && i + 2 < input.size()) {
      auto hi = input[i + 1];
      auto lo = input[i + 2];
      auto fromHex = [](char c) -> int {
        if (c >= '0' && c <= '9')
          return c - '0';
        if (c >= 'a' && c <= 'f')
          return 10 + c - 'a';
        if (c >= 'A' && c <= 'F')
          return 10 + c - 'A';
        return -1;
      };
      int h = fromHex(hi), l = fromHex(lo);
      if (h >= 0 && l >= 0) {
        out += static_cast<char>((h << 4) | l);
        i += 2;
        continue;
      }
    }
    out += input[i];
  }
  return out;
}

std::string guessContentType(const std::string& entry) {
  auto dot = entry.rfind('.');
  if (dot == std::string::npos)
    return "application/octet-stream";
  auto ext = entry.substr(dot);
  std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) {
    return std::tolower(c);
  });
  if (ext == ".json")
    return "application/json";
  if (ext == ".glb")
    return "model/gltf-binary";
  if (ext == ".gltf")
    return "model/gltf+json";
  if (ext == ".png")
    return "image/png";
  if (ext == ".jpg" || ext == ".jpeg")
    return "image/jpeg";
  if (ext == ".ktx2")
    return "image/ktx2";
  if (ext == ".subtree")
    return "application/octet-stream";
  return "application/octet-stream";
}

class ArchiveAssetAccessor : public CesiumAsync::IAssetAccessor {
public:
  ArchiveAssetAccessor(
      const std::shared_ptr<CesiumAsync::IAssetAccessor>& pAssetAccessor,
      std::string archivePath)
      : _pAssetAccessor(pAssetAccessor), _archivePath(std::move(archivePath)) {
    std::replace(_archivePath.begin(), _archivePath.end(), '\\', '/');
  }

  ~ArchiveAssetAccessor() override {
    std::lock_guard<std::mutex> lock(_mutex);
    for (auto* z : _pool) {
      if (z)
        zip_close(z);
    }
  }

  CesiumAsync::Future<std::shared_ptr<CesiumAsync::IAssetRequest>>
  get(const CesiumAsync::AsyncSystem& asyncSystem,
      const std::string& url,
      const std::vector<THeader>& headers) override {
    auto entry = _tryExtractEntry(url);
    if (!entry.has_value()) {
      // Not an archive URL — delegate to the inner accessor.
      return _pAssetAccessor->get(asyncSystem, url, headers);
    }
    std::string entryName = std::move(*entry);
    return asyncSystem.runInWorkerThread(
        [this, url, entryName = std::move(entryName)]()
            -> std::shared_ptr<CesiumAsync::IAssetRequest> {
          return _readEntry(url, entryName);
        });
  }

  CesiumAsync::Future<std::shared_ptr<CesiumAsync::IAssetRequest>> request(
      const CesiumAsync::AsyncSystem& asyncSystem,
      const std::string& verb,
      const std::string& url,
      const std::vector<THeader>& headers,
      const std::span<const std::byte>& contentPayload) override {
    auto entry = _tryExtractEntry(url);
    if (!entry.has_value()) {
      return _pAssetAccessor
          ->request(asyncSystem, verb, url, headers, contentPayload);
    }
    // Archives are read-only; ignore verb/payload for archive entries.
    return get(asyncSystem, url, headers);
  }

  void tick() noexcept override { _pAssetAccessor->tick(); }

private:
  std::shared_ptr<CesiumAsync::IAssetRequest>
  _readEntry(const std::string& url, const std::string& entry) {
    auto* handle = _borrowHandle();
    if (!handle) {
      auto resp = std::make_shared<PyAssetResponse>(
          500,
          "",
          CesiumAsync::HttpHeaders{},
          std::vector<std::byte>{});
      return std::make_shared<PyAssetRequest>(
          "GET",
          url,
          CesiumAsync::HttpHeaders{},
          resp);
    }

    std::vector<std::byte> data;
    bool ok = CesiumIo::readZipEntry(handle, entry, data);
    _returnHandle(handle);

    if (!ok) {
      auto resp = std::make_shared<PyAssetResponse>(
          404,
          "",
          CesiumAsync::HttpHeaders{},
          std::vector<std::byte>{});
      return std::make_shared<PyAssetRequest>(
          "GET",
          url,
          CesiumAsync::HttpHeaders{},
          resp);
    }

    auto resp = std::make_shared<PyAssetResponse>(
        200,
        guessContentType(entry),
        CesiumAsync::HttpHeaders{},
        std::move(data));
    return std::make_shared<PyAssetRequest>(
        "GET",
        url,
        CesiumAsync::HttpHeaders{},
        resp);
  }

  zip_t* _borrowHandle() {
    {
      std::lock_guard<std::mutex> lock(_mutex);
      if (!_pool.empty()) {
        auto* h = _pool.back();
        _pool.pop_back();
        return h;
      }
    }
    int err = 0;
    return zip_open(_archivePath.c_str(), ZIP_RDONLY, &err);
  }

  void _returnHandle(zip_t* h) {
    if (!h)
      return;
    std::lock_guard<std::mutex> lock(_mutex);
    _pool.push_back(h);
  }

  // Returns std::nullopt if the URL is NOT an archive reference
  // (i.e. it should be delegated to the inner accessor).
  std::optional<std::string> _tryExtractEntry(const std::string& url) const {
    std::string norm = percentDecode(url);
    std::replace(norm.begin(), norm.end(), '\\', '/');

    // Strip file:// scheme.
    if (norm.size() > 7 &&
        std::equal(
            norm.begin(),
            norm.begin() + 7,
            "file://",
            [](char a, char b) { return std::tolower(a) == b; })) {
      norm = norm.substr(7);
      if (norm.size() >= 3 && norm[0] == '/' && norm[2] == ':') {
        norm = norm.substr(1);
      }
    }

    std::string normLower = norm;
    std::transform(
        normLower.begin(),
        normLower.end(),
        normLower.begin(),
        [](unsigned char c) { return std::tolower(c); });

    // 1) Marker-based: .../data.3tz/subtree/0.json
    for (const char* ext : {".3tz/", ".zip/"}) {
      auto pos = normLower.find(ext);
      if (pos != std::string::npos) {
        auto entry = norm.substr(pos + std::strlen(ext));
        while (!entry.empty() && entry[0] == '/')
          entry = entry.substr(1);
        return entry.empty() ? std::string("tileset.json") : entry;
      }
    }

    // 2) Path starts with the archive base path.
    std::string baseLower = _archivePath;
    std::transform(
        baseLower.begin(),
        baseLower.end(),
        baseLower.begin(),
        [](unsigned char c) { return std::tolower(c); });
    if (normLower.size() >= baseLower.size() &&
        normLower.substr(0, baseLower.size()) == baseLower) {
      auto entry = norm.substr(_archivePath.size());
      while (!entry.empty() && entry[0] == '/')
        entry = entry.substr(1);
      return entry.empty() ? std::string("tileset.json") : entry;
    }

    // Not an archive URL — return nullopt to signal fallthrough.
    return std::nullopt;
  }

  std::shared_ptr<CesiumAsync::IAssetAccessor> _pAssetAccessor;
  std::string _archivePath;
  std::mutex _mutex;
  std::vector<zip_t*> _pool;
};

// ---------------------------------------------------------------------------
// Trampoline: allows Python subclasses of IAssetAccessor
// ---------------------------------------------------------------------------
class PyIAssetAccessor : public CesiumAsync::IAssetAccessor {
public:
  using CesiumAsync::IAssetAccessor::IAssetAccessor;

  CesiumAsync::Future<std::shared_ptr<CesiumAsync::IAssetRequest>>
  get(const CesiumAsync::AsyncSystem& asyncSystem,
      const std::string& url,
      const std::vector<THeader>& headers) override {
    py::gil_scoped_acquire gil;
    py::dict pyHeaders;
    for (const auto& h : headers) {
      pyHeaders[py::str(h.first)] = py::str(h.second);
    }
    auto overrideFn = pybind11::get_override(
        static_cast<const CesiumAsync::IAssetAccessor*>(this),
        "get");
    if (overrideFn) {
      auto result = overrideFn(asyncSystem, url, pyHeaders);
      auto* ptr = result.cast<
          CesiumAsync::Future<std::shared_ptr<CesiumAsync::IAssetRequest>>*>();
      return std::move(*ptr);
    }
    throw std::runtime_error(
        "Pure virtual IAssetAccessor.get() not implemented");
  }

  CesiumAsync::Future<std::shared_ptr<CesiumAsync::IAssetRequest>> request(
      const CesiumAsync::AsyncSystem& asyncSystem,
      const std::string& verb,
      const std::string& url,
      const std::vector<THeader>& headers,
      const std::span<const std::byte>& contentPayload) override {
    py::gil_scoped_acquire gil;
    py::dict pyHeaders;
    for (const auto& h : headers) {
      pyHeaders[py::str(h.first)] = py::str(h.second);
    }
    py::bytes pyPayload(
        reinterpret_cast<const char*>(contentPayload.data()),
        contentPayload.size());
    auto overrideFn = pybind11::get_override(
        static_cast<const CesiumAsync::IAssetAccessor*>(this),
        "request");
    if (overrideFn) {
      auto result = overrideFn(asyncSystem, verb, url, pyHeaders, pyPayload);
      auto* ptr = result.cast<
          CesiumAsync::Future<std::shared_ptr<CesiumAsync::IAssetRequest>>*>();
      return std::move(*ptr);
    }
    throw std::runtime_error(
        "Pure virtual IAssetAccessor.request() not implemented");
  }

  void tick() noexcept override {
    try {
      py::gil_scoped_acquire gil;
      auto overrideFn = pybind11::get_override(
          static_cast<const CesiumAsync::IAssetAccessor*>(this),
          "tick");
      if (overrideFn) {
        overrideFn();
      }
    } catch (...) {
      // tick is noexcept — swallow exceptions
    }
  }
};

} // namespace

void initAsyncBindings(py::module& m) {
  py::class_<CesiumAsync::CaseInsensitiveCompare>(m, "CaseInsensitiveCompare")
      .def(py::init<>())
      .def("__call__", &CesiumAsync::CaseInsensitiveCompare::operator());

  py::class_<
      CesiumAsync::IAssetResponse,
      std::shared_ptr<CesiumAsync::IAssetResponse>>(m, "IAssetResponse")
      .def("status_code", &CesiumAsync::IAssetResponse::statusCode)
      .def("content_type", &CesiumAsync::IAssetResponse::contentType)
      .def(
          "headers",
          [](const CesiumAsync::IAssetResponse& self) {
            return CesiumPython::headersToPyDict(self.headers());
          })
      .def("data", [](const CesiumAsync::IAssetResponse& self) {
        py::handle base = py::cast(&self, py::return_value_policy::reference);
        auto d = self.data();
        return CesiumPython::bytesToNumpyView(d.data(), d.size(), base);
      });

  py::class_<
      PyAssetResponse,
      CesiumAsync::IAssetResponse,
      std::shared_ptr<PyAssetResponse>>(m, "AssetResponse")
      .def(
          py::init([](uint16_t statusCode,
                      const std::string& contentType,
                      const py::dict& headers,
                      const py::bytes& data) {
            return std::make_shared<PyAssetResponse>(
                statusCode,
                contentType,
                CesiumPython::dictToHeaders(headers),
                CesiumPython::bytesToVector(data));
          }),
          py::arg("status_code"),
          py::arg("content_type"),
          py::arg("headers") = py::dict(),
          py::arg("data") = py::bytes());

  py::class_<
      CesiumAsync::IAssetRequest,
      std::shared_ptr<CesiumAsync::IAssetRequest>>(m, "IAssetRequest")
      .def("method", &CesiumAsync::IAssetRequest::method)
      .def("url", &CesiumAsync::IAssetRequest::url)
      .def(
          "headers",
          [](const CesiumAsync::IAssetRequest& self) {
            return CesiumPython::headersToPyDict(self.headers());
          })
      .def(
          "response",
          [](const CesiumAsync::IAssetRequest& self) -> py::object {
            const CesiumAsync::IAssetResponse* response = self.response();
            if (!response) {
              return py::none();
            }
            return py::cast(response, py::return_value_policy::reference);
          });

  py::class_<
      PyAssetRequest,
      CesiumAsync::IAssetRequest,
      std::shared_ptr<PyAssetRequest>>(m, "AssetRequest")
      .def(
          py::init([](const std::string& method,
                      const std::string& url,
                      const py::dict& headers,
                      std::shared_ptr<CesiumAsync::IAssetResponse> response) {
            return std::make_shared<PyAssetRequest>(
                method,
                url,
                CesiumPython::dictToHeaders(headers),
                std::move(response));
          }),
          py::arg("method"),
          py::arg("url"),
          py::arg("headers") = py::dict(),
          py::arg("response") = nullptr);

  py::class_<
      CesiumAsync::ITaskProcessor,
      PyITaskProcessor,
      std::shared_ptr<CesiumAsync::ITaskProcessor>>(m, "ITaskProcessor")
      .def(py::init<>())
      .def("start_task", &CesiumAsync::ITaskProcessor::startTask);

  py::class_<
      NativeTaskProcessor,
      CesiumAsync::ITaskProcessor,
      std::shared_ptr<NativeTaskProcessor>>(
      m,
      "NativeTaskProcessor",
      "Pure-C++ thread-pool ITaskProcessor — zero GIL overhead.\n\n"
      "cesium-native's internal tasks (tile decompression, content parsing,\n"
      "etc.) are pure C++ and never need the Python GIL.  Using a\n"
      "NativeTaskProcessor instead of a Python ``ITaskProcessor`` subclass\n"
      "eliminates all GIL acquisition/release overhead from the task\n"
      "scheduling hot path, enabling full parallel tile loading.\n\n"
      "Example::\n\n"
      "    proc = cesium.async_.NativeTaskProcessor()  # auto thread count\n"
      "    proc = cesium.async_.NativeTaskProcessor(num_threads=8)\n"
      "    async_system = cesium.async_.AsyncSystem(proc)\n")
      .def(
          py::init<int>(),
          py::arg("num_threads") = 0,
          "Create a native task processor.\n\n"
          "Parameters\n"
          "----------\n"
          "num_threads : int\n"
          "    Number of worker threads.  0 (default) = "
          "min(hardware_concurrency, 16).\n")
      .def(
          "shutdown",
          &NativeTaskProcessor::shutdown,
          "Stop all worker threads and drain the task queue.\n\n"
          "Safe to call multiple times.  Also called automatically on "
          "destruction.\n");

  // GIL-free file I/O helper for Python asset accessors
  m.def(
      "read_file_bytes",
      [](const std::string& path) -> py::bytes {
        // Read the file entirely in C++ with the GIL released.
        // This is the key performance fix for Python IAssetAccessor
        // implementations: Path.read_bytes() holds the GIL for the
        // entire read, serialising all tile I/O across threads.
        std::string content;
        {
          py::gil_scoped_release release;
          std::ifstream file(path, std::ios::binary | std::ios::ate);
          if (!file) {
            throw std::runtime_error("File not found: " + path);
          }
          auto size = file.tellg();
          file.seekg(0);
          content.resize(static_cast<size_t>(size));
          file.read(content.data(), size);
          if (!file) {
            throw std::runtime_error("Failed to read file: " + path);
          }
        }
        // GIL is re-acquired here (scoped_release destroyed) before
        // constructing the Python bytes object.
        return py::bytes(content.data(), content.size());
      },
      py::arg("path"),
      "Read a file into bytes with the GIL released during I/O.\n\n"
      "Use this instead of ``Path.read_bytes()`` in Python ``IAssetAccessor``\n"
      "implementations to avoid serialising tile I/O on the GIL.\n"
      "The GIL is only held briefly to construct the returned ``bytes`` "
      "object.\n\n"
      "Parameters\n"
      "----------\n"
      "path : str\n"
      "    Absolute filesystem path to read.\n\n"
      "Returns\n"
      "-------\n"
      "bytes\n"
      "    File contents.\n");

  py::class_<
      CesiumAsync::IAssetAccessor,
      PyIAssetAccessor,
      std::shared_ptr<CesiumAsync::IAssetAccessor>>(m, "IAssetAccessor")
      .def(py::init<>())
      .def(
          "get",
          [](CesiumAsync::IAssetAccessor& self,
             const CesiumAsync::AsyncSystem& asyncSystem,
             const std::string& url,
             const py::dict& headers) {
            auto headerPairs = CesiumPython::dictToHeaderPairs(headers);
            py::gil_scoped_release release;
            return self.get(asyncSystem, url, headerPairs);
          },
          py::arg("async_system"),
          py::arg("url"),
          py::arg("headers") = py::dict())
      .def(
          "request",
          [](CesiumAsync::IAssetAccessor& self,
             const CesiumAsync::AsyncSystem& asyncSystem,
             const std::string& verb,
             const std::string& url,
             const py::dict& headers,
             const py::bytes& contentPayload) {
            std::vector<std::byte> payload =
                CesiumPython::bytesToVector(contentPayload);
            auto headerPairs = CesiumPython::dictToHeaderPairs(headers);
            py::gil_scoped_release release;
            return self.request(
                asyncSystem,
                verb,
                url,
                headerPairs,
                std::span<const std::byte>(payload.data(), payload.size()));
          },
          py::arg("async_system"),
          py::arg("verb"),
          py::arg("url"),
          py::arg("headers") = py::dict(),
          py::arg("content_payload") = py::bytes())
      .def(
          "tick",
          &CesiumAsync::IAssetAccessor::tick,
          py::call_guard<py::gil_scoped_release>());

  py::class_<CesiumAsync::ThreadPool>(m, "ThreadPool")
      .def(py::init<int32_t>(), py::arg("number_of_threads"));

  auto futureVoid = py::class_<FutureVoid>(m, "FutureVoid");
  CesiumPython::bindFutureVoid(futureVoid);

  auto sharedFutureVoid = py::class_<SharedFutureVoid>(m, "SharedFutureVoid");
  CesiumPython::bindSharedFutureVoid(sharedFutureVoid);

  auto promiseVoid = py::class_<PromiseVoid>(m, "PromiseVoid");
  CesiumPython::bindPromiseVoid(promiseVoid);

  auto futureAssetRequest =
      py::class_<FutureAssetRequest>(m, "FutureAssetRequest");
  CesiumPython::bindFuture(futureAssetRequest);

  auto sharedFutureAssetRequest =
      py::class_<SharedFutureAssetRequest>(m, "SharedFutureAssetRequest");
  CesiumPython::bindSharedFuture(sharedFutureAssetRequest);

  // FutureBytes / SharedFutureBytes — custom wait() returning py::bytes
  py::class_<FutureBytes>(m, "FutureBytes")
      .def(
          "wait",
          [](FutureBytes& self) {
            ByteVector v;
            {
              py::gil_scoped_release release;
              v = self.wait();
            }
            return py::bytes(reinterpret_cast<const char*>(v.data()), v.size());
          })
      .def(
          "wait_in_main_thread",
          [](FutureBytes& self) {
            ByteVector v;
            {
              py::gil_scoped_release release;
              v = self.waitInMainThread();
            }
            return py::bytes(reinterpret_cast<const char*>(v.data()), v.size());
          })
      .def("is_ready", &FutureBytes::isReady)
      .def("share", [](FutureBytes& self) { return std::move(self).share(); });

  py::class_<SharedFutureBytes>(m, "SharedFutureBytes")
      .def(
          "wait",
          [](const SharedFutureBytes& self) {
            ByteVector v;
            {
              py::gil_scoped_release release;
              v = self.wait();
            }
            return py::bytes(reinterpret_cast<const char*>(v.data()), v.size());
          })
      .def(
          "wait_in_main_thread",
          [](SharedFutureBytes& self) {
            ByteVector v;
            {
              py::gil_scoped_release release;
              v = self.waitInMainThread();
            }
            return py::bytes(reinterpret_cast<const char*>(v.data()), v.size());
          })
      .def("is_ready", &SharedFutureBytes::isReady);

  auto promiseAssetRequest =
      py::class_<PromiseAssetRequest>(m, "PromiseAssetRequest");
  CesiumPython::bindPromise<PromiseAssetRequest, AssetRequestPtr>(
      promiseAssetRequest);

  auto promiseBytes =
      py::class_<PromiseBytes>(m, "PromiseBytes")
          .def(
              "resolve",
              [](const PromiseBytes& self, py::bytes value) {
                std::string_view sv(value);
                ByteVector bv(
                    reinterpret_cast<const std::byte*>(sv.data()),
                    reinterpret_cast<const std::byte*>(sv.data() + sv.size()));
                self.resolve(std::move(bv));
              },
              py::arg("value"))
          .def(
              "reject",
              [](const PromiseBytes& self, const std::string& message) {
                self.reject(std::runtime_error(message));
              },
              py::arg("message"))
          .def_property_readonly("future", &PromiseBytes::getFuture);

  // --- Generic Future/SharedFuture/Promise (py::object payload) -----------
  // Registered as "Future", "SharedFuture", "Promise" — the primary generic
  // classes users interact with.  Supports subscript syntax at runtime via
  // __class_getitem__:  Future[int], Promise[MyResult], etc.
  // Type-specific variants (FutureVoid, FutureAssetRequest, …) remain for
  // C++ API return types.
  auto futureCls = py::class_<FutureObject>(m, "Future");
  CesiumPython::bindFuture(futureCls);
  futureCls.def_static(
      "__class_getitem__",
      [futureCls](py::object /* item */) { return futureCls; });

  auto sharedFutureCls =
      py::class_<SharedFutureObject>(m, "SharedFuture");
  CesiumPython::bindSharedFuture(sharedFutureCls);
  sharedFutureCls.def_static(
      "__class_getitem__",
      [sharedFutureCls](py::object /* item */) { return sharedFutureCls; });

  auto promiseCls = py::class_<PromiseObject>(m, "Promise");
  promiseCls
      .def(
          "resolve",
          [](const PromiseObject& self, py::object value) {
            self.resolve(std::move(value));
          },
          py::arg("value"))
      .def(
          "reject",
          [](const PromiseObject& self, const std::string& message) {
            self.reject(std::runtime_error(message));
          },
          py::arg("message"))
      .def_property_readonly("future", &PromiseObject::getFuture);
  promiseCls.def_static(
      "__class_getitem__",
      [promiseCls](py::object /* item */) { return promiseCls; });

  py::class_<CesiumAsync::AsyncSystem>(m, "AsyncSystem")
      .def(
          py::init<const std::shared_ptr<CesiumAsync::ITaskProcessor>&>(),
          py::arg("task_processor"))
      .def(
          "dispatch_main_thread_tasks",
          &CesiumAsync::AsyncSystem::dispatchMainThreadTasks,
          py::call_guard<py::gil_scoped_release>())
      .def(
          "dispatch_one_main_thread_task",
          &CesiumAsync::AsyncSystem::dispatchOneMainThreadTask,
          py::call_guard<py::gil_scoped_release>())
      .def(
          "create_thread_pool",
          &CesiumAsync::AsyncSystem::createThreadPool,
          py::arg("number_of_threads"))
      .def(
          "create_resolved_future_asset_request",
          [](const CesiumAsync::AsyncSystem& self,
             const AssetRequestPtr& request) {
            return self.createResolvedFuture<AssetRequestPtr>(
                AssetRequestPtr(request));
          })
      .def(
          "create_promise_void",
          [](const CesiumAsync::AsyncSystem& self) {
            return self.createPromise<void>();
          })
      .def(
          "create_promise_asset_request",
          [](const CesiumAsync::AsyncSystem& self) {
            return self.createPromise<AssetRequestPtr>();
          })
      .def(
          "create_promise_bytes",
          [](const CesiumAsync::AsyncSystem& self) {
            return self.createPromise<ByteVector>();
          })
      .def(
          "create_promise",
          [](const CesiumAsync::AsyncSystem& self) {
            return self.createPromise<py::object>();
          },
          "Create a generic Promise that can be resolved with any Python "
          "object.")
      .def(
          "create_resolved_future",
          [](const CesiumAsync::AsyncSystem& self, py::object value) {
            return self.createResolvedFuture<py::object>(std::move(value));
          },
          py::arg("value"),
          "Create a Future already resolved with the given value.")
      .def(
          "run_in_worker_thread",
          [](const CesiumAsync::AsyncSystem& self, py::function callback) {
            return self.runInWorkerThread(
                [callback = std::move(callback)]() mutable {
                  py::gil_scoped_acquire gil;
                  callback();
                });
          },
          py::arg("callback"))
      .def(
          "run_in_main_thread",
          [](const CesiumAsync::AsyncSystem& self, py::function callback) {
            return self.runInMainThread(
                [callback = std::move(callback)]() mutable {
                  py::gil_scoped_acquire gil;
                  callback();
                });
          },
          py::arg("callback"))
      .def(
          "run_in_thread_pool",
          [](const CesiumAsync::AsyncSystem& self,
             const CesiumAsync::ThreadPool& threadPool,
             py::function callback) {
            return self.runInThreadPool(
                threadPool,
                [callback = std::move(callback)]() mutable {
                  py::gil_scoped_acquire gil;
                  callback();
                });
          },
          py::arg("thread_pool"),
          py::arg("callback"))
      .def("__eq__", &CesiumAsync::AsyncSystem::operator==, py::is_operator())
      .def("__ne__", &CesiumAsync::AsyncSystem::operator!=, py::is_operator())
      .def(
          "all_void",
          [](const CesiumAsync::AsyncSystem& self, py::list futures) {
            std::vector<FutureVoid> vec;
            vec.reserve(futures.size());
            for (auto& item : futures) {
              vec.push_back(std::move(item.cast<FutureVoid&>()));
            }
            return self.all<void>(std::move(vec));
          },
          py::arg("futures"));

  py::class_<CesiumAsync::CacheResponse>(m, "CacheResponse")
      .def(
          py::init([](uint16_t statusCode,
                      CesiumAsync::HttpHeaders headers,
                      std::vector<std::byte> data) {
            return CesiumAsync::CacheResponse(
                statusCode,
                std::move(headers),
                std::move(data));
          }))
      .def_readwrite("status_code", &CesiumAsync::CacheResponse::statusCode)
      .def_property(
          "headers",
          [](const CesiumAsync::CacheResponse& self) {
            return CesiumPython::headersToPyDict(self.headers);
          },
          [](CesiumAsync::CacheResponse& self, const py::dict& headers) {
            self.headers = CesiumPython::dictToHeaders(headers);
          })
      .def_property(
          "data",
          [](const CesiumAsync::CacheResponse& self) {
            return CesiumPython::vectorToBytes(self.data);
          },
          [](CesiumAsync::CacheResponse& self, const py::bytes& data) {
            self.data = CesiumPython::bytesToVector(data);
          });

  py::class_<CesiumAsync::CacheRequest>(m, "CacheRequest")
      .def(
          py::init([](CesiumAsync::HttpHeaders headers,
                      std::string method,
                      std::string url) {
            return CesiumAsync::CacheRequest(
                std::move(headers),
                std::move(method),
                std::move(url));
          }))
      .def_property(
          "headers",
          [](const CesiumAsync::CacheRequest& self) {
            return CesiumPython::headersToPyDict(self.headers);
          },
          [](CesiumAsync::CacheRequest& self, const py::dict& headers) {
            self.headers = CesiumPython::dictToHeaders(headers);
          })
      .def_readwrite("method", &CesiumAsync::CacheRequest::method)
      .def_readwrite("url", &CesiumAsync::CacheRequest::url);

  py::class_<CesiumAsync::CacheItem>(m, "CacheItem")
      .def(
          py::init([](std::time_t expiryTime,
                      CesiumAsync::CacheRequest cacheRequest,
                      CesiumAsync::CacheResponse cacheResponse) {
            return CesiumAsync::CacheItem(
                expiryTime,
                std::move(cacheRequest),
                std::move(cacheResponse));
          }))
      .def_readwrite("expiry_time", &CesiumAsync::CacheItem::expiryTime)
      .def_readwrite("cache_request", &CesiumAsync::CacheItem::cacheRequest)
      .def_readwrite("cache_response", &CesiumAsync::CacheItem::cacheResponse);

  py::class_<
      CesiumAsync::ICacheDatabase,
      std::shared_ptr<CesiumAsync::ICacheDatabase>>(m, "ICacheDatabase")
      .def(
          "get_entry",
          &CesiumAsync::ICacheDatabase::getEntry,
          py::call_guard<py::gil_scoped_release>())
      .def(
          "store_entry",
          [](CesiumAsync::ICacheDatabase& self,
             const std::string& key,
             std::time_t expiryTime,
             const std::string& url,
             const std::string& requestMethod,
             const py::dict& requestHeaders,
             uint16_t statusCode,
             const py::dict& responseHeaders,
             const py::bytes& responseData) {
            std::vector<std::byte> body =
                CesiumPython::bytesToVector(responseData);
            auto requestHeadersNative =
                CesiumPython::dictToHeaders(requestHeaders);
            auto responseHeadersNative =
                CesiumPython::dictToHeaders(responseHeaders);
            py::gil_scoped_release release;
            return self.storeEntry(
                key,
                expiryTime,
                url,
                requestMethod,
                requestHeadersNative,
                statusCode,
                responseHeadersNative,
                std::span<const std::byte>(body.data(), body.size()));
          })
      .def(
          "prune",
          &CesiumAsync::ICacheDatabase::prune,
          py::call_guard<py::gil_scoped_release>())
      .def(
          "clear_all",
          &CesiumAsync::ICacheDatabase::clearAll,
          py::call_guard<py::gil_scoped_release>());

  py::class_<
      CesiumAsync::SqliteCache,
      CesiumAsync::ICacheDatabase,
      std::shared_ptr<CesiumAsync::SqliteCache>>(m, "SqliteCache")
      .def(
          py::init([](const std::string& databaseName, uint64_t maxItems) {
            return std::make_shared<CesiumAsync::SqliteCache>(
                spdlog::default_logger(),
                databaseName,
                maxItems);
          }),
          py::arg("database_name"),
          py::arg("max_items") = 4096);

  py::class_<
      CesiumAsync::CachingAssetAccessor,
      CesiumAsync::IAssetAccessor,
      std::shared_ptr<CesiumAsync::CachingAssetAccessor>>(
      m,
      "CachingAssetAccessor")
      .def(
          py::init(
              [](const std::shared_ptr<CesiumAsync::IAssetAccessor>& pInner,
                 const std::string& cacheDirectory,
                 int32_t requestsPerCachePrune) {
                auto cacheDb = std::make_shared<CesiumAsync::SqliteCache>(
                    spdlog::default_logger(),
                    cacheDirectory);
                return std::make_shared<CesiumAsync::CachingAssetAccessor>(
                    spdlog::default_logger(),
                    pInner,
                    cacheDb,
                    requestsPerCachePrune);
              }),
          py::arg("asset_accessor"),
          py::arg("cache_directory"),
          py::arg("requests_per_cache_prune") = 10000);

  py::class_<
      CesiumAsync::GunzipAssetAccessor,
      CesiumAsync::IAssetAccessor,
      std::shared_ptr<CesiumAsync::GunzipAssetAccessor>>(
      m,
      "GunzipAssetAccessor")
      .def(
          py::init<const std::shared_ptr<CesiumAsync::IAssetAccessor>&>(),
          py::arg("asset_accessor"));

  py::class_<
      ArchiveAssetAccessor,
      CesiumAsync::IAssetAccessor,
      std::shared_ptr<ArchiveAssetAccessor>>(
      m,
      "ArchiveAssetAccessor",
      "Decorator IAssetAccessor for .3tz / .zip archives.\n\n"
      "Wraps an inner ``IAssetAccessor`` (typically ``CurlAssetAccessor``).\n"
      "URLs that point into the archive are read directly from the zip via\n"
      "libzip (pure C++, no GIL).  All other URLs are forwarded to the\n"
      "inner accessor.\n\n"
      "Composable with ``CachingAssetAccessor`` and "
      "``GunzipAssetAccessor``::\n\n"
      "    curl = CurlAssetAccessor()\n"
      "    accessor = ArchiveAssetAccessor(curl, '/data/boston.3tz')\n"
      "    accessor = CachingAssetAccessor(accessor, cache_dir)\n")
      .def(
          py::init<
              const std::shared_ptr<CesiumAsync::IAssetAccessor>&,
              std::string>(),
          py::arg("asset_accessor"),
          py::arg("archive_path"),
          "Wrap an accessor with archive support.\n\n"
          "Parameters\n"
          "----------\n"
          "asset_accessor : IAssetAccessor\n"
          "    Inner accessor used for non-archive URLs.\n"
          "archive_path : str\n"
          "    Absolute path to a .3tz or .zip archive.\n");

  py::class_<CesiumAsync::CesiumIonAssetAccessor::UpdatedToken>(
      m,
      "UpdatedToken")
      .def(py::init<>())
      .def_readwrite(
          "token",
          &CesiumAsync::CesiumIonAssetAccessor::UpdatedToken::token)
      .def_readwrite(
          "authorization_header",
          &CesiumAsync::CesiumIonAssetAccessor::UpdatedToken::
              authorizationHeader);

  py::class_<
      CesiumAsync::CesiumIonAssetAccessor,
      CesiumAsync::IAssetAccessor,
      std::shared_ptr<CesiumAsync::CesiumIonAssetAccessor>>(
      m,
      "CesiumIonAssetAccessor")
      .def(
          "notify_owner_is_being_destroyed",
          &CesiumAsync::CesiumIonAssetAccessor::notifyOwnerIsBeingDestroyed);

  py::class_<CesiumAsync::NetworkAssetDescriptor>(m, "NetworkAssetDescriptor")
      .def(py::init<>())
      .def_readwrite("url", &CesiumAsync::NetworkAssetDescriptor::url)
      .def_readwrite("headers", &CesiumAsync::NetworkAssetDescriptor::headers)
      .def(
          "load_from_network",
          &CesiumAsync::NetworkAssetDescriptor::loadFromNetwork,
          py::arg("async_system"),
          py::arg("asset_accessor"))
      .def(
          "load_bytes_from_network",
          [](const CesiumAsync::NetworkAssetDescriptor& self,
             const CesiumAsync::AsyncSystem& asyncSystem,
             const std::shared_ptr<CesiumAsync::IAssetAccessor>&
                 pAssetAccessor) {
            return self.loadBytesFromNetwork(asyncSystem, pAssetAccessor)
                .thenImmediately(
                    [](CesiumUtility::Result<ByteVector>&& result)
                        -> ByteVector {
                      if (!result.value.has_value()) {
                        std::string msg = "loadBytesFromNetwork failed";
                        if (!result.errors.errors.empty()) {
                          msg = result.errors.errors.front();
                        }
                        throw std::runtime_error(msg);
                      }
                      return std::move(*result.value);
                    });
          },
          py::arg("async_system"),
          py::arg("asset_accessor"))
      .def(
          "__eq__",
          &CesiumAsync::NetworkAssetDescriptor::operator==,
          py::is_operator());
}
