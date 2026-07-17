pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
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
            color: Theme.surface

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

                TabBar {
                    id: contactTabs
                    Layout.fillWidth: true

                    TabButton {
                        text: qsTr("联系人")
                    }

                    TabButton {
                        text: qsTr("申请")
                    }
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

                StackLayout {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    currentIndex: contactTabs.currentIndex

                    Item {
                        ListView {
                            id: contactList
                            anchors.fill: parent
                            model: root.controller.contacts
                            clip: true
                            boundsBehavior: Flickable.StopAtBounds

                            ScrollBar.vertical: ScrollBar {}

                            delegate: Item {
                                id: contactDelegate

                                required property int index
                                required property string displayName
                                required property string statusText
                                required property string avatarColor
                                required property bool online
                                required property bool favorite

                                width: ListView.view.width
                                height: 68

                                ItemDelegate {
                                    anchors.fill: parent
                                    highlighted: contactDelegate.index
                                                 === root.controller.selectedContactIndex
                                    onClicked: {
                                        root.controller.selectContact(contactDelegate.index)
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

                        EmptyState {
                            anchors.centerIn: parent
                            visible: contactList.count === 0
                            title: qsTr("还没有联系人")
                            description: qsTr("通过用户 ID 搜索并发送好友申请。")
                        }
                    }

                    Item {
                        ListView {
                            id: requestList
                            anchors.fill: parent
                            model: root.controller.requests
                            clip: true
                            spacing: Tokens.space2
                            topMargin: Tokens.space2
                            bottomMargin: Tokens.space2
                            boundsBehavior: Flickable.StopAtBounds

                            ScrollBar.vertical: ScrollBar {}

                            delegate: Item {
                                id: requestDelegate

                                required property int index
                                required property string displayName
                                required property string requestMessage
                                required property string avatarColor
                                required property string requestKind
                                required property string requestStatus

                                width: ListView.view.width
                                height: 112

                                Rectangle {
                                    anchors.fill: parent
                                    anchors.leftMargin: Tokens.space2
                                    anchors.rightMargin: Tokens.space2
                                    radius: Tokens.radiusMedium
                                    color: Theme.surfaceMuted

                                    RowLayout {
                                        anchors.fill: parent
                                        anchors.margins: Tokens.space3
                                        spacing: Tokens.space3

                                        Avatar {
                                            name: requestDelegate.displayName
                                            avatarColor: requestDelegate.avatarColor
                                            size: Tokens.avatarMedium
                                        }

                                        ColumnLayout {
                                            Layout.fillWidth: true
                                            spacing: Tokens.space1

                                            Label {
                                                Layout.fillWidth: true
                                                text: requestDelegate.displayName
                                                color: Theme.textPrimary
                                                font.pixelSize: Typography.titleSmall
                                                font.bold: true
                                            }

                                            Label {
                                                Layout.fillWidth: true
                                                text: (requestDelegate.requestKind === "group"
                                                       ? qsTr("群审批 · ") : "")
                                                      + requestDelegate.requestMessage
                                                color: Theme.textSecondary
                                                font.pixelSize: Typography.bodySmall
                                                wrapMode: Text.Wrap
                                                maximumLineCount: 2
                                                elide: Text.ElideRight
                                            }

                                            RowLayout {
                                                visible: requestDelegate.requestStatus === "pending"

                                                Button {
                                                    text: qsTr("接受")
                                                    onClicked: root.controller.resolveRequest(
                                                                   requestDelegate.index, true)
                                                }

                                                Button {
                                                    text: qsTr("忽略")
                                                    flat: true
                                                    onClicked: root.controller.resolveRequest(
                                                                   requestDelegate.index, false)
                                                }
                                            }

                                            Label {
                                                visible: requestDelegate.requestStatus !== "pending"
                                                text: requestDelegate.requestStatus === "accepted"
                                                      ? qsTr("已接受") : qsTr("已忽略")
                                                color: requestDelegate.requestStatus === "accepted"
                                                       ? Theme.success : Theme.textSecondary
                                                font.pixelSize: Typography.bodySmall
                                            }
                                        }
                                    }
                                }
                            }
                        }

                        EmptyState {
                            anchors.centerIn: parent
                            visible: requestList.count === 0
                            title: qsTr("没有待处理申请")
                        }
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
                        text: qsTr("选择成员")
                        color: Theme.textPrimary
                        font.bold: true
                    }

                    Repeater {
                        model: [qsTr("林晓"), qsTr("周宁"), qsTr("陈屿")]

                        delegate: CheckBox {
                            required property string modelData
                            text: modelData
                        }
                    }

                    Label {
                        text: qsTr("Fake Scenario：确认后不写入服务端。")
                        color: Theme.textSecondary
                        font.pixelSize: Typography.caption
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
                    text: Icons.back + "  " + qsTr("联系人")
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
