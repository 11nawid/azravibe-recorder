// Azravibe — design tokens: colors and stylesheet (OBS-inspired neutral dark).
#pragma once

#include <QString>

namespace az {
inline constexpr const char* BG       = "#15171C";   // window body
inline constexpr const char* PANEL    = "#1D2027";   // cards / inputs
inline constexpr const char* PANEL2   = "#23262E";   // raised
inline constexpr const char* BORDER   = "#272B34";   // hairline
inline constexpr const char* TEXT     = "#E9ECF1";
inline constexpr const char* DIM      = "#8D93A0";
inline constexpr const char* ACCENT   = "#64748B";   // slate — muted steel accent
inline constexpr const char* STEEL    = "#C7CBD4";
inline constexpr const char* RED      = "#FF4D5E";
inline constexpr const char* AMBER    = "#E0A458";

inline QString styleSheet() {
    return QStringLiteral(R"QSS(
        QWidget#root {
            background: %1;
            border-radius: 16px;
            border: 1px solid rgba(255, 255, 255, 12);
        }
        QWidget#titlebar { background: transparent; }
        QLabel#appName { color: %2; font-size: 12px; font-weight: 600; }
        QLabel#appNameFaint { color: %7; font-size: 12px; font-weight: 400; }
        QWidget#outputbar { background: %3; border: 1px solid %4; border-radius: 11px; }
        QLabel#locationPath { color: #C9CDD6; font-size: 11px; }
        QToolButton#iconButton {
            background: transparent; border: 0; border-radius: 8px; padding: 5px 10px;
        }
        QToolButton#iconButton:hover { background: #23262E; }
        QToolButton#closeButton {
            background: transparent; border: 0; border-radius: 8px; padding: 5px 10px;
        }
        QToolButton#closeButton:hover { background: #33222A; }
        QComboBox {
            background: %3; color: %2;
            border: 1px solid %4; border-radius: 11px;
            padding: 10px 12px; padding-right: 34px; font-size: 12px;
        }
        QComboBox:hover { border-color: #323743; }
        QComboBox:focus { border-color: #3F4757; }
        QComboBox:disabled { color: %5; background: #191B21; }
        QComboBox QAbstractItemView {
            background: %6; color: %2;
            border: 1px solid #2E3340; border-radius: 12px;
            padding: 6px; outline: 0;
            selection-background-color: #2E3848;
            selection-color: %2;
        }
        QComboBox QAbstractItemView::item { padding: 8px 10px; border-radius: 8px; min-height: 22px; }
        QToolTip {
            background: %6; color: %2; border: 1px solid #2F3440; border-radius: 8px; padding: 6px 10px;
        }
    )QSS")
        .arg(QString::fromUtf8(BG),     /*%1*/
             QString::fromUtf8(TEXT),   /*%2*/
             QString::fromUtf8(PANEL),  /*%3*/
             QString::fromUtf8(BORDER), /*%4*/
             QString::fromUtf8(DIM),    /*%5*/
             QString::fromUtf8(PANEL2), /*%6*/
             QString::fromUtf8(DIM)     /*%7*/);
}
} // namespace az
