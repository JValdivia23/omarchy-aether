import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

ApplicationWindow {
    id: win
    objectName: "aetherWindow"
    width: 1120
    height: 790
    minimumWidth: 820
    minimumHeight: 630
    visible: true
    title: "Aether"
    color: theme.background
    Material.theme: Material.Dark
    Material.accent: theme.accent
    Material.foreground: theme.foreground
    Material.roundedScale: theme.radius

    readonly property color previewBackground: backend.colors.length > 7 ? backend.colors[0].color : theme.background
    readonly property color previewForeground: backend.colors.length > 7 ? backend.colors[7].color : theme.foreground
    onClosing: function(close) { if (backend.busy) close.accepted = false }

    Shortcut { sequence: "?"; context: Qt.ApplicationShortcut; onActivated: help.open() }
    Shortcut { sequence: "Q"; context: Qt.ApplicationShortcut; onActivated: { if (!backend.busy) Qt.quit() } }
    Shortcut { sequence: "O"; context: Qt.ApplicationShortcut; enabled: !backend.busy; onActivated: backend.chooseWallpaper() }
    Shortcut { sequence: "C"; context: Qt.ApplicationShortcut; enabled: !backend.busy; onActivated: backend.chooseCollection() }
    Shortcut { sequence: "B"; context: Qt.ApplicationShortcut; enabled: !backend.busy; onActivated: backend.nativePicker() }
    Shortcut { sequence: "Space"; context: Qt.ApplicationShortcut; enabled: !backend.busy; onActivated: backend.primaryAction() }
    Shortcut { sequence: "Ctrl+Return"; context: Qt.ApplicationShortcut; enabled: backend.ready && !backend.busy; onActivated: confirmation.open() }

    Item {
        anchors.fill: parent
        LayoutMirroring.enabled: Qt.application.layoutDirection === Qt.RightToLeft
        LayoutMirroring.childrenInherit: true

        ScrollView {
            anchors.fill: parent
            anchors.margins: 28
            contentWidth: availableWidth
            ColumnLayout {
                width: parent.width
                spacing: 18
                RowLayout {
                    Layout.fillWidth: true
                    ColumnLayout {
                        spacing: 4
                        Label { text: "AETHER"; font.pixelSize: 26; font.letterSpacing: 4; font.bold: true; textFormat: Text.PlainText }
                        Label { text: qsTr("Wallpaper atmosphere. Tokyo Night clarity."); font.pixelSize: 14; textFormat: Text.PlainText }
                    }
                    Item { Layout.fillWidth: true }
                    AetherButton { text: qsTr("Shortcuts"); onClicked: help.open() }
                }

                RowLayout {
                    spacing: 10
                    AetherButton { objectName: "chooseWallpaper"; text: qsTr("Choose wallpaper"); enabled: !backend.busy; onClicked: backend.chooseWallpaper() }
                    AetherButton { text: qsTr("Choose collection"); enabled: !backend.busy; onClicked: backend.chooseCollection() }
                    AetherButton { text: qsTr("Native picker"); enabled: !backend.busy; onClicked: backend.nativePicker() }
                    Item { Layout.fillWidth: true }
                    BusyIndicator { running: backend.busy; visible: running; Layout.preferredWidth: 32; Layout.preferredHeight: 32 }
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 265
                    color: win.previewBackground
                    border.color: theme.accent
                    border.width: 1
                    radius: theme.radius
                    clip: true
                    RowLayout {
                        anchors.fill: parent
                        anchors.margins: 16
                        spacing: 24
                        Item {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            Image {
                                anchors.fill: parent
                                source: backend.image
                                fillMode: Image.PreserveAspectCrop
                                sourceSize.width: 1200
                                asynchronous: true
                                visible: source.toString() !== ""
                            }
                            Label {
                                anchors.centerIn: parent
                                width: parent.width - 20
                                horizontalAlignment: Text.AlignHCenter
                                wrapMode: Text.WordWrap
                                text: qsTr("Choose an image to preview its system palette.")
                                textFormat: Text.PlainText
                                visible: backend.image.toString() === ""
                            }
                        }
                        ColumnLayout {
                            Layout.preferredWidth: 330
                            Layout.fillHeight: true
                            spacing: 8
                            Label { text: qsTr("SYSTEM PREVIEW"); color: win.previewForeground; font.letterSpacing: 2; font.pixelSize: 11; textFormat: Text.PlainText }
                            Label { text: "~/project  main"; color: win.previewForeground; font.family: "monospace"; font.pixelSize: 16; textFormat: Text.PlainText }
                            Repeater {
                                model: backend.colors.length > 7 ? [3, 2, 5, 4, 6, 1] : []
                                Label {
                                    required property int modelData
                                    required property int index
                                    readonly property var item: backend.colors[modelData]
                                    text: [qsTr("Warning"), qsTr("Success"), qsTr("Violet"), qsTr("Blue"), qsTr("Cyan"), qsTr("Error")][index]
                                    color: item.color
                                    font.family: "monospace"
                                    font.pixelSize: 15
                                    textFormat: Text.PlainText
                                }
                            }
                            Item { Layout.fillHeight: true }
                        }
                    }
                }

                Label { text: backend.fileName || qsTr("No wallpaper selected"); Layout.fillWidth: true; elide: Text.ElideMiddle; textFormat: Text.PlainText }

                GridLayout {
                    Layout.fillWidth: true
                    columns: 8
                    uniformCellWidths: true
                    columnSpacing: 10
                    rowSpacing: 14
                    Repeater {
                        model: backend.colors
                        ColumnLayout {
                            required property var modelData
                            Layout.fillWidth: true
                            spacing: 5
                            Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 30; color: modelData.color; radius: Math.min(4, theme.radius); border.color: theme.foreground; border.width: modelData.name === "background" ? 1 : 0 }
                            Label { text: modelData.name; font.pixelSize: 10; Layout.fillWidth: true; elide: Text.ElideRight; textFormat: Text.PlainText }
                            Label { text: modelData.color; font.family: "monospace"; font.pixelSize: 10; textFormat: Text.PlainText }
                        }
                    }
                }

                GridLayout {
                    visible: backend.ready
                    columns: win.width >= 1000 ? 3 : 1
                    columnSpacing: 24
                    rowSpacing: 6
                    Label {
                        text: { translator.language; return qsTr("Body contrast: %1:1").arg(Number(backend.metrics.body_contrast || 0).toLocaleString(Qt.locale(), 'f', 2)) }
                        font.bold: true
                        textFormat: Text.PlainText
                    }
                    Label {
                        text: { translator.language; return qsTr("Lowest text contrast: %1:1").arg(Number(backend.metrics.minimum_text_contrast || 0).toLocaleString(Qt.locale(), 'f', 2)) }
                        textFormat: Text.PlainText
                    }
                    Label { text: qsTr("%n distinct ANSI colors", "", backend.metrics.unique_ansi_colors || 0); textFormat: Text.PlainText }
                }

                Label { text: backend.status; Layout.fillWidth: true; wrapMode: Text.WordWrap; textFormat: Text.PlainText }
                Label { text: backend.error; visible: text.length > 0; Layout.fillWidth: true; wrapMode: Text.WordWrap; color: "#ffb4ab"; textFormat: Text.PlainText }
                RowLayout {
                    Layout.fillWidth: true
                    AetherButton { text: qsTr("Generate palette"); enabled: !backend.busy; highlighted: true; onClicked: backend.primaryAction() }
                    Item { Layout.fillWidth: true }
                    AetherButton { objectName: "applyTheme"; text: qsTr("Apply through Omarchy"); enabled: backend.ready && !backend.busy && backend.image.toString() !== ""; onClicked: confirmation.open() }
                }
            }
        }
    }

    Dialog {
        id: confirmation
        anchors.centerIn: parent
        width: Math.min(500, win.width - 60)
        title: qsTr("Apply Aether?")
        modal: true
        standardButtons: Dialog.Ok | Dialog.Cancel
        Label { width: parent.width; text: qsTr("This changes your wallpaper and system theme using Omarchy. Application-specific settings stay untouched."); wrapMode: Text.WordWrap; textFormat: Text.PlainText }
        onAccepted: backend.applyTheme()
    }
    Dialog {
        id: help
        objectName: "shortcutHelp"
        anchors.centerIn: parent
        width: Math.min(520, win.width - 60)
        title: qsTr("Keyboard shortcuts")
        modal: true
        standardButtons: Dialog.Close
        Label { text: qsTr("O  Choose wallpaper\nC  Choose collection\nB  Native picker\nSpace  Generate palette\nCtrl+Enter  Apply with confirmation\n?  Show shortcuts\nQ  Quit when idle"); wrapMode: Text.WordWrap; textFormat: Text.PlainText }
    }
}
