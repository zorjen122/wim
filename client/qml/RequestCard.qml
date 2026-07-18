pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Wim.Client

Item {
    id: root

    required property string displayName
    required property string requestMessage
    required property string avatarColor
    required property string requestKind
    required property string requestStatus
    signal resolutionRequested(bool accepted)

    readonly property bool submitting: requestStatus === "accepting"
                                       || requestStatus === "declining"
    implicitHeight: requestStatus === "pending" || submitting ? 132 : 104

    Rectangle {
        anchors.fill: parent
        anchors.leftMargin: Tokens.space3
        anchors.rightMargin: Tokens.space3
        radius: Tokens.radiusMedium
        color: Theme.surface
        border.width: 1
        border.color: Theme.border

        RowLayout {
            anchors.fill: parent
            anchors.margins: Tokens.space3
            spacing: Tokens.space3

            Avatar {
                name: root.displayName
                avatarColor: root.avatarColor
                size: Tokens.avatarLarge
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: Tokens.space1

                RowLayout {
                    Layout.fillWidth: true

                    Label {
                        Layout.fillWidth: true
                        text: root.displayName
                        color: Theme.textPrimary
                        font.pixelSize: Typography.titleSmall
                        font.bold: true
                        elide: Text.ElideRight
                    }

                    Label {
                        text: root.requestKind === "group"
                              ? qsTr("入群申请") : qsTr("好友申请")
                        color: Theme.accent
                        font.pixelSize: Typography.caption
                        font.bold: true
                    }
                }

                Label {
                    Layout.fillWidth: true
                    text: root.requestMessage.length > 0
                          ? root.requestMessage : qsTr("没有附加说明")
                    color: Theme.textSecondary
                    font.pixelSize: Typography.bodySmall
                    wrapMode: Text.Wrap
                    maximumLineCount: 2
                    elide: Text.ElideRight
                }

                RowLayout {
                    visible: root.requestStatus === "pending"
                    spacing: Tokens.space2

                    Button {
                        text: qsTr("接受")
                        onClicked: root.resolutionRequested(true)
                    }

                    Button {
                        text: qsTr("忽略")
                        flat: true
                        onClicked: root.resolutionRequested(false)
                    }
                }

                RowLayout {
                    visible: root.submitting
                    spacing: Tokens.space2

                    BusyIndicator {
                        running: root.submitting
                        implicitWidth: 24
                        implicitHeight: 24
                    }

                    Label {
                        text: root.requestStatus === "accepting"
                              ? qsTr("正在接受…") : qsTr("正在忽略…")
                        color: Theme.textSecondary
                        font.pixelSize: Typography.bodySmall
                    }
                }

                Label {
                    visible: root.requestStatus === "accepted"
                             || root.requestStatus === "declined"
                    text: root.requestStatus === "accepted"
                          ? qsTr("已接受") : qsTr("已忽略")
                    color: root.requestStatus === "accepted"
                           ? Theme.success : Theme.textSecondary
                    font.pixelSize: Typography.bodySmall
                    font.bold: true
                }
            }
        }
    }
}
