#pragma once
#include <QGuiApplication>

#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
#include <QtGui/qguiapplication_platform.h>
#if QT_CONFIG(wayland)
#define LUDASH_HAS_QT_WAYLAND_SYNC 1
#endif
#endif
#ifndef LUDASH_HAS_QT_WAYLAND_SYNC
#define LUDASH_HAS_QT_WAYLAND_SYNC 0
#endif
