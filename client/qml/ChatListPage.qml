pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQml.Models
import Wim.Client

Rectangle {
    id: root

    required property var controller
    signal conversationOpened()
    property string filterMode: "all"

    color: Theme.surface

    function openConversation(index) {
        if (index < 0)
            return
        root.controller.selectConversation(index)
        root.conversationOpened()
    }

    function sourceIndexOf(item) {
        return item ? item.sourceIndex : -1
    }

    function matchesConversation(conversationId, title, preview, unreadCount) {
        const query = searchField.text.trim().toLocaleLowerCase()
        const matchesQuery = query.length === 0
                || title.toLocaleLowerCase().includes(query)
                || preview.toLocaleLowerCase().includes(query)
        if (!matchesQuery)
            return false

        switch (filterMode) {
        case "unread":
            return unreadCount > 0
        case "groups":
            return conversationId.startsWith("group:")
        default:
            return true
        }
    }

    DelegateModel {
        id: filteredConversations
        model: root.controller.conversations
        filterOnGroup: "visibleConversations"

        groups: DelegateModelGroup {
            name: "visibleConversations"
            includeByDefault: true
        }

        delegate: Item {
            id: conversationDelegate

            required property string conversationId
            required property int sourceIndex
            required property string title
            required property string preview
            required property string timestamp
            required property string avatarColor
            required property int unreadCount
            required property bool pinned
            required property bool muted
            required property bool online

            DelegateModel.groups:
                root.matchesConversation(conversationId, title, preview,
                                         unreadCount)
                ? ["visibleConversations"] : []

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
                selected: conversationDelegate.sourceIndex
                          === root.controller.selectedConversationIndex
                onActivated:
                    root.openConversation(conversationDelegate.sourceIndex)
                onPinRequested:
                    root.controller.togglePinned(conversationDelegate.sourceIndex)
                onMuteRequested:
                    root.controller.toggleMuted(conversationDelegate.sourceIndex)
                onMarkReadRequested:
                    root.controller.markRead(conversationDelegate.sourceIndex)
            }
        }
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
                onClicked: root.controller.currentSection = "contacts"
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

        ButtonGroup {
            id: conversationFilterGroup
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.leftMargin: Tokens.space3
            Layout.rightMargin: Tokens.space3
            Layout.bottomMargin: Tokens.space2
            spacing: Tokens.space1

            Repeater {
                model: [
                    { "label": qsTr("全部"), "mode": "all" },
                    { "label": qsTr("未读"), "mode": "unread" },
                    { "label": qsTr("群聊"), "mode": "groups" }
                ]

                delegate: Button {
                    required property var modelData

                    Layout.fillWidth: true
                    text: modelData.label
                    checkable: true
                    checked: root.filterMode === modelData.mode
                    flat: !checked
                    ButtonGroup.group: conversationFilterGroup
                    Accessible.name: qsTr("筛选：%1").arg(text)
                    onClicked: root.filterMode = modelData.mode
                }
            }
        }

        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true

            ListView {
                id: conversationList
                objectName: "conversationList"
                anchors.fill: parent
                visible: count > 0
                clip: true
                model: filteredConversations
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
                    root.openConversation(root.sourceIndexOf(currentItem))
                    event.accepted = true
                }
                Keys.onEnterPressed: event => {
                    root.openConversation(root.sourceIndexOf(currentItem))
                    event.accepted = true
                }

                ScrollBar.vertical: ScrollBar {}

            }

            EmptyState {
                anchors.centerIn: parent
                visible: conversationList.count === 0
                title: searchField.text.length > 0 || root.filterMode !== "all"
                       ? qsTr("没有匹配的会话") : qsTr("还没有会话")
                description: searchField.text.length > 0
                             ? qsTr("请尝试其他关键词。")
                             : root.filterMode !== "all"
                               ? qsTr("当前筛选条件下没有会话。")
                               : qsTr("添加联系人或创建群聊后，会话会出现在这里。")
                actionText: searchField.text.length > 0
                            || root.filterMode !== "all"
                            ? qsTr("清除筛选") : qsTr("开始新会话")
                onActionRequested: {
                    if (searchField.text.length > 0 || root.filterMode !== "all") {
                        searchField.clear()
                        root.filterMode = "all"
                    } else {
                        root.controller.currentSection = "contacts"
                    }
                }
            }
        }
    }
}
