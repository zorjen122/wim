import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import WimiClient

ColumnLayout {
    id: root

    property string iconText: "✦"
    property string title: qsTr("这里还没有内容")
    property string description: ""
    property string actionText: ""
    signal actionRequested()

    spacing: Tokens.space3

    Label {
        Layout.alignment: Qt.AlignHCenter
        text: root.iconText
        color: Theme.accent
        font.pixelSize: 42
    }

    Label {
        Layout.alignment: Qt.AlignHCenter
        text: root.title
        color: Theme.textPrimary
        font.pixelSize: Typography.title
        font.bold: true
        horizontalAlignment: Text.AlignHCenter
    }

    Label {
        Layout.alignment: Qt.AlignHCenter
        Layout.maximumWidth: 360
        visible: text.length > 0
        text: root.description
        color: Theme.textSecondary
        font.pixelSize: Typography.body
        wrapMode: Text.Wrap
        horizontalAlignment: Text.AlignHCenter
    }

    Button {
        Layout.alignment: Qt.AlignHCenter
        visible: root.actionText.length > 0
        text: root.actionText
        onClicked: root.actionRequested()
    }
}

