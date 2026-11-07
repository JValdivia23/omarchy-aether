import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material

Button {
    id: control
    Material.roundedScale: theme.radius
    implicitHeight: 40
    leftPadding: 16
    rightPadding: 16
    background: Rectangle {
        radius: theme.radius
        color: control.highlighted ? theme.accent : Qt.alpha(theme.foreground, control.down ? 0.18 : control.hovered ? 0.12 : 0.06)
        border.width: control.activeFocus ? 2 : 1
        border.color: control.activeFocus ? theme.accent : Qt.alpha(theme.foreground, 0.25)
        opacity: control.enabled ? 1 : 0.5
    }
    contentItem: Text {
        text: control.text
        font: control.font
        color: control.highlighted ? theme.accentForeground : theme.foreground
        opacity: control.enabled ? 1 : 0.6
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
        textFormat: Text.PlainText
    }
}
