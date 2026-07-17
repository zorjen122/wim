import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Wim.Client

Rectangle {
    id: root

    required property var controller
    property bool compactMode: false

    color: Theme.canvas

    ScrollView {
        anchors.fill: parent
        contentWidth: availableWidth

        ColumnLayout {
            width: Math.min(720, parent.width - Tokens.space6 * 2)
            x: Math.max(Tokens.space6, (parent.width - width) / 2)
            spacing: Tokens.space4

            Item {
                Layout.preferredHeight: Tokens.space3
            }

            Label {
                text: qsTr("设置")
                color: Theme.textPrimary
                font.pixelSize: Typography.headline
                font.bold: true
            }

            GroupBox {
                Layout.fillWidth: true
                title: qsTr("外观")

                ColumnLayout {
                    anchors.fill: parent

                    Switch {
                        text: qsTr("深色主题")
                        checked: Theme.dark
                        onToggled: Theme.dark = checked
                    }

                    Label {
                        Layout.fillWidth: true
                        text: qsTr("星空蓝主题使用语义色，页面不会直接保存颜色。")
                        color: Theme.textSecondary
                        font.pixelSize: Typography.bodySmall
                        wrapMode: Text.Wrap
                    }
                }
            }

            GroupBox {
                Layout.fillWidth: true
                title: qsTr("通知")

                ColumnLayout {
                    anchors.fill: parent

                    Switch {
                        text: qsTr("桌面通知")
                        checked: root.controller.desktopNotificationsAvailable
                        enabled: false
                    }

                    Switch {
                        text: qsTr("消息声音")
                        checked: true
                        enabled: false
                    }

                    Label {
                        Layout.fillWidth: true
                        text: root.controller.desktopNotificationsAvailable
                              ? qsTr("已连接 freedesktop Notifications 服务。")
                              : qsTr("当前桌面会话未提供 freedesktop Notifications 服务。")
                        color: Theme.textSecondary
                        font.pixelSize: Typography.bodySmall
                        wrapMode: Text.Wrap
                    }

                    Button {
                        text: qsTr("发送测试通知")
                        onClicked: root.controller.sendTestDesktopNotification()
                        Accessible.name: qsTr("发送测试桌面通知")
                    }

                    Label {
                        Layout.fillWidth: true
                        visible: root.controller.notificationTestStatus !== "idle"
                        text: root.controller.notificationTestStatus === "sent"
                              ? qsTr("测试通知已交给桌面通知服务。")
                              : qsTr("测试通知不可用，请检查 Linux 桌面会话的通知服务。")
                        color: root.controller.notificationTestStatus === "sent"
                               ? Theme.success : Theme.warning
                        font.pixelSize: Typography.bodySmall
                        wrapMode: Text.Wrap
                    }
                }
            }

            GroupBox {
                Layout.fillWidth: true
                title: qsTr("存储")

                RowLayout {
                    anchors.fill: parent

                    ColumnLayout {
                        Layout.fillWidth: true

                        Label {
                            text: qsTr("本地缓存")
                            color: Theme.textPrimary
                            font.pixelSize: Typography.body
                        }

                        Label {
                            text: qsTr("%1 Repository · 本地优先")
                                  .arg(root.controller.repositoryKind.toUpperCase())
                            color: Theme.textSecondary
                            font.pixelSize: Typography.bodySmall
                        }
                    }

                    Button {
                        text: qsTr("管理")
                        enabled: false
                    }
                }
            }

            GroupBox {
                Layout.fillWidth: true
                title: qsTr("关于")

                ColumnLayout {
                    anchors.fill: parent

                    Label {
                        text: qsTr("WIM Client 0.1.0")
                        color: Theme.textPrimary
                        font.pixelSize: Typography.titleSmall
                        font.bold: true
                    }

                    Label {
                        Layout.fillWidth: true
                        text: qsTr("Qt 6 + QML + C++20 · 个人开源友好项目")
                        color: Theme.textSecondary
                        font.pixelSize: Typography.bodySmall
                        wrapMode: Text.Wrap
                    }
                }
            }

            Item {
                Layout.preferredHeight: Tokens.space6
            }
        }
    }
}
