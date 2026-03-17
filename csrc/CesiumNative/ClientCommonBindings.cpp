#include "FutureTemplates.h"
#include "ResponseTemplates.h"

#include <CesiumAsync/AsyncSystem.h>
#include <CesiumAsync/IAssetAccessor.h>
#include <CesiumClientCommon/ErrorResponse.h>
#include <CesiumClientCommon/JwtTokenUtility.h>
#include <CesiumClientCommon/OAuth2PKCE.h>
#include <CesiumClientCommon/fillWithRandomBytes.h>

#include <pybind11/functional.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <rapidjson/document.h>

#include <memory>

namespace py = pybind11;

using ResultOAuth2TokenResponse =
    CesiumUtility::Result<CesiumClientCommon::OAuth2TokenResponse>;
using FutureResultOAuth2TokenResponse =
    CesiumAsync::Future<ResultOAuth2TokenResponse>;

void initClientCommonBindings(py::module& m) {
  py::class_<CesiumClientCommon::OAuth2TokenResponse>(m, "OAuth2TokenResponse")
      .def(py::init<>())
      .def_readwrite(
          "access_token",
          &CesiumClientCommon::OAuth2TokenResponse::accessToken)
      .def_readwrite(
          "refresh_token",
          &CesiumClientCommon::OAuth2TokenResponse::refreshToken);

  py::class_<CesiumClientCommon::OAuth2ClientOptions>(m, "OAuth2ClientOptions")
      .def(py::init<>())
      .def_readwrite(
          "client_id",
          &CesiumClientCommon::OAuth2ClientOptions::clientID)
      .def_readwrite(
          "redirect_path",
          &CesiumClientCommon::OAuth2ClientOptions::redirectPath)
      .def_readwrite(
          "redirect_port",
          &CesiumClientCommon::OAuth2ClientOptions::redirectPort)
      .def_readwrite(
          "use_json_body",
          &CesiumClientCommon::OAuth2ClientOptions::useJsonBody);

  m.def(
      "parse_token_payload_summary",
      [](const std::string& token) {
        auto parsed =
            CesiumClientCommon::JwtTokenUtility::parseTokenPayload(token);
        py::dict out;
        out["has_value"] = parsed.value.has_value();

        py::list errors;
        for (const auto& error : parsed.errors.errors) {
          errors.append(error);
        }
        out["errors"] = errors;

        py::list warnings;
        for (const auto& warning : parsed.errors.warnings) {
          warnings.append(warning);
        }
        out["warnings"] = warnings;

        py::list keys;
        if (parsed.value && parsed.value->IsObject()) {
          for (auto it = parsed.value->MemberBegin();
               it != parsed.value->MemberEnd();
               ++it) {
            keys.append(it->name.GetString());
          }
        }
        out["keys"] = keys;

        return out;
      },
      py::arg("token"));

  m.def(
      "parse_error_response",
      [](py::bytes payloadBytes) {
        std::string payload = payloadBytes;
        std::vector<std::byte> bytePayload(payload.size());
        for (size_t i = 0; i < payload.size(); ++i) {
          bytePayload[i] =
              static_cast<std::byte>(static_cast<unsigned char>(payload[i]));
        }
        std::string outError, outErrorDesc;
        bool found = CesiumClientCommon::parseErrorResponse(
            std::span<const std::byte>(bytePayload.data(), bytePayload.size()),
            outError,
            outErrorDesc);
        py::dict result;
        result["found"] = found;
        result["error"] = outError;
        result["error_description"] = outErrorDesc;
        return result;
      },
      py::arg("payload"));

  m.def(
      "fill_with_random_bytes",
      [](size_t length) {
        std::vector<uint8_t> data(length);
        CesiumClientCommon::fillWithRandomBytes(std::span<uint8_t>(data));
        return py::bytes(
            reinterpret_cast<const char*>(data.data()),
            data.size());
      },
      py::arg("length"));

  // ---- OAuth2PKCE ----------------------------------------------------------
  py::class_<ResultOAuth2TokenResponse> resultOAuth2Cls(
      m,
      "ResultOAuth2TokenResponse");
  CesiumPython::bindResult(resultOAuth2Cls);

  py::class_<FutureResultOAuth2TokenResponse> futResultOAuth2Cls(
      m,
      "FutureResultOAuth2TokenResponse");
  CesiumPython::bindFuture(futResultOAuth2Cls);

  py::class_<CesiumClientCommon::OAuth2PKCE>(m, "OAuth2PKCE")
      .def_static(
          "authorize",
          [](const CesiumAsync::AsyncSystem& asyncSystem,
             const std::shared_ptr<CesiumAsync::IAssetAccessor>& pAssetAccessor,
             const std::string& friendlyAppName,
             const CesiumClientCommon::OAuth2ClientOptions& clientOptions,
             const std::vector<std::string>& scopes,
             py::function openUrlCallback,
             const std::string& tokenEndpointUrl,
             const std::string& authorizeBaseUrl) {
            return CesiumClientCommon::OAuth2PKCE::authorize(
                asyncSystem,
                pAssetAccessor,
                friendlyAppName,
                clientOptions,
                scopes,
                [cb = std::move(openUrlCallback)](const std::string& url) {
                  py::gil_scoped_acquire gil;
                  cb(url);
                },
                tokenEndpointUrl,
                authorizeBaseUrl);
          },
          py::arg("async_system"),
          py::arg("asset_accessor"),
          py::arg("friendly_app_name"),
          py::arg("client_options"),
          py::arg("scopes"),
          py::arg("open_url_callback"),
          py::arg("token_endpoint_url"),
          py::arg("authorize_base_url"))
      .def_static(
          "refresh",
          &CesiumClientCommon::OAuth2PKCE::refresh,
          py::arg("async_system"),
          py::arg("asset_accessor"),
          py::arg("client_options"),
          py::arg("refresh_base_url"),
          py::arg("refresh_token"));
}
