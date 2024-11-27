import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ApplicationWindow {
    id: window

    property var windowWidth: Math.round(fontMetrics.height * 32.2856)
    property var windowHeight: Math.round(fontMetrics.height * 13.9528)
    property var heightSafeMargin: 15

    minimumWidth: Math.max(windowWidth, mainLayout.Layout.minimumWidth) + mainLayout.anchors.margins * 2
    minimumHeight: Math.max(windowHeight, mainLayout.Layout.minimumHeight) + mainLayout.anchors.margins * 2 + heightSafeMargin
    maximumWidth: minimumWidth
    maximumHeight: minimumHeight
    visible: true
    onClosing: {
        hpa.setResult("fail");
    }

    FontMetrics {
        id: fontMetrics
    }

    SystemPalette {
        id: system

        colorGroup: SystemPalette.Active
    }

    Item {
        id: mainLayout

        anchors.fill: parent
        Keys.onEscapePressed: (e) => {
            hpa.setResult("fail");
        }
        Keys.onReturnPressed: (e) => {
            hpa.setResult("auth:" + passwordField.text);
        }
        Keys.onEnterPressed: (e) => {
            hpa.setResult("auth:" + passwordField.text);
        }

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 4

            Label {
                color: Qt.darker(system.windowText, 0.8)
                font.bold: true
                font.pointSize: Math.round(fontMetrics.height * 1.05)
                text: "Authenticating for " + hpa.getUser()
                Layout.alignment: Qt.AlignHCenter
                Layout.maximumWidth: parent.width
                elide: Text.ElideRight
                wrapMode: Text.WordWrap
            }

            HSeparator {
                Layout.topMargin: fontMetrics.height / 2
                Layout.bottomMargin: fontMetrics.height / 2
            }

            Label {
                color: system.windowText
                text: hpa.getMessage()
                Layout.maximumWidth: parent.width
                elide: Text.ElideRight
                wrapMode: Text.WordWrap
            }

            TextField {
                id: passwordField

                Layout.topMargin: fontMetrics.height / 2
                placeholderText: "Password"
                Layout.alignment: Qt.AlignHCenter
                hoverEnabled: true
                persistentSelection: true
                echoMode: TextInput.Password
                focus: true

                Connections {
                    target: hpa
                    function onFocusField() {
                        passwordField.focus = true;
                    }
                    function onBlockInput(block) {
                        passwordField.readOnly = block;
                        if (!block) {
                            passwordField.focus = true;
                            passwordField.selectAll();
                        }
                    }
                }

            }

            Label {
                id: errorLabel

                color: "red"
                font.italic: true
                Layout.topMargin: 0
                text: ""
                Layout.alignment: Qt.AlignHCenter

                Connections {
                    target: hpa
                    function onSetErrorString(e) {
                        errorLabel.text = e;
                    }
                }

            }

            Rectangle {
                color: "transparent"
                Layout.fillHeight: true
            }

            HSeparator {
                Layout.topMargin: fontMetrics.height / 2
                Layout.bottomMargin: fontMetrics.height / 2
            }

            RowLayout {
                Layout.alignment: Qt.AlignRight
                Layout.rightMargin: fontMetrics.height / 2

                Button {
                    text: "Cancel"
                    onClicked: (e) => {
                        hpa.setResult("fail");
                    }
                }

                Button {
                    text: "Authenticate"
                    onClicked: (e) => {
                        hpa.setResult("auth:" + passwordField.text);
                    }
                }

            }

        }

    }

    component Separator: Rectangle {
        color: Qt.darker(window.palette.text, 1.5)
    }

    component HSeparator: Separator {
        implicitHeight: 1
        Layout.fillWidth: true
        Layout.leftMargin: fontMetrics.height * 8
        Layout.rightMargin: fontMetrics.height * 8
    }

}
