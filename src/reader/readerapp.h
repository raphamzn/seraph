#pragma once

#include <QString>
#include <functional>

class QGuiApplication;

// `seraph --view <file>`: a standalone markdown reader window. Skips the file
// manager entirely — no single-instance socket, no session.json, no models —
// and loads ReaderMain.qml with only what reading needs.
int runReader(QGuiApplication &app, const QString &path, bool timingEnabled,
              const std::function<void(const char *)> &mark);
