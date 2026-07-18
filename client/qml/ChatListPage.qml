pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Wim.Client

Rectangle {
    id: root

    required property var controller
    signal conversationOpened()

    color: Theme.surface

    function openConversation(index) {
        if (index < 0)
            return
        root.controller.selectConversation(index)
        root.conversationOpened()
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        RowLayout {
            Layout.fillWidth: true
            Layout.leftMargin: Tokens.space4
            Layout.rightMargin: Tokens.space3
            Layout.topMargin: Tokens.space3
            Layout.bottomMargin: Tokens.space2
            spacing: Tokens.space2

            Label {
                Layout.fillWidth: true
                text: qsTr("会话")
                color: Theme.textPrimary
                font.pixelSize: Typography.headline
                font.bold: true
            }

            ToolButton {
                icon.source: Icons.compose
                icon.color: Theme.textPrimary
                Accessible.name: qsTr("新建会话")
            }
        }

        TextField {
            id: searchField
            Layout.fillWidth: true
            Layout.leftMargin: Tokens.space3
            Layout.rightMargin: Tokens.space3
            Layout.bottomMargin: Tokens.space2
            leftPadding: searchIcon.width + Tokens.space3 * 2
            placeholderText: qsTr("搜索会话")
            selectByMouse: true
            Accessible.name: qsTr("搜索会话")

            AppIcon {
                id: searchIcon
                anchors.left: parent.left
                anchors.leftMargin: Tokens.space3
                anchors.verticalCenter: parent.verticalCenter
                width: 18
                height: 18
                source: Icons.search
                color: Theme.textSecondary
            }

            background: Rectangle {
                radius: Tokens.radiusMedium
                color: Theme.surfaceMuted
                border.width: searchField.activeFocus ? 1 : 0
                border.color: Theme.accent
            }
        }

        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true

            ListView {
                id: conversationList
                anchors.fill: parent
                visible: count > 0
                clip: true
                currentIndex: root.controller.selectedConversationIndex
                model: root.controller.conversations
                boundsBehavior: Flickable.StopAtBounds
                keyNavigationEnabled: true
                focus: true

                Component.onCompleted: Qt.callLater(function() {
                    contentY = root.controller.conversationListPosition()
                })
                Component.onDestruction:
                    root.controller.saveConversationListPosition(contentY)
                onMovementEnded:
                    root.controller.saveConversationListPosition(contentY)

                Keys.onReturnPressed: event => {
                    root.openConversation(currentIndex)
                    event.accepted = true
                }
                Keys.onEnterPressed: event => {
                    root.openConversation(currentIndex)
                    event.accepted = true
                }

                ScrollBar.vertical: ScrollBar {}

                delegate: Item {
                    id: conversationDelegate

                    required property int index
                    required property string title
                    required property string preview
                    required property string timestamp
                    required property string avatarColor
                    required property int unreadCount
                    required property bool pinned
                    required property bool muted
                    required property bool online

                    width: ListView.view.width
                    height: conversationRow.height

                    ConversationRow {
                        id: conversationRow

                        anchors.left: parent.left
                        anchors.right: parent.right
                        conversationTitle: conversationDelegate.title
                        previewText: conversationDelegate.preview
                        timeText: conversationDelegate.timestamp
                        avatarColor: conversationDelegate.avatarColor
                        unreadCount: conversationDelegate.unreadCount
                        pinned: conversationDelegate.pinned
                        muted: conversationDelegate.muted
                        online: conversationDelegate.online
                        selected: conversationDelegate.index
                                  === root.controller.selectedConversationIndex
                        onActivated:
                            root.openConversation(conversationDelegate.index)
                        onPinRequested:
                            root.controller.togglePinned(conversationDelegate.index)
                        onMuteRequested:
                            root.controller.toggleMuted(conversationDelegate.index)
                        onMarkReadRequested:
                            root.controller.markRead(conversationDelegate.index)
                    }
                }
            }

            EmptyState {
                anchors.centerIn: parent
                visible: conversationList.count === 0
                title: qsTr("还没有会话")
                description: qsTr("添加联系人或创建群聊后，会话会出现在这里。")
                actionText: qsTr("开始新会话")
            }
        }
    }
}
