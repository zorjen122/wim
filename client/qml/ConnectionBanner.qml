import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import WimiClient

Rectangle {
    id: root

    required property string status
    readonly property bool actionableStatus:
        status !== "online" && status !== "auth-required"
        && status !== "auth-expired"
    readonly property bool warningStatus:
        status === "offline-cached" || status === "reconnecting"
    readonly property string statusText: {
        switch (status) {
        case "connecting": return qsTr("正在连接服务…")
        case "reconnecting": return qsTr("连接已中断，正在恢复…")
        case "offline-cached": return qsTr("当前离线，可继续浏览和发送")
        case "syncing": return qsTr("正在同步最新内容…")
        case "recovering-gap": return qsTr("正在补齐缺失消息…")
        default: return qsTr("正在恢复客户端状态…")
        }
    }

    visible: actionableStatus
    implicitHeight: visible ? 32 : 0
    color: warningStatus ? Theme.warning : Theme.accentContainer

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: Tokens.space4
        anchors.rightMargin: Tokens.space4
        spacing: Tokens.space2

        BusyIndicator {
            visible: root.status === "connecting"
                     || root.status === "reconnecting"
                     || root.status === "syncing"
                     || root.status === "recovering-gap"
            Layout.preferredWidth: 18
            Layout.preferredHeight: 18
            running: visible
        }

        Label {
            Layout.fillWidth: true
            text: root.statusText
            color: root.warningStatus ? Theme.canvas : Theme.textPrimary
            font.pixelSize: Typography.bodySmall
            horizontalAlignment: Text.AlignHCenter
            elide: Text.ElideRight
            Accessible.name: text
        }
    }
}
