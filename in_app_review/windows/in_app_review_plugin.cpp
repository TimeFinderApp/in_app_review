#include "include/in_app_review/in_app_review_plugin.h"

// This must be included before many other Windows headers.
#include <windows.h>

// For getPlatformVersion; remove unless needed for your plugin implementation.
#include <VersionHelpers.h>

#include <flutter/method_channel.h>
#include <flutter/plugin_registrar_windows.h>
#include <flutter/standard_method_codec.h>

#include <memory>
#include <sstream>

// Include WinRT headers
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Services.Store.h>

using namespace winrt;
using namespace Windows::Services::Store;

namespace in_app_review
{

    // static
    void InAppReviewPlugin::RegisterWithRegistrar(
        flutter::PluginRegistrarWindows *registrar)
    {
        auto channel =
            std::make_unique<flutter::MethodChannel<flutter::EncodableValue>>(
                registrar->messenger(), "dev.britannio.in_app_review",
                &flutter::StandardMethodCodec::GetInstance());

        auto plugin = std::make_unique<InAppReviewPlugin>();

        channel->SetMethodCallHandler(
            [plugin_pointer = plugin.get()](const auto &call, auto result)
            {
                plugin_pointer->HandleMethodCall(call, std::move(result));
            });

        registrar->AddPlugin(std::move(plugin));
    }

    InAppReviewPlugin::InAppReviewPlugin() {}

    InAppReviewPlugin::~InAppReviewPlugin() {}

    // Check if Windows version is 1809 or later (required for Store API)
    bool IsWindowsVersionCompatible()
    {
        return IsWindowsVersionOrGreater(10, 0, 17763);
    }

    // Helper function to check if app is installed from Microsoft Store
    winrt::fire_and_forget CheckIfInstalledFromStore(
        std::function<void(bool, std::string)> callback)
    {
        try
        {
            StoreContext storeContext = StoreContext::GetDefault();
            
            if (!storeContext)
            {
                callback(false, "STORE_API_UNAVAILABLE");
                return;
            }
            
            auto license = co_await storeContext.GetAppLicenseAsync();
            bool isFromStore = !license.SkuStoreId().empty();
            callback(isFromStore, "");
        }
        catch (winrt::hresult_error const& ex)
        {
            std::ostringstream errorStream;
            errorStream << "WINRT_ERROR: " << std::hex << ex.code() << " - " << winrt::to_string(ex.message());
            callback(false, errorStream.str());
        }
        catch (...)
        {
            callback(false, "UNKNOWN_ERROR");
        }
    }

    // Helper function to request in-app review
    winrt::fire_and_forget RequestInAppReview(
        std::function<void(bool, std::string)> callback)
    {
        try
        {
            StoreContext storeContext = StoreContext::GetDefault();
            
            if (!storeContext)
            {
                callback(false, "STORE_API_UNAVAILABLE");
                return;
            }
            
            auto reviewResult = co_await storeContext.RequestRateAndReviewAppAsync();

            if (!reviewResult)
            {
                callback(false, "NULL_REVIEW_RESULT");
                return;
            }

            bool success = false;
            std::string errorMsg = "";

            switch (reviewResult.Status())
            {
            case StoreRateAndReviewStatus::Succeeded:
                success = true;
                break;
            case StoreRateAndReviewStatus::CanceledByUser:
                errorMsg = "USER_CANCELED";
                break;
            case StoreRateAndReviewStatus::NetworkError:
                errorMsg = "NETWORK_ERROR";
                break;
            default:
                errorMsg = "STORE_ERROR_" + std::to_string(static_cast<int>(reviewResult.Status()));
                break;
            }

            callback(success, errorMsg);
        }
        catch (winrt::hresult_error const& ex)
        {
            std::ostringstream errorStream;
            errorStream << "WINRT_ERROR: " << std::hex << ex.code() << " - " << winrt::to_string(ex.message());
            callback(false, errorStream.str());
        }
        catch (...)
        {
            callback(false, "UNKNOWN_ERROR");
        }
    }

    void InAppReviewPlugin::HandleMethodCall(
        const flutter::MethodCall<flutter::EncodableValue> &method_call,
        std::unique_ptr<flutter::MethodResult<flutter::EncodableValue>> result)
    {
        try 
        {
            if (method_call.method_name() == "isAvailable")
            {
                // First check if Windows version is compatible
                if (!IsWindowsVersionCompatible())
                {
                    result->Success(flutter::EncodableValue(false));
                    return;
                }

                // Then check if app is installed from Microsoft Store
                CheckIfInstalledFromStore(
                    [result = result.release()](bool isAvailable, std::string errorMsg)
                    {
                        std::unique_ptr<flutter::MethodResult<flutter::EncodableValue>> resultPtr(
                            static_cast<flutter::MethodResult<flutter::EncodableValue> *>(result));
                        
                        if (!errorMsg.empty()) {
                            resultPtr->Error("WINDOWS_STORE_ERROR", errorMsg);
                            return;
                        }
                        
                        resultPtr->Success(flutter::EncodableValue(isAvailable));
                    });
            }
            else if (method_call.method_name() == "requestReview")
            {
                // First check if Windows version is compatible
                if (!IsWindowsVersionCompatible())
                {
                    result->Error("VERSION_ERROR", "Windows version is not compatible");
                    return;
                }
                
                // Must be called from UI thread for Windows Store API
                RequestInAppReview(
                    [result = result.release()](bool success, std::string errorMsg)
                    {
                        std::unique_ptr<flutter::MethodResult<flutter::EncodableValue>> resultPtr(
                            static_cast<flutter::MethodResult<flutter::EncodableValue> *>(result));
                        if (success)
                        {
                            resultPtr->Success();
                        }
                        else
                        {
                            resultPtr->Error("WINDOWS_REVIEW_ERROR", errorMsg);
                        }
                    });
        }
        else
        {
            result->NotImplemented();
        }
        }
        catch (const std::exception& e) 
        {
            result->Error("EXCEPTION", std::string("C++ exception: ") + e.what());
        }
        catch (...) 
        {
            result->Error("EXCEPTION", "Unknown C++ exception occurred");
        }
    }

} // namespace in_app_review

// This creates the plugin as a library to be statically linked
extern "C" __declspec(dllexport) void InAppReviewPluginRegisterWithRegistrar(
    FlutterDesktopPluginRegistrarRef registrar)
{
    // Convert the C-style registrar to the C++ registrar wrapper
    auto registrar_windows = 
        std::make_unique<flutter::PluginRegistrarWindows>(registrar);
    // Call the RegisterWithRegistrar method defined in the InAppReviewPlugin class
    in_app_review::InAppReviewPlugin::RegisterWithRegistrar(registrar_windows.get());
}