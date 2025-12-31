import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: contourTabBar
    property var model: terminalSessions
    property int currentIndex: 0

    signal tabSelected(int index)
    signal tabClosed(int index)
    signal newTabRequested()

    height: 44
    color: "#252525"
    
    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 8
        anchors.rightMargin: 8
        anchors.topMargin: 4
        anchors.bottomMargin: 4
        spacing: 6

        ListView {
            id: tabList
            Layout.fillWidth: true
            Layout.fillHeight: true
            orientation: ListView.Horizontal
            boundsBehavior: Flickable.StopAtBounds
            interactive: contentWidth > width
            clip: true
            spacing: 4
            model: tabBar.model
            currentIndex: tabBar.currentIndex

            delegate: Rectangle {
                id: tabItem
                readonly property bool active: model.active
                readonly property bool hovered: mouseArea.containsMouse
                readonly property real availableWidth: tabList.width - (tabList.count - 1) * tabList.spacing
                readonly property real dynamicWidth: tabList.count > 0 ? Math.max(100, Math.min(availableWidth / tabList.count, 300)) : 200

                height: tabList.height
                width: Math.min(Math.max(100, titleText.implicitWidth + 48), dynamicWidth)
                color: active ? "#3a3a3a" : (hovered ? "#323232" : "#2a2a2a")
                border.color: active ? "#0f88a0" : "transparent"
                border.width: active ? 1 : 0
                radius: 6

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 12
                    anchors.rightMargin: 6
                    anchors.topMargin: 2
                    anchors.bottomMargin: 2
                    spacing: 8

                    Text {
                        id: titleText
                        Layout.fillWidth: true
                        text: model.title || `Tab ${index + 1}`
                        color: active ? "#ffffff" : "#b0b0b0"
                        font.family: "Monospace"
                        font.pixelSize: 12
                        font.weight: Font.Normal
                        elide: Text.ElideRight
                        verticalAlignment: Text.AlignVCenter
                    }

                    Rectangle {
                        id: closeButton
                        visible: active || hovered
                        Layout.preferredWidth: 24
                        Layout.preferredHeight: 24
                        color: closeButtonMouse.containsMouse ? "#d73f3f" : "transparent"
                        radius: 3

                        Text {
                            anchors.centerIn: parent
                            text: "×"
                            color: closeButtonMouse.containsMouse ? "#ffffff" : "#808080"
                            font.pixelSize: 14
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }

                        MouseArea {
                            id: closeButtonMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            onClicked: (mouse) => {
                                contourTabBar.tabClosed(index)
                                mouse.accepted = true
                            }
                        }
                    }
                }

                MouseArea {
                    id: mouseArea
                    anchors.fill: parent
                    anchors.rightMargin: closeButton.visible ? closeButton.width + 8 : 0
                    hoverEnabled: true
                    acceptedButtons: Qt.LeftButton | Qt.MiddleButton
                    
                    onClicked: (mouse) => {
                        if (mouse.button === Qt.LeftButton) {
                            contourTabBar.tabSelected(index)
                        } else if (mouse.button === Qt.MiddleButton) {
                            contourTabBar.tabClosed(index)
                        }
                    }
                }
            }
        }

        Button {
            id: addButton
            Layout.preferredWidth: 32
            Layout.preferredHeight: 32
            Layout.alignment: Qt.AlignVCenter
            
            background: Rectangle {
                color: addButton.hovered ? "#333333" : "#2a2a2a"
                border.color: addButton.hovered ? "#444444" : "transparent"
                border.width: 1
                radius: 4
            }
            
            contentItem: Text {
                text: "+"
                color: addButton.hovered ? "#00a8b8" : "#909090"
                font.pixelSize: 16
                font.weight: Font.Bold
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }
            
            onClicked: contourTabBar.newTabRequested()
        }
    }
}
