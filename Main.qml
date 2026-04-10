import QtQuick
import QtQuick.Controls

Window {
    id: root
    width: Screen.width
    height: Screen.height
    visible: true
    color: "white"

    Item {
        id: mainContainer
        width: root.height 
        height: root.width
        
        // Rotate around the top-left and then translate it back into view
        rotation: 90
        anchors.centerIn: parent

        Column {
            anchors.fill: parent
            anchors.margins: 20
            spacing: 10
            // START K3S OVERLAY
            Rectangle {
                width: parent.width
                height: 200
                color: "#EEEEEE"
                border.color: "black"
                border.width: 3
                visible: !kubectl.k3sRunning  // ONLY SHOW IF K3S IS DOWN
                
                Column {
                    anchors.centerIn: parent
                    spacing: 10
                    Text { 
                        text: "K3s is not reachable!"; 
                        font.pixelSize: 30; font.bold: true; color: "red" 
                    }
                    Rectangle {
                        width: 250; height: 70; color: "black"
                        Text { text: "START K3S"; color: "white"; anchors.centerIn: parent }
                        TapHandler {
                            onTapped: kubectl.startK3s()
                        }
                    }
                }
            }
            // Header Section
            Rectangle {
                width: parent.width; height: 60
                color: "black"
                Text {
                    text: " K9s-Paper: Pods (All Namespaces)"
                    color: "white"
                    font.pixelSize: 24
                    font.bold: true
                    anchors.verticalCenter: parent.verticalCenter
                }
                Text {
                    text: "Refresh [R]" 
                    color: "white"
                    anchors.right: parent.right
                    anchors.rightMargin: 10
                    anchors.verticalCenter: parent.verticalCenter
                }
                MouseArea { anchors.fill: parent; onClicked: kubectl.refresh() }
            }

            // Table Header
            Row {
                width: parent.width
                spacing: 0
                Repeater {
                    model: ["NAMESPACE", "NAME", "READY", "STATUS"]
                    delegate: Rectangle {
                        width: index === 1 ? parent.width * 0.4 : parent.width * 0.2
                        height: 30
                        border.color: "black"
                        Text { 
                            text: modelData; font.bold: true; anchors.centerIn: parent 
                            font.pixelSize: 18
                        }
                    }
                }
            }

            // Pod List
            ListView {
                visible: kubectl.k3sRunning
                width: parent.width
                height: parent.height - 120
                model: kubectl.podList
                clip: true
                delegate: Item {
                    width: parent.width
                    height: 40
                    Row {
                        width: parent.width
                        Rectangle { width: parent.width * 0.2; height: 40; border.color: "black"
                            Text { text: modelData.namespace; anchors.centerIn: parent; elide: Text.ElideRight; width: parent.width - 10 }
                        }
                        Rectangle { width: parent.width * 0.4; height: 40; border.color: "black"
                            Text { text: modelData.name; anchors.left: parent.left; anchors.leftMargin: 5; anchors.verticalCenter: parent.verticalCenter; elide: Text.ElideRight; width: parent.width - 10 }
                        }
                        Rectangle { width: parent.width * 0.2; height: 40; border.color: "black"
                            Text { text: modelData.ready; anchors.centerIn: parent }
                        }
                        Rectangle { width: parent.width * 0.2; height: 40; border.color: "black"
                            Text { 
                                text: modelData.status
                                anchors.centerIn: parent
                                font.bold: modelData.status !== "Running"
                            }
                        }
                    }
                }
            }
        }
    }

    Component.onCompleted: kubectl.refresh()
}
