#include "compositor/ShellModuleSchema/ShellModuleSchema.hpp"
#include <QJsonArray>
#include <QJsonDocument>
#include <QRegularExpression>
#include <cmath>

namespace LuDash {
QStringList shellModuleIds() {
    return {"panel", "wallpaper", "launcher", "overview", "settings", "setup",
            "session", "feedback", "compatibility"};
}

QJsonObject defaultModuleDocument() {
    QJsonObject modules;
    for (const auto& id : shellModuleIds()) {
        QJsonObject config;
        if (id == "launcher") {
            config = QJsonObject{{"buttonSize", 38},
                                 {"logoScale", 88},
                                 {"backgroundOpacity", 18},
                                 {"glow", true},
                                 {"orbit", true}};
        } else if (id == "panel") {
            config = QJsonObject{{"backgroundVisible", false},
                                 {"contrastShells", true},
                                 {"workspacePills", true},
                                 {"workspaceInactiveWidth", 18},
                                 {"workspaceActiveWidth", 38},
                                 {"workspacePillHeight", 10},
                                 {"shellOpacity", 20},
                                 {"workspaceTransition", true},
                                 {"transitionDuration", 420}};
        } else if (id == "overview") {
            config = QJsonObject{{"showMedia", true},
                                 {"quickControls", true},
                                 {"calendarImage", ""},
                                 {"shortcuts", QJsonArray{"files", "terminal", "settings"}}};
        }
        modules[id] = QJsonObject{
            {"enabled", true},
            {"style", QJsonObject{{"width", 0},
                                  {"height", 0},
                                  {"margin", id == "panel" || id == "wallpaper" ? 0 : 12},
                                  {"radius", id == "panel" ? 18 : id == "wallpaper" ? 0 : 24},
                                  {"background", "inherit"},
                                  {"foreground", "inherit"},
                                  {"accent", "inherit"},
                                  {"fontSize", 13},
                                  {"edge", "top"},
                                  {"x", 0},
                                  {"y", 0}}},
            {"config", config},
            {"custom", QJsonObject{{"enabled", false}, {"entry", ""}}}};
    }
    return {{"schemaVersion", 1}, {"modules", modules}};
}

namespace {
bool fail(QString* error, const QString& text) {
    if (error) *error = text;
    return false;
}

bool keysAllowed(const QJsonObject& object, const QStringList& keys) {
    for (auto it = object.begin(); it != object.end(); ++it)
        if (!keys.contains(it.key())) return false;
    return true;
}

bool integer(const QJsonValue& value, int low, int high) {
    const double n = value.toDouble(-1);
    return value.isDouble() && std::isfinite(n) && std::floor(n) == n && n >= low && n <= high;
}

bool validOverviewShortcuts(const QJsonValue& value) {
    if (!value.isArray() || value.toArray().size() > 6)
        return false;
    const QStringList allowed{"files", "terminal", "settings", "monitor", "network", "plugins"};
    QSet<QString> seen;
    for (const auto& item : value.toArray()) {
        if (!item.isString() || !allowed.contains(item.toString()) || seen.contains(item.toString()))
            return false;
        seen.insert(item.toString());
    }
    return true;
}
} // namespace

bool validateModuleDocument(const QByteArray& text, QJsonObject* normalized, QString* error) {
    if (!normalized || text.size() > 16384)
        return fail(error, "Module JSON must be at most 16 KiB.");

    QJsonParseError parseError;
    const auto document = QJsonDocument::fromJson(text, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject())
        return fail(error, "Invalid module JSON: " + parseError.errorString());

    const auto root = document.object();
    if (!keysAllowed(root, {"schemaVersion", "modules"}) ||
        !integer(root.value("schemaVersion"), 1, 1) ||
        !root.value("modules").isObject())
        return fail(error, "Expected schemaVersion 1 and a modules object.");

    auto result = defaultModuleDocument();
    auto modules = result.value("modules").toObject();
    const auto input = root.value("modules").toObject();
    const QRegularExpression color("^#(?:[0-9a-fA-F]{6}|[0-9a-fA-F]{8})$");
    const QRegularExpression entry("^[a-zA-Z0-9_-]+/[a-zA-Z0-9_-]+\\.qml$");

    for (auto it = input.begin(); it != input.end(); ++it) {
        const auto id = it.key();
        if (!modules.contains(id) || !it.value().isObject())
            return fail(error, "Unknown module or invalid object: " + id);

        const auto source = it.value().toObject();
        auto module = modules.value(id).toObject();
        if (!keysAllowed(source, {"enabled", "style", "config", "custom"}))
            return fail(error, "Unknown module field: " + id);

        if (source.contains("enabled")) {
            if (!source.value("enabled").isBool())
                return fail(error, "Module enabled must be boolean: " + id);
            if (!source.value("enabled").toBool() &&
                QStringList{"settings", "setup", "feedback"}.contains(id))
                return fail(error, "Recovery modules cannot be disabled: " + id);
            module["enabled"] = source.value("enabled");
        }

        if (source.contains("style")) {
            if (!source.value("style").isObject())
                return fail(error, "Module style must be an object: " + id);
            auto style = module.value("style").toObject();
            const auto changes = source.value("style").toObject();
            for (auto field = changes.begin(); field != changes.end(); ++field) {
                const auto key = field.key();
                const auto value = field.value();
                bool valid = false;
                if (key == "width")
                    valid = integer(value, 0, 0) || integer(value, id == "settings" ? 800 : 320, 3840);
                else if (key == "height")
                    valid = integer(value, 0, 0) ||
                            integer(value, id == "panel" ? 24 : id == "settings" || id == "setup" ? 480 : 80,
                                    id == "panel" ? 96 : 2160);
                else if (key == "x" || key == "y") valid = integer(value, 0, 3840);
                else if (key == "margin" || key == "radius") valid = integer(value, 0, 64);
                else if (key == "fontSize") valid = integer(value, 10, 28);
                else if (key == "background" || key == "foreground" || key == "accent")
                    valid = value.isString() &&
                            (value.toString() == "inherit" || color.match(value.toString()).hasMatch());
                else if (key == "edge")
                    valid = value.isString() &&
                            (value.toString() == "top" || value.toString() == "bottom") &&
                            (id == "panel" || value.toString() == "top");
                if (!valid) return fail(error, "Invalid style field: " + id + "." + key);
                style[key] = value;
            }
            module["style"] = style;
        }

        if (source.contains("config")) {
            if (!source.value("config").isObject())
                return fail(error, "Module config must be an object: " + id);
            const auto changes = source.value("config").toObject();
            if (id != "launcher" && id != "panel" && id != "overview" && !changes.isEmpty())
                return fail(error, "This module does not have module-specific config fields.");
            auto config = module.value("config").toObject();
            if (id == "launcher") {
                if (!keysAllowed(changes, {"buttonSize", "logoScale", "backgroundOpacity", "glow", "orbit"}))
                    return fail(error, "Unknown launcher config field.");
                for (auto field = changes.begin(); field != changes.end(); ++field) {
                    const auto key = field.key();
                    const auto value = field.value();
                    bool valid = false;
                    if (key == "buttonSize") valid = integer(value, 28, 64);
                    else if (key == "logoScale") valid = integer(value, 50, 120);
                    else if (key == "backgroundOpacity") valid = integer(value, 0, 100);
                    else if (key == "glow" || key == "orbit") valid = value.isBool();
                    if (!valid) return fail(error, "Invalid launcher config field: " + key);
                    config[key] = value;
                }
            } else if (id == "panel") {
                if (!keysAllowed(changes, {"backgroundVisible", "contrastShells", "workspacePills",
                                           "workspaceInactiveWidth", "workspaceActiveWidth", "workspacePillHeight",
                                           "shellOpacity", "workspaceTransition", "transitionDuration"}))
                    return fail(error, "Unknown panel config field.");
                for (auto field = changes.begin(); field != changes.end(); ++field) {
                    const auto key = field.key();
                    const auto value = field.value();
                    bool valid = false;
                    if (key == "backgroundVisible" || key == "contrastShells" || key == "workspacePills" ||
                        key == "workspaceTransition") valid = value.isBool();
                    else if (key == "workspaceInactiveWidth") valid = integer(value, 8, 48);
                    else if (key == "workspaceActiveWidth") valid = integer(value, 18, 72);
                    else if (key == "workspacePillHeight") valid = integer(value, 4, 20);
                    else if (key == "shellOpacity") valid = integer(value, 0, 60);
                    else if (key == "transitionDuration") valid = integer(value, 180, 900);
                    if (!valid) return fail(error, "Invalid panel config field: " + key);
                    config[key] = value;
                }
                if (config.value("workspaceActiveWidth").toInt() < config.value("workspaceInactiveWidth").toInt())
                    return fail(error, "Active workspace pill must be at least as wide as an inactive pill.");
            } else if (id == "overview") {
                if (!keysAllowed(changes, {"showMedia", "quickControls", "calendarImage", "shortcuts"}))
                    return fail(error, "Unknown overview config field.");
                for (auto field = changes.begin(); field != changes.end(); ++field) {
                    const auto key = field.key();
                    const auto value = field.value();
                    bool valid = false;
                    if (key == "showMedia" || key == "quickControls") valid = value.isBool();
                    else if (key == "calendarImage") {
                        const QString image = value.toString();
                        valid = value.isString() && image.size() <= 4096 &&
                                (image.isEmpty() || (image.startsWith("file://") &&
                                 QRegularExpression("\\.(?:png|jpe?g|webp|gif)$", QRegularExpression::CaseInsensitiveOption)
                                     .match(image).hasMatch()));
                    } else if (key == "shortcuts") valid = validOverviewShortcuts(value);
                    if (!valid) return fail(error, "Invalid overview config field: " + key);
                    config[key] = value;
                }
            }
            module["config"] = config;
        }

        if (source.contains("custom")) {
            if (!source.value("custom").isObject())
                return fail(error, "Custom module configuration must be an object.");
            const auto custom = source.value("custom").toObject();
            if (!keysAllowed(custom, {"enabled", "entry"}) ||
                !custom.value("enabled").isBool() || !custom.value("entry").isString())
                return fail(error, "Custom modules require boolean enabled and string entry.");
            const auto path = custom.value("entry").toString();
            if ((!path.isEmpty() && !entry.match(path).hasMatch()) ||
                (custom.value("enabled").toBool() && path.isEmpty()))
                return fail(error, "Entry must be a relative module/Main.qml path with no traversal.");
            module["custom"] = custom;
        }

        modules[id] = module;
    }

    result["modules"] = modules;
    *normalized = result;
    return true;
}
} // namespace LuDash
