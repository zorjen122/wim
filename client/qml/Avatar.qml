import QtQuick
import QtQuick.Controls
import WimiClient

Rectangle {
    id: root

    required property string name
    property color avatarColor: Theme.accent
    property int size: Tokens.avatarLarge
    property bool online: false

    implicitWidth: size
    implicitHeight: size
    radius: width / 2
    color: avatarColor

    function initials(value) {
        const trimmed = value.trim()
        if (trimmed.length === 0)
            return "?"
        return trimmed.slice(0, Math.min(2, trimmed.length)).toUpperCase()
    }

    Label {
        anchors.centerIn: parent
        text: root.initials(root.name)
        color: "white"
        font.bold: true
        font.pixelSize: Math.max(Typography.caption, root.size * 0.34)
    }

    Rectangle {
        visible: root.online
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        width: Math.max(10, root.size * 0.25)
        height: width
        radius: width / 2
        color: Theme.success
        border.width: 2
        border.color: Theme.surface
    }
}

