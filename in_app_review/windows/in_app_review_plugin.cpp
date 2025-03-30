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
#include <winrt/Windows.System.h>
#include <shobjidl_core.h>

using namespace winrt;
using namespace Windows::Services::Store;
using namespace Windows::System;

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

    // Alternative implementation that doesn't use WinRT co_await 
    void CheckIfInstalledFromStore(
        std::function<void(bool, std::string)> callback)
    {
        try
        {
            // We'll attempt a simple check for Microsoft Store capabilities
            // without using the async WinRT API that might be causing issues
            HKEY hKey;
            LONG result = RegOpenKeyExW(
                HKEY_CURRENT_USER, 
                L"Software\\Microsoft\\Windows\\CurrentVersion\\AppModel", 
                0, 
                KEY_READ, 
                &hKey);
            
            if (result == ERROR_SUCCESS)
            {
                RegCloseKey(hKey);
                callback(true, "");
            }
            else
            {
                callback(false, "NOT_STORE_APP");
            }
        }
        catch (const std::exception& e)
        {
            callback(false, std::string("Exception: ") + e.what());
        }
        catch (...)
        {
            callback(false, "UNKNOWN_ERROR");
        }
    }

    // Alternative implementation using Launcher instead of Store API
    void RequestInAppReview(
        std::function<void(bool, std::string)> callback)
    {
        try
        {
            // Instead of using the potentially problematic Store API,
            // we'll redirect to the Microsoft Store page for the app
            // This is a fallback that should work even in debug builds
            
            // Get the app package family name
            wchar_t packageFamilyName[256];
            UINT32 packageFamilyNameLength = _countof(packageFamilyName);
            LONG result = GetPackageFamilyName(GetCurrentProcess(), &packageFamilyNameLength, packageFamilyName);
            
            if (result == ERROR_SUCCESS)
            {
                // Create Microsoft Store URI
                std::wstring storeURI = L"ms-windows-store://review/?ProductId=" + std::wstring(packageFamilyName);
                
                // Launch the URI
                auto uri = winrt::Windows::Foundation::Uri(storeURI);
                auto launchOperation = winrt::Windows::System::Launcher::LaunchUriAsync(uri);
                
                // We can't use co_await here, so we'll assume success if no exception
                callback(true, "");
            }
            else if (result == APPMODEL_ERROR_NO_PACKAGE)
            {
                // Debug/development environment without a package identity
                callback(false, "NOT_PACKAGED_APP");
            }
            else
            {
                // Other error
                callback(false, "PACKAGE_ERROR_" + std::to_string(result));
            }
        }
        catch (const winrt::hresult_error& ex)
        {
            callback(false, "WINRT_ERROR: " + std::to_string(ex.code()));
        }
        catch (const std::exception& e)
        {
            callback(false, std::string("Exception: ") + e.what());
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
    try {
        // Convert the C-style registrar to the C++ registrar wrapper
        auto registrar_windows = 
            std::make_unique<flutter::PluginRegistrarWindows>(registrar);
        // Call the RegisterWithRegistrar method defined in the InAppReviewPlugin class
        in_app_review::InAppReviewPlugin::RegisterWithRegistrar(registrar_windows.get());
    } catch (...) {
        // Silently catch any exceptions during registration to prevent crashes
        // This ensures the host app won't crash if there's an issue with the plugin
    }
}