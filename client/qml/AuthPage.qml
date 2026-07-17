import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Wim.Client

Rectangle {
    id: root

    property string reasonText: ""
    property string mode: "sign-in"
    property bool busy: false
    property bool networkMode: false
    property string externalError: ""
    property string errorText: ""
    signal authenticated()
    signal authenticationRequested(string username, string password)

    color: Theme.scrim

    function submit() {
        errorText = ""
        if (accountField.text.trim().length === 0
                || (mode !== "reset" && passwordField.text.length === 0)) {
            errorText = qsTr("请完整填写账号和密码。")
            return
        }
        if (root.networkMode && root.mode === "sign-in") {
            root.authenticationRequested(accountField.text.trim(),
                                         passwordField.text)
            return
        }
        root.busy = true
        authTimer.restart()
    }

    Timer {
        id: authTimer
        interval: 650
        onTriggered: {
            root.busy = false
            root.authenticated()
        }
    }

    Rectangle {
        anchors.centerIn: parent
        width: Math.min(440, parent.width - Tokens.space6 * 2)
        height: Math.min(620, parent.height - Tokens.space6 * 2)
        radius: Tokens.radiusLarge
        color: Theme.surface
        border.width: 1
        border.color: Theme.border

        Flickable {
            anchors.fill: parent
            anchors.margins: Tokens.space6
            contentHeight: form.implicitHeight
            clip: true

            ColumnLayout {
                id: form
                width: parent.width
                spacing: Tokens.space4

                Rectangle {
                    Layout.alignment: Qt.AlignHCenter
                    Layout.preferredWidth: 56
                    Layout.preferredHeight: 56
                    radius: Tokens.radiusLarge
                    color: Theme.accent

                    Label {
                        anchors.centerIn: parent
                        text: "W"
                        color: Theme.dark ? Theme.canvas : Theme.surface
                        font.pixelSize: Typography.headline
                        font.bold: true
                    }
                }

                Label {
                    Layout.fillWidth: true
                    text: root.mode === "sign-up" ? qsTr("创建 WIM 账号")
                          : root.mode === "reset" ? qsTr("恢复密码")
                          : qsTr("重新登录")
                    color: Theme.textPrimary
                    font.pixelSize: Typography.headline
                    font.bold: true
                    horizontalAlignment: Text.AlignHCenter
                }

                Label {
                    Layout.fillWidth: true
                    visible: root.reasonText.length > 0
                    text: root.reasonText
                    color: Theme.textSecondary
                    font.pixelSize: Typography.bodySmall
                    wrapMode: Text.Wrap
                    horizontalAlignment: Text.AlignHCenter
                }

                TextField {
                    id: accountField
                    Layout.fillWidth: true
                    placeholderText: qsTr("账号或邮箱")
                    selectByMouse: true
                    Accessible.name: qsTr("账号或邮箱")
                }

                TextField {
                    id: displayNameField
                    Layout.fillWidth: true
                    visible: root.mode === "sign-up"
                    placeholderText: qsTr("显示名称")
                    selectByMouse: true
                    Accessible.name: qsTr("显示名称")
                }

                TextField {
                    id: passwordField
                    Layout.fillWidth: true
                    visible: root.mode !== "reset"
                    placeholderText: qsTr("密码")
                    echoMode: TextInput.Password
                    onAccepted: root.submit()
                    Accessible.name: qsTr("密码")
                }

                Label {
                    Layout.fillWidth: true
                    visible: root.errorText.length > 0
                             || root.externalError.length > 0
                    text: root.externalError.length > 0
                          ? root.externalError : root.errorText
                    color: Theme.error
                    font.pixelSize: Typography.bodySmall
                    wrapMode: Text.Wrap
                }

                Button {
                    Layout.fillWidth: true
                    enabled: !root.busy
                    text: root.busy ? qsTr("请稍候…")
                          : root.mode === "sign-up" ? qsTr("注册")
                          : root.mode === "reset" ? qsTr("发送恢复邮件")
                          : qsTr("登录")
                    onClicked: root.submit()
                }

                RowLayout {
                    Layout.alignment: Qt.AlignHCenter

                    Button {
                        flat: true
                        text: root.mode === "sign-up" ? qsTr("已有账号")
                              : qsTr("创建账号")
                        onClicked: root.mode = root.mode === "sign-up"
                                   ? "sign-in" : "sign-up"
                    }

                    Button {
                        flat: true
                        visible: root.mode !== "sign-up"
                        text: root.mode === "reset" ? qsTr("返回登录")
                              : qsTr("忘记密码")
                        onClicked: root.mode = root.mode === "reset"
                                   ? "sign-in" : "reset"
                    }
                }

                Label {
                    Layout.fillWidth: true
                    text: root.networkMode
                          ? qsTr("登录会先请求 Auth Gate，再连接其返回的 Connection Gateway。")
                          : qsTr("Fake 认证：输入任意非空内容即可关闭预览。")
                    color: Theme.textSecondary
                    font.pixelSize: Typography.caption
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.Wrap
                }
            }
        }
    }
}
