import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import WimiClient

Rectangle {
    id: root

    property string reasonText: ""
    property string mode: "sign-in"
    property bool busy: false
    property bool networkMode: false
    property string gateUrl: ""
    property string gateConfigurationStatus: "idle"
    property string externalError: ""
    property string errorText: ""
    property string noticeText: ""
    signal authenticated()
    signal authenticationRequested(string username, string password)
    signal verificationCodeRequested(string email)
    signal registrationRequested(string username, string password,
                                 string email, string verificationCode)
    signal passwordResetRequested(string username, string email,
                                  string verificationCode, string newPassword)
    signal gateUrlSaveRequested(string gateUrl)

    color: Theme.scrim

    function completeNetworkOperation(operation, message) {
        noticeText = message
        errorText = ""
        if (operation === "sign-up" || operation === "forget-password") {
            mode = "sign-in"
            passwordField.clear()
            verificationCodeField.clear()
        }
    }

    function submit() {
        errorText = ""
        noticeText = ""
        if (accountField.text.trim().length === 0
                || passwordField.text.length === 0) {
            errorText = qsTr("请完整填写账号和密码。")
            return
        }
        if (root.mode !== "sign-in"
                && (emailField.text.trim().length === 0
                    || verificationCodeField.text.trim().length === 0)) {
            errorText = qsTr("请完整填写邮箱和验证码。")
            return
        }
        if (root.networkMode) {
            if (root.mode === "sign-in") {
                root.authenticationRequested(accountField.text.trim(),
                                             passwordField.text)
            } else if (root.mode === "sign-up") {
                root.registrationRequested(accountField.text.trim(),
                                           passwordField.text,
                                           emailField.text.trim(),
                                           verificationCodeField.text.trim())
            } else {
                root.passwordResetRequested(
                            accountField.text.trim(), emailField.text.trim(),
                            verificationCodeField.text.trim(),
                            passwordField.text)
            }
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
                    text: root.mode === "sign-up" ? qsTr("创建 WIMI 账号")
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
                    id: emailField
                    Layout.fillWidth: true
                    visible: root.mode !== "sign-in"
                    placeholderText: qsTr("邮箱")
                    selectByMouse: true
                    inputMethodHints: Qt.ImhEmailCharactersOnly
                    Accessible.name: qsTr("邮箱")
                }

                TextField {
                    id: passwordField
                    Layout.fillWidth: true
                    placeholderText: root.mode === "reset" ? qsTr("新密码")
                                                           : qsTr("密码")
                    echoMode: TextInput.Password
                    onAccepted: root.submit()
                    Accessible.name: placeholderText
                }

                RowLayout {
                    Layout.fillWidth: true
                    visible: root.mode !== "sign-in"

                    TextField {
                        id: verificationCodeField
                        Layout.fillWidth: true
                        placeholderText: qsTr("验证码")
                        selectByMouse: true
                        Accessible.name: qsTr("验证码")
                    }

                    Button {
                        enabled: !root.busy
                                 && emailField.text.trim().length > 0
                        text: qsTr("获取验证码")
                        onClicked: {
                            root.errorText = ""
                            root.noticeText = ""
                            root.verificationCodeRequested(
                                        emailField.text.trim())
                        }
                    }
                }

                Label {
                    Layout.fillWidth: true
                    visible: root.networkMode
                    text: qsTr("Auth Gate")
                    color: Theme.textPrimary
                    font.pixelSize: Typography.titleSmall
                    font.bold: true
                }

                RowLayout {
                    Layout.fillWidth: true
                    visible: root.networkMode

                    TextField {
                        id: gateUrlField
                        Layout.fillWidth: true
                        text: root.gateUrl
                        placeholderText: qsTr("http://192.168.1.10:18080")
                        inputMethodHints: Qt.ImhUrlCharactersOnly
                        selectByMouse: true
                        Accessible.name: qsTr("Auth Gate 地址")
                    }

                    Button {
                        text: qsTr("保存")
                        onClicked: root.gateUrlSaveRequested(
                                       gateUrlField.text.trim())
                    }
                }

                Label {
                    Layout.fillWidth: true
                    visible: root.networkMode
                             && root.gateConfigurationStatus !== "idle"
                    text: root.gateConfigurationStatus === "invalid"
                          ? qsTr("地址无效，请使用 http:// 或 https:// 地址。")
                          : root.gateConfigurationStatus === "restart-required"
                            ? qsTr("已保存，请完全退出并重新启动客户端。")
                            : qsTr("服务器地址已保存。")
                    color: root.gateConfigurationStatus === "invalid"
                           ? Theme.error : Theme.success
                    font.pixelSize: Typography.bodySmall
                    wrapMode: Text.Wrap
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

                Label {
                    Layout.fillWidth: true
                    visible: root.noticeText.length > 0
                    text: root.noticeText
                    color: Theme.success
                    font.pixelSize: Typography.bodySmall
                    wrapMode: Text.Wrap
                }

                Button {
                    Layout.fillWidth: true
                    enabled: !root.busy
                    text: root.busy ? qsTr("请稍候…")
                          : root.mode === "sign-up" ? qsTr("注册")
                          : root.mode === "reset" ? qsTr("重置密码")
                          : qsTr("登录")
                    onClicked: root.submit()
                }

                RowLayout {
                    Layout.alignment: Qt.AlignHCenter

                    Button {
                        flat: true
                        text: root.mode === "sign-up" ? qsTr("已有账号")
                              : qsTr("创建账号")
                        onClicked: {
                            root.mode = root.mode === "sign-up"
                                      ? "sign-in" : "sign-up"
                            root.errorText = ""
                            root.noticeText = ""
                        }
                    }

                    Button {
                        flat: true
                        visible: root.mode !== "sign-up"
                        text: root.mode === "reset" ? qsTr("返回登录")
                              : qsTr("忘记密码")
                        onClicked: {
                            root.mode = root.mode === "reset"
                                      ? "sign-in" : "reset"
                            root.errorText = ""
                            root.noticeText = ""
                        }
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
