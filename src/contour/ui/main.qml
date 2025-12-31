// vim:syntax=qml
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window
import Qt.labs.platform as Platform
import "."

ApplicationWindow
{
    id: appWindow
    visible: true
    flags: Qt.FramelessWindowHint | Qt.Window
    color: "transparent"

    title: vtui.session ? vtui.session.title : "Contour Terminal"

    width: 1000
    height: 700
    minimumWidth: 400
    minimumHeight: 300

    // Initialise the window size from the terminal's implicit (configured)
    // size once on creation. Do not use a binding (`width: vtui.implicitWidth`)
    // because that would re-evaluate every time implicitWidth changes — e.g.
    // when updateImplicitSize() runs after a screen/DPR change — and snap the
    // window back to the implicit size, overriding any WM-assigned geometry.
    // Tiling WMs that resize the window to a tile, and users who resize
    // manually, expect the window to keep its assigned size.
    Component.onCompleted: {
        width = vtui.implicitWidth
        height = vtui.implicitHeight
    }

    Rectangle {
        id: mainWindow
        anchors.fill: parent
        color: "#1a1a1a"
        radius: 0
        
        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 0
            spacing: 0

            // Custom Title Bar
            Rectangle {
                id: titleBar
                Layout.fillWidth: true
                Layout.preferredHeight: 44
                color: "#252525"
                z: 1000

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 12
                    anchors.rightMargin: 8
                    spacing: 0

                    TabBar {
                        id: tabBar
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        model: terminalSessions
                        onTabSelected: terminalSessions.switchToTabAt(index)
                        onTabClosed: terminalSessions.closeTabAt(index)
                        onNewTabRequested: terminalSessions.addSession()
                    }

                    // Window Control Buttons
                    RowLayout {
                        Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
                        spacing: 2

                        Button {
                            id: minimizeBtn
                            Layout.preferredWidth: 36
                            Layout.preferredHeight: 32
                            
                            background: Rectangle {
                                color: minimizeBtn.hovered ? "#3a3a3a" : "transparent"
                                radius: 4
                            }
                            
                            contentItem: Text {
                                text: "−"
                                color: minimizeBtn.hovered ? "#ffffff" : "#c0c0c0"
                                font.pixelSize: 16
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                            }
                            
                            onClicked: appWindow.showMinimized()
                        }

                        Button {
                            id: maximizeBtn
                            Layout.preferredWidth: 36
                            Layout.preferredHeight: 32
                            
                            background: Rectangle {
                                color: maximizeBtn.hovered ? "#3a3a3a" : "transparent"
                                radius: 4
                            }
                            
                            contentItem: Text {
                                text: appWindow.visibility === Window.Maximized ? "⬜" : "☐"
                                color: maximizeBtn.hovered ? "#ffffff" : "#c0c0c0"
                                font.pixelSize: 13
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                            }
                            
                            onClicked: {
                                if (appWindow.visibility === Window.Maximized) {
                                    appWindow.showNormal()
                                } else {
                                    appWindow.showMaximized()
                                }
                            }
                        }

                        Button {
                            id: closeBtn
                            Layout.preferredWidth: 36
                            Layout.preferredHeight: 32
                            
                            background: Rectangle {
                                color: closeBtn.hovered ? "#d73f3f" : "transparent"
                                radius: 4
                            }
                            
                            contentItem: Text {
                                text: "✕"
                                color: closeBtn.hovered ? "#ffffff" : "#c0c0c0"
                                font.pixelSize: 14
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                            }
                            
                            onClicked: appWindow.close()
                        }
                    }
                }

                // Title bar drag handler
                DragHandler {
                    target: null
                    onActiveChanged: {
                        if (active && appWindow.visibility !== Window.Maximized) {
                            appWindow.startSystemMove()
                        }
                    }
                }
            }

            Terminal {
                id: vtui
                focus: true
                visible: true
                Layout.fillWidth: true
                Layout.fillHeight: true
                onShowNotification: (title, content) => appWindow.showNotification(title, content)
                onOpacityChanged: appWindow.applyOpacity()
            }
        }
    }

    // Context menu area
    MouseArea {
        id: contextMouseArea
        anchors.fill: parent
        anchors.topMargin: 44
        acceptedButtons: Qt.RightButton
        propagateComposedEvents: true
        
        onClicked: (mouse) => {
            if (mouse.button === Qt.RightButton) {
                contextMenu.popup()
            }
        }
    }

    Platform.Menu {
        id: contextMenu
        Platform.MenuItem {
            text: qsTr("Copy")
            //enabled: vtui.session && vtui.session.hasSelection()
            onTriggered: vtui.session.copySelectionToClipboard()
        }
        Platform.MenuItem {
            text: qsTr("Paste")
            onTriggered: vtui.session.pasteFromClipboardStrip(false)
        }
        Platform.MenuItem {
            text: qsTr("Open in File Manager")
            onTriggered: vtui.session.openFileManager()
        }
    }

    onClosing: {
        console.log("Terminal closed. Removing session.");
        terminalSessions.closeWindow();
    }

    onWidthChanged: function() {
        vtui.updateSizeWidget()
    }

    onHeightChanged: function() {
        vtui.updateSizeWidget()
    }

    function applyOpacity() {
        vtui.opacity = vtui.session.opacity;
    }

    function showNotification(title, content) {
        // "OSC 777 ; notify ; <TITLE> ; <CONTENT> ST"
        // Example: printf "\033]777;notify;Hello Title;Hello Content\033\\"
        console.log("Notification [%1]: %2".arg(title).arg(content))
        if (trayIcon.supportsMessages) {
            trayIcon.showMessage("Contour: %1".arg(title), content, 5000)
        }
    }

    // NB: This requires Qt 5.12+
    // See https://doc.qt.io/qt-5/qml-qt-labs-platform-systemtrayicon.html#availability for details.
    Platform.SystemTrayIcon {
        id: trayIcon
        visible: false
        icon.source: "qrc:/contour/logo-256.png"
        icon.name: "Contour Terminal"

        menu: Platform.Menu {
            Platform.MenuItem {
                text: qsTr("Quit")
                onTriggered: Qt.quit()
            }
        }

        onMessageClicked: hide()
    }
}
