import QtQuick
import QtQuick.Layouts
import QtQuick.Window
import Seraph
import Quill as Q

Window {
    id: root
    // Not translated: syncHyprlandRounding() addresses this window by its title.
    title: "Seraph Settings"
    flags: Qt.Dialog | Qt.FramelessWindowHint
    color: "transparent"

    width: dialogWidth
    height: pageContainer.implicitHeight
    minimumWidth: dialogWidth
    minimumHeight: height

    readonly property int dialogWidth: Math.min(920, (transientParent ? transientParent.width : 920) - 32)
    readonly property int dialogRadius: draftRadiusLarge + 6

    function syncHyprlandRounding() {
        fileOps.setHyprlandRounding(root.title, root.dialogRadius)
        fileOps.setHyprlandBorder(root.title, 0)
    }

    onDialogRadiusChanged: {
        if (root.visible)
            syncHyprlandRounding()
    }

    readonly property color sectionBorderColor: Qt.rgba(Theme.text.r, Theme.text.g, Theme.text.b, 0.08)
    // "auto" tracks the system light/dark preference; only offered as the
    // default where the system actually reports one.
    readonly property string autoThemeValue: "auto"
    readonly property string autoThemeLabel: qsTr("Follow System")
    readonly property string defaultThemeName: autoThemeValue
    readonly property string defaultLightThemeName: "catppuccin-latte"
    readonly property string defaultDarkThemeName: "catppuccin-mocha"
    readonly property bool followSystemTheme: draftTheme === autoThemeValue
    readonly property bool systemIsDark: theme.systemColorScheme !== "light"
    readonly property string defaultIconThemeName: "Adwaita"
    readonly property string defaultSidebarPosition: "left"
    readonly property int defaultSidebarWidth: 200
    readonly property int defaultRadiusSmall: 4
    readonly property int defaultRadiusMedium: 8
    readonly property int defaultRadiusLarge: 12
    readonly property int defaultGridSpacing: 12
    readonly property bool defaultTransparencyEnabled: true
    readonly property real defaultTransparencyLevel: 1.0
    readonly property bool defaultAnimationsEnabled: true
    readonly property int defaultAnimDurationFast: 100
    readonly property int defaultAnimDuration: 200
    readonly property int defaultAnimDurationSlow: 350
    readonly property string defaultAnimCurveEnter: "OutCubic"
    readonly property string defaultAnimCurveExit: "InCubic"
    readonly property string defaultAnimCurveTransition: "Bezier"
    readonly property bool defaultShowWindowControls: false
    readonly property string defaultWindowButtonLayout: ":minimize,maximize,close"
    readonly property string defaultSortBy: "name"
    readonly property bool defaultSortAscending: true
    readonly property bool defaultRememberSortPerFolder: true
    readonly property string defaultStartupLocation: "last"

    // Sort column dropdown: parallel label/value arrays.
    readonly property var sortByLabels: [qsTr("Name"), qsTr("Size"), qsTr("Modified"), qsTr("Type")]
    readonly property var sortByValues: ["name", "size", "modified", "type"]
    readonly property var viewLabels: [qsTr("Grid"), qsTr("Detailed"), qsTr("Miller")]
    readonly property var viewValues: ["grid", "detailed", "miller"]

    readonly property string defaultLanguage: "auto"
    // Language names are endonyms on purpose: someone stuck in a language they
    // can't read must still find their own. Only "Automatic" is translated.
    readonly property var languageValues: ["auto", "pt_BR", "en"]
    readonly property var languageLabels: [
        qsTr("Automatic (%1)").arg(languageManager.effectiveLanguage === "pt_BR" ? "Português" : "English"),
        "Português (Brasil)",
        "English"
    ]

    // Startup folder dropdown: parallel label/value arrays.
    readonly property var startupLocationLabels: [qsTr("Last session"), qsTr("Home folder")]
    readonly property var startupLocationValues: ["last", "home"]

    property bool currentShowHidden: false
    property bool currentSidebarVisible: true
    property int currentSidebarWidth: 200

    property var themeOptions: []
    property var lightThemeOptions: []
    property var darkThemeOptions: []
    property var fontOptions: []
    property var iconThemeOptions: []
    property var availableThemeValues: []
    property var availableFontValues: []
    property var availableIconThemeValues: []
    property bool optionSourcesPrimed: false
    property bool syncingFromConfig: false
    property bool pendingSettingsDirty: false

    property string draftTheme: config.theme
    property string draftLanguage: config.language
    property string draftLightTheme: config.lightTheme
    property string draftDarkTheme: config.darkTheme
    property string draftFontFamily: config.fontFamily
    property string draftIconTheme: config.iconTheme
    property bool draftTintFolderIcons: (config.folderIconTint || "off").toLowerCase() !== "off"
    property bool draftDarkMode: true
    property bool draftShowHidden: currentShowHidden
    property bool draftSidebarVisible: currentSidebarVisible
    property string draftSidebarPosition: config.sidebarPosition
    property int draftSidebarWidth: currentSidebarWidth
    property int draftRadiusSmall: config.radiusSmall
    property int draftRadiusMedium: config.radiusMedium
    property int draftRadiusLarge: config.radiusLarge
    property int draftGridSpacing: config.gridSpacing
    property bool draftTransparencyEnabled: config.transparencyEnabled
    property real draftTransparencyLevel: config.transparencyLevel
    property bool draftAnimationsEnabled: config.animationsEnabled
    property int draftAnimDurationFast: config.animDurationFast
    property int draftAnimDuration: config.animDuration
    property int draftAnimDurationSlow: config.animDurationSlow
    property string draftAnimCurveEnter: config.animCurveEnter
    property string draftAnimCurveExit: config.animCurveExit
    property string draftAnimCurveTransition: config.animCurveTransition

    readonly property var curveOptions: ["OutCubic", "InOutCubic", "InCubic", "OutQuad", "InOutQuad", "OutExpo", "InOutExpo", "OutBack", "Linear", "Bezier"]

    property bool draftShowWindowControls: config.showWindowControls
    property string draftWindowButtonLayout: config.windowButtonLayout

    property string draftSortBy: config.sortBy
    property bool draftSortAscending: config.sortAscending
    property bool draftRememberSortPerFolder: config.rememberSortPerFolder
    property string draftDefaultView: config.defaultView
    property bool draftRememberViewPerFolder: config.rememberViewPerFolder
    property string draftStartupLocation: config.startupLocation

    // Helpers to decompose the layout string for the UI
    readonly property var _layoutParts: {
        var layout = draftWindowButtonLayout || ":minimize,maximize,close"
        var parts = layout.split(":")
        var leftStr = parts[0] || ""
        var rightStr = parts.length > 1 ? parts[1] : ""
        var allButtons = []
        if (leftStr) allButtons = allButtons.concat(leftStr.split(",").filter(function(s) { return s.trim() !== "" }))
        if (rightStr) allButtons = allButtons.concat(rightStr.split(",").filter(function(s) { return s.trim() !== "" }))
        return {
            side: leftStr && !rightStr ? "left" : "right",
            hasClose: allButtons.indexOf("close") >= 0,
            hasMinimize: allButtons.indexOf("minimize") >= 0,
            hasMaximize: allButtons.indexOf("maximize") >= 0
        }
    }

    function rebuildButtonLayout(side, hasClose, hasMinimize, hasMaximize) {
        var buttons = []
        if (hasMinimize) buttons.push("minimize")
        if (hasMaximize) buttons.push("maximize")
        if (hasClose) buttons.push("close")
        var str = buttons.join(",")
        draftWindowButtonLayout = side === "left" ? (str + ":") : (":" + str)
        applySettingsNow()
    }

    signal remoteConnectRequested()
    signal keyboardShortcutsRequested()
    signal closed()

    readonly property string systemFontLabel: qsTr("System Default")

    Component { id: paletteSectionIcon; IconSettings {} }
    Component { id: layoutSectionIcon; IconPanelLeft {} }
    Component { id: motionSectionIcon; IconClock {} }
    Component { id: toolsSectionIcon; IconFolder {} }

    property int currentSectionIndex: 0
    readonly property bool compactNavigation: dialogWidth < 860
    readonly property var sectionNavItems: [
        { title: qsTr("Look & Feel"), iconComponent: paletteSectionIcon },
        { title: qsTr("Layout"), iconComponent: layoutSectionIcon },
        { title: qsTr("Motion"), iconComponent: motionSectionIcon },
        { title: qsTr("Tools"), iconComponent: toolsSectionIcon }
    ]
    readonly property var sectionItems: [
        { title: qsTr("Look & Feel"), subtitle: qsTr("Theme, typography, icons, and surface styling."), iconComponent: paletteSectionIcon },
        { title: qsTr("Layout"), subtitle: qsTr("Sidebar behavior, file visibility, and toolbar controls."), iconComponent: layoutSectionIcon },
        { title: qsTr("Motion"), subtitle: qsTr("Animation timing and easing across the interface."), iconComponent: motionSectionIcon },
        { title: qsTr("Tools"), subtitle: qsTr("Shortcuts, remote locations, and config behavior."), iconComponent: toolsSectionIcon }
    ]

    function showSection(index) {
        currentSectionIndex = index
        if (sideTabs)
            sideTabs.currentIndex = index
        if (compactSectionNav)
            compactSectionNav.currentIndex = index
        if (contentFlick)
            contentFlick.contentY = 0
    }

    function primeOptionSources() {
        if (optionSourcesPrimed)
            return

        availableThemeValues = config.availableThemes
        availableFontValues = config.availableFonts
        availableIconThemeValues = config.availableIconThemes
        optionSourcesPrimed = true
    }

    function buildOptions(values, currentValue, fallbackValue) {
        var options = []
        for (var i = 0; i < values.length; ++i)
            options.push(values[i])

        var preferredValue = currentValue !== "" ? currentValue : fallbackValue
        if (preferredValue && options.indexOf(preferredValue) === -1)
            options.unshift(preferredValue)

        if (options.length === 0 && fallbackValue)
            options.push(fallbackValue)

        return options
    }

    function buildFontOptions() {
        var options = [systemFontLabel]
        for (var i = 0; i < availableFontValues.length; ++i)
            options.push(availableFontValues[i])

        if (draftFontFamily !== "" && options.indexOf(draftFontFamily) === -1)
            options.push(draftFontFamily)

        return options
    }

    function optionIndex(options, value, fallbackIndex) {
        var index = options.indexOf(value)
        return index >= 0 ? index : fallbackIndex
    }

    function themeLabelForValue(value) {
        return value === autoThemeValue ? autoThemeLabel : value
    }

    function themeValueForLabel(label) {
        return label === autoThemeLabel ? autoThemeValue : label
    }

    // The theme and font lists hold the translated "Follow System"/"System
    // Default" labels as sentinels, so they are rebuilt when the language
    // changes while the panel is open.
    Connections {
        target: languageManager
        function onLanguageChanged() {
            root.themeOptions = root.buildThemeOptions()
            root.fontOptions = root.buildFontOptions()
        }
    }

    function buildThemeOptions() {
        var concrete = buildOptions(availableThemeValues,
                                    draftTheme === autoThemeValue ? "" : draftTheme,
                                    defaultDarkThemeName)
        return [autoThemeLabel].concat(concrete)
    }

    function isDarkTheme(themeName) {
        if (themeName === autoThemeValue)
            return systemIsDark
        return themeName !== defaultLightThemeName && themeName !== draftLightTheme
    }

    function setDraftTheme(themeName) {
        draftTheme = themeName
        draftDarkMode = isDarkTheme(themeName)
    }

    // While following the system, the Dark Mode toggle mirrors it live.
    onSystemIsDarkChanged: {
        if (followSystemTheme)
            draftDarkMode = systemIsDark
    }

    function bindAppearancePreview() {
        Theme.radiusSmall = Qt.binding(function() {
            return root.visible ? root.draftRadiusSmall : config.radiusSmall
        })
        Theme.radiusMedium = Qt.binding(function() {
            return root.visible ? root.draftRadiusMedium : config.radiusMedium
        })
        Theme.radiusLarge = Qt.binding(function() {
            return root.visible ? root.draftRadiusLarge : config.radiusLarge
        })
        Theme.gridSpacing = Qt.binding(function() {
            return root.visible ? root.draftGridSpacing : config.gridSpacing
        })
        Theme.transparencyEnabled = Qt.binding(function() {
            return root.visible ? root.draftTransparencyEnabled : config.transparencyEnabled
        })
        Theme.transparencyLevel = Qt.binding(function() {
            return root.visible ? root.draftTransparencyLevel : Math.max(0, Math.min(1, config.transparencyLevel))
        })
        Theme.animationsEnabled = Qt.binding(function() {
            return root.visible ? root.draftAnimationsEnabled : config.animationsEnabled
        })
    }

    function resetToDefaults() {
        draftLightTheme = defaultLightThemeName
        draftDarkTheme = defaultDarkThemeName
        setDraftTheme(defaultThemeName)
        draftFontFamily = ""
        draftIconTheme = defaultIconThemeName
        draftTintFolderIcons = false
        draftShowHidden = false
        draftSidebarVisible = true
        draftSidebarPosition = defaultSidebarPosition
        draftSidebarWidth = defaultSidebarWidth
        draftRadiusSmall = defaultRadiusSmall
        draftRadiusMedium = defaultRadiusMedium
        draftRadiusLarge = defaultRadiusLarge
        draftGridSpacing = defaultGridSpacing
        draftTransparencyEnabled = defaultTransparencyEnabled
        draftTransparencyLevel = defaultTransparencyLevel
        draftAnimationsEnabled = defaultAnimationsEnabled
        draftAnimDurationFast = defaultAnimDurationFast
        draftAnimDuration = defaultAnimDuration
        draftAnimDurationSlow = defaultAnimDurationSlow
        draftAnimCurveEnter = defaultAnimCurveEnter
        draftAnimCurveExit = defaultAnimCurveExit
        draftAnimCurveTransition = defaultAnimCurveTransition
        draftShowWindowControls = defaultShowWindowControls
        draftWindowButtonLayout = defaultWindowButtonLayout
        draftSortBy = defaultSortBy
        draftSortAscending = defaultSortAscending
        draftRememberSortPerFolder = defaultRememberSortPerFolder
        draftStartupLocation = defaultStartupLocation
        draftLanguage = defaultLanguage
        applySettingsNow()
    }

    function syncFromCurrentState() {
        primeOptionSources()
        syncingFromConfig = true
        try {
            draftTheme = config.theme
            draftLanguage = config.language
            draftLightTheme = config.lightTheme
            draftDarkTheme = config.darkTheme
            draftDarkMode = isDarkTheme(draftTheme)
            themeOptions = buildThemeOptions()
            lightThemeOptions = buildOptions(availableThemeValues, draftLightTheme, defaultLightThemeName)
            darkThemeOptions = buildOptions(availableThemeValues, draftDarkTheme, defaultDarkThemeName)

            draftFontFamily = config.fontFamily
            fontOptions = buildFontOptions()

            draftIconTheme = config.iconTheme
            draftTintFolderIcons = (config.folderIconTint || "off").toLowerCase() !== "off"
            iconThemeOptions = buildOptions(availableIconThemeValues, draftIconTheme, "Adwaita")

            draftShowHidden = currentShowHidden
            draftSidebarVisible = currentSidebarVisible
            draftSidebarPosition = config.sidebarPosition
            draftSidebarWidth = currentSidebarWidth
            draftRadiusSmall = config.radiusSmall
            draftRadiusMedium = Math.max(config.radiusMedium, draftRadiusSmall)
            draftRadiusLarge = Math.max(config.radiusLarge, draftRadiusMedium)
            draftGridSpacing = config.gridSpacing
            draftTransparencyEnabled = config.transparencyEnabled
            draftTransparencyLevel = config.transparencyLevel
            draftAnimationsEnabled = config.animationsEnabled
            draftAnimDurationFast = config.animDurationFast
            draftAnimDuration = config.animDuration
            draftAnimDurationSlow = config.animDurationSlow
            draftAnimCurveEnter = config.animCurveEnter
            draftAnimCurveExit = config.animCurveExit
            draftAnimCurveTransition = config.animCurveTransition
            draftShowWindowControls = config.showWindowControls
            draftWindowButtonLayout = config.windowButtonLayout
            draftSortBy = config.sortBy
            draftSortAscending = config.sortAscending
            draftRememberSortPerFolder = config.rememberSortPerFolder
            draftDefaultView = config.defaultView
            draftRememberViewPerFolder = config.rememberViewPerFolder
            draftStartupLocation = config.startupLocation
        } finally {
            syncingFromConfig = false
        }
    }

    function openPanel() {
        syncFromCurrentState()
        showSection(0)
        // Center over the parent window
        if (transientParent) {
            root.x = transientParent.x + Math.round((transientParent.width - root.width) / 2)
            root.y = transientParent.y + Math.round((transientParent.height - root.height) / 2)
        }
        root.show()
        root.raise()
        root.requestActivate()
        root.syncHyprlandRounding()
    }

    function closePanel() {
        flushPendingChanges()
        root.hide()
        root.closed()
    }

    function openRemoteConnect() {
        closePanel()
        remoteConnectRequested()
    }

    function openKeyboardShortcuts() {
        closePanel()
        keyboardShortcutsRequested()
    }

    function currentSettings() {
        return {
            theme: draftTheme,
            language: draftLanguage,
            lightTheme: draftLightTheme,
            darkTheme: draftDarkTheme,
            fontFamily: draftFontFamily,
            iconTheme: draftIconTheme,
            folderIconTint: draftTintFolderIcons ? "accent" : "off",
            showHidden: draftShowHidden,
            sidebarVisible: draftSidebarVisible,
            sidebarPosition: draftSidebarPosition,
            sidebarWidth: draftSidebarWidth,
            radiusSmall: draftRadiusSmall,
            radiusMedium: draftRadiusMedium,
            radiusLarge: draftRadiusLarge,
            gridSpacing: draftGridSpacing,
            transparencyEnabled: draftTransparencyEnabled,
            transparencyLevel: draftTransparencyLevel,
            animationsEnabled: draftAnimationsEnabled,
            animDurationFast: draftAnimDurationFast,
            animDuration: draftAnimDuration,
            animDurationSlow: draftAnimDurationSlow,
            animCurveEnter: draftAnimCurveEnter,
            animCurveExit: draftAnimCurveExit,
            animCurveTransition: draftAnimCurveTransition,
            showWindowControls: draftShowWindowControls,
            windowButtonLayout: draftWindowButtonLayout,
            sortBy: draftSortBy,
            sortAscending: draftSortAscending,
            rememberSortPerFolder: draftRememberSortPerFolder,
            defaultView: draftDefaultView,
            rememberViewPerFolder: draftRememberViewPerFolder,
            startupLocation: draftStartupLocation
        }
    }

    function queueSettingsApply() {
        if (syncingFromConfig)
            return

        pendingSettingsDirty = true
        settingsApplyTimer.restart()
    }

    function applyPendingSettings() {
        if (!pendingSettingsDirty)
            return

        pendingSettingsDirty = false
        settingsApplyTimer.stop()
        config.saveSettings(currentSettings())
    }

    function applySettingsNow() {
        if (syncingFromConfig)
            return

        pendingSettingsDirty = true
        applyPendingSettings()
    }

    function flushPendingChanges() {
        applyPendingSettings()
    }

    onClosing: {
        root.flushPendingChanges()
        root.closed()
    }

    Component.onCompleted: {
        root.primeOptionSources()
        root.bindAppearancePreview()
    }

    Timer {
        id: settingsApplyTimer
        interval: 140
        onTriggered: root.applyPendingSettings()
    }

    // Close on Escape
    Shortcut {
        sequence: "Escape"
        enabled: root.visible
        onActivated: root.closePanel()
    }

    Component {
        id: lookPageComponent

        ColumnLayout {
            width: pageLoader.width
            spacing: 6

            Text {
                text: qsTr("Language")
                color: Theme.accent
                font.pointSize: Theme.fontSmall
                font.bold: true
                Layout.bottomMargin: 4
            }

            AnchoredDropdown {
                Layout.fillWidth: true
                label: qsTr("Interface language")
                model: root.languageLabels
                currentIndex: Math.max(0, root.languageValues.indexOf(root.draftLanguage))
                onSelected: (index, _) => {
                    root.draftLanguage = root.languageValues[index]
                    root.applySettingsNow()
                }
            }

            Q.Separator { Layout.topMargin: 8; Layout.bottomMargin: 8 }

            RowLayout {
                Layout.fillWidth: true
                Layout.bottomMargin: 8
                spacing: 12

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 2

                    Text {
                        Layout.fillWidth: true
                        text: qsTr("Dark Mode")
                        color: Theme.text
                        font.pointSize: Theme.fontNormal + 2
                        font.bold: true
                        wrapMode: Text.WordWrap
                    }

                    Text {
                        Layout.fillWidth: true
                        visible: root.followSystemTheme
                        text: theme.systemColorScheme === "unknown"
                            ? qsTr("Following the system — no preference reported")
                            : qsTr("Following the system")
                        color: Theme.muted
                        font.pointSize: Theme.fontSmall
                        wrapMode: Text.WordWrap
                    }
                }

                Q.Toggle {
                    label: ""
                    // Locked while the theme tracks the system; pick a concrete
                    // theme below to drive it manually again.
                    enabled: !root.followSystemTheme
                    checked: root.draftDarkMode
                    onToggled: (value) => {
                        root.setDraftTheme(value ? root.draftDarkTheme : root.draftLightTheme)
                        root.applySettingsNow()
                    }
                }
            }

            Q.Separator { Layout.bottomMargin: 8 }

            Text {
                text: qsTr("Theme")
                color: Theme.accent
                font.pointSize: Theme.fontSmall
                font.bold: true
                Layout.bottomMargin: 4
            }

            AnchoredDropdown {
                Layout.fillWidth: true
                label: qsTr("Theme")
                model: root.themeOptions
                currentIndex: root.optionIndex(root.themeOptions, root.themeLabelForValue(root.draftTheme), 0)
                onSelected: (_, value) => {
                    root.setDraftTheme(root.themeValueForLabel(value))
                    root.applySettingsNow()
                }
            }

            AnchoredDropdown {
                visible: root.followSystemTheme
                Layout.fillWidth: true
                label: qsTr("Light Theme")
                model: root.lightThemeOptions
                currentIndex: root.optionIndex(root.lightThemeOptions, root.draftLightTheme, 0)
                onSelected: (_, value) => {
                    root.draftLightTheme = value
                    root.applySettingsNow()
                }
            }

            AnchoredDropdown {
                visible: root.followSystemTheme
                Layout.fillWidth: true
                label: qsTr("Dark Theme")
                model: root.darkThemeOptions
                currentIndex: root.optionIndex(root.darkThemeOptions, root.draftDarkTheme, 0)
                onSelected: (_, value) => {
                    root.draftDarkTheme = value
                    root.applySettingsNow()
                }
            }

            AnchoredDropdown {
                Layout.fillWidth: true
                label: qsTr("Font")
                model: root.fontOptions
                currentIndex: root.optionIndex(root.fontOptions, root.draftFontFamily === "" ? root.systemFontLabel : root.draftFontFamily, 0)
                onSelected: (_, value) => {
                    root.draftFontFamily = value === root.systemFontLabel ? "" : value
                    root.applySettingsNow()
                }
            }

            AnchoredDropdown {
                Layout.fillWidth: true
                label: qsTr("Icon Pack")
                model: root.iconThemeOptions
                currentIndex: root.optionIndex(root.iconThemeOptions, root.draftIconTheme, 0)
                onSelected: (_, value) => {
                    root.draftIconTheme = value
                    root.applySettingsNow()
                }
            }

            RowLayout {
                Layout.fillWidth: true
                Layout.topMargin: 4
                spacing: 12

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 2

                    Text {
                        Layout.fillWidth: true
                        text: qsTr("Tint Folder Icons")
                        color: Theme.text
                        font.pointSize: Theme.fontNormal
                        wrapMode: Text.WordWrap
                    }

                    Text {
                        Layout.fillWidth: true
                        text: qsTr("Re-hue folders to the accent, keeping the pack's shading")
                        color: Theme.muted
                        font.pointSize: Theme.fontSmall
                        wrapMode: Text.WordWrap
                    }
                }

                Q.Toggle {
                    label: ""
                    checked: root.draftTintFolderIcons
                    onToggled: (value) => {
                        root.draftTintFolderIcons = value
                        root.applySettingsNow()
                    }
                }
            }

            Text {
                text: qsTr("Surface Styling")
                color: Theme.accent
                font.pointSize: Theme.fontSmall
                font.bold: true
                Layout.topMargin: 12
                Layout.bottomMargin: 4
            }

            Q.Toggle {
                Layout.fillWidth: true
                label: qsTr("Transparent containers")
                checked: root.draftTransparencyEnabled
                onToggled: (value) => {
                    root.draftTransparencyEnabled = value
                    root.applySettingsNow()
                }
            }

            Q.Slider {
                Layout.fillWidth: true
                label: qsTr("Transparency")
                from: 0
                to: 100
                stepSize: 1
                showValue: true
                enabled: root.draftTransparencyEnabled
                value: root.draftTransparencyLevel * 100
                onMoved: (value) => {
                    root.draftTransparencyLevel = value / 100
                    root.queueSettingsApply()
                }
            }

            Q.Slider {
                Layout.fillWidth: true
                label: qsTr("Small radius")
                from: 0
                to: 24
                stepSize: 1
                showValue: true
                value: root.draftRadiusSmall
                onMoved: (value) => {
                    root.draftRadiusSmall = Math.round(value)
                    if (root.draftRadiusMedium < root.draftRadiusSmall)
                        root.draftRadiusMedium = root.draftRadiusSmall
                    if (root.draftRadiusLarge < root.draftRadiusMedium)
                        root.draftRadiusLarge = root.draftRadiusMedium
                    root.queueSettingsApply()
                }
            }

            Q.Slider {
                Layout.fillWidth: true
                label: qsTr("Medium radius")
                from: root.draftRadiusSmall
                to: 28
                stepSize: 1
                showValue: true
                value: root.draftRadiusMedium
                onMoved: (value) => {
                    root.draftRadiusMedium = Math.round(value)
                    if (root.draftRadiusLarge < root.draftRadiusMedium)
                        root.draftRadiusLarge = root.draftRadiusMedium
                    root.queueSettingsApply()
                }
            }

            Q.Slider {
                Layout.fillWidth: true
                label: qsTr("Large radius")
                from: root.draftRadiusMedium
                to: 32
                stepSize: 1
                showValue: true
                value: root.draftRadiusLarge
                onMoved: (value) => {
                    root.draftRadiusLarge = Math.round(value)
                    root.queueSettingsApply()
                }
            }
        }
    }

    Component {
        id: layoutPageComponent

        ColumnLayout {
            width: pageLoader.width
            spacing: 6

            Text {
                text: qsTr("Browsing")
                color: Theme.accent
                font.pointSize: Theme.fontSmall
                font.bold: true
                Layout.bottomMargin: 4
            }

            Q.Checkbox {
                label: qsTr("Show hidden files")
                checked: root.draftShowHidden
                onToggled: (value) => {
                    root.draftShowHidden = value
                    root.applySettingsNow()
                }
            }

            AnchoredDropdown {
                Layout.fillWidth: true
                label: qsTr("Default view")
                model: root.viewLabels
                currentIndex: Math.max(0, root.viewValues.indexOf(root.draftDefaultView))
                onSelected: (index, _) => {
                    root.draftDefaultView = root.viewValues[index]
                    root.applySettingsNow()
                }
            }

            Q.Toggle {
                Layout.fillWidth: true
                label: qsTr("Remember view per folder")
                checked: root.draftRememberViewPerFolder
                onToggled: (value) => {
                    root.draftRememberViewPerFolder = value
                    root.applySettingsNow()
                }
            }

            Q.Toggle {
                Layout.fillWidth: true
                label: qsTr("Show sidebar")
                checked: root.draftSidebarVisible
                onToggled: (value) => {
                    root.draftSidebarVisible = value
                    root.applySettingsNow()
                }
            }

            Q.Toggle {
                Layout.fillWidth: true
                label: qsTr("Sidebar on right")
                enabled: root.draftSidebarVisible
                checked: root.draftSidebarPosition === "right"
                onToggled: (value) => {
                    root.draftSidebarPosition = value ? "right" : "left"
                    root.applySettingsNow()
                }
            }

            Q.Slider {
                Layout.fillWidth: true
                label: qsTr("Sidebar width")
                from: 160
                to: 480
                stepSize: 10
                showValue: true
                enabled: root.draftSidebarVisible
                value: root.draftSidebarWidth
                onMoved: (value) => {
                    root.draftSidebarWidth = Math.round(value)
                    root.queueSettingsApply()
                }
            }

            Q.Slider {
                Layout.fillWidth: true
                label: qsTr("Grid spacing")
                from: 0
                to: 48
                stepSize: 2
                showValue: true
                value: root.draftGridSpacing
                onMoved: (value) => {
                    root.draftGridSpacing = Math.round(value)
                    root.queueSettingsApply()
                }
            }

            Text {
                text: qsTr("Startup")
                color: Theme.accent
                font.pointSize: Theme.fontSmall
                font.bold: true
                Layout.topMargin: 12
                Layout.bottomMargin: 4
            }

            AnchoredDropdown {
                Layout.fillWidth: true
                label: qsTr("Open on launch")
                model: root.startupLocationLabels
                currentIndex: Math.max(0, root.startupLocationValues.indexOf(root.draftStartupLocation))
                onSelected: (index, _) => {
                    root.draftStartupLocation = root.startupLocationValues[index]
                    root.applySettingsNow()
                }
            }

            Text {
                text: qsTr("Sorting")
                color: Theme.accent
                font.pointSize: Theme.fontSmall
                font.bold: true
                Layout.topMargin: 12
                Layout.bottomMargin: 4
            }

            AnchoredDropdown {
                Layout.fillWidth: true
                label: qsTr("Default sort")
                model: root.sortByLabels
                currentIndex: Math.max(0, root.sortByValues.indexOf(root.draftSortBy))
                onSelected: (index, _) => {
                    root.draftSortBy = root.sortByValues[index]
                    root.applySettingsNow()
                }
            }

            Q.Toggle {
                Layout.fillWidth: true
                label: qsTr("Ascending order")
                checked: root.draftSortAscending
                onToggled: (value) => {
                    root.draftSortAscending = value
                    root.applySettingsNow()
                }
            }

            Q.Toggle {
                Layout.fillWidth: true
                label: qsTr("Remember sort per folder")
                checked: root.draftRememberSortPerFolder
                onToggled: (value) => {
                    root.draftRememberSortPerFolder = value
                    root.applySettingsNow()
                }
            }

            Text {
                text: qsTr("Window Controls")
                color: Theme.accent
                font.pointSize: Theme.fontSmall
                font.bold: true
                Layout.topMargin: 12
                Layout.bottomMargin: 4
            }

            Q.Toggle {
                Layout.fillWidth: true
                label: qsTr("Show window controls")
                checked: root.draftShowWindowControls
                onToggled: (value) => {
                    root.draftShowWindowControls = value
                    root.applySettingsNow()
                }
            }

            Q.Toggle {
                Layout.fillWidth: true
                label: qsTr("Buttons on left")
                enabled: root.draftShowWindowControls
                checked: root._layoutParts.side === "left"
                onToggled: (value) => {
                    root.rebuildButtonLayout(
                        value ? "left" : "right",
                        root._layoutParts.hasClose,
                        root._layoutParts.hasMinimize,
                        root._layoutParts.hasMaximize
                    )
                }
            }

            Q.Checkbox {
                label: qsTr("Close button")
                enabled: root.draftShowWindowControls
                checked: root._layoutParts.hasClose
                onToggled: (value) => {
                    root.rebuildButtonLayout(root._layoutParts.side, value, root._layoutParts.hasMinimize, root._layoutParts.hasMaximize)
                }
            }

            Q.Checkbox {
                label: qsTr("Minimize button")
                enabled: root.draftShowWindowControls
                checked: root._layoutParts.hasMinimize
                onToggled: (value) => {
                    root.rebuildButtonLayout(root._layoutParts.side, root._layoutParts.hasClose, value, root._layoutParts.hasMaximize)
                }
            }

            Q.Checkbox {
                label: qsTr("Maximize button")
                enabled: root.draftShowWindowControls
                checked: root._layoutParts.hasMaximize
                onToggled: (value) => {
                    root.rebuildButtonLayout(root._layoutParts.side, root._layoutParts.hasClose, root._layoutParts.hasMinimize, value)
                }
            }

            Rectangle {
                Layout.fillWidth: true
                implicitHeight: 54
                radius: Theme.radiusMedium
                color: Theme.containerColor(Theme.surface, 0.22)
                border.width: 1
                border.color: root.sectionBorderColor
                opacity: root.draftShowWindowControls ? 1 : 0.6

                RowLayout {
                    anchors.fill: parent
                    anchors.margins: 12
                    spacing: 8

                    RowLayout {
                        visible: root._layoutParts.side === "left"
                        spacing: 6

                        Rectangle { visible: root._layoutParts.hasMinimize; width: 12; height: 12; radius: 6; color: Theme.warning }
                        Rectangle { visible: root._layoutParts.hasMaximize; width: 12; height: 12; radius: 6; color: Theme.success }
                        Rectangle { visible: root._layoutParts.hasClose; width: 12; height: 12; radius: 6; color: Theme.error }
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        height: 6
                        radius: 3
                        color: Qt.rgba(Theme.text.r, Theme.text.g, Theme.text.b, 0.08)
                    }

                    RowLayout {
                        visible: root._layoutParts.side !== "left"
                        spacing: 6

                        Rectangle { visible: root._layoutParts.hasMinimize; width: 12; height: 12; radius: 6; color: Theme.warning }
                        Rectangle { visible: root._layoutParts.hasMaximize; width: 12; height: 12; radius: 6; color: Theme.success }
                        Rectangle { visible: root._layoutParts.hasClose; width: 12; height: 12; radius: 6; color: Theme.error }
                    }
                }
            }
        }
    }

    Component {
        id: motionPageComponent

        ColumnLayout {
            width: pageLoader.width
            spacing: 6

            RowLayout {
                Layout.fillWidth: true
                Layout.bottomMargin: 8
                spacing: 12

                Text {
                    text: qsTr("Animations")
                    color: Theme.text
                    font.pointSize: Theme.fontNormal + 2
                    font.bold: true
                }

                Item { Layout.fillWidth: true }

                Q.Toggle {
                    label: ""
                    checked: root.draftAnimationsEnabled
                    onToggled: (value) => {
                        root.draftAnimationsEnabled = value
                        root.applySettingsNow()
                    }
                }
            }

            Q.Separator { Layout.bottomMargin: 8 }

            Text {
                text: qsTr("Timing")
                color: Theme.accent
                font.pointSize: Theme.fontSmall
                font.bold: true
                Layout.bottomMargin: 4
            }

            Q.Slider {
                Layout.fillWidth: true
                label: qsTr("Fast")
                from: 0
                to: 500
                stepSize: 10
                showValue: true
                enabled: root.draftAnimationsEnabled
                value: root.draftAnimDurationFast
                onMoved: (value) => {
                    root.draftAnimDurationFast = Math.round(value)
                    root.queueSettingsApply()
                }
            }

            Q.Slider {
                Layout.fillWidth: true
                label: qsTr("Normal")
                from: 0
                to: 1000
                stepSize: 10
                showValue: true
                enabled: root.draftAnimationsEnabled
                value: root.draftAnimDuration
                onMoved: (value) => {
                    root.draftAnimDuration = Math.round(value)
                    root.queueSettingsApply()
                }
            }

            Q.Slider {
                Layout.fillWidth: true
                label: qsTr("Slow")
                from: 0
                to: 1500
                stepSize: 10
                showValue: true
                enabled: root.draftAnimationsEnabled
                value: root.draftAnimDurationSlow
                onMoved: (value) => {
                    root.draftAnimDurationSlow = Math.round(value)
                    root.queueSettingsApply()
                }
            }

            Text {
                text: qsTr("Curves")
                color: Theme.accent
                font.pointSize: Theme.fontSmall
                font.bold: true
                Layout.topMargin: 12
                Layout.bottomMargin: 4
            }

            AnchoredDropdown {
                Layout.fillWidth: true
                label: qsTr("Enter")
                enabled: root.draftAnimationsEnabled
                model: root.curveOptions
                currentIndex: Math.max(0, root.curveOptions.indexOf(root.draftAnimCurveEnter))
                onSelected: (_, value) => {
                    root.draftAnimCurveEnter = value
                    root.applySettingsNow()
                }
            }

            AnchoredDropdown {
                Layout.fillWidth: true
                label: qsTr("Exit")
                enabled: root.draftAnimationsEnabled
                model: root.curveOptions
                currentIndex: Math.max(0, root.curveOptions.indexOf(root.draftAnimCurveExit))
                onSelected: (_, value) => {
                    root.draftAnimCurveExit = value
                    root.applySettingsNow()
                }
            }

            AnchoredDropdown {
                Layout.fillWidth: true
                label: qsTr("Transition")
                enabled: root.draftAnimationsEnabled
                model: root.curveOptions
                currentIndex: Math.max(0, root.curveOptions.indexOf(root.draftAnimCurveTransition))
                onSelected: (_, value) => {
                    root.draftAnimCurveTransition = value
                    root.applySettingsNow()
                }
            }
        }
    }

    Component {
        id: toolsPageComponent

        ColumnLayout {
            width: pageLoader.width
            spacing: 6

            Text {
                text: qsTr("Utilities")
                color: Theme.accent
                font.pointSize: Theme.fontSmall
                font.bold: true
                Layout.bottomMargin: 4
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 12

                Q.Button {
                    Layout.fillWidth: true
                    text: qsTr("Keyboard Shortcuts")
                    onClicked: root.openKeyboardShortcuts()
                }

                Q.Button {
                    Layout.fillWidth: true
                    text: qsTr("Connect to Network Location")
                    variant: "ghost"
                    onClicked: root.openRemoteConnect()
                }
            }
        }
    }

    Item {
        id: pageContainer
        anchors.fill: parent

        property real pageContentHeight: pageLoader.item ? pageLoader.item.implicitHeight : 0
        implicitHeight: Math.max(460, Math.min(pageContentHeight + 120, 640,
            (root.transientParent ? root.transientParent.height : 768) - 140))

        Rectangle {
            anchors.fill: parent
            color: Theme.containerColor(Theme.mantle, 0.9)
            border.width: 1
            border.color: root.sectionBorderColor

            Rectangle {
                id: closeButton
                z: 10
                anchors.top: parent.top
                anchors.right: parent.right
                anchors.topMargin: 8
                anchors.rightMargin: 8
                width: 28
                height: 28
                radius: Theme.radiusSmall
                color: closeHover.hovered
                    ? Qt.rgba(Theme.text.r, Theme.text.g, Theme.text.b, 0.1)
                    : "transparent"

                IconX {
                    anchors.centerIn: parent
                    size: 16
                    color: Theme.text
                }

                HoverHandler { id: closeHover }
                TapHandler { onTapped: root.closePanel() }
            }

            ColumnLayout {
                anchors.fill: parent
                spacing: 0

                RowLayout {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    spacing: 0

                    Rectangle {
                        visible: !root.compactNavigation
                        Layout.fillHeight: true
                        Layout.preferredWidth: 184
                        color: Theme.containerColor(Theme.crust, 0.96)

                        ColumnLayout {
                            anchors.fill: parent
                            anchors.margins: 8
                            anchors.topMargin: 16
                            spacing: 2

                            Row {
                                Layout.leftMargin: 12
                                Layout.bottomMargin: 12
                                spacing: 6

                                IconSettings {
                                    size: 16
                                    color: Theme.text
                                }

                                Text {
                                    text: qsTr("Settings")
                                    color: Theme.text
                                    font.pointSize: Theme.fontNormal + 1
                                    font.bold: true
                                }
                            }

                            Q.Tabs {
                                id: sideTabs
                                Layout.fillWidth: true
                                Layout.fillHeight: true
                                orientation: Qt.Vertical
                                model: root.sectionNavItems
                                labelRole: "title"
                                iconComponentRole: "iconComponent"
                                currentIndex: root.currentSectionIndex
                                sideTabHeight: 36
                                sideTabWidth: 168
                                onTabChanged: (index) => root.showSection(index)
                            }

                            Q.Button {
                                Layout.fillWidth: true
                                Layout.leftMargin: 12
                                Layout.rightMargin: 12
                                Layout.topMargin: 8
                                text: qsTr("Reset to Defaults")
                                variant: "ghost"
                                onClicked: resetConfirmDialog.open()
                            }
                        }
                    }

                    Q.Separator {
                        visible: !root.compactNavigation
                        orientation: Qt.Vertical
                        Layout.fillHeight: true
                    }

                    Item {
                        Layout.fillWidth: true
                        Layout.fillHeight: true

                        ColumnLayout {
                            anchors.fill: parent
                            anchors.margins: 20
                            spacing: 12

                            Q.Tabs {
                                id: compactSectionNav
                                visible: root.compactNavigation
                                Layout.fillWidth: true
                                model: root.sectionNavItems
                                labelRole: "title"
                                currentIndex: root.currentSectionIndex
                                onTabChanged: (index) => root.showSection(index)
                            }

                            Text {
                                text: root.sectionItems[root.currentSectionIndex].title
                                color: Theme.text
                                font.pointSize: Theme.fontLarge + 2
                                font.bold: true
                            }

                            Flickable {
                                id: contentFlick
                                Layout.fillWidth: true
                                Layout.fillHeight: true
                                clip: true
                                contentWidth: width
                                contentHeight: pageLoader.item ? pageLoader.item.implicitHeight : 0
                                boundsBehavior: Flickable.StopAtBounds
                                interactive: contentHeight > height

                                Loader {
                                    id: pageLoader
                                    width: contentFlick.width
                                    sourceComponent: root.currentSectionIndex === 0
                                        ? lookPageComponent
                                        : root.currentSectionIndex === 1
                                            ? layoutPageComponent
                                            : root.currentSectionIndex === 2
                                                ? motionPageComponent
                                                : toolsPageComponent
                                }
                            }
                        }
                    }
                }

            }
        }
    }

    // Guard the destructive reset behind an explicit confirmation so it can't be
    // triggered by an accidental click.
    Q.Dialog {
        id: resetConfirmDialog
        anchors.fill: parent
        title: qsTr("Reset to Defaults?")
        dialogWidth: 400
        z: 1200
        initialFocusItem: resetCancelButton

        onAccepted: root.resetToDefaults()

        // Kept as wrapping content (not the Card subtitle, which doesn't wrap and
        // would overflow the dialog, pushing the buttons outside it).
        Text {
            Layout.fillWidth: true
            text: qsTr("This restores every appearance, layout, and behavior setting to its default. Bookmarks and files are not affected.")
            color: Theme.subtext
            font.pointSize: Theme.fontSmall
            wrapMode: Text.WordWrap
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.topMargin: 8
            spacing: 10

            Item { Layout.fillWidth: true }

            Q.Button {
                id: resetCancelButton
                text: qsTr("Cancel")
                variant: "ghost"
                onClicked: resetConfirmDialog.reject()
            }

            Q.Button {
                text: qsTr("Reset")
                variant: "danger"
                onClicked: resetConfirmDialog.accept()
            }
        }
    }
}
