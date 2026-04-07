#include "FutureTemplates.h"
#include "ResponseTemplates.h"

#include <CesiumAsync/AsyncSystem.h>
#include <CesiumAsync/Future.h>
#include <CesiumAsync/IAssetAccessor.h>
#include <CesiumIonClient/ApplicationData.h>
#include <CesiumIonClient/Assets.h>
#include <CesiumIonClient/Connection.h>
#include <CesiumIonClient/Defaults.h>
#include <CesiumIonClient/Geocoder.h>
#include <CesiumIonClient/LoginToken.h>
#include <CesiumIonClient/Profile.h>
#include <CesiumIonClient/Response.h>
#include <CesiumIonClient/Token.h>
#include <CesiumIonClient/TokenList.h>
#include <CesiumUtility/Result.h>

#include <pybind11/functional.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;

namespace {

// Type aliases for Response specializations
using ResponseProfile = CesiumIonClient::Response<CesiumIonClient::Profile>;
using ResponseDefaults = CesiumIonClient::Response<CesiumIonClient::Defaults>;
using ResponseAssets = CesiumIonClient::Response<CesiumIonClient::Assets>;
using ResponseAsset = CesiumIonClient::Response<CesiumIonClient::Asset>;
using ResponseToken = CesiumIonClient::Response<CesiumIonClient::Token>;
using ResponseTokenList = CesiumIonClient::Response<CesiumIonClient::TokenList>;
using ResponseNoValue = CesiumIonClient::Response<CesiumIonClient::NoValue>;
using ResponseGeocoderResult =
    CesiumIonClient::Response<CesiumIonClient::GeocoderResult>;
using ResponseAppData =
    CesiumIonClient::Response<CesiumIonClient::ApplicationData>;

// Type aliases for Future<Response<T>>
using FutureResponseProfile = CesiumAsync::Future<ResponseProfile>;
using FutureResponseDefaults = CesiumAsync::Future<ResponseDefaults>;
using FutureResponseAssets = CesiumAsync::Future<ResponseAssets>;
using FutureResponseAsset = CesiumAsync::Future<ResponseAsset>;
using FutureResponseToken = CesiumAsync::Future<ResponseToken>;
using FutureResponseTokenList = CesiumAsync::Future<ResponseTokenList>;
using FutureResponseNoValue = CesiumAsync::Future<ResponseNoValue>;
using FutureResponseGeocoderResult =
    CesiumAsync::Future<ResponseGeocoderResult>;
using FutureResponseAppData = CesiumAsync::Future<ResponseAppData>;

// Type aliases for static methods returning Future<Result<T>>
using ResultConnection = CesiumUtility::Result<CesiumIonClient::Connection>;
using FutureResultConnection = CesiumAsync::Future<ResultConnection>;

// Future<optional<string>> for getApiUrl
using FutureOptionalString = CesiumAsync::Future<std::optional<std::string>>;

} // namespace

void initIonClientBindings(py::module& m) {
  py::enum_<CesiumIonClient::AuthenticationMode>(m, "AuthenticationMode")
      .value("CesiumIon", CesiumIonClient::AuthenticationMode::CesiumIon)
      .value("Saml", CesiumIonClient::AuthenticationMode::Saml)
      .value("SingleUser", CesiumIonClient::AuthenticationMode::SingleUser)
      .export_values();

  py::enum_<CesiumIonClient::SortOrder>(m, "SortOrder")
      .value("Ascending", CesiumIonClient::SortOrder::Ascending)
      .value("Descending", CesiumIonClient::SortOrder::Descending)
      .export_values();

  py::enum_<CesiumIonClient::GeocoderRequestType>(m, "GeocoderRequestType")
      .value("Search", CesiumIonClient::GeocoderRequestType::Search)
      .value("Autocomplete", CesiumIonClient::GeocoderRequestType::Autocomplete)
      .export_values();

  py::enum_<CesiumIonClient::GeocoderProviderType>(m, "GeocoderProviderType")
      .value("Google", CesiumIonClient::GeocoderProviderType::Google)
      .value("Bing", CesiumIonClient::GeocoderProviderType::Bing)
      .value("Default", CesiumIonClient::GeocoderProviderType::Default)
      .export_values();

  py::class_<CesiumIonClient::ApplicationData>(m, "ApplicationData")
      .def(py::init<>())
      .def_readwrite(
          "authentication_mode",
          &CesiumIonClient::ApplicationData::authenticationMode)
      .def_readwrite(
          "data_store_type",
          &CesiumIonClient::ApplicationData::dataStoreType)
      .def_readwrite(
          "attribution",
          &CesiumIonClient::ApplicationData::attribution)
      .def(
          "needs_oauth_authentication",
          &CesiumIonClient::ApplicationData::needsOauthAuthentication);

  py::class_<CesiumIonClient::Asset>(m, "Asset")
      .def(py::init<>())
      .def_readwrite("id", &CesiumIonClient::Asset::id)
      .def_readwrite("name", &CesiumIonClient::Asset::name)
      .def_readwrite("description", &CesiumIonClient::Asset::description)
      .def_readwrite("attribution", &CesiumIonClient::Asset::attribution)
      .def_readwrite("type", &CesiumIonClient::Asset::type)
      .def_readwrite("bytes", &CesiumIonClient::Asset::bytes)
      .def_readwrite("date_added", &CesiumIonClient::Asset::dateAdded)
      .def_readwrite("status", &CesiumIonClient::Asset::status)
      .def_readwrite(
          "percent_complete",
          &CesiumIonClient::Asset::percentComplete);

  py::class_<CesiumIonClient::Assets>(m, "Assets")
      .def(py::init<>())
      .def_readwrite("link", &CesiumIonClient::Assets::link)
      .def_readwrite("items", &CesiumIonClient::Assets::items);

  py::class_<CesiumIonClient::Token>(m, "Token")
      .def(py::init<>())
      .def_readwrite("id", &CesiumIonClient::Token::id)
      .def_readwrite("name", &CesiumIonClient::Token::name)
      .def_readwrite("token", &CesiumIonClient::Token::token)
      .def_readwrite("date_added", &CesiumIonClient::Token::dateAdded)
      .def_readwrite("date_modified", &CesiumIonClient::Token::dateModified)
      .def_readwrite("date_last_used", &CesiumIonClient::Token::dateLastUsed)
      .def_readwrite("asset_ids", &CesiumIonClient::Token::assetIds)
      .def_readwrite("is_default", &CesiumIonClient::Token::isDefault)
      .def_readwrite("allowed_urls", &CesiumIonClient::Token::allowedUrls)
      .def_readwrite("scopes", &CesiumIonClient::Token::scopes);

  py::class_<CesiumIonClient::TokenList>(m, "TokenList")
      .def(py::init<>())
      .def_readwrite("items", &CesiumIonClient::TokenList::items);

  py::class_<CesiumIonClient::ListTokensOptions>(m, "ListTokensOptions")
      .def(py::init<>())
      .def_readwrite("limit", &CesiumIonClient::ListTokensOptions::limit)
      .def_readwrite("page", &CesiumIonClient::ListTokensOptions::page)
      .def_readwrite("search", &CesiumIonClient::ListTokensOptions::search)
      .def_readwrite("sort_by", &CesiumIonClient::ListTokensOptions::sortBy)
      .def_readwrite(
          "sort_order",
          &CesiumIonClient::ListTokensOptions::sortOrder);

  py::class_<CesiumIonClient::ProfileStorage>(m, "ProfileStorage")
      .def(py::init<>())
      .def_readwrite("used", &CesiumIonClient::ProfileStorage::used)
      .def_readwrite("available", &CesiumIonClient::ProfileStorage::available)
      .def_readwrite("total", &CesiumIonClient::ProfileStorage::total);

  py::class_<CesiumIonClient::Profile>(m, "Profile")
      .def(py::init<>())
      .def_readwrite("id", &CesiumIonClient::Profile::id)
      .def_readwrite("scopes", &CesiumIonClient::Profile::scopes)
      .def_readwrite("username", &CesiumIonClient::Profile::username)
      .def_readwrite("email", &CesiumIonClient::Profile::email)
      .def_readwrite("email_verified", &CesiumIonClient::Profile::emailVerified)
      .def_readwrite("avatar", &CesiumIonClient::Profile::avatar)
      .def_readwrite("storage", &CesiumIonClient::Profile::storage);

  py::class_<CesiumIonClient::DefaultAssets>(m, "DefaultAssets")
      .def(py::init<>())
      .def_readwrite("imagery", &CesiumIonClient::DefaultAssets::imagery)
      .def_readwrite("terrain", &CesiumIonClient::DefaultAssets::terrain)
      .def_readwrite("buildings", &CesiumIonClient::DefaultAssets::buildings);

  py::class_<CesiumIonClient::QuickAddRasterOverlay>(m, "QuickAddRasterOverlay")
      .def(py::init<>())
      .def_readwrite("name", &CesiumIonClient::QuickAddRasterOverlay::name)
      .def_readwrite(
          "asset_id",
          &CesiumIonClient::QuickAddRasterOverlay::assetId)
      .def_readwrite(
          "subscribed",
          &CesiumIonClient::QuickAddRasterOverlay::subscribed);

  py::class_<CesiumIonClient::QuickAddAsset>(m, "QuickAddAsset")
      .def(py::init<>())
      .def_readwrite("name", &CesiumIonClient::QuickAddAsset::name)
      .def_readwrite("object_name", &CesiumIonClient::QuickAddAsset::objectName)
      .def_readwrite(
          "description",
          &CesiumIonClient::QuickAddAsset::description)
      .def_readwrite("asset_id", &CesiumIonClient::QuickAddAsset::assetId)
      .def_readwrite("type", &CesiumIonClient::QuickAddAsset::type)
      .def_readwrite("subscribed", &CesiumIonClient::QuickAddAsset::subscribed)
      .def_readwrite(
          "raster_overlays",
          &CesiumIonClient::QuickAddAsset::rasterOverlays);

  py::class_<CesiumIonClient::Defaults>(m, "Defaults")
      .def(py::init<>())
      .def_readwrite(
          "default_assets",
          &CesiumIonClient::Defaults::defaultAssets)
      .def_readwrite(
          "quick_add_assets",
          &CesiumIonClient::Defaults::quickAddAssets);

  py::class_<CesiumIonClient::GeocoderAttribution>(m, "GeocoderAttribution")
      .def(py::init<>())
      .def_readwrite("html", &CesiumIonClient::GeocoderAttribution::html)
      .def_readwrite(
          "show_on_screen",
          &CesiumIonClient::GeocoderAttribution::showOnScreen);

  py::class_<CesiumIonClient::GeocoderFeature>(m, "GeocoderFeature")
      .def_readwrite(
          "display_name",
          &CesiumIonClient::GeocoderFeature::displayName)
      .def_property_readonly(
          "globe_rectangle",
          &CesiumIonClient::GeocoderFeature::getGlobeRectangle)
      .def_property_readonly(
          "cartographic",
          &CesiumIonClient::GeocoderFeature::getCartographic);

  py::class_<CesiumIonClient::GeocoderResult>(m, "GeocoderResult")
      .def(py::init<>())
      .def_readwrite(
          "attributions",
          &CesiumIonClient::GeocoderResult::attributions)
      .def_readwrite("features", &CesiumIonClient::GeocoderResult::features);

  py::class_<CesiumIonClient::LoginToken>(m, "LoginToken")
      .def(
          py::init<const std::string&, const std::optional<std::time_t>&>(),
          py::arg("token"),
          py::arg("expiration_time"))
      .def("is_valid", &CesiumIonClient::LoginToken::isValid)
      .def_property_readonly(
          "expiration_time",
          &CesiumIonClient::LoginToken::getExpirationTime)
      .def_property_readonly("token", &CesiumIonClient::LoginToken::getToken)
      .def_static("parse_summary", [](const std::string& token_string) {
        auto result = CesiumIonClient::LoginToken::parse(token_string);
        py::dict out;
        out["has_value"] = py::bool_(result.value.has_value());
        out["errors"] = py::cast(result.errors.errors);
        out["warnings"] = py::cast(result.errors.warnings);
        if (result.value) {
          out["token"] = py::cast(result.value->getToken());
          out["expiration_time"] = py::cast(result.value->getExpirationTime());
        } else {
          out["token"] = py::none();
          out["expiration_time"] = py::none();
        }
        return out;
      });

  // --- NoValue ---
  py::class_<CesiumIonClient::NoValue>(m, "NoValue").def(py::init<>());

  // --- Response<T> specializations ---
  auto respProfile = py::class_<ResponseProfile>(m, "ResponseProfile");
  CesiumPython::bindResponse(respProfile);

  auto respDefaults = py::class_<ResponseDefaults>(m, "ResponseDefaults");
  CesiumPython::bindResponse(respDefaults);

  auto respAssets = py::class_<ResponseAssets>(m, "ResponseAssets");
  CesiumPython::bindResponse(respAssets);

  auto respAsset = py::class_<ResponseAsset>(m, "ResponseAsset");
  CesiumPython::bindResponse(respAsset);

  auto respToken = py::class_<ResponseToken>(m, "ResponseToken");
  CesiumPython::bindResponse(respToken);

  auto respTokenList = py::class_<ResponseTokenList>(m, "ResponseTokenList");
  CesiumPython::bindResponse(respTokenList);

  auto respNoValue = py::class_<ResponseNoValue>(m, "ResponseNoValue");
  CesiumPython::bindResponse(respNoValue);

  auto respGeocoderResult =
      py::class_<ResponseGeocoderResult>(m, "ResponseGeocoderResult");
  CesiumPython::bindResponse(respGeocoderResult);

  auto respAppData = py::class_<ResponseAppData>(m, "ResponseApplicationData");
  CesiumPython::bindResponse(respAppData);

  // --- Result<Connection> for authorize ---
  auto resultConnection = py::class_<ResultConnection>(m, "ResultConnection");
  CesiumPython::bindResult(resultConnection);

  // --- Future<Response<T>> specializations ---
  auto futRespProfile =
      py::class_<FutureResponseProfile>(m, "FutureResponseProfile");
  CesiumPython::bindFuture(futRespProfile);

  auto futRespDefaults =
      py::class_<FutureResponseDefaults>(m, "FutureResponseDefaults");
  CesiumPython::bindFuture(futRespDefaults);

  auto futRespAssets =
      py::class_<FutureResponseAssets>(m, "FutureResponseAssets");
  CesiumPython::bindFuture(futRespAssets);

  auto futRespAsset = py::class_<FutureResponseAsset>(m, "FutureResponseAsset");
  CesiumPython::bindFuture(futRespAsset);

  auto futRespToken = py::class_<FutureResponseToken>(m, "FutureResponseToken");
  CesiumPython::bindFuture(futRespToken);

  auto futRespTokenList =
      py::class_<FutureResponseTokenList>(m, "FutureResponseTokenList");
  CesiumPython::bindFuture(futRespTokenList);

  auto futRespNoValue =
      py::class_<FutureResponseNoValue>(m, "FutureResponseNoValue");
  CesiumPython::bindFuture(futRespNoValue);

  auto futRespGeocoderResult = py::class_<FutureResponseGeocoderResult>(
      m,
      "FutureResponseGeocoderResult");
  CesiumPython::bindFuture(futRespGeocoderResult);

  auto futRespAppData =
      py::class_<FutureResponseAppData>(m, "FutureResponseApplicationData");
  CesiumPython::bindFuture(futRespAppData);

  auto futResultConnection =
      py::class_<FutureResultConnection>(m, "FutureResultConnection");
  CesiumPython::bindFuture(futResultConnection);

  auto futOptStr = py::class_<FutureOptionalString>(m, "FutureOptionalString");
  CesiumPython::bindFuture(futOptStr);

  // --- Connection ---
  py::class_<CesiumIonClient::Connection>(m, "Connection")
      .def(
          py::init<
              const CesiumAsync::AsyncSystem&,
              const std::shared_ptr<CesiumAsync::IAssetAccessor>&,
              const std::string&,
              const CesiumIonClient::ApplicationData&,
              const std::string&>(),
          py::arg("async_system"),
          py::arg("asset_accessor"),
          py::arg("access_token"),
          py::arg("app_data"),
          py::arg("api_url") = "https://api.cesium.com")
      .def(
          py::init<
              const CesiumAsync::AsyncSystem&,
              const std::shared_ptr<CesiumAsync::IAssetAccessor>&,
              const CesiumIonClient::LoginToken&,
              const std::string&,
              int64_t,
              const std::string&,
              const CesiumIonClient::ApplicationData&,
              const std::string&>(),
          py::arg("async_system"),
          py::arg("asset_accessor"),
          py::arg("access_token"),
          py::arg("refresh_token"),
          py::arg("client_id"),
          py::arg("redirect_path"),
          py::arg("app_data"),
          py::arg("api_url") = "https://api.cesium.com")
      .def(
          py::init<
              const CesiumAsync::AsyncSystem&,
              const std::shared_ptr<CesiumAsync::IAssetAccessor>&,
              const CesiumIonClient::ApplicationData&,
              const std::string&>(),
          py::arg("async_system"),
          py::arg("asset_accessor"),
          py::arg("app_data"),
          py::arg("api_url"))
      // --- Properties ---
      .def_property_readonly(
          "access_token",
          &CesiumIonClient::Connection::getAccessToken)
      .def_property_readonly(
          "refresh_token",
          &CesiumIonClient::Connection::getRefreshToken)
      .def_property_readonly(
        "api_url",
        [](const CesiumIonClient::Connection& self) {
          return self.getApiUrl();
        })
      .def_property_readonly(
          "async_system",
          &CesiumIonClient::Connection::getAsyncSystem)
      .def_property_readonly(
          "asset_accessor",
          &CesiumIonClient::Connection::getAssetAccessor)
      // --- Static methods ---
      .def_static(
          "id_from_token",
          &CesiumIonClient::Connection::getIdFromToken,
          py::arg("token"))
      .def_static(
          "authorize",
          &CesiumIonClient::Connection::authorize,
          py::arg("async_system"),
          py::arg("asset_accessor"),
          py::arg("friendly_application_name"),
          py::arg("client_id"),
          py::arg("redirect_path"),
          py::arg("scopes"),
          py::arg("open_url_callback"),
          py::arg("app_data"),
          py::arg("ion_api_url") = "https://api.cesium.com/",
          py::arg("ion_authorize_url") = "https://ion.cesium.com/oauth")
      .def_static(
          "app_data",
          &CesiumIonClient::Connection::appData,
          py::arg("async_system"),
          py::arg("asset_accessor"),
          py::arg("api_url") = "https://api.cesium.com")
      .def_static(
          "get_api_url_from_server",
          [](const CesiumAsync::AsyncSystem& asyncSystem,
             const std::shared_ptr<CesiumAsync::IAssetAccessor>& pAssetAccessor,
             const std::string& ionUrl) {
            return CesiumIonClient::Connection::getApiUrl(
                asyncSystem,
                pAssetAccessor,
                ionUrl);
          },
          py::arg("async_system"),
          py::arg("asset_accessor"),
          py::arg("ion_url"))
      // --- Instance async methods ---
      .def("me", &CesiumIonClient::Connection::me)
      .def("defaults", &CesiumIonClient::Connection::defaults)
      .def("assets", &CesiumIonClient::Connection::assets)
      .def(
          "tokens",
          [](const CesiumIonClient::Connection& self,
             const CesiumIonClient::ListTokensOptions& options) {
            return self.tokens(options);
          },
          py::arg("options") = CesiumIonClient::ListTokensOptions{})
      .def("asset", &CesiumIonClient::Connection::asset, py::arg("asset_id"))
      .def("token", &CesiumIonClient::Connection::token, py::arg("token_id"))
      .def(
          "next_page",
          &CesiumIonClient::Connection::nextPage,
          py::arg("current_page"))
      .def(
          "previous_page",
          &CesiumIonClient::Connection::previousPage,
          py::arg("current_page"))
      .def(
          "create_token",
          &CesiumIonClient::Connection::createToken,
          py::arg("name"),
          py::arg("scopes"),
          py::arg("asset_ids") = std::nullopt,
          py::arg("allowed_urls") = std::nullopt)
      .def(
          "modify_token",
          &CesiumIonClient::Connection::modifyToken,
          py::arg("token_id"),
          py::arg("new_name"),
          py::arg("new_asset_ids"),
          py::arg("new_scopes"),
          py::arg("new_allowed_urls"))
      .def(
          "geocode",
          &CesiumIonClient::Connection::geocode,
          py::arg("provider"),
          py::arg("type"),
          py::arg("query"));
}
