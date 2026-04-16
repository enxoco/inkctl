import QtQuick
import QtQuick.Controls

Window {
    id: root
    width: Screen.width
    height: Screen.height
    visible: true
    color: "white"

    // The Paper Pro screen is 1620x2160. 
    // This container ensures the rotation happens around the center correctly.
    Item {
        id: mainContainer
        width: root.height 
        height: root.width
        rotation: 90
        anchors.centerIn: parent

        Column {
            anchors.fill: parent
            anchors.margins: 20
            spacing: 10

            // START K3S OVERLAY
            Rectangle {
                width: parent.width
                height: kubectl.k3sRunning ? 0 : 200
                color: "#EEEEEE"
                border.color: "black"
                border.width: 3
                visible: !kubectl.k3sRunning
                clip: true
                
                Column {
                    anchors.centerIn: parent
                    spacing: 15
                    Text { 
                        text: "k3s is not running!"; 
                        font.pixelSize: 35; font.bold: true; color: "black" 
                    }
                    Button {
                        text: "START K3S"
                        width: 200; height: 60
                        onClicked: kubectl.startK3s()
                    }
                }
            }

            // Header Section
            Rectangle {
                width: parent.width; height: 80
                color: "black"
                
                Column {
                    anchors.left: parent.left
                    anchors.leftMargin: 20
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 2

                    Text {
                        text: "InkCTL: Pods"
                        color: "white"
                        font.pixelSize: 28
                        font.bold: true
                    }
                    Text {
                        text: kubectl.hostname
                        color: "#aaaaaa"
                        font.pixelSize: 16
                    }
                }

                Row {
                    anchors.right: parent.right
                    anchors.rightMargin: 20
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 20

                    // Distinct Refresh Button
                    Rectangle {
                        width: 140; height: 50; color: "#333333"; border.color: "white"
                        Text { text: "Refresh"; color: "white"; anchors.centerIn: parent; font.pixelSize: 20 }
                        MouseArea { anchors.fill: parent; onClicked: kubectl.refresh() }
                    }

                    // Distinct Exit Button
                    Rectangle {
                        width: 140; height: 50; color: "#333333"; border.color: "white"
                        Text { text: "Exit"; color: "white"; anchors.centerIn: parent; font.pixelSize: 20 }
                        MouseArea { anchors.fill: parent; onClicked: kubectl.exitToSystem() }
                    }
                }
            }

            // Table Header
            Row {
                width: parent.width
                height: 40
                Repeater {
                    model: ["NAMESPACE", "NAME", "READY", "STATUS"]
                    delegate: Rectangle {
                        width: index === 1 ? (parent.width * 0.4) : (parent.width * 0.2)
                        height: parent.height
                        border.color: "black"
                        color: "#f0f0f0"
                        Text { text: modelData; font.bold: true; anchors.centerIn: parent; font.pixelSize: 18 }
                    }
                }
            }

            // Pod List
            ListView {
                visible: kubectl.k3sRunning
                width: parent.width
                height: parent.height - 150 // Adjusted for header heights
                model: kubectl.podList
                clip: true
                delegate: Rectangle {
                    width: ListView.view.width
                    height: 50
                    border.color: "#cccccc"
                    Row {
                        anchors.fill: parent
                        Rectangle { width: parent.width * 0.2; height: 50; border.width: 0.5
                            Text { text: modelData.namespace; anchors.centerIn: parent; elide: Text.ElideRight; width: parent.width - 10 }
                        }
                        Rectangle { width: parent.width * 0.4; height: 50; border.width: 0.5
                            Text { text: modelData.name; anchors.left: parent.left; anchors.leftMargin: 10; anchors.verticalCenter: parent.verticalCenter; elide: Text.ElideRight; width: parent.width - 10 }
                        }
                        Rectangle { width: parent.width * 0.2; height: 50; border.width: 0.5
                            Text { text: modelData.ready; anchors.centerIn: parent }
                        }
                        Rectangle { width: parent.width * 0.2; height: 50; border.width: 0.5
                            Text {
                                text: modelData.status; anchors.centerIn: parent
                                font.bold: modelData.status !== "Running"
                                color: modelData.status === "Running" ? "black" : "red"
                            }
                        }
                    }
                }
            }
        }
    }

    Component.onCompleted: kubectl.refresh()
}