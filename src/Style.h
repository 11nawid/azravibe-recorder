// Azravibe — design tokens: colors and stylesheet.
#pragma once

#include <QString>

namespace az {
inline constexpr const char* BG       = "#0F0F16";   // window body
inline constexpr const char* PANEL    = "#15151D";   // cards / inputs
inline constexpr const char* PANEL2   = "#1A1A23";   // raised
inline constexpr const char* BORDER   = "#23232F";   // hairline
inline constexpr const char* TEXT     = "#ECECF3";
inline constexpr const char* DIM      = "#8B8B9D";
inline constexpr const char* VIOLET   = "#8B5CF6";
inline constexpr const char* CYAN     = "#22D3EE";
inline constexpr const char* RED      = "#FF4D5E";
inline constexpr const char* AMBER    = "#FFB454";

inline QString styleSheet() {
    return QStringLiteral(R"QSS(
        QWidget#root {
            background: %1;
            border-radius: 18px;
            border: 1px solid rgba(255, 255, 255, 14);
        }
        QWidget#titlebar { background: transparent; }
        QLabel#appName { color: %2; font-size: 12px; font-weight: 600; }
        QLabel#appNameFaint { color: %7; font-size: 12px; font-weight: 400; }
        QToolButton#iconButton {
            background: transparent; border: 0; border-radius: 9px; padding: 5px 10px;
        }
        QToolButton#iconButton:hover { background: #1F1F29; }
        QToolButton#closeButton {
            background: transparent; border: 0; border-radius: 9px; padding: 5px 10px;
        }
        QToolButton#closeButton:hover { background: #33171E; }
        QComboBox {
            background: %3; color: %2;
            border: 1px solid %4; border-radius: 11px;
            padding: 10px 12px; padding-right: 34px; font-size: 12px;
        }
        QComboBox:hover { border-color: #2E2E3B; }
        QComboBox:focus { border-color: rgba(139, 92, 246, 130); }
        QComboBox:disabled { color: %5; background: #121218; }
        QComboBox QAbstractItemView {
            background: %6; color: %2;
            border: 1px solid #26262F; border-radius: 12px;
            padding: 6px; outline: 0;
            selection-background-color: rgba(139, 92, 246, 64);
            selection-color: %2;
        }
        QComboBox QAbstractItemView::item { padding: 8px 10px; border-radius: 8px; min-height: 22px; }
        QToolTip {
            background: %6; color: %2; border: 1px solid #2A2A35; border-radius: 8px; padding: 6px 10px;
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
