/*
 * @Description: 
 * @Author: ekibun
 * @Date: 2020-08-23 17:38:03
 * @LastEditors: ekibun
 * @LastEditTime: 2020-08-24 15:06:44
 */
#include <windows.h>
#include <functional>
#include <wrl.h>
#include <flutter/standard_method_codec.h>
#include <comdef.h>
#include <winerror.h>
#include <codecvt>

namespace webview
{
#undef interface
#include "webview/WebView2.h"

  auto TAG = "FlutterWebviewException";

  LPWSTR to_lpwstr(const std::string s)
  {
    int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, NULL, 0);
    wchar_t *ws = new wchar_t[n];
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, ws, n);
    return ws;
  }

  std::string from_lpwstr(wchar_t *wchar)
  {
    std::string szDst;
    wchar_t *wText = wchar;
    DWORD dwNum = WideCharToMultiByte(CP_OEMCP, NULL, wText, -1, NULL, 0, NULL, FALSE);
    char *psText;
    psText = new char[dwNum];
    WideCharToMultiByte(CP_OEMCP, NULL, wText, -1, psText, dwNum, NULL, FALSE);
    szDst = psText;
    delete[] psText;
    return szDst;
  }

  class Offscreen
      : public ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler,
        public ICoreWebView2CreateCoreWebView2ControllerCompletedHandler,
        public ICoreWebView2NavigationCompletedEventHandler,
        public ICoreWebView2WebMessageReceivedEventHandler,
        public ICoreWebView2WebResourceRequestedEventHandler
  {
    ICoreWebView2Controller *webviewController = nullptr;
    ICoreWebView2 *webviewWindow = nullptr;
    HWND hwnd;
    std::shared_ptr<flutter::MethodChannel<flutter::EncodableValue>> channel;
    flutter::MethodResult<flutter::EncodableValue> *result;

    void invokeChannelMethod(std::string name, flutter::EncodableValue args)
    {
      auto map = new flutter::EncodableMap();
      (*map)[std::string("webview")] = (int64_t)this;
      (*map)[std::string("args")] = args;

      channel->InvokeMethod(
          name,
          std::make_unique<flutter::EncodableValue>(*map),
          nullptr);
    }

  public:
    Offscreen(HWND hwnd,
              std::shared_ptr<flutter::MethodChannel<flutter::EncodableValue>> channel,
              flutter::MethodResult<flutter::EncodableValue> *result)
        : hwnd(hwnd), channel(channel), result(result)
    {
      if (FAILED(CreateCoreWebView2Environment(this)))
      {
        result->Error(TAG, "Failed at CreateCoreWebView2Environment");
        delete this->result;
        delete this;
      }
    }

    ULONG STDMETHODCALLTYPE AddRef() { return 1; }
    ULONG STDMETHODCALLTYPE Release() { return 1; }
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, LPVOID *ppv)
    {
      return S_OK;
    }
    HRESULT STDMETHODCALLTYPE Invoke(HRESULT res,
                                     ICoreWebView2Environment *env)
    {
      env->CreateCoreWebView2Controller(hwnd, this);
      return S_OK;
    }
    HRESULT STDMETHODCALLTYPE Invoke(HRESULT res,
                                     ICoreWebView2Controller *controller)
    {
      controller->AddRef();
      this->webviewController = controller;

      EventRegistrationToken token;
      controller->get_CoreWebView2(&webviewWindow);
      webviewWindow->AddRef();
      webviewWindow->AddWebResourceRequestedFilter(L"*", COREWEBVIEW2_WEB_RESOURCE_CONTEXT_ALL);
      webviewWindow->add_WebResourceRequested(this, &token);
      webviewWindow->add_NavigationCompleted(this, &token);
      webviewWindow->add_WebMessageReceived(this, &token);
      result->Success((int64_t)this);
      delete result;
      result = nullptr;

      return S_OK;
    }
    HRESULT STDMETHODCALLTYPE Invoke(
        ICoreWebView2 *sender,
        ICoreWebView2NavigationCompletedEventArgs *args)
    {
      BOOL success = FALSE;
      args->get_IsSuccess(&success);
      if (!success)
      {
        COREWEBVIEW2_WEB_ERROR_STATUS webErrorStatus;
        args->get_WebErrorStatus(&webErrorStatus);
        invokeChannelMethod("onNavigationCompleted", (int64_t) webErrorStatus);
      } else {
        invokeChannelMethod("onNavigationCompleted", flutter::EncodableValue());
      }
      return S_OK;
    }
    HRESULT STDMETHODCALLTYPE Invoke(
        ICoreWebView2 *sender, ICoreWebView2WebMessageReceivedEventArgs *args)
    {
      LPWSTR message;
      args->TryGetWebMessageAsString(&message);
      invokeChannelMethod("onMessage", from_lpwstr(message));
      sender->PostWebMessageAsString(message);

      CoTaskMemFree(message);
      return S_OK;
    }

    HRESULT STDMETHODCALLTYPE
    Invoke(ICoreWebView2 *sender,
           ICoreWebView2WebResourceRequestedEventArgs *args)
    {
      ICoreWebView2WebResourceRequest *request;
      args->get_Request(&request);

      std::map<flutter::EncodableValue, flutter::EncodableValue> frequest;
      LPWSTR str;
      request->get_Uri(&str);
      frequest[std::string("url")] = from_lpwstr(str);
      CoTaskMemFree(str);
      request->get_Method(&str);
      frequest[std::string("method")] = from_lpwstr(str);
      CoTaskMemFree(str);
      ICoreWebView2HttpRequestHeaders *headers;
      request->get_Headers(&headers);
      ICoreWebView2HttpHeadersCollectionIterator *iterator;
      flutter::EncodableList fheaders;
      if (SUCCEEDED(headers->GetIterator(&iterator)))
      {
        BOOL hasCurrent = FALSE;
        while (SUCCEEDED(iterator->get_HasCurrentHeader(&hasCurrent)) && hasCurrent)
        {
          LPWSTR name, value;
          if (FAILED(iterator->GetCurrentHeader(&name, &value)))
            break;
          std::map<flutter::EncodableValue, flutter::EncodableValue> kv;
          kv[from_lpwstr(name)] = from_lpwstr(value);
          CoTaskMemFree(name);
          CoTaskMemFree(value);
          fheaders.emplace_back(kv);
          BOOL hasNext = FALSE;
          if (FAILED(iterator->MoveNext(&hasNext)) || !hasNext)
            break;
        }
      }
      frequest[std::string("headers")] = fheaders;

      invokeChannelMethod("onRequest", frequest);

      return E_INVALIDARG;
    }

    bool navigate(std::string url)
    {
      if (webviewWindow)
      {
        return SUCCEEDED(webviewWindow->Navigate(to_lpwstr(url)));
      }
      return false;
    }

    bool evaluate(std::string script, flutter::MethodResult<flutter::EncodableValue> *jsResult)
    {
      if (webviewWindow)
      {
        return SUCCEEDED(webviewWindow->ExecuteScript(
            to_lpwstr(script),
            Microsoft::WRL::Callback<ICoreWebView2ExecuteScriptCompletedHandler>(
                [jsResult](HRESULT errorCode, LPCWSTR resultObjectAsJson) -> HRESULT {
                  if (resultObjectAsJson)
                    jsResult->Success(from_lpwstr((wchar_t *)resultObjectAsJson));
                  else
                    jsResult->Error(TAG, "Error at ExecuteScript", nullptr);
                  return S_OK;
                })
                .Get()));
      }
      return false;
    }

    bool callDevToolsProtocolMethod(std::string methodName, std::string parametersAsJson, flutter::MethodResult<flutter::EncodableValue> *callResult)
    {
      if (webviewWindow)
      {
        return SUCCEEDED(webviewWindow->CallDevToolsProtocolMethod(
            to_lpwstr(methodName), to_lpwstr(parametersAsJson),
            Microsoft::WRL::Callback<ICoreWebView2CallDevToolsProtocolMethodCompletedHandler>(
                [callResult](HRESULT errorCode, LPCWSTR resultObjectAsJson) -> HRESULT {
                  if (resultObjectAsJson)
                    callResult->Success(from_lpwstr((wchar_t *)resultObjectAsJson));
                  else
                    callResult->Error(TAG, "Error at CallDevToolsProtocolMethod", nullptr);
                  return S_OK;
                })
                .Get()));
      }
      return false;
    }

    ~Offscreen()
    {
      std::cout << "close" << std::endl;
      if (webviewController)
        webviewController->Close();
    }

    // ICoreWebView2Controller *webviewController = nullptr;
    // ICoreWebView2 *webviewWindow = nullptr;
    // flutter::MethodChannel<flutter::EncodableValue> *_channel;

    // LPWSTR to_lpwstr(const std::string s)
    // {
    //   int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, NULL, 0);
    //   wchar_t *ws = new wchar_t[n];
    //   MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, ws, n);
    //   return ws;
    // }

    // void invokeChannelMethod(std::string name, flutter::EncodableValue args, int64_t webview)
    // {
    //   auto map = new flutter::EncodableMap();
    //   (*map)[std::string("webview")] = webview;
    //   (*map)[std::string("args")] = args;

    //   _channel->InvokeMethod(
    //       name,
    //       std::make_unique<flutter::EncodableValue>(map),
    //       nullptr);
    // }

    // Offscreen(HWND hwnd, flutter::MethodChannel<flutter::EncodableValue> *channel, flutter::MethodResult<flutter::EncodableValue> *result)
    // {
    //   this->_channel = channel;

    //   std::cout << "create" << std::endl;

    //   HRESULT res = CreateCoreWebView2Environment(
    //       new webview2_com_handler(hwnd, this));
    //   // Microsoft::WRL::Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(
    //   //     [this](HRESULT result, ICoreWebView2Environment *env) -> HRESULT {
    //   //       std::cout << "env:" << env << std::endl;

    //   //       env->CreateCoreWebView2Controller(
    //   //           _hwnd,
    //   //           Microsoft::WRL::Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(
    //   //               [this](HRESULT result, ICoreWebView2Controller *controller) -> HRESULT {
    //   //                 std::cout << "controller:" << controller << std::endl;

    //   //                 webviewController = controller;
    //   //                 webviewController->get_CoreWebView2(&webviewWindow);
    //   //                 webviewWindow->AddRef();

    //   // ICoreWebView2Settings *Settings;
    //   // webviewWindow->get_Settings(&Settings);
    //   // Settings->put_IsScriptEnabled(TRUE);
    //   // Settings->put_AreDefaultScriptDialogsEnabled(TRUE);
    //   // Settings->put_IsWebMessageEnabled(TRUE);

    //   // RECT bounds;
    //   // GetClientRect(hwnd, &bounds);
    //   // webviewController->put_Bounds(bounds);

    //   // EventRegistrationToken token;
    //   // webviewWindow->AddWebResourceRequestedFilter(L"*", COREWEBVIEW2_WEB_RESOURCE_CONTEXT_ALL);
    //   // webviewWindow->add_WebResourceRequested(
    //   //     Microsoft::WRL::Callback<ICoreWebView2WebResourceRequestedEventHandler>(
    //   //         [this](ICoreWebView2 *sender, ICoreWebView2WebResourceRequestedEventArgs *args) -> HRESULT {
    //   //           ICoreWebView2WebResourceRequest *request;
    //   //           args->get_Request(&request);

    //   //           std::map<flutter::EncodableValue, flutter::EncodableValue> frequest;
    //   //           LPWSTR str;
    //   //           request->get_Uri(&str);
    //   //           frequest[std::string("url")] = wchar_tToString(str);
    //   //           std::cout << "url:" << str << std::endl;

    //   //           request->get_Method(&str);
    //   //           frequest[std::string("method")] = wchar_tToString(str);
    //   //           // TODO request->get_Content(&stream);
    //   //           ICoreWebView2HttpRequestHeaders *headers;
    //   //           request->get_Headers(&headers);
    //   //           ICoreWebView2HttpHeadersCollectionIterator *iterator;
    //   //           headers->GetIterator(&iterator);
    //   //           BOOL hasCurrent = FALSE;
    //   //           flutter::EncodableList fheaders;

    //   //           while (SUCCEEDED(iterator->get_HasCurrentHeader(&hasCurrent)) && hasCurrent)
    //   //           {
    //   //             LPWSTR name, value;
    //   //             iterator->GetCurrentHeader(&name, &value);
    //   //             std::map<flutter::EncodableValue, flutter::EncodableValue> kv;
    //   //             kv[wchar_tToString(name)] = wchar_tToString(value);
    //   //             fheaders.emplace_back(kv);
    //   //           }
    //   //           frequest[std::string("headers")] = fheaders;

    //   //           channel("onRequest", frequest, (int64_t)this);
    //   //           // // Override the response with an empty one to block the image.
    //   //           // // If put_Response is not called, the request will continue as normal.
    //   //           // wil::com_ptr<ICoreWebView2WebResourceResponse> response;
    //   //           // CHECK_FAILURE(m_webViewEnvironment->CreateWebResourceResponse(
    //   //           //     nullptr, 403 /*NoContent*/, L"Blocked", L"", &response));
    //   //           // CHECK_FAILURE(args->put_Response(response.get()));
    //   //           return E_INVALIDARG;
    //   //         })
    //   //         .Get(),
    //   //     &token);

    //   // std::cout << token.value << ":" << webviewWindow->Navigate(L"https://www.acfun.cn/bangumi/aa6001745") << std::endl;
    //   //             return S_OK;
    //   //           })
    //   //           .Get());
    //   //   return S_OK;
    //   // })
    //   // .Get());
    //   std::cout << "res:" << res << std::endl;
    // }
  };
} // namespace webview
