pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import Wim.Client

Item {
    id: root

    required property var controller
    readonly property bool compact: width < Tokens.compactBreakpoint
                                    || height < 520
    property bool compactConversationVisible:
        controller.compactConversationRequested

    focus: true
    Keys.onReleased: event => {
        if ((event.key === Qt.Key_Back || event.key === Qt.Key_Escape)
                && root.compact && root.compactConversationVisible) {
            root.compactConversationVisible = false
            event.accepted = true
        }
    }

    Connections {
        target: root.controller

        function onCurrentSectionChanged() {
            root.compactConversationVisible = false
        }
    }

    Loader {
        anchors.fill: parent
        sourceComponent: root.compact ? compactShell : desktopShell
    }

    Component {
        id: desktopShell

        RowLayout {
            spacing: 0

            AppNavigationRail {
                Layout.fillHeight: true
                Layout.preferredWidth: Tokens.navigationRailWidth
                currentSection: root.controller.currentSection
                onSectionRequested: section =>
                    root.controller.currentSection = section
            }

            StackLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                currentIndex: root.controller.currentSection === "chats" ? 0
                              : root.controller.currentSection === "contacts" ? 1 : 2

                RowLayout {
                    spacing: 0

                    ChatListPage {
                        Layout.fillHeight: true
                        Layout.preferredWidth:
                            root.width >= Tokens.expandedBreakpoint
                            ? Tokens.conversationListWidth : 304
                        controller: root.controller
                    }

                    Rectangle {
                        Layout.preferredWidth: 1
                        Layout.fillHeight: true
                        color: Theme.divider
                    }

                    ConversationPage {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        controller: root.controller
                    }
                }

                ContactsPage {
                    controller: root.controller
                    compactMode: false
                }

                SettingsPage {
                    controller: root.controller
                    compactMode: false
                }
            }
        }
    }

    Component {
        id: compactShell

        ColumnLayout {
            spacing: 0

            Loader {
                Layout.fillWidth: true
                Layout.fillHeight: true
                sourceComponent: {
                    if (root.controller.currentSection === "contacts")
                        return compactContacts
                    if (root.controller.currentSection === "settings")
                        return compactSettings
                    return root.compactConversationVisible
                           ? compactConversation : compactList
                }
            }

            AppBottomNavigation {
                Layout.fillWidth: true
                visible: !root.compactConversationVisible
                currentSection: root.controller.currentSection
                onSectionRequested: section =>
                    root.controller.currentSection = section
            }
        }
    }

    Component {
        id: compactList

        ChatListPage {
            controller: root.controller
            onConversationOpened: root.compactConversationVisible = true
        }
    }

    Component {
        id: compactConversation

        ConversationPage {
            controller: root.controller
            compactMode: true
            onBackRequested: root.compactConversationVisible = false
        }
    }

    Component {
        id: compactContacts

        ContactsPage {
            controller: root.controller
            compactMode: true
        }
    }

    Component {
        id: compactSettings

        SettingsPage {
            controller: root.controller
            compactMode: true
        }
    }
}
