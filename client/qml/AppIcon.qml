import QtQuick
import QtQuick.Effects

Item {
    id: root

    property alias source: iconImage.source
    property color color: "black"

    implicitWidth: 24
    implicitHeight: 24

    Image {
        id: iconImage
        anchors.fill: parent
        sourceSize.width: width
        sourceSize.height: height
        fillMode: Image.PreserveAspectFit
        visible: false
    }

    MultiEffect {
        anchors.fill: parent
        source: iconImage
        colorization: 1.0
        colorizationColor: root.color
    }
}
