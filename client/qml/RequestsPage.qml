pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import WimiClient

Rectangle {
    id: root

    required property var controller
    property bool compactMode: false
    readonly property bool showingHistory: requestTabs.currentIndex === 1
    readonly property int visibleCount: showingHistory
                                        ? controller.requests.resolvedCount
                                        : controller.requests.pendingCount

    color: Theme.canvas

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 72
            color: Theme.surface

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: root.compactMode ? Tokens.space4
                                                     : Tokens.space6
                anchors.rightMargin: root.compactMode ? Tokens.space4
                                                      : Tokens.space6

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 0

                    Label {
                        text: qsTr("申请")
                        color: Theme.textPrimary
                        font.pixelSize: Typography.headline
                        font.bold: true
                    }

                    Label {
                        text: root.controller.requests.pendingCount > 0
                              ? qsTr("%1 项等待处理")
                                    .arg(root.controller.requests.pendingCount)
                              : qsTr("当前没有待处理事项")
                        color: Theme.textSecondary
                        font.pixelSize: Typography.bodySmall
                    }
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

        TabBar {
            id: requestTabs

            Layout.fillWidth: true

            TabButton {
                text: root.controller.requests.pendingCount > 0
                      ? qsTr("待处理 (%1)")
                            .arg(root.controller.requests.pendingCount)
                      : qsTr("待处理")
            }

            TabButton {
                text: qsTr("历史")
            }
        }

        Label {
            Layout.fillWidth: true
            Layout.leftMargin: Tokens.space4
            Layout.rightMargin: Tokens.space4
            Layout.topMargin: visible ? Tokens.space2 : 0
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
                id: requestList
                objectName: "requestList"

                anchors.fill: parent
                model: root.controller.requests
                clip: true
                spacing: Tokens.space2
                topMargin: Tokens.space3
                bottomMargin: Tokens.space4
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
                    readonly property bool matchesTab:
                        root.showingHistory
                        ? requestStatus === "accepted"
                          || requestStatus === "declined"
                        : requestStatus !== "accepted"
                          && requestStatus !== "declined"

                    width: ListView.view.width
                    height: matchesTab ? requestCard.implicitHeight : 0
                    visible: matchesTab

                    RequestCard {
                        id: requestCard

                        width: parent.width
                        displayName: requestDelegate.displayName
                        requestMessage: requestDelegate.requestMessage
                        avatarColor: requestDelegate.avatarColor
                        requestKind: requestDelegate.requestKind
                        requestStatus: requestDelegate.requestStatus
                        onResolutionRequested: accepted =>
                            root.controller.resolveRequest(
                                requestDelegate.index, accepted)
                    }
                }
            }

            EmptyState {
                anchors.centerIn: parent
                visible: root.visibleCount === 0
                title: root.showingHistory ? qsTr("还没有处理记录")
                                           : qsTr("没有待处理申请")
                description: root.showingHistory
                             ? qsTr("接受或忽略的申请会保留在这里。")
                             : qsTr("新的好友申请和入群审批会出现在这里。")
            }
        }
    }
}
