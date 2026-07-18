import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Wim.Client

ItemDelegate {
    id: root

    required property string conversationTitle
    required property string previewText
    required property string timeText
    property color avatarColor: Theme.accent
    property int unreadCount: 0
    property bool pinned: false
    property bool muted: false
    property bool online: false
    property bool selected: false

    signal activated()
    signal pinRequested()
    signal muteRequested()
    signal markReadRequested()

    height: 72
    hoverEnabled: true
    focusPolicy: Qt.StrongFocus
    Accessible.name: conversationTitle + ", " + previewText
    onClicked: activated()
    onPressAndHold: contextMenu.popup()

    TapHandler {
        acceptedButtons: Qt.RightButton
        onTapped: contextMenu.popup()
    }

    Menu {
        id: contextMenu

        MenuItem {
            text: root.pinned ? qsTr("取消置顶") : qsTr("置顶会话")
            onTriggered: root.pinRequested()
        }

        MenuItem {
            text: root.muted ? qsTr("打开通知") : qsTr("静音通知")
            onTriggered: root.muteRequested()
        }

        MenuItem {
            enabled: root.unreadCount > 0
            text: qsTr("标为已读")
            onTriggered: root.markReadRequested()
        }
    }

    background: Rectangle {
        color: root.selected ? Theme.selection
                             : root.hovered ? Theme.surfaceMuted : Theme.surface

        Rectangle {
            visible: root.selected
            anchors.left: parent.left
            width: 3
            height: parent.height
            color: Theme.accent
        }
    }

    contentItem: RowLayout {
        spacing: Tokens.space3

        Avatar {
            name: root.conversationTitle
            avatarColor: root.avatarColor
            online: root.online
            size: Tokens.avatarLarge
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: Tokens.space1

            RowLayout {
                Layout.fillWidth: true
                spacing: Tokens.space2

                Label {
                    Layout.fillWidth: true
                    text: root.conversationTitle
                    color: Theme.textPrimary
                    font.pixelSize: Typography.titleSmall
                    font.bold: root.unreadCount > 0 || root.selected
                    elide: Text.ElideRight
                }

                Label {
                    text: root.timeText
                    color: root.unreadCount > 0 ? Theme.accent : Theme.textSecondary
                    font.pixelSize: Typography.caption
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: Tokens.space1

                Label {
                    Layout.fillWidth: true
                    text: root.previewText
                    color: Theme.textSecondary
                    font.pixelSize: Typography.bodySmall
                    elide: Text.ElideRight
                }

                AppIcon {
                    visible: root.pinned
                    Layout.preferredWidth: 14
                    Layout.preferredHeight: 14
                    source: Icons.pin
                    color: Theme.textSecondary
                }

                AppIcon {
                    visible: root.muted
                    Layout.preferredWidth: 14
                    Layout.preferredHeight: 14
                    source: Icons.muted
                    color: Theme.textSecondary
                }

                Rectangle {
                    visible: root.unreadCount > 0
                    implicitWidth: Math.max(22, unreadLabel.implicitWidth + Tokens.space2)
                    implicitHeight: 22
                    radius: Tokens.radiusFull
                    color: root.muted ? Theme.textSecondary : Theme.accent

                    Label {
                        id: unreadLabel
                        anchors.centerIn: parent
                        text: root.unreadCount > 99 ? "99+" : root.unreadCount
                        color: "white"
                        font.pixelSize: Typography.caption
                        font.bold: true
                    }
                }
            }
        }
    }
}
