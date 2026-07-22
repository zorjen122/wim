pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import WimiClient

Rectangle {
    id: root

    required property string currentSection
    required property int pendingRequestCount
    signal sectionRequested(string section)

    color: Theme.dark ? Theme.canvas : Theme.surfaceMuted

    ColumnLayout {
        anchors.fill: parent
        anchors.topMargin: Tokens.space3
        anchors.bottomMargin: Tokens.space3
        spacing: Tokens.space2

        Rectangle {
            Layout.alignment: Qt.AlignHCenter
            Layout.preferredWidth: 42
            Layout.preferredHeight: 42
            radius: Tokens.radiusMedium
            color: Theme.accent

            Label {
                anchors.centerIn: parent
                text: "W"
                color: Theme.dark ? Theme.canvas : Theme.surface
                font.bold: true
                font.pixelSize: Typography.title
            }
        }

        Item {
            Layout.preferredHeight: Tokens.space3
        }

        Repeater {
            model: [
                {"section": "chats", "label": qsTr("会话"), "icon": Icons.chats},
                {"section": "contacts", "label": qsTr("联系人"), "icon": Icons.contacts},
                {"section": "requests", "label": qsTr("申请"), "icon": Icons.requests},
                {"section": "settings", "label": qsTr("设置"), "icon": Icons.settings}
            ]

            delegate: ToolButton {
                id: navigationButton

                required property var modelData
                required property int index
                Layout.alignment: Qt.AlignHCenter
                Layout.preferredWidth: 52
                Layout.preferredHeight: 54
                text: modelData.label
                font.pixelSize: index === 0 ? Typography.bodySmall : Typography.caption
                checked: root.currentSection === modelData.section
                checkable: true
                onClicked: root.sectionRequested(modelData.section)
                Accessible.name: modelData.label

                contentItem: Item {
                    Column {
                        anchors.centerIn: parent
                        spacing: Tokens.space1

                        Item {
                            anchors.horizontalCenter: parent.horizontalCenter
                            width: 18
                            height: 18

                            AppIcon {
                                anchors.fill: parent
                                source: navigationButton.modelData.icon
                                color: navigationButton.checked
                                       ? Theme.accent : Theme.textSecondary
                            }

                            Rectangle {
                                visible: navigationButton.modelData.section === "requests"
                                         && root.pendingRequestCount > 0
                                anchors.left: parent.horizontalCenter
                                anchors.leftMargin: Tokens.space1
                                anchors.top: parent.top
                                anchors.topMargin: -Tokens.space1
                                implicitWidth: Math.max(16, railRequestCount.implicitWidth
                                                       + Tokens.space1 * 2)
                                implicitHeight: 16
                                radius: Tokens.radiusFull
                                color: Theme.error

                                Label {
                                    id: railRequestCount
                                    anchors.centerIn: parent
                                    text: root.pendingRequestCount > 99
                                          ? "99+" : root.pendingRequestCount
                                    color: "white"
                                    font.pixelSize: 9
                                    font.bold: true
                                }
                            }
                        }

                        Label {
                            anchors.horizontalCenter: parent.horizontalCenter
                            text: navigationButton.text
                            color: navigationButton.checked
                                   ? Theme.accent : Theme.textSecondary
                            font: navigationButton.font
                        }
                    }
                }

                background: Rectangle {
                    radius: Tokens.radiusMedium
                    color: navigationButton.checked ? Theme.accentContainer
                                                    : navigationButton.hovered
                                                      ? Theme.surface : "transparent"
                }
            }
        }

        Item {
            Layout.fillHeight: true
        }

        Avatar {
            Layout.alignment: Qt.AlignHCenter
            name: qsTr("我")
            avatarColor: Theme.accent
            size: Tokens.avatarMedium
        }
    }

    Rectangle {
        anchors.right: parent.right
        width: 1
        height: parent.height
        color: Theme.divider
    }
}
