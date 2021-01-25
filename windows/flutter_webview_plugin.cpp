/*
 * @Description: 
 * @Author: ekibun
 * @Date: 2020-08-20 23:25:54
 * @LastEditors: ekibun
 * @LastEditTime: 2020-08-25 22:49:04
 */
#include "include/flutter_webview/flutter_webview_plugin.h"

// This must be included before many other Windows headers.
#include <windows.h>

#include <flutter/method_channel.h>
#include <flutter/plugin_registrar_windows.h>
#include <flutter/standard_method_codec.h>
#include <flutter/method_result_functions.h>

#include "offscreen.hpp"

namespace
{

  class FlutterWebviewPlugin : public flutter::Plugin
  {
  public:
    static void RegisterWithRegistrar(flutter::PluginRegistrarWindows *registrar);

    FlutterWebviewPlugin();

    virtual ~FlutterWebviewPlugin();

  private:
    // Called when a method is called on this plugin's channel from Dart.
    void HandleMethodCall(
        const flutter::MethodCall<flutter::EncodableValue> &method_call,
        std::unique_ptr<flutter::MethodResult<flutter::EncodableValue>> result);
  };

  std::shared_ptr<flutter::MethodChannel<flutter::EncodableValue>> channel;
  const flutter::EncodableValue &ValueOrNull(const flutter::EncodableMap &map, const char *key)
  {
    static flutter::EncodableValue null_value;
    auto it = map.find(flutter::EncodableValue(key));
    if (it == map.end())
    {
      return null_value;
    }
    return it->second;
  }

  HWND hWnd;

  // static
  void FlutterWebviewPlugin::RegisterWithRegistrar(
      flutter::PluginRegistrarWindows *registrar)
  {
    channel = std::make_unique<flutter::MethodChannel<flutter::EncodableValue>>(
        registrar->messenger(), "soko.ekibun.flutter_webview",
        &flutter::StandardMethodCodec::GetInstance());

    hWnd = registrar->GetView()->GetNativeWindow();

    auto plugin = std::make_unique<FlutterWebviewPlugin>();

    channel->SetMethodCallHandler(
        [plugin_pointer = plugin.get()](const auto &call, auto result) {
          plugin_pointer->HandleMethodCall(call, std::move(result));
        });

    registrar->AddPlugin(std::move(plugin));
  }

  FlutterWebviewPlugin::FlutterWebviewPlugin() {}

  FlutterWebviewPlugin::~FlutterWebviewPlugin() {}

  std::vector<std::string> split(std::string str, std::string pattern)
  {
	  std::string::size_type pos;
	  std::vector<std::string> result;
	  str += pattern;
	  int size = str.size();
	  for (int i = 0; i < size; i++)
	  {
		  pos = str.find(pattern, i);
		  if (pos < size)
		  {
			  std::string s = str.substr(i, pos - i);
			  result.push_back(s);
			  i = pos + pattern.size() - 1;
		  }
	  }
	  return result;
  }

  std::string getUrlDomain(std::string url) {
	  int i = url.find("://", 0);
	  int j = url.find("/", i + 3);
	  if (i > 0 && j > i)
		  return url.substr(i + 3, j - i - 3);
	  return "";
  }

  void FlutterWebviewPlugin::HandleMethodCall(
      const flutter::MethodCall<flutter::EncodableValue> &method_call,
      std::unique_ptr<flutter::MethodResult<flutter::EncodableValue>> result)
  {
    if (method_call.method_name().compare("create") == 0)
    {
      new webview::Offscreen(hWnd, channel, result.release());
    }
    else if (method_call.method_name().compare("navigate") == 0)
    {
      flutter::EncodableMap args = *std::get_if<flutter::EncodableMap>(method_call.arguments());
      webview::Offscreen *wv = (webview::Offscreen *)std::get<int64_t>(ValueOrNull(args, "webview"));
      std::string url = std::get<std::string>(ValueOrNull(args, "url"));
      flutter::EncodableValue response = (wv && wv->navigate(url));
      result->Success(&response);
    }
    else if (method_call.method_name().compare("evaluate") == 0)
    {
      flutter::EncodableMap args = *std::get_if<flutter::EncodableMap>(method_call.arguments());
      webview::Offscreen *wv = (webview::Offscreen *)std::get<int64_t>(ValueOrNull(args, "webview"));
      std::string script = std::get<std::string>(ValueOrNull(args, "script"));
      if(!(wv && wv->evaluate(script, result.release()))){
        result->Error(webview::TAG, "Error at evaluate");
      }
    }
	else if (method_call.method_name().compare("setCookies") == 0) 
	{
		flutter::EncodableMap args = *std::get_if<flutter::EncodableMap>(method_call.arguments());
		webview::Offscreen* wv = (webview::Offscreen*)std::get<int64_t>(ValueOrNull(args, "webview"));
		std::string url = std::get<std::string>(ValueOrNull(args, "url"));
		std::string cookies = std::get<std::string>(ValueOrNull(args, "cookies"));
		if (wv && cookies.length() > 0 && url.length() > 0) {
			std::vector<std::string> ster = split(cookies, ";");
			std::vector<std::string>::iterator iter;
			std::string json = "[";
			std::string _domain = getUrlDomain(url);
			for (iter = ster.begin(); iter != ster.end(); iter++) {
				std::vector<std::string> ster2 = split(*iter, "=");
				if (ster2.size() > 1) {
					std::vector<std::string>::iterator iterB = ster2.begin();
					std::string _s = *iterB; iterB++;
					std::string _b = *iterB;
					json = json + "{\"url\":\"" + url +
						"\",\"name\":\"" + _s +
						"\",\"value\":\"" + _b  +
						"\",\"domain\":\"" + _domain +
						"\",\"path\":\"/" + 
						"\"},";					
				}
			}
			if (json.length() > 1) {
				json = json.substr(0, json.length() - 1) + "]";
			}			
			if (!(json.length() > 2 && wv->callDevToolsProtocolMethod("Network.setCookies", "{\"cookies\":" + json + "}", result.release()))) {
				result->Error(webview::TAG, "Error at setCookies");
				return;
			}
		} else
			result->Error(webview::TAG, "Unset at setCookies");
	}
    else if (method_call.method_name().compare("setUserAgent") == 0)
    {
      flutter::EncodableMap args = *std::get_if<flutter::EncodableMap>(method_call.arguments());
      webview::Offscreen *wv = (webview::Offscreen *)std::get<int64_t>(ValueOrNull(args, "webview"));
      std::string ua = std::get<std::string>(ValueOrNull(args, "ua"));
      if(!(wv && wv->callDevToolsProtocolMethod("Network.setUserAgentOverride", "{\"userAgent\":\"" + ua + "\"}", result.release()))){
        result->Error(webview::TAG, "Error at evaluate");
      }
    }
    else if (method_call.method_name().compare("close") == 0)
    {
      webview::Offscreen *wv = (webview::Offscreen *)*std::get_if<int64_t>(method_call.arguments());
      delete wv;
      result->Success();
    }
    else
    {
      result->NotImplemented();
    }
  }

} // namespace

void FlutterWebviewPluginRegisterWithRegistrar(
    FlutterDesktopPluginRegistrarRef registrar)
{
  FlutterWebviewPlugin::RegisterWithRegistrar(
      flutter::PluginRegistrarManager::GetInstance()
          ->GetRegistrar<flutter::PluginRegistrarWindows>(registrar));
}
