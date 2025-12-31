import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: tabBar
    property var model: terminalSessions

    signal tabSelected(int index)
    signal tabClosed(int index)
    signal newTabRequested()

    height: 40
    color: "#0e1218"
    border.color: "#16202c"
    radius: 0

    RowLayout {
        anchors.fill: parent
        anchors.margins: 8
        spacing: 8

        ListView {
            id: tabList
            Layout.fillWidth: true
            Layout.fillHeight: true
            orientation: ListView.Horizontal
            boundsBehavior: Flickable.StopAtBounds
            interactive: contentWidth > width
            clip: true
            spacing: 6
            model: tabBar.model

            delegate: Rectangle {
                id: tabItem
                readonly property bool active: model.active

                height: tabList.height - 8
                width: Math.max(120, titleText.implicitWidth + closeButton.implicitWidth + 32)
                radius: 8
                color: active ? "#142131" : "#0f141c"
                border.color: active ? "#2ea8b8" : "#1c2733"
                layer.enabled: true
                layer.samples: 4

                RowLayout {
                    anchors.fill: parent
                    anchors.margins: 8
                    spacing: 8

                    Text {
                        id: titleText
                        Layout.fillWidth: true
                        text: model.title || `Tab ${index + 1}`
                        color: active ? "#e4ecf5" : "#b8c4d1"
                        font.family: "Source Code Pro"
                        font.weight: Font.Medium
                        elide: Text.ElideRight
                        verticalAlignment: Text.AlignVCenter
                    }

                    ToolButton {
                        id: closeButton
                        text: "×"
                        padding: 4
                        font.pixelSize: 14
                        hoverEnabled: true
                        onClicked: tabBar.tabClosed(index)
                    }
                }

                TapHandler {
                    acceptedButtons: Qt.LeftButton
                    onTapped: tabBar.tabSelected(index)
                    grabPermissions: PointerHandler.TakeOverForbidden
                }

                TapHandler {
                    acceptedButtons: Qt.MiddleButton
                    onTapped: tabBar.tabClosed(index)
                    grabPermissions: PointerHandler.TakeOverForbidden
                }
            }
        }

        Button {
            id: addButton
            text: "+"
            Layout.preferredWidth: 36
            Layout.fillHeight: true
            background: Rectangle {
                radius: 8
                color: addButton.pressed ? "#1b2a33" : "#14323e"
                border.color: "#245b6d"
            }
            contentItem: Text {
                text: addButton.text
                color: "#d9e6f2"
                font.family: "Source Code Pro"
                font.weight: Font.Bold
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }
            onClicked: tabBar.newTabRequested()
        }
    }
}
