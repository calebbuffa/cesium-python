#pragma once

#include <CesiumUtility/IntrusivePointer.h>

#include <pybind11/pybind11.h>

PYBIND11_DECLARE_HOLDER_TYPE(T, CesiumUtility::IntrusivePointer<T>);
