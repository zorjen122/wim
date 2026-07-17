import QtQuick
import QtQuick.Controls
import Wim.Client

Rectangle {
    id: root

    required property string status

    readonly property string label: {
        switch (status) {
        case "online": return qsTr("在线")
        case "auth-required": return qsTr("等待登录")
        case "connecting": return qsTr("连接中")
        case "reconnecting": return qsTr("正在重连")
        case "offline-cached": return qsTr("离线 · 本地数据")
        case "syncing": return qsTr("正在同步")
        case "recovering-gap": return qsTr("正在补齐缺口")
        case "auth-expired": return qsTr("登录已过期")
        case "storage-error": return qsTr("本地存储错误")
        default: return status
        }
    }
    readonly property color statusColor: {
        switch (status) {
        case "online": return Theme.success
        case "auth-required": return Theme.warning
        case "connecting":
        case "reconnecting": return Theme.warning
        case "offline-cached": return Theme.warning
        case "syncing":
        case "recovering-gap": return Theme.accent
        case "auth-expired": return Theme.error
        case "storage-error": return Theme.error
        default: return Theme.textSecondary
        }
    }

    implicitWidth: content.implicitWidth + Tokens.space4
    implicitHeight: 26
    radius: Tokens.radiusFull
    color: Qt.alpha(statusColor, Theme.dark ? 0.20 : 0.12)

    Label {
        id: content
        anchors.centerIn: parent
        text: root.label
        color: root.statusColor
        font.pixelSize: Typography.caption
        font.bold: true
    }
}
