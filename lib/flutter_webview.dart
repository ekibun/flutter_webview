/*
 * @Description: quickjs engine
 * @Author: ekibun
 * @Date: 2020-08-08 08:29:09
 * @LastEditors: ekibun
 * @LastEditTime: 2020-08-24 13:33:20
 */
import 'dart:async';
import 'package:flutter/services.dart';

/// Handle function to manage js call with `dart(method, ...args)` function.
typedef MethodHandler = Future<dynamic> Function(String method, dynamic args);

/// return this in [MethodHandler] to mark method not implemented.
class MethodHandlerNotImplement {}

/// FlutterWebview instance.
/// Each [FlutterWebview] object creates a new thread that runs a simple js loop.
/// Make sure call `destroy` to terminate thread and release memory when you don't need it.
class FlutterWebview {
  dynamic _webview;

  _ensureEngine() async {
    if (_webview == null) {
      _webview = await _FlutterWebview.instance._channel.invokeMethod("create");
      print(_webview);
    }
  }

  /// Set a handler to manage js call with `dart(method, ...args)` function.
  setMethodHandler(MethodHandler handler) async {
    await _ensureEngine();
    _FlutterWebview.instance.methodHandlers[_webview] = handler;
  }

  /// Terminate thread and release memory.
  destroy() async {
    if (_webview != null) {
      await _FlutterWebview.instance._channel
          .invokeMethod("close", _webview);
      _webview = null;
    }
  }

  /// Evaluate js script.
  Future<bool> navigate(String url, { String script }) async {
    await _ensureEngine();
    var arguments = {"webview": _webview, "url": url, "script": script ?? "" };
    return _FlutterWebview.instance._channel.invokeMethod("navigate", arguments);
  }
}

class _FlutterWebview {
  factory _FlutterWebview() => _getInstance();
  static _FlutterWebview get instance => _getInstance();
  static _FlutterWebview _instance;
  MethodChannel _channel = const MethodChannel('soko.ekibun.flutter_webview');
  Map<dynamic, MethodHandler> methodHandlers =
      Map<dynamic, MethodHandler>();
  _FlutterWebview._internal() {
    _channel.setMethodCallHandler((call) async {
      var engine = call.arguments["webview"];
      var args = call.arguments["args"];
      if (methodHandlers[engine] == null) return call.noSuchMethod(null);
      var ret = await methodHandlers[engine](call.method, args);
      if (ret is MethodHandlerNotImplement) return call.noSuchMethod(null);
      return ret;
    });
  }

  static _FlutterWebview _getInstance() {
    if (_instance == null) {
      _instance = new _FlutterWebview._internal();
    }
    return _instance;
  }
}
