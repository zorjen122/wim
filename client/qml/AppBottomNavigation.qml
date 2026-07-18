pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Wim.Client

Rectangle {
    id: root

    required property string currentSection
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

                        AppIcon {
                            anchors.verticalCenter: parent.verticalCenter
                            width: 18
                            height: 18
                            source: navigationButton.modelData.icon
                            color: navigationButton.checked
                                   ? Theme.accent : Theme.textSecondary
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
