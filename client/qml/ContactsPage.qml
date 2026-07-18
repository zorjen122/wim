pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQml.Models
import Wim.Client

Item {
    id: root

    required property var controller
    property bool compactMode: false
    property bool profileVisible: false

    focus: true
    Keys.onReleased: event => {
        if ((event.key === Qt.Key_Back || event.key === Qt.Key_Escape)
                && root.compactMode && root.profileVisible) {
            root.profileVisible = false
            event.accepted = true
        }
    }

    Loader {
        anchors.fill: parent
        sourceComponent: root.compactMode ? compactContacts : desktopContacts
    }

    Component {
        id: desktopContacts

        RowLayout {
            spacing: 0

            Loader {
                Layout.fillHeight: true
                Layout.preferredWidth: Tokens.conversationListWidth
                sourceComponent: contactBrowser
            }

            Rectangle {
                Layout.fillHeight: true
                Layout.preferredWidth: 1
                color: Theme.divider
            }

            Loader {
                Layout.fillWidth: true
                Layout.fillHeight: true
                sourceComponent: profilePanel
            }
        }
    }

    Component {
        id: compactContacts

        Loader {
            sourceComponent: root.profileVisible ? profilePanel : contactBrowser
        }
    }

    Component {
        id: contactBrowser

        Rectangle {
            id: contactBrowserRoot

            color: Theme.surface

            function matchesContact(userId, displayName, statusText) {
                const query = contactSearchField.text.trim().toLocaleLowerCase()
                return query.length === 0
                        || userId.toLocaleLowerCase().includes(query)
                        || displayName.toLocaleLowerCase().includes(query)
                        || statusText.toLocaleLowerCase().includes(query)
            }

            DelegateModel {
                id: filteredContacts
                model: root.controller.contacts
                filterOnGroup: "visibleContacts"

                groups: DelegateModelGroup {
                    name: "visibleContacts"
                    includeByDefault: true
                }

                delegate: Item {
                    id: contactDelegate

                    required property string userId
                    required property int sourceIndex
                    required property string displayName
                    required property string statusText
                    required property string avatarColor
                    required property bool online
                    required property bool favorite

                    DelegateModel.groups:
                        contactBrowserRoot.matchesContact(userId, displayName,
                                                          statusText)
                        ? ["visibleContacts"] : []

                    width: ListView.view.width
                    height: 68

                    ItemDelegate {
                        anchors.fill: parent
                        highlighted: contactDelegate.sourceIndex
                                     === root.controller.selectedContactIndex
                        onClicked: {
                            root.controller.selectContact(
                                contactDelegate.sourceIndex)
                            if (root.compactMode)
                                root.profileVisible = true
                        }

                        contentItem: RowLayout {
                            spacing: Tokens.space3

                            Avatar {
                                name: contactDelegate.displayName
                                avatarColor: contactDelegate.avatarColor
                                online: contactDelegate.online
                                size: Tokens.avatarLarge
                            }

                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: Tokens.space1

                                Label {
                                    Layout.fillWidth: true
                                    text: contactDelegate.displayName
                                    color: Theme.textPrimary
                                    font.pixelSize: Typography.titleSmall
                                    font.bold: true
                                    elide: Text.ElideRight
                                }

                                Label {
                                    Layout.fillWidth: true
                                    text: contactDelegate.statusText
                                    color: contactDelegate.online
                                           ? Theme.success : Theme.textSecondary
                                    font.pixelSize: Typography.bodySmall
                                }
                            }

                            Label {
                                visible: contactDelegate.favorite
                                text: "★"
                                color: Theme.warning
                            }
                        }
                    }
                }
            }

            ColumnLayout {
                anchors.fill: parent
                spacing: 0

                RowLayout {
                    Layout.fillWidth: true
                    Layout.margins: Tokens.space3

                    Label {
                        Layout.fillWidth: true
                        text: qsTr("联系人")
                        color: Theme.textPrimary
                        font.pixelSize: Typography.headline
                        font.bold: true
                    }

                    ToolButton {
                        text: "+人"
                        onClicked: addFriendDialog.open()
                        Accessible.name: qsTr("添加好友")
                    }

                    ToolButton {
                        text: "+"
                        onClicked: createGroupDialog.open()
                        Accessible.name: qsTr("创建群聊")
                    }
                }

                TextField {
                    id: contactSearchField

                    Layout.fillWidth: true
                    Layout.leftMargin: Tokens.space3
                    Layout.rightMargin: Tokens.space3
                    Layout.bottomMargin: Tokens.space2
                    placeholderText: qsTr("搜索联系人或用户 ID")
                    selectByMouse: true
                    Accessible.name: qsTr("搜索联系人")
                }

                Label {
                    Layout.fillWidth: true
                    Layout.leftMargin: Tokens.space3
                    Layout.rightMargin: Tokens.space3
                    Layout.bottomMargin: visible ? Tokens.space2 : 0
                    visible: root.controller.serviceActionStatus.length > 0
                    text: root.controller.serviceActionStatus
                    color: Theme.textSecondary
                    font.pixelSize: Typography.bodySmall
                    wrapMode: Text.Wrap
                }

                Item {
                    Layout.fillWidth: true
                    Layout.fillHeight: true

                    ListView {
                        id: contactList
                        objectName: "contactList"
                        anchors.fill: parent
                        model: filteredContacts
                        clip: true
                        boundsBehavior: Flickable.StopAtBounds

                        ScrollBar.vertical: ScrollBar {}
                    }

                    EmptyState {
                        anchors.centerIn: parent
                        visible: contactList.count === 0
                        title: contactSearchField.text.length > 0
                               ? qsTr("没有匹配的联系人")
                               : qsTr("还没有联系人")
                        description: contactSearchField.text.length > 0
                                     ? qsTr("请尝试姓名、状态或用户 ID。")
                                     : qsTr("通过用户 ID 搜索并发送好友申请。")
                        actionText: contactSearchField.text.length > 0
                                    ? qsTr("清除搜索") : ""
                        onActionRequested: contactSearchField.clear()
                    }
                }
            }

            Dialog {
                id: addFriendDialog

                anchors.centerIn: parent
                width: Math.min(420, parent.width - Tokens.space6 * 2)
                modal: true
                title: qsTr("添加好友")
                standardButtons: Dialog.Ok | Dialog.Cancel
                onAccepted: root.controller.sendFriendRequest(
                                friendUidField.text,
                                friendMessageField.text)

                ColumnLayout {
                    width: parent.width
                    spacing: Tokens.space3

                    TextField {
                        id: friendUidField
                        Layout.fillWidth: true
                        placeholderText: qsTr("用户 ID")
                        inputMethodHints: Qt.ImhDigitsOnly
                        Accessible.name: qsTr("好友用户 ID")
                    }

                    TextField {
                        id: friendMessageField
                        Layout.fillWidth: true
                        placeholderText: qsTr("验证消息（可选）")
                        Accessible.name: qsTr("好友验证消息")
                    }

                    Button {
                        text: qsTr("按群 ID 申请入群")
                        flat: true
                        onClicked: {
                            addFriendDialog.close()
                            joinGroupDialog.open()
                        }
                    }
                }
            }

            Dialog {
                id: joinGroupDialog

                anchors.centerIn: parent
                width: Math.min(420, parent.width - Tokens.space6 * 2)
                modal: true
                title: qsTr("申请加入群聊")
                standardButtons: Dialog.Ok | Dialog.Cancel
                onAccepted: root.controller.joinGroup(
                                groupIdField.text,
                                groupRequestMessageField.text)

                ColumnLayout {
                    width: parent.width
                    spacing: Tokens.space3

                    TextField {
                        id: groupIdField
                        Layout.fillWidth: true
                        placeholderText: qsTr("群 ID")
                        inputMethodHints: Qt.ImhDigitsOnly
                        Accessible.name: qsTr("群 ID")
                    }

                    TextField {
                        id: groupRequestMessageField
                        Layout.fillWidth: true
                        placeholderText: qsTr("申请说明（可选）")
                        Accessible.name: qsTr("入群申请说明")
                    }
                }
            }

            Dialog {
                id: createGroupDialog

                anchors.centerIn: parent
                width: Math.min(420, parent.width - Tokens.space6 * 2)
                modal: true
                title: qsTr("创建群聊")
                standardButtons: Dialog.Ok | Dialog.Cancel
                onAccepted: root.controller.createGroup(groupNameField.text)

                ColumnLayout {
                    width: parent.width
                    spacing: Tokens.space3

                    TextField {
                        id: groupNameField
                        Layout.fillWidth: true
                        placeholderText: qsTr("群聊名称")
                        Accessible.name: qsTr("群聊名称")
                    }

                    Label {
                        visible: !root.controller.networkEnabled
                        text: qsTr("选择成员（演示）")
                        color: Theme.textPrimary
                        font.bold: true
                    }

                    Repeater {
                        model: [qsTr("林晓"), qsTr("周宁"), qsTr("陈屿")]

                        delegate: CheckBox {
                            required property string modelData
                            visible: !root.controller.networkEnabled
                            text: modelData
                        }
                    }

                    Label {
                        Layout.fillWidth: true
                        text: root.controller.networkEnabled
                              ? qsTr("创建后群里暂时只有你；其他用户可通过群 ID 申请加入。")
                              : qsTr("Fake Scenario：确认后不写入服务端。")
                        color: Theme.textSecondary
                        font.pixelSize: Typography.caption
                        wrapMode: Text.Wrap
                    }
                }
            }
        }
    }

    Component {
        id: profilePanel

        Rectangle {
            color: Theme.canvas

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: Tokens.space6
                spacing: Tokens.space4

                ToolButton {
                    visible: root.compactMode
                    text: qsTr("联系人")
                    icon.source: Icons.back
                    icon.color: Theme.textPrimary
                    display: AbstractButton.TextBesideIcon
                    onClicked: root.profileVisible = false
                    Accessible.name: qsTr("返回联系人列表")
                }

                Item {
                    Layout.fillHeight: true
                    Layout.maximumHeight: 80
                }

                Avatar {
                    Layout.alignment: Qt.AlignHCenter
                    name: root.controller.selectedContactName
                    avatarColor: Theme.accent
                    size: Tokens.avatarProfile
                }

                Label {
                    Layout.alignment: Qt.AlignHCenter
                    text: root.controller.selectedContactName
                    color: Theme.textPrimary
                    font.pixelSize: Typography.headline
                    font.bold: true
                }

                Label {
                    Layout.alignment: Qt.AlignHCenter
                    text: root.controller.selectedContactStatus
                    color: Theme.textSecondary
                    font.pixelSize: Typography.body
                }

                RowLayout {
                    Layout.alignment: Qt.AlignHCenter

                    Button {
                        text: qsTr("发消息")
                        onClicked: root.controller.startConversationWithSelectedContact()
                    }

                    Button {
                        text: qsTr("收藏")
                        flat: true
                        onClicked: root.controller.toggleContactFavorite(
                                       root.controller.selectedContactIndex)
                    }
                }

                Rectangle {
                    Layout.alignment: Qt.AlignHCenter
                    Layout.maximumWidth: 520
                    Layout.fillWidth: true
                    Layout.preferredHeight: 1
                    color: Theme.divider
                }

                Label {
                    Layout.alignment: Qt.AlignHCenter
                    Layout.maximumWidth: 520
                    Layout.fillWidth: true
                    text: qsTr("用户资料将在服务端提供稳定 profile 接口后扩展。当前页面仅消费联系人领域模型。")
                    color: Theme.textSecondary
                    font.pixelSize: Typography.body
                    wrapMode: Text.Wrap
                    horizontalAlignment: Text.AlignHCenter
                }

                Item {
                    Layout.fillHeight: true
                }
            }
        }
    }
}
