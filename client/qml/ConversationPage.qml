import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Wim.Client

Rectangle {
    id: root

    required property var controller
    property bool compactMode: false
    signal backRequested()

    color: Theme.canvas

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 64
            color: Theme.surface

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: Tokens.space3
                anchors.rightMargin: Tokens.space3
                spacing: Tokens.space2

                ToolButton {
                    visible: root.compactMode
                    icon.source: Icons.back
                    icon.color: Theme.textPrimary
                    onClicked: root.backRequested()
                    Accessible.name: qsTr("返回会话列表")
                }

                Avatar {
                    visible: root.controller.selectedConversationIndex >= 0
                    name: root.controller.selectedConversationTitle
                    size: Tokens.avatarMedium
                    avatarColor: Theme.accent
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 0

                    Label {
                        Layout.fillWidth: true
                        text: root.controller.selectedConversationTitle
                        color: Theme.textPrimary
                        font.pixelSize: Typography.titleSmall
                        font.bold: true
                        elide: Text.ElideRight
                    }

                    Label {
                        visible: root.controller.selectedConversationIndex >= 0
                        text: qsTr("最近在线")
                        color: Theme.textSecondary
                        font.pixelSize: Typography.caption
                    }
                }

                ToolButton {
                    icon.source: Icons.search
                    icon.color: Theme.textSecondary
                    Accessible.name: qsTr("在会话中搜索")
                }

                ToolButton {
                    icon.source: Icons.more
                    icon.color: Theme.textSecondary
                    onClicked: detailsDrawer.open()
                    Accessible.name: qsTr("会话详情")
                }
            }

            Rectangle {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                height: 1
                color: Theme.divider
            }
        }

        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true

            ListView {
                id: timeline
                anchors.fill: parent
                anchors.leftMargin: Math.max(Tokens.space3,
                                             (parent.width - 900) / 2)
                anchors.rightMargin: Math.max(Tokens.space3,
                                              (parent.width - 900) / 2)
                anchors.topMargin: Tokens.space3
                anchors.bottomMargin: Tokens.space2
                visible: root.controller.selectedConversationIndex >= 0
                clip: true
                spacing: Tokens.space1
                model: root.controller.messages
                boundsBehavior: Flickable.StopAtBounds
                verticalLayoutDirection: ListView.TopToBottom

                ScrollBar.vertical: ScrollBar {}

                onCountChanged: Qt.callLater(positionViewAtEnd)
                onMovementEnded: root.controller.saveTimelinePosition(contentY)

                Component.onCompleted: Qt.callLater(function() {
                    contentY = root.controller.timelinePosition()
                })
                Component.onDestruction:
                    root.controller.saveTimelinePosition(contentY)

                delegate: Item {
                    id: messageDelegate

                    required property var clientMessageId
                    required property string body
                    required property string timestamp
                    required property bool outgoing
                    required property string deliveryState

                    width: ListView.view.width
                    height: messageBubble.height

                    MessageBubble {
                        id: messageBubble

                        width: parent.width
                        messageText: messageDelegate.body
                        timeText: messageDelegate.timestamp
                        outgoing: messageDelegate.outgoing
                        deliveryState: messageDelegate.deliveryState
                    }
                }
            }

            Button {
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                anchors.rightMargin: Tokens.space4
                anchors.bottomMargin: Tokens.space3
                visible: root.controller.selectedConversationUnreadCount > 0
                text: qsTr("↓ %1 条未读")
                      .arg(root.controller.selectedConversationUnreadCount)
                onClicked: {
                    const firstUnread = Math.max(
                        0, timeline.count
                           - root.controller.selectedConversationUnreadCount)
                    timeline.positionViewAtIndex(firstUnread, ListView.Center)
                    root.controller.markRead(
                        root.controller.selectedConversationIndex)
                }
                Accessible.name: qsTr("跳到未读消息")
            }

            EmptyState {
                anchors.centerIn: parent
                visible: root.controller.selectedConversationIndex < 0
                title: qsTr("选择一个会话")
                description: qsTr("从会话列表选择联系人，或开始一个新会话。")
            }
        }

        MessageComposer {
            id: composer

            Layout.fillWidth: true
            visible: root.controller.selectedConversationIndex >= 0
            onSendRequested: text => root.controller.sendMessage(text)
            onDraftEdited: text => root.controller.draftText = text

            Component.onCompleted: draftText = root.controller.draftText
        }
    }

    Connections {
        target: root.controller

        function onSelectedConversationChanged() {
            composer.draftText = root.controller.draftText
            Qt.callLater(function() {
                timeline.contentY = root.controller.timelinePosition()
            })
        }

        function onDraftTextChanged() {
            if (composer.draftText !== root.controller.draftText)
                composer.draftText = root.controller.draftText
        }
    }

    Drawer {
        id: detailsDrawer

        edge: Qt.RightEdge
        width: Math.min(360, root.width * 0.9)
        height: root.height
        modal: root.compactMode
        dim: root.compactMode

        contentItem: Rectangle {
            color: Theme.surface

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: Tokens.space4
                spacing: Tokens.space4

                RowLayout {
                    Layout.fillWidth: true

                    Label {
                        Layout.fillWidth: true
                        text: qsTr("会话详情")
                        color: Theme.textPrimary
                        font.pixelSize: Typography.title
                        font.bold: true
                    }

                    ToolButton {
                        text: "×"
                        onClicked: detailsDrawer.close()
                        Accessible.name: qsTr("关闭会话详情")
                    }
                }

                Avatar {
                    Layout.alignment: Qt.AlignHCenter
                    name: root.controller.selectedConversationTitle
                    avatarColor: Theme.accent
                    size: Tokens.avatarProfile
                }

                Label {
                    Layout.alignment: Qt.AlignHCenter
                    text: root.controller.selectedConversationTitle
                    color: Theme.textPrimary
                    font.pixelSize: Typography.title
                    font.bold: true
                }

                RowLayout {
                    Layout.fillWidth: true

                    Button {
                        Layout.fillWidth: true
                        text: qsTr("静音")
                        onClicked: root.controller.toggleMuted(
                                       root.controller.selectedConversationIndex)
                    }

                    Button {
                        Layout.fillWidth: true
                        text: qsTr("搜索")
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 1
                    color: Theme.divider
                }

                Label {
                    text: qsTr("共享内容")
                    color: Theme.textPrimary
                    font.pixelSize: Typography.titleSmall
                    font.bold: true
                }

                EmptyState {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    iconText: "▦"
                    title: qsTr("暂无共享内容")
                    description: qsTr("文件、图片和链接将在相应能力接入后显示。")
                }
            }
        }
    }
}
