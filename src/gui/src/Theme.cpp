/*
 * InputServer -- mouse and keyboard sharing utility
 *
 * This package is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * found in the file LICENSE that should have accompanied this file.
 *
 * This package is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include "Theme.h"

#include <QApplication>
#include <QPalette>
#include <QStyle>
#include <QStyleFactory>
#include <QStyleHints>
#include <QWidget>

namespace theme {

namespace {

struct Colors {
    const char* window;
    const char* card;
    const char* input;
    const char* border;
    const char* track;
    const char* text;
    const char* muted;
    const char* accent;
    const char* accent_hover;
    const char* success;
    const char* warning;
    const char* danger;
};

constexpr Colors kLight{
    "#f4f5f7", "#ffffff", "#ffffff", "#dfe3e8", "#e8ebef",
    "#1b1f24", "#5f6b7a",
    "#2563eb", "#1d4ed8", "#15803d", "#b45309", "#dc2626",
};

constexpr Colors kDark{
    "#15171b", "#1e2127", "#262a31", "#30353e", "#2a2e35",
    "#e8eaee", "#9aa3b1",
    "#3b82f6", "#60a5fa", "#22c55e", "#f59e0b", "#ef4444",
};

bool systemIsDark()
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    return QGuiApplication::styleHints()->colorScheme() == Qt::ColorScheme::Dark;
#else
    return false;
#endif
}

QPalette paletteFor(const Colors& c)
{
    QPalette p;
    p.setColor(QPalette::Window, QColor(c.window));
    p.setColor(QPalette::WindowText, QColor(c.text));
    p.setColor(QPalette::Base, QColor(c.input));
    p.setColor(QPalette::AlternateBase, QColor(c.track));
    p.setColor(QPalette::Text, QColor(c.text));
    p.setColor(QPalette::Button, QColor(c.card));
    p.setColor(QPalette::ButtonText, QColor(c.text));
    p.setColor(QPalette::ToolTipBase, QColor(c.card));
    p.setColor(QPalette::ToolTipText, QColor(c.text));
    p.setColor(QPalette::PlaceholderText, QColor(c.muted));
    p.setColor(QPalette::Highlight, QColor(c.accent));
    p.setColor(QPalette::HighlightedText, QColor("#ffffff"));
    p.setColor(QPalette::Link, QColor(c.accent));
    p.setColor(QPalette::Mid, QColor(c.border));
    for (auto role : {QPalette::WindowText, QPalette::Text, QPalette::ButtonText}) {
        p.setColor(QPalette::Disabled, role, QColor(c.muted));
    }
    return p;
}

QString styleSheetFor(const Colors& c)
{
    // %1 window, %2 card, %3 input, %4 border, %5 track, %6 text, %7 muted,
    // %8 accent, %9 accent hover, %10 success, %11 warning, %12 danger
    return QStringLiteral(R"(
QMainWindow, QDialog { background: %1; }
QToolTip { background: %2; color: %6; border: 1px solid %4; padding: 4px 6px; }

QLabel#labelAppTitle { font-size: 20px; font-weight: 600; }
QLabel#labelAppSubtitle, QLabel[hint="true"] { color: %7; }
QLabel[cardTitle="true"] { font-size: 15px; font-weight: 600; }
QLabel[warning="true"] {
    background: %5; border-left: 3px solid %11; border-radius: 6px; padding: 8px 10px;
}

QGroupBox[card="true"], QFrame[card="true"] {
    background: %2; border: 1px solid %4; border-radius: 12px;
    margin-top: 0px; padding: 16px;
    padding-top: 4px;
}
QGroupBox[card="true"]::title {
    subcontrol-origin: margin; subcontrol-position: top left;
    width: 0px; height: 0px; padding: 0px; margin: 0px; border: none;
}
QGroupBox[card="true"]::indicator { width: 0px; height: 0px; border: none; image: none; }

QFrame#frameRoleSwitch { background: %5; border-radius: 10px; }
QFrame#frameRoleSwitch QPushButton {
    background: transparent; border: none; border-radius: 8px;
    padding: 8px 16px; color: %7; font-weight: 500;
}
QFrame#frameRoleSwitch QPushButton:hover { color: %6; }
QFrame#frameRoleSwitch QPushButton:checked { background: %2; color: %6; }
QFrame#frameRoleSwitch QPushButton:disabled { color: %7; }
QFrame#frameRoleSwitch QPushButton:checked:disabled { background: %2; color: %7; }

QFrame#frameStatusPill { background: %5; border-radius: 11px; }
QFrame#frameStatusPill QLabel { font-weight: 500; }
QFrame#frameStatusPill QLabel#labelStatusDot { color: %7; }
QFrame#frameStatusPill[state="starting"] QLabel#labelStatusDot,
QFrame#frameStatusPill[state="stopping"] QLabel#labelStatusDot,
QFrame#frameStatusPill[state="restarting"] QLabel#labelStatusDot { color: %11; }
QFrame#frameStatusPill[state="on"] QLabel#labelStatusDot { color: %10; }

QPushButton {
    background: %2; color: %6; border: 1px solid %4; border-radius: 8px;
    padding: 6px 14px;
}
QPushButton:hover { border-color: %8; }
QPushButton:pressed { background: %5; }
QPushButton:disabled { color: %7; border-color: %4; }
QPushButton[primary="true"] {
    background: %8; color: #ffffff; border: none; font-weight: 600;
    padding: 9px 26px;
}
QPushButton[primary="true"]:hover { background: %9; }
QPushButton[primary="true"][running="true"] { background: %12; }
QPushButton[primary="true"][running="true"]:enabled:hover { background: %13; }
QPushButton[primary="true"][running="true"]:enabled:pressed { background: %14; }
QPushButton[primary="true"]:disabled { background: %5; color: %7; }
QPushButton#m_pButtonReload:enabled:hover { background: %5; border-color: %8; }
QPushButton#m_pButtonReload:enabled:pressed { background: %4; }

QLineEdit {
    background: %3; color: %6; border: 1px solid %4; border-radius: 6px;
    padding: 5px 8px; selection-background-color: %8;
}
QLineEdit:focus { border-color: %8; }
QComboBox { padding: 4px 8px; }
QListWidget#availableServers {
    background: %3; border: 1px solid %4; border-radius: 8px; padding: 4px;
}
QListWidget#availableServers::item { padding: 10px; border-radius: 6px; }
QListWidget#availableServers::item:selected { background: %8; color: #ffffff; }
QListWidget#availableServers::item:hover:!selected { background: %5; }
QLineEdit:disabled { color: %7; }

QToolButton { background: transparent; border: none; border-radius: 6px; color: %7; padding: 4px 8px; }
QToolButton:hover { background: %5; color: %6; }
QToolButton:checked { color: %6; }
)")
        .arg(c.window, c.card, c.input, c.border, c.track, c.text, c.muted, c.accent, c.accent_hover)
        .arg(c.success, c.warning, c.danger, QColor(c.danger).darker(115).name(), QColor(c.danger).darker(135).name());
}

void apply(QApplication& app)
{
    const Colors& colors = systemIsDark() ? kDark : kLight;
    app.setPalette(paletteFor(colors));
    app.setStyleSheet(styleSheetFor(colors));
}

} // namespace

void install(QApplication& app)
{
    // Fusion renders the same on Windows and macOS and honors the palette,
    // which the native styles only partly do.
    if (QStyle* fusion = QStyleFactory::create(QStringLiteral("Fusion"))) {
        app.setStyle(fusion);
    }
    apply(app);

#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    QObject::connect(QGuiApplication::styleHints(), &QStyleHints::colorSchemeChanged, &app,
                     [&app](Qt::ColorScheme) { apply(app); });
#endif
}

void repolish(QWidget* widget)
{
    widget->style()->unpolish(widget);
    widget->style()->polish(widget);
    for (auto* child : widget->findChildren<QWidget*>()) {
        child->style()->unpolish(child);
        child->style()->polish(child);
    }
    widget->update();
}

} // namespace theme
