# TODO

## UX/UI Design for a DAW

### Some UX features for discussion

#### Navigation

- Horizontal scrolling
  - via trackpad/mouse wheel, keyboard (mod +/-)
  - With inertia?
- Scroll bars with adjustable thumb size reflecting zoom level
- Overview/minimap strip above timeline showing entire project length
- Timeline markers at key points and regions
- Ruler showing time/bars/subdiv/frames at top
  - Click on rules places the cursor (Davinci)
- Keyboard shortcuts for start/end/markers (Home/End/number keys)
- Middle-mouse drag for quick scrolling

#### Zooming

- Alt/Option/Cmd + scroll wheel for horizontal zoom
- Shift + scroll wheel for vertical track height
- Zoom to selection (Z key)
- Zoom to fit entire project (Alt/Option + F, Shift-Z)
- Key commands for zoom in/out (+ and -)
- Zoom slider in tool/status bar
- Double-click timeline ruler to toggle between zoom levels

#### Selection

- Click and drag in timeline for time range selection
- Shift/Cmd + click to extend selection
- Double-click clip to select whole clip
- Alt + click for split/razor tool
- Click empty space to deselect
- Marquee selection for multiple clips
- Selection sync between arrangement and clip editors

#### Smart Behaviors

- Snap to grid/other clips (toggleable)
- Auto-scroll during playback
- Zoom follows playhead option
- Track folding/grouping
- Preview thumbnails

#### Multi-touch gestures (if supported)

- Pinch to zoom
- Two-finger scroll
- Three-finger swipe for project navigation

#### Context menus

- Right-click for clip operations
- Right-click ruler for time signature/tempo changes, ruler options, placing markers, adding time signature/tempo changes
- Right-click track header for track operations

#### The key principles are

- Multiple ways to achieve the same task (key shortcuts, menu, tools, popup menus, dragging)
- Consistent behavior across tools
- Quick access to common operations
- Precise control when needed
- Clear visual feedback
- Flexible but predictable snap behavior, not way too magnetic

#### UX in similar Software

#### Ableton Live
#### Cavalry
#### Waveform
#### Reaper

##### Davinci Resolve

- SMPT time is from the left of the ruler, not in transport controls
- Transport bar
  - Buttons: Go to Prev Clip, Play reverse, Stop (stop and go to last pos option), Play, Go to Next Clip, Loop
- Toolbar:
  - Tools:
    - Selection (arrow, A)
    - Trim Edit (T) — moves clip content within the clip extends, not moving clip edit positions
    - Dynamic Trim Mode (slip/slide, W)
    - Blade edit (blade, B)
  - Icons Switchers: Snap (magnet), Linked selection (chain), Position lock (lock)
  - Markers: Flags (G, marks clips only), Markers (M, places timeline marker on the ruler)
  - Zoom: Full extent zoom (Shift-Z), Detail zoom, Custom zoom, Zoom in (Cmd+=), Zoom slider, Zoom out (Cmd+-)
- Navigation keys
  - Move to beginning/end — Home/End, Fn-Left/Right
  - Select prev/next clip — Cmd+Left/Right
  - Prev/Next frame — Left/Right
  - Cmd+=/- — Horizontal zoom in/out
- At the ruler
  - Left click — places the cursor here
  - Right click — ruler popup menu
  - Mouse wheel — do nothing
  - Shift-mouse wheel — do nothing
  - Alt-wheel — zoom horizontally around mouse pos
  - Cmd-wheel — scroll horizontally
  - Wheel drug — no nothing (silly, must scroll horizontally as in tracks)
- At a track
  - Left click — selects clip
  - Right click — clip popup menu
  - Mouse wheel (no mod) — scrolls the thumbnail of the track vertically
  - Shift-mouse wheel — zooms track height
  - Alt-wheel — zoom horizontally  around mouse pos
  - Cmd-wheel — scroll horizontally
  - Wheel drug — scroll horizontally
  - Left-click dragging — move a clip around
- Between tracks
  - Left-click dragging scrolls tracks vertically
