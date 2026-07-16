# Pending changelog ledger

One bullet per merged user-relevant change, staged by the `plan-progress` skill at
phase/milestone completion — while the knowledge is fresh. **Drained by `release-process` §0**
into the real lanes (A: root CHANGELOG.md · B: `changelog.mjs add` · C: Discord) and then
EMPTIED. Bullets that never reach a lane are how release notes go missing — drain, don't append
around.

**Format:** `- [lane A|B|C] [artifact] one-line summary (PR #n / sha)`

<!-- Drained 2026-07-16 for the 0.4.0 release (plugin 0.4.0 + extensions 0.2.0): every bullet
     verified present in Lane A ([0.2.0]/[0.3.0]/[0.4.0] sections), Lane B (changelog.json
     0.1.x/0.2.0), or Lane C (DISCORD_CHANGELOG [0.4.0]). -->
- [lane A] [plugin] Widget-completion wave 0+1 (0.5.0): 8 new widgets (ColorBlock/SimpleGradient/ComplexGradient/Hyperlink/EnableBox/ScissorRectBox/BackgroundBlur/InvalidationPanel — 38 tags), universal ToolTipText style key (P1), Slot builder ZOrder/Position/Size + Style builder Clipping/ToolTipText + check-style-builders CI gate (TD-013), Contract.AdapterMasks registry meta-gate over ForEachAdapter (TD-011), Button bIsFocusable + ProgressBar BarFillType (TD-012 riders) (plan/widget-completion)
- [lane C] [community] Wave 1 of "1.0 wraps every official widget": 8 new tags, tooltips on any element, and the anti-drift rails for the ~30 widgets still coming (plan/widget-completion)
- [lane A] [plugin] Widget-completion wave 2 (0.6.0): 12 widgets (EditableText/InlineEditableTextBlock/VirtualKeyboardEntry/ColorWheel/ColorSpectrum/ColorGradingWheel(+AdvancedWidgets dep)/InputKeySelector/VolumeControl/RadialBox/TextScroller/LayeredImage/ExpandableButton) — 50 tags; UseImperativeHandle ref-publishing overload + WidgetFromHandle<T> (P2); TD-012 sweep (Slider/ScrollBox setters, TextBlock LineHeightPercentage+OverflowPolicy — 16 style keys) (plan/widget-completion)
- [lane A] [plugin] Widget-completion wave 3 (0.7.0): 10 protocol widgets (ConstraintCanvas P5a / Splitter P5b / MenuAnchor P3 / NotificationList+PushNotification P4 / NumericDropDown / BreadcrumbTrail / LinkedBox / WindowTitleBarArea / VirtualJoystick / SearchableComboBox sinceUE-5.7) — 60 tags, 16 slot keys, sinceUE schema annotation (plan/widget-completion)
- [lane A] [plugin] Widget-completion wave 4 (0.8.0): TreeView (TD-022 closed — sub-roots, GetChildren accessor, controlled expansion, HeaderRow columns P5c) + VectorInputBox/RotatorInputBox + Splitter2x2 (D-W4 role quadrants) — 63 tags; Batch3c mount suite (battery 126); ConstraintCanvas attach-then-apply slot fix (plan/widget-completion)
- [lane C] [community] Wave 4/0.8.0: TreeView lands, widget-completion plan fully implemented — 63 tags, 65+ wrapped widgets (plan/widget-completion)
