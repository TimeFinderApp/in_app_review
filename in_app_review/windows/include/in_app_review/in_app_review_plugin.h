#ifndef FLUTTER_PLUGIN_IN_APP_REVIEW_PLUGIN_H_
#define FLUTTER_PLUGIN_IN_APP_REVIEW_PLUGIN_H_

#include <flutter/method_channel.h>
#include <flutter/plugin_registrar_windows.h>

#include <memory>

namespace in_app_review
{

    class InAppReviewPlugin : public flutter::Plugin
    {
    public:
        static void RegisterWithRegistrar(flutter::PluginRegistrarWindows *registrar);

        InAppReviewPlugin();

        virtual ~InAppReviewPlugin();

        // Disallow copy and assign.
        InAppReviewPlugin(const InAppReviewPlugin &) = delete;
        InAppReviewPlugin &operator=(const InAppReviewPlugin &) = delete;

    private:
        // Called when a method is called on this plugin's channel from Dart.
        void HandleMethodCall(
            const flutter::MethodCall<flutter::EncodableValue> &method_call,
            std::unique_ptr<flutter::MethodResult<flutter::EncodableValue>> result);
    };

} // namespace in_app_review

// This extern "C" is needed for registering the plugin when it's included
// as a static library.
#if defined(__cplusplus)
extern "C"
{
#endif

    // Plugin registration function for the Windows platform.
    // This is the function that will be automatically called by Flutter to register the plugin.
    __declspec(dllexport) void InAppReviewPluginRegisterWithRegistrar(
        FlutterDesktopPluginRegistrarRef registrar);

#if defined(__cplusplus)
} // extern "C"
#endif

#endif // FLUTTER_PLUGIN_IN_APP_REVIEW_PLUGIN_H_