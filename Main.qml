import QtQuick
import QtQuick.Window

Window {
    width: Screen.width
    height: Screen.height
    visible: true
    color: "white"

    Column {
        anchors.centerIn: parent
        spacing: 20

        Text {
            text: "K8s Dashboard"
            font.pixelSize: 48
            font.bold: true
            color: "black"
        }

        Text {
            text: kubectl.podStatus
            font.pixelSize: 32
            color: "black"
        }

        Rectangle {
            width: 200; height: 60
            color: "black"
            Text {
                text: "REFRESH"
                color: "white"
                anchors.centerIn: parent
            }
            MouseArea {
                anchors.fill: parent
                onClicked: kubectl.refresh()
            }
        }
    }
}
