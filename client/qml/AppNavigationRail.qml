pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Wim.Client

Rectangle {
    id: root

    required property string currentSection
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
                {"section": "settings", "label": qsTr("设置"), "icon": Icons.settings}
            ]

            delegate: ToolButton {
                id: navigationButton

                required property var modelData
                required property int index
                Layout.alignment: Qt.AlignHCenter
                Layout.preferredWidth: 52
                Layout.preferredHeight: 54
                text: modelData.icon + "\n" + modelData.label
                font.pixelSize: index === 0 ? Typography.bodySmall : Typography.caption
                checked: root.currentSection === modelData.section
                checkable: true
                onClicked: root.sectionRequested(modelData.section)
                Accessible.name: modelData.label

                contentItem: Label {
                    text: navigationButton.text
                    color: navigationButton.checked ? Theme.accent : Theme.textSecondary
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                    font: navigationButton.font
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
