import QtQuick

Item {
    id: card
    required property var shell
    MediaController {
        id: controller
        shell: card.shell
        polling: card.visible
    }
    MediaView {
        anchors.fill: parent
        shell: card.shell
        media: controller.media
        preferredService: controller.preferredService
        preferredBus: controller.preferredBus
        errorMessage: controller.errorMessage
        onAction: (name, value) => controller.run(name, value)
        onPlayerSelected: (service, bus) => controller.selectPlayer(service, bus)
    }
}
