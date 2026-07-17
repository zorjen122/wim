import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Wim.Client

Item {
    id: root

    required property string messageText
    required property string timeText
    required property bool outgoing
    required property string deliveryState

    readonly property string stateText: {
        switch (deliveryState) {
        case "pending": return qsTr("保存中")
        case "waiting-accept": return qsTr("发送中")
        case "accepted": return "✓"
        case "delivered": return "✓✓"
        case "read": return "✓✓"
        case "unknown": return qsTr("待确认")
        case "retryable-failed": return qsTr("可重试")
        case "permanent-failed": return qsTr("发送失败")
        default: return ""
        }
    }
    readonly property color stateColor: {
        switch (deliveryState) {
        case "read": return Theme.accent
        case "unknown":
        case "retryable-failed": return Theme.warning
        case "permanent-failed": return Theme.error
        default: return Theme.textSecondary
        }
    }

    implicitHeight: bubble.implicitHeight + Tokens.space1

    Rectangle {
        id: bubble

        anchors.right: root.outgoing ? parent.right : undefined
        anchors.left: root.outgoing ? undefined : parent.left
        width: Math.min(parent.width * 0.76, 540)
        implicitHeight: messageColumn.implicitHeight + Tokens.space3 * 2
        radius: Tokens.radiusLarge
        color: root.outgoing ? Theme.messageOutgoing : Theme.messageIncoming
        border.width: root.outgoing ? 0 : 1
        border.color: Theme.border

        ColumnLayout {
            id: messageColumn
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.margins: Tokens.space3
            spacing: Tokens.space1

            Label {
                Layout.fillWidth: true
                text: root.messageText
                color: Theme.textPrimary
                font.pixelSize: Typography.body
                wrapMode: Text.WrapAnywhere
                textFormat: Text.PlainText
            }

            RowLayout {
                Layout.alignment: Qt.AlignRight
                spacing: Tokens.space1

                Label {
                    text: root.timeText
                    color: Theme.textSecondary
                    font.pixelSize: Typography.caption
                }

                Label {
                    visible: root.outgoing
                    text: root.stateText
                    color: root.stateColor
                    font.pixelSize: Typography.caption
                    font.bold: root.deliveryState === "read"
                }
            }
        }
    }
}

