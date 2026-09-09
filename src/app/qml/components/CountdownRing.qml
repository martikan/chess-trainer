import QtQuick
import QtQuick.Controls as Controls
import org.kde.kirigami as Kirigami

Item {
    id: root

    property int remainingMs: 0
    property int totalMs: 30000

    readonly property real progress: totalMs > 0
        ? Math.max(0, Math.min(1, remainingMs / totalMs))
        : 0

    readonly property int remainingSeconds: Math.ceil(remainingMs / 1000)

    implicitWidth: Kirigami.Units.gridUnit * 7
    implicitHeight: implicitWidth

    onProgressChanged: canvas.requestPaint()

    Canvas {
        id: canvas
        anchors.fill: parent

        readonly property real strokeWidth: Kirigami.Units.smallSpacing * 1.5

        onPaint: {
            const context = getContext("2d");
            context.reset();

            const centreX = width / 2;
            const centreY = height / 2;
            const radius = Math.min(width, height) / 2 - strokeWidth;

            context.lineWidth = strokeWidth;
            context.lineCap = "round";

            // Track
            context.beginPath();
            context.strokeStyle = Kirigami.Theme.alternateBackgroundColor;
            context.arc(centreX, centreY, radius, 0, 2 * Math.PI);
            context.stroke();

            if (root.progress <= 0) {
                return;
            }

            // Remaining time, sweeping clockwise from twelve o'clock.
            context.beginPath();
            context.strokeStyle = Kirigami.Theme.highlightColor;
            context.arc(centreX, centreY, radius,
                        -Math.PI / 2,
                        -Math.PI / 2 + (2 * Math.PI * root.progress));
            context.stroke();
        }
    }

    Column {
        anchors.centerIn: parent
        spacing: 0

        Kirigami.Heading {
            level: 1
            horizontalAlignment: Text.AlignHCenter
            anchors.horizontalCenter: parent.horizontalCenter
            text: root.remainingSeconds
        }

        Controls.Label {
            anchors.horizontalCenter: parent.horizontalCenter
            text: i18nc("abbreviation for seconds", "SEC")
            font.pointSize: Kirigami.Theme.smallFont.pointSize
            font.letterSpacing: 2
            opacity: 0.6
        }
    }
}
