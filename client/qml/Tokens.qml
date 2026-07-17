pragma Singleton

import QtQuick

QtObject {
    readonly property int space1: 4
    readonly property int space2: 8
    readonly property int space3: 12
    readonly property int space4: 16
    readonly property int space6: 24
    readonly property int space8: 32

    readonly property int radiusSmall: 8
    readonly property int radiusMedium: 12
    readonly property int radiusLarge: 16
    readonly property int radiusFull: 999

    readonly property int iconSmall: 16
    readonly property int iconMedium: 20
    readonly property int iconLarge: 24

    readonly property int avatarSmall: 32
    readonly property int avatarMedium: 40
    readonly property int avatarLarge: 48
    readonly property int avatarProfile: 72

    readonly property int touchTarget: 44
    readonly property int navigationRailWidth: 72
    readonly property int conversationListWidth: 344
    readonly property int compactBreakpoint: 720
    readonly property int expandedBreakpoint: 1100

    readonly property int fastDuration: 120
    readonly property int normalDuration: 180
}

