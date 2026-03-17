#pragma once

#include <pybind11/functional.h>
#include <pybind11/pybind11.h>

#include <exception>
#include <string>
#include <type_traits>
#include <utility>

namespace py = pybind11;

namespace CesiumPython {

namespace detail {
template <typename V> py::object toPyObject(V&& v) {
  if constexpr (std::is_same_v<std::decay_t<V>, py::object>) {
    return std::forward<V>(v);
  } else {
    return py::cast(std::forward<V>(v));
  }
}
} // namespace detail

// ---- Future<T> ------------------------------------------------------------

/// Void specialisation: callback receives nothing, returns FutureVoid.
template <typename FutureT>
auto bindFutureVoid(py::class_<FutureT>& cls) -> py::class_<FutureT>& {
  cls.def(
         "wait",
         &FutureT::wait,
         py::call_guard<py::gil_scoped_release>(),
         "Block until resolved (non-main thread).")
      .def(
          "wait_in_main_thread",
          &FutureT::waitInMainThread,
          py::call_guard<py::gil_scoped_release>(),
          "Block until resolved (main thread, dispatches queued tasks).")
      .def("is_ready", &FutureT::isReady)
      .def("share", [](FutureT& self) { return std::move(self).share(); })
      .def(
          "then_in_worker_thread",
          [](FutureT& self, py::function callback) {
            return std::move(self).thenInWorkerThread(
                [cb = std::move(callback)]() {
                  py::gil_scoped_acquire gil;
                  cb();
                });
          },
          py::arg("callback"))
      .def(
          "then_in_main_thread",
          [](FutureT& self, py::function callback) {
            return std::move(self).thenInMainThread(
                [cb = std::move(callback)]() {
                  py::gil_scoped_acquire gil;
                  cb();
                });
          },
          py::arg("callback"))
      .def(
          "__await__",
          [](FutureT& self) {
            py::module_ asyncio = py::module_::import("asyncio");
            py::object loop = asyncio.attr("get_running_loop")();
            py::object pyFuture = loop.attr("create_future")();

            // Prevent captures from preventing GC until callback fires.
            py::handle loopH = loop;
            py::handle futH = pyFuture;
            loopH.inc_ref();
            futH.inc_ref();

            std::move(self)
                .thenInWorkerThread([loopH, futH]() {
                  py::gil_scoped_acquire gil;
                  py::object l = py::reinterpret_steal<py::object>(loopH);
                  py::object f = py::reinterpret_steal<py::object>(futH);
                  l.attr(
                      "call_soon_threadsafe")(f.attr("set_result"), py::none());
                })
                .catchImmediately([loopH, futH](const std::exception& e) {
                  py::gil_scoped_acquire gil;
                  py::object l = py::reinterpret_steal<py::object>(loopH);
                  py::object f = py::reinterpret_steal<py::object>(futH);
                  l.attr("call_soon_threadsafe")(
                      f.attr("set_exception"),
                      py::module_::import("builtins")
                          .attr("RuntimeError")(e.what()));
                });
            return pyFuture.attr("__await__")();
          })
      .def(
          "catch_in_main_thread",
          [](FutureT& self, py::function callback) {
            return std::move(self).catchInMainThread(
                [cb = std::move(callback)](const std::exception& e) {
                  py::gil_scoped_acquire gil;
                  cb(py::str(e.what()));
                });
          },
          py::arg("callback"));
  return cls;
}

/// Non-void specialisation: callback receives T, returns Future<result>.
template <typename FutureT>
auto bindFuture(
    py::class_<FutureT>& cls,
    py::return_value_policy rvp = py::return_value_policy::move)
    -> py::class_<FutureT>& {
  cls.def(
         "wait",
         [](FutureT& self) { return self.wait(); },
         py::call_guard<py::gil_scoped_release>(),
         rvp)
      .def(
          "wait_in_main_thread",
          [](FutureT& self) { return self.waitInMainThread(); },
          py::call_guard<py::gil_scoped_release>(),
          rvp)
      .def("is_ready", &FutureT::isReady)
      .def("share", [](FutureT& self) { return std::move(self).share(); })
      .def(
          "then_in_worker_thread",
          [](FutureT& self, py::function callback) {
            return std::move(self).thenInWorkerThread(
                [cb = std::move(callback)](auto value) {
                  py::gil_scoped_acquire gil;
                  cb(std::move(value));
                });
          },
          py::arg("callback"))
      .def(
          "then_in_main_thread",
          [](FutureT& self, py::function callback) {
            return std::move(self).thenInMainThread(
                [cb = std::move(callback)](auto value) {
                  py::gil_scoped_acquire gil;
                  cb(std::move(value));
                });
          },
          py::arg("callback"))
      .def(
          "__await__",
          [](FutureT& self) {
            py::module_ asyncio = py::module_::import("asyncio");
            py::object loop = asyncio.attr("get_running_loop")();
            py::object pyFuture = loop.attr("create_future")();

            py::handle loopH = loop;
            py::handle futH = pyFuture;
            loopH.inc_ref();
            futH.inc_ref();

            std::move(self)
                .thenInWorkerThread([loopH, futH](auto value) {
                  py::gil_scoped_acquire gil;
                  py::object l = py::reinterpret_steal<py::object>(loopH);
                  py::object f = py::reinterpret_steal<py::object>(futH);
                  l.attr("call_soon_threadsafe")(
                      f.attr("set_result"),
                      detail::toPyObject(std::move(value)));
                })
                .catchImmediately([loopH, futH](const std::exception& e) {
                  py::gil_scoped_acquire gil;
                  py::object l = py::reinterpret_steal<py::object>(loopH);
                  py::object f = py::reinterpret_steal<py::object>(futH);
                  l.attr("call_soon_threadsafe")(
                      f.attr("set_exception"),
                      py::module_::import("builtins")
                          .attr("RuntimeError")(e.what()));
                });
            return pyFuture.attr("__await__")();
          })
      .def(
          "catch_in_main_thread",
          [](FutureT& self, py::function callback) {
            using ValueType =
                std::decay_t<decltype(std::declval<FutureT&>().wait())>;
            return std::move(self).catchInMainThread(
                [cb = std::move(callback)](
                    const std::exception& e) -> ValueType {
                  py::gil_scoped_acquire gil;
                  return cb(py::str(e.what())).template cast<ValueType>();
                });
          },
          py::arg("callback"));
  return cls;
}

// ---- SharedFuture<T> ------------------------------------------------------

template <typename SFutureT>
auto bindSharedFutureVoid(py::class_<SFutureT>& cls) -> py::class_<SFutureT>& {
  cls.def(
         "wait",
         [](SFutureT& self) { self.wait(); },
         py::call_guard<py::gil_scoped_release>(),
         "Block until resolved (non-main thread).")
      .def(
          "wait_in_main_thread",
          [](SFutureT& self) { self.waitInMainThread(); },
          py::call_guard<py::gil_scoped_release>(),
          "Block until resolved (main thread, dispatches queued tasks).")
      .def("is_ready", &SFutureT::isReady)
      .def(
          "then_in_worker_thread",
          [](SFutureT& self, py::function callback) {
            return self.thenInWorkerThread([cb = std::move(callback)]() {
              py::gil_scoped_acquire gil;
              cb();
            });
          },
          py::arg("callback"))
      .def(
          "then_in_main_thread",
          [](SFutureT& self, py::function callback) {
            return self.thenInMainThread([cb = std::move(callback)]() {
              py::gil_scoped_acquire gil;
              cb();
            });
          },
          py::arg("callback"))
      .def(
          "__await__",
          [](SFutureT& self) {
            py::module_ asyncio = py::module_::import("asyncio");
            py::object loop = asyncio.attr("get_running_loop")();
            py::object pyFuture = loop.attr("create_future")();

            py::handle loopH = loop;
            py::handle futH = pyFuture;
            loopH.inc_ref();
            futH.inc_ref();

            self.thenInWorkerThread([loopH, futH]() {
                  py::gil_scoped_acquire gil;
                  py::object l = py::reinterpret_steal<py::object>(loopH);
                  py::object f = py::reinterpret_steal<py::object>(futH);
                  l.attr(
                      "call_soon_threadsafe")(f.attr("set_result"), py::none());
                })
                .catchImmediately([loopH, futH](const std::exception& e) {
                  py::gil_scoped_acquire gil;
                  py::object l = py::reinterpret_steal<py::object>(loopH);
                  py::object f = py::reinterpret_steal<py::object>(futH);
                  l.attr("call_soon_threadsafe")(
                      f.attr("set_exception"),
                      py::module_::import("builtins")
                          .attr("RuntimeError")(e.what()));
                });
            return pyFuture.attr("__await__")();
          })
      .def(
          "catch_in_main_thread",
          [](SFutureT& self, py::function callback) {
            return self.catchInMainThread(
                [cb = std::move(callback)](const std::exception& e) {
                  py::gil_scoped_acquire gil;
                  cb(py::str(e.what()));
                });
          },
          py::arg("callback"));
  return cls;
}

template <typename SFutureT>
auto bindSharedFuture(
    py::class_<SFutureT>& cls,
    py::return_value_policy rvp = py::return_value_policy::move)
    -> py::class_<SFutureT>& {
  cls.def(
         "wait",
         [](const SFutureT& self) { return self.wait(); },
         py::call_guard<py::gil_scoped_release>(),
         rvp)
      .def(
          "wait_in_main_thread",
          [](SFutureT& self) { return self.waitInMainThread(); },
          py::call_guard<py::gil_scoped_release>(),
          rvp)
      .def("is_ready", &SFutureT::isReady)
      .def(
          "then_in_worker_thread",
          [](SFutureT& self, py::function callback) {
            return self.thenInWorkerThread(
                [cb = std::move(callback)](auto value) {
                  py::gil_scoped_acquire gil;
                  cb(std::move(value));
                });
          },
          py::arg("callback"))
      .def(
          "then_in_main_thread",
          [](SFutureT& self, py::function callback) {
            return self.thenInMainThread(
                [cb = std::move(callback)](auto value) {
                  py::gil_scoped_acquire gil;
                  cb(std::move(value));
                });
          },
          py::arg("callback"))
      .def(
          "__await__",
          [](SFutureT& self) {
            py::module_ asyncio = py::module_::import("asyncio");
            py::object loop = asyncio.attr("get_running_loop")();
            py::object pyFuture = loop.attr("create_future")();

            py::handle loopH = loop;
            py::handle futH = pyFuture;
            loopH.inc_ref();
            futH.inc_ref();

            self.thenInWorkerThread([loopH, futH](auto value) {
                  py::gil_scoped_acquire gil;
                  py::object l = py::reinterpret_steal<py::object>(loopH);
                  py::object f = py::reinterpret_steal<py::object>(futH);
                  l.attr("call_soon_threadsafe")(
                      f.attr("set_result"),
                      detail::toPyObject(std::move(value)));
                })
                .catchImmediately([loopH, futH](const std::exception& e) {
                  py::gil_scoped_acquire gil;
                  py::object l = py::reinterpret_steal<py::object>(loopH);
                  py::object f = py::reinterpret_steal<py::object>(futH);
                  l.attr("call_soon_threadsafe")(
                      f.attr("set_exception"),
                      py::module_::import("builtins")
                          .attr("RuntimeError")(e.what()));
                });
            return pyFuture.attr("__await__")();
          })
      .def(
          "catch_in_main_thread",
          [](SFutureT& self, py::function callback) {
            using ValueType =
                std::decay_t<decltype(std::declval<SFutureT&>().wait())>;
            return self.catchInMainThread(
                [cb = std::move(callback)](
                    const std::exception& e) -> ValueType {
                  py::gil_scoped_acquire gil;
                  return cb(py::str(e.what())).template cast<ValueType>();
                });
          },
          py::arg("callback"));
  return cls;
}

// ---- Promise<T> -----------------------------------------------------------

/// Void specialisation: resolve() takes no arguments.
template <typename PromiseT>
auto bindPromiseVoid(py::class_<PromiseT>& cls) -> py::class_<PromiseT>& {
  cls.def("resolve", &PromiseT::resolve)
      .def(
          "reject",
          [](const PromiseT& self, const std::string& message) {
            self.reject(std::runtime_error(message));
          },
          py::arg("message"))
      .def_property_readonly("future", &PromiseT::getFuture);
  return cls;
}

/// Non-void specialisation: resolve(value) takes ValueT.
template <typename PromiseT, typename ValueT>
auto bindPromise(py::class_<PromiseT>& cls) -> py::class_<PromiseT>& {
  cls.def(
         "resolve",
         [](const PromiseT& self, const ValueT& value) { self.resolve(value); },
         py::arg("value"))
      .def(
          "reject",
          [](const PromiseT& self, const std::string& message) {
            self.reject(std::runtime_error(message));
          },
          py::arg("message"))
      .def_property_readonly("future", &PromiseT::getFuture);
  return cls;
}

} // namespace CesiumPython
