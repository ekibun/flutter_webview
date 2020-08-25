/*
 * @Description: example
 * @Author: ekibun
 * @Date: 2020-08-08 08:16:51
 * @LastEditors: ekibun
 * @LastEditTime: 2020-08-24 22:28:21
 */
import 'dart:async';

import 'package:flutter/material.dart';
import 'dart:typed_data';

import 'package:flutter_qjs/flutter_qjs.dart';
import 'package:flutter_webview/flutter_webview.dart';

import 'code/editor.dart';

void main() {
  runApp(MyApp());
}

class MyApp extends StatelessWidget {
  const MyApp({Key key}) : super(key: key);

  @override
  Widget build(BuildContext context) {
    return MaterialApp(
      title: 'flutter_qjs',
      debugShowCheckedModeBanner: false,
      theme: ThemeData(
        appBarTheme: AppBarTheme(brightness: Brightness.dark, elevation: 0),
        backgroundColor: Colors.grey[300],
        primaryColorBrightness: Brightness.dark,
      ),
      routes: {
        'home': (BuildContext context) => TestPage(),
      },
      initialRoute: 'home',
    );
  }
}

class TestPage extends StatefulWidget {
  @override
  State<StatefulWidget> createState() => _TestPageState();
}

Future<dynamic> webview(String url, Function callback) async {
  Completer c = new Completer();
  var webview = FlutterWebview();
  await webview.setMethodHandler((String method, dynamic args) async {
    print("$method($args)");
    if (method == "onNavigationCompleted") {
      await Future.delayed(Duration(seconds: 10));
      if (!c.isCompleted) c.completeError("Webview Call timeout 10 seconds after page completed.");
    } else if ((await callback([method, args])) == true) {
      print(args);
      if (!c.isCompleted) c.complete(args);
    }
    return;
  });
  await webview.navigate(url);
  Future.delayed(Duration(seconds: 100)).then((value) {
    if (!c.isCompleted) c.completeError("Webview Call timeout 100 seconds.");
  });
  dynamic data = await c.future;
  webview.destroy();
  print(data);
  return data;
}

class _TestPageState extends State<TestPage> {
  String code, resp;
  FlutterJs engine;

  _createEngine() async {
    if (engine != null) return;
    engine = FlutterJs();
    await engine.setMethodHandler((String method, List arg) async {
      switch (method) {
        case "webview":
          return await webview(arg[0], arg[1]);
        default:
          return JsMethodHandlerNotImplement();
      }
    });
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(
        title: Text("JS engine test"),
      ),
      body: SingleChildScrollView(
        padding: const EdgeInsets.all(16),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            SingleChildScrollView(
              scrollDirection: Axis.horizontal,
              child: Row(
                children: [
                  FlatButton(child: Text("create engine"), onPressed: _createEngine),
                  FlatButton(
                      child: Text("evaluate"),
                      onPressed: () async {
                        if (engine == null) {
                          print("please create engine first");
                          return;
                        }
                        try {
                          resp = (await engine.evaluate(code ?? '', "<eval>")).toString();
                        } catch (e) {
                          resp = e.toString();
                        }
                        setState(() {});
                      }),
                  FlatButton(
                      child: Text("close engine"),
                      onPressed: () async {
                        if (engine == null) return;
                        await engine.destroy();
                        engine = null;
                      }),
                ],
              ),
            ),
            Container(
              padding: const EdgeInsets.all(12),
              color: Colors.grey.withOpacity(0.1),
              constraints: BoxConstraints(minHeight: 200),
              child: CodeEditor(
                onChanged: (v) {
                  code = v;
                },
              ),
            ),
            SizedBox(height: 16),
            Text("result:"),
            SizedBox(height: 16),
            Container(
              width: double.infinity,
              padding: const EdgeInsets.all(12),
              color: Colors.green.withOpacity(0.05),
              constraints: BoxConstraints(minHeight: 100),
              child: SelectableText(resp ?? ''),
            ),
          ],
        ),
      ),
    );
  }
}
