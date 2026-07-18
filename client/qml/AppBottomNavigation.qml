pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Wim.Client

Rectangle {
    id: root

    required property string currentSection
    required property int pendingRequestCount
    signal sectionRequested(string section)

    implicitHeight: 64
    color: Theme.surface

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: Tokens.space3
        anchors.rightMargin: Tokens.space3
        spacing: Tokens.space2

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
                Layout.fillWidth: true
                Layout.fillHeight: true
                text: modelData.label
                checked: root.currentSection === modelData.section
                checkable: true
                onClicked: root.sectionRequested(modelData.section)
                Accessible.name: modelData.label

                contentItem: Item {
                    Row {
                        anchors.centerIn: parent
                        spacing: Tokens.space2

                        Item {
                            anchors.verticalCenter: parent.verticalCenter
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
                                implicitWidth: Math.max(16, bottomRequestCount.implicitWidth
                                                       + Tokens.space1 * 2)
                                implicitHeight: 16
                                radius: Tokens.radiusFull
                                color: Theme.error

                                Label {
                                    id: bottomRequestCount
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
                            anchors.verticalCenter: parent.verticalCenter
                            text: navigationButton.text
                            color: navigationButton.checked
                                   ? Theme.accent : Theme.textSecondary
                            font.pixelSize: Typography.bodySmall
                            font.bold: navigationButton.checked
                        }
                    }
                }

                background: Rectangle {
                    radius: Tokens.radiusMedium
                    color: navigationButton.checked ? Theme.accentContainer : "transparent"
                }
            }
        }
    }

    Rectangle {
        anchors.top: parent.top
        width: parent.width
        height: 1
        color: Theme.divider
    }
}
