pragma Singleton

import QtQuick

QtObject {
    property bool dark: false

    readonly property color accent: dark ? "#7EA6FF" : "#315FD6"
    readonly property color accentHover: dark ? "#9AB9FF" : "#274EBC"
    readonly property color accentContainer: dark ? "#1D3568" : "#DFE7FF"
    readonly property color canvas: dark ? "#0B1020" : "#F5F7FC"
    readonly property color surface: dark ? "#121A2B" : "#FFFFFF"
    readonly property color surfaceElevated: dark ? "#182238" : "#FFFFFF"
    readonly property color surfaceMuted: dark ? "#1A2438" : "#EDF1F8"
    readonly property color textPrimary: dark ? "#F3F6FF" : "#172033"
    readonly property color textSecondary: dark ? "#AEBAD0" : "#5F6B7E"
    readonly property color border: dark ? "#2A3852" : "#DDE3EC"
    readonly property color divider: dark ? "#223049" : "#E8ECF2"
    readonly property color success: dark ? "#6FD3A5" : "#18794E"
    readonly property color warning: dark ? "#F3BE67" : "#9A6700"
    readonly property color error: dark ? "#FF8A8A" : "#C93737"
    readonly property color messageIncoming: dark ? "#182238" : "#FFFFFF"
    readonly property color messageOutgoing: dark ? "#1B315C" : "#E3EAFF"
    readonly property color selection: dark ? "#203A70" : "#E8EEFF"
    readonly property color scrim: dark ? "#99000000" : "#660B1020"
}

