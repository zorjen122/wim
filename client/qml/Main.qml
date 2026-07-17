import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Wim.Client

ApplicationWindow {
    id: window

    width: app.requestedWindowWidth
    height: app.requestedWindowHeight
    minimumWidth: 360
    minimumHeight: 360
    visible: true
    title: qsTr("WIM")
    color: Theme.canvas

    palette.window: Theme.canvas
    palette.windowText: Theme.textPrimary
    palette.base: Theme.surface
    palette.text: Theme.textPrimary
    palette.button: Theme.surfaceMuted
    palette.buttonText: Theme.textPrimary
    palette.highlight: Theme.accent
    palette.highlightedText: Theme.dark ? Theme.canvas : Theme.surface

    AppController {
        id: app
    }

    Component.onCompleted: Theme.dark = app.darkThemeRequested

    ColumnLayout {
        anchors.fill: parent
        anchors.topMargin: SafeArea.margins.top
        anchors.leftMargin: SafeArea.margins.left
        anchors.rightMargin: SafeArea.margins.right
        anchors.bottomMargin: SafeArea.margins.bottom
        spacing: 0

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 48
            color: Theme.surface

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: Tokens.space3
                anchors.rightMargin: Tokens.space3
                spacing: Tokens.space3

                Label {
                    text: qsTr("UI 场景")
                    color: Theme.textSecondary
                    font.pixelSize: Typography.bodySmall
                }

                ComboBox {
                    id: scenarioPicker
                    Layout.preferredWidth: Math.min(210, window.width * 0.38)
                    model: app.scenarios
                    currentIndex: app.scenarios.indexOf(app.scenarioName)
                    onActivated: app.scenarioName = currentText
                    Accessible.name: qsTr("假数据场景")
                }

                StatusBadge {
                    status: app.connectionStatus
                }

                Label {
                    visible: window.width >= 740
                    text: app.repositoryKind.toUpperCase()
                    color: Theme.textSecondary
                    font.pixelSize: Typography.caption
                }

                Item {
                    Layout.fillWidth: true
                }

                Label {
                    visible: window.width >= 620
                    text: window.width < Tokens.compactBreakpoint
                          ? qsTr("Compact")
                          : window.width < Tokens.expandedBreakpoint
                            ? qsTr("Medium") : qsTr("Expanded")
                    color: Theme.textSecondary
                    font.pixelSize: Typography.caption
                }

                Switch {
                    checked: Theme.dark
                    text: window.width >= 820 ? qsTr("深色") : ""
                    icon.name: Icons.moon
                    onToggled: Theme.dark = checked
                    Accessible.name: qsTr("切换深色主题")
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

        AdaptiveShell {
            Layout.fillWidth: true
            Layout.fillHeight: true
            controller: app
        }
    }

    AuthPage {
        anchors.fill: parent
        z: 100
        visible: app.authRequired
        networkMode: app.networkEnabled
        busy: app.authenticationBusy
        externalError: app.authenticationError
        reasonText: app.networkEnabled
                    ? qsTr("本地会话已经恢复。登录后将连接 Connection Gateway 并增量同步。")
                    : qsTr("登录状态已过期。你的本地会话仍然保留，重新登录后继续同步。")
        onAuthenticationRequested: (username, password) =>
            app.authenticate(username, password)
        onAuthenticated: app.completeAuthentication()
    }
}
