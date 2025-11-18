# Advanced Automation

Complete guide to automation modes, automation curves, and advanced parameter control in Tracktion Engine.

## Automation Modes (v3.2+)

Automation modes control how parameter automation interacts with live edits:

### Available Modes

```cpp
// Set automation mode for parameter
param->setAutomationMode(te::AutomationMode::read);    // Play automation
param->setAutomationMode(te::AutomationMode::write);   // Overwrite automation
param->setAutomationMode(te::AutomationMode::touch);   // Write while touching
param->setAutomationMode(te::AutomationMode::latch);   // Write after touching

// Get current mode
auto mode = param->getAutomationMode();
```

### Mode Behaviors

**Read Mode**:
- Plays back existing automation
- No writing/overwriting
- Default mode

**Write Mode**:
- Overwrites automation while playing
- Creates new automation curve
- Destructive to existing data

**Touch Mode**:
- Writes automation only while parameter is being changed
- Returns to read mode when released
- Non-destructive to untouched sections

**Latch Mode**:
- Writes automation after first touch
- Continues writing until playback stops
- Useful for live performance

### Using Automation Modes

```cpp
// Enable automation mode for live control
param->setAutomationActive(true);
param->setAutomationMode(te::AutomationMode::touch);

// In UI: when slider is being dragged
slider.onDragStart = [param]
{
    param->beginParameterChangeGesture();
};

slider.onValueChange = [param, &slider]
{
    param->setParameter(slider.getValue(), juce::sendNotification);
    // Automation curve updates in touch/write mode
};

slider.onDragEnd = [param]
{
    param->endParameterChangeGesture();
};
```

## Automation Curve Bypass (v3.2)

```cpp
// Temporarily bypass automation without deleting
auto& curve = param->getCurve();
curve.setBypass(true);   // Ignore automation
curve.setBypass(false);  // Use automation

// Check bypass state
bool isBypassed = curve.isBypassed();
```

## Advanced Curve Manipulation

### Creating Complex Curves

```cpp
auto& curve = param->getCurve();

// Add automation points
curve.addPoint(0.0, 0.0, 0.5);   // time, value, curvature
curve.addPoint(1.0, 1.0, 0.5);
curve.addPoint(2.0, 0.5, 0.0);   // Sharp corner (0.0 curvature)

// Remove points
curve.removePoint(pointIndex);
curve.clearAllPoints();

// Move points
curve.movePoint(pointIndex, newTime, newValue);
curve.setPointCurvature(pointIndex, 0.8);  // Smooth curve

// Get value at time
float value = curve.getValueAt(timeInSeconds);
```

### Curve Interpolation

```cpp
// Different curve shapes via curvature
curve.addPoint(time, value, 0.0);   // Linear (sharp)
curve.addPoint(time, value, 0.5);   // Smooth S-curve
curve.addPoint(time, value, 1.0);   // Very smooth

// Custom interpolation
for (int i = 0; i <= 10; ++i)
{
    double t = i / 10.0;
    double time = startTime + t * duration;
    float value = customFunction(t);  // Your interpolation function

    curve.addPoint(time, value, 0.5);
}
```

## Automation Recording

### Record Automation During Playback

```cpp
// Setup for automation recording
param->setAutomationMode(te::AutomationMode::write);
param->setAutomationActive(true);

// Start playback
edit.getTransport().play(false);

// Changes to parameter are automatically recorded
param->setParameter(newValue, juce::sendNotification);

// Stop playback - automation is saved
edit.getTransport().stop(false, false);
```

### Punch-In Automation Recording

```cpp
// Set punch range
auto& transport = edit.getTransport();
transport.setLoopRange({ punchInTime, punchOutTime });

// Use touch mode for punch recording
param->setAutomationMode(te::AutomationMode::touch);

// Start playback
transport.play(false);

// Automation writes only during punch range when touching parameter
```

## Automation List Management

### Accessing All Automation

```cpp
// Get all automatable parameters in edit
juce::Array<te::AutomatableParameter*> allParams;

for (auto track : edit.getAllTracks())
{
    for (auto plugin : track->pluginList)
    {
        auto params = plugin->getAutomatableParameters();
        allParams.addArray(params);
    }
}

// Process each parameter's automation
for (auto param : allParams)
{
    auto& curve = param->getCurve();

    if (curve.getNumPoints() > 0)
    {
        // Has automation
        processAutomation(curve);
    }
}
```

### Copying/Pasting Automation

```cpp
// Copy automation from one parameter to another
void copyAutomation(te::AutomatableParameter& source,
                   te::AutomatableParameter& dest)
{
    auto& sourceCurve = source.getCurve();
    auto& destCurve = dest.getCurve();

    destCurve.clearAllPoints();

    for (int i = 0; i < sourceCurve.getNumPoints(); ++i)
    {
        double time = sourceCurve.getPointTime(i);
        float value = sourceCurve.getPointValue(i);
        float curve = sourceCurve.getPointCurvature(i);

        destCurve.addPoint(time, value, curve);
    }
}
```

### Scaling/Offsetting Automation

```cpp
// Scale automation values
void scaleAutomation(te::AutomatableParameter& param, float scale)
{
    auto& curve = param->getCurve();

    for (int i = 0; i < curve.getNumPoints(); ++i)
    {
        float value = curve.getPointValue(i);
        float time = curve.getPointTime(i);
        float curvature = curve.getPointCurvature(i);

        curve.removePoint(i);
        curve.addPoint(time, value * scale, curvature);
    }
}

// Offset automation in time
void offsetAutomation(te::AutomatableParameter& param, double timeOffset)
{
    auto& curve = param->getCurve();

    // Collect points
    std::vector<Point> points;
    for (int i = 0; i < curve.getNumPoints(); ++i)
    {
        points.push_back({
            curve.getPointTime(i) + timeOffset,
            curve.getPointValue(i),
            curve.getPointCurvature(i)
        });
    }

    // Rebuild curve
    curve.clearAllPoints();
    for (auto& p : points)
        curve.addPoint(p.time, p.value, p.curvature);
}
```

## Automation Modifiers Integration

### Combining Automation with Modifiers

```cpp
// Base automation curve
param->setAutomationActive(true);
auto& curve = param->getCurve();
curve.addPoint(0.0, 0.5, 0.5);
curve.addPoint(4.0, 0.5, 0.5);

// Add LFO modifier on top
auto lfo = new te::LFOModifier(*param);
lfo->rate = 4.0;      // 4 Hz
lfo->depth = 0.2;     // 20% modulation
lfo->shape = te::LFOModifier::Shape::sine;

param->addModifier(lfo);

// Result: Base automation (0.5) + LFO modulation (±0.2)
```

### Stacking Multiple Modifiers

```cpp
// Base value from automation
param->setAutomationActive(true);

// Modifier 1: LFO
auto lfo = new te::LFOModifier(*param);
lfo->depth = 0.1;
param->addModifier(lfo);

// Modifier 2: Random
auto random = new te::RandomModifier(*param);
random->depth = 0.05;
param->addModifier(random);

// Final value = automation + LFO + random
```

## Automation Listeners

### Monitoring Automation Changes

```cpp
struct AutomationListener : te::AutomatableParameter::Listener
{
    void currentValueChanged(te::AutomatableParameter& param,
                           float newValue) override
    {
        // Value changed (by automation, modifier, or user)
        updateUI(newValue);
    }

    void curveChanged(te::AutomatableParameter& param) override
    {
        // Automation curve edited
        redrawAutomationLane();
    }
};

param->addListener(myListener);
```

## Best Practices

1. **Mode Selection**: Use touch mode for live performance recording
2. **Curve Cleanup**: Remove redundant points for efficiency
3. **Bypass vs Delete**: Use bypass to A/B test automation
4. **Gesture Boundaries**: Always use begin/endParameterChangeGesture()
5. **Thread Safety**: Modify automation curves on message thread only
6. **Undo Support**: Pass UndoManager when adding/modifying points
7. **Performance**: Avoid excessive automation points (< 100 per second)

## Common Patterns

### Smooth Fade

```cpp
void createFade(te::AutomatableParameter& param,
               double startTime, double endTime,
               float startValue, float endValue)
{
    auto& curve = param.getCurve();
    curve.addPoint(startTime, startValue, 0.7);  // Smooth curve
    curve.addPoint(endTime, endValue, 0.7);
}
```

### Stepped Automation

```cpp
void createSteps(te::AutomatableParameter& param,
                double startTime, int numSteps, double stepDuration)
{
    auto& curve = param.getCurve();

    for (int i = 0; i < numSteps; ++i)
    {
        double time = startTime + i * stepDuration;
        float value = (i % 2 == 0) ? 0.0f : 1.0f;

        curve.addPoint(time, value, 0.0);  // Sharp steps (0.0 curvature)
    }
}
```

### Envelope-Based Automation

```cpp
void createADSR(te::AutomatableParameter& param,
               double startTime,
               double attack, double decay, float sustain, double release)
{
    auto& curve = param.getCurve();

    curve.addPoint(startTime, 0.0f, 0.5);
    curve.addPoint(startTime + attack, 1.0f, 0.5);
    curve.addPoint(startTime + attack + decay, sustain, 0.5);
    curve.addPoint(startTime + attack + decay + release, 0.0f, 0.5);
}
```
