# JUCE GUI Patterns and Best Practices

Comprehensive guide to building user interfaces with JUCE.

## Component Basics

### Component Lifecycle

```cpp
class MyComponent : public juce::Component
{
public:
    MyComponent()
    {
        // 1. Create child components
        addAndMakeVisible(button);
        button.setButtonText("Click Me");
        button.onClick = [this] { handleButtonClick(); };

        addAndMakeVisible(slider);
        slider.setRange(0.0, 1.0);
        slider.onValueChange = [this] { handleSliderChange(); };
    }

    void paint(juce::Graphics& g) override
    {
        // 2. Draw custom graphics
        g.fillAll(juce::Colours::darkgrey);
        g.setColour(juce::Colours::white);
        g.drawText("Hello JUCE", getLocalBounds(), juce::Justification::centred);
    }

    void resized() override
    {
        // 3. Layout child components
        auto bounds = getLocalBounds();
        button.setBounds(bounds.removeFromTop(50).reduced(10));
        slider.setBounds(bounds.removeFromTop(100).reduced(10));
    }

private:
    void handleButtonClick()
    {
        DBG("Button clicked!");
    }

    void handleSliderChange()
    {
        DBG("Slider value: " << slider.getValue());
    }

    juce::TextButton button;
    juce::Slider slider;
};
```

---

## Layout Strategies

### 1. Manual Layout (Rectangle Methods)

Simple and intuitive for basic layouts:

```cpp
void resized() override
{
    auto bounds = getLocalBounds();

    // Remove sections from bounds
    header.setBounds(bounds.removeFromTop(60));
    footer.setBounds(bounds.removeFromBottom(40));
    sidebar.setBounds(bounds.removeFromLeft(200));

    // Remaining space
    content.setBounds(bounds);

    // With margins
    auto area = getLocalBounds().reduced(10);  // 10px margin all around
    title.setBounds(area.removeFromTop(30).reduced(5));
}
```

**Rectangle Helper Methods**:
- `removeFromTop/Bottom/Left/Right(int pixels)` - Remove and return section
- `reduced(int pixels)` - Reduce size equally on all sides
- `reduced(int horiz, int vert)` - Reduce with different values
- `withTrimmedTop/Bottom/Left/Right(int pixels)` - Remove without returning
- `proportionOfWidth/Height(float proportion)` - Get size as proportion
- `getCentreX/Y()`, `getRight()`, `getBottom()` - Get positions

### 2. FlexBox Layout

For flexible, responsive layouts:

```cpp
void resized() override
{
    juce::FlexBox fb;
    fb.flexDirection = juce::FlexBox::Direction::row;
    fb.justifyContent = juce::FlexBox::JustifyContent::spaceBetween;
    fb.alignItems = juce::FlexBox::AlignItems::center;

    // Add items with flex properties
    fb.items.add(juce::FlexItem(button1).withFlex(1).withMargin(5));
    fb.items.add(juce::FlexItem(button2).withFlex(2).withMargin(5));
    fb.items.add(juce::FlexItem(button3).withFlex(1).withMargin(5));

    fb.performLayout(getLocalBounds());
}
```

**FlexBox Options**:
- `flexDirection`: row, column, rowReverse, columnReverse
- `justifyContent`: flexStart, flexEnd, center, spaceBetween, spaceAround
- `alignItems`: flexStart, flexEnd, center, stretch
- `flexWrap`: noWrap, wrap, wrapReverse

**FlexItem Properties**:
- `withFlex(float)` - Flex grow factor
- `withWidth/Height(float)` - Fixed dimensions
- `withMinWidth/Height(float)` - Minimum size
- `withMaxWidth/Height(float)` - Maximum size
- `withMargin(float)` - Spacing around item
- `withOrder(int)` - Display order

### 3. Grid Layout

For grid-based layouts:

```cpp
void resized() override
{
    juce::Grid grid;

    using Track = juce::Grid::TrackInfo;
    using Fr = juce::Grid::Fr;

    // Define columns and rows
    grid.templateRows = { Track(Fr(1)), Track(Fr(2)), Track(Fr(1)) };
    grid.templateColumns = { Track(Fr(1)), Track(Fr(1)), Track(Fr(1)) };

    // Add items
    grid.items = {
        juce::GridItem(header).withArea(1, 1, 2, 4),  // Spans all columns
        juce::GridItem(sidebar).withArea(2, 1),
        juce::GridItem(content).withArea(2, 2, 3, 4),  // Spans 2 columns
        juce::GridItem(footer).withArea(3, 1, 4, 4)
    };

    grid.performLayout(getLocalBounds());
}
```

**Track sizing**:
- `Fr(float)` - Fractional unit (like CSS fr)
- `Px(float)` - Fixed pixels
- `auto` - Automatic sizing

---

## Custom Drawing with Graphics

### Basic Shapes

```cpp
void paint(juce::Graphics& g) override
{
    auto bounds = getLocalBounds().toFloat();

    // Fill background
    g.fillAll(juce::Colours::black);

    // Set color
    g.setColour(juce::Colours::white);

    // Draw shapes
    g.drawRect(bounds.reduced(10), 2.0f);  // 2px stroke
    g.fillRect(bounds.reduced(20));

    g.drawEllipse(bounds.reduced(30), 3.0f);
    g.fillEllipse(bounds.reduced(40));

    g.drawRoundedRectangle(bounds.reduced(50), 10.0f, 2.0f);  // 10px corner radius
    g.fillRoundedRectangle(bounds.reduced(60), 10.0f);

    // Draw line
    g.drawLine(10, 10, 100, 100, 2.0f);

    // Draw text
    g.setFont(20.0f);
    g.drawText("Hello", bounds, juce::Justification::centred);
}
```

### Gradients

```cpp
void paint(juce::Graphics& g) override
{
    auto bounds = getLocalBounds().toFloat();

    // Linear gradient
    juce::ColourGradient gradient(
        juce::Colours::blue, bounds.getTopLeft(),
        juce::Colours::red, bounds.getBottomRight(),
        false);  // false = linear, true = radial

    g.setGradientFill(gradient);
    g.fillRect(bounds);

    // Radial gradient
    juce::ColourGradient radial(
        juce::Colours::white, bounds.getCentre(),
        juce::Colours::black, bounds.getBottomRight(),
        true);  // true = radial

    radial.addColour(0.5, juce::Colours::yellow);  // Mid-point color
    g.setGradientFill(radial);
    g.fillEllipse(bounds.reduced(50));
}
```

### Paths

```cpp
void paint(juce::Graphics& g) override
{
    juce::Path path;

    // Start at point
    path.startNewSubPath(50, 50);

    // Draw lines
    path.lineTo(150, 50);
    path.lineTo(100, 150);
    path.closeSubPath();  // Close back to start

    // Draw curves
    path.quadraticTo(200, 100, 250, 150);  // Control point, end point
    path.cubicTo(250, 200, 300, 200, 300, 250);  // Two control points, end point

    // Draw arc
    path.addArc(100, 100, 200, 200,  // bounds
               0, juce::MathConstants<float>::pi,  // start/end angle
               true);  // withSegment

    // Stroke or fill
    g.setColour(juce::Colours::white);
    g.strokePath(path, juce::PathStrokeType(2.0f));
    g.fillPath(path);
}
```

### Images

```cpp
class ImageComponent : public juce::Component
{
public:
    ImageComponent()
    {
        // Load image
        image = juce::ImageCache::getFromFile(juce::File("/path/to/image.png"));
    }

    void paint(juce::Graphics& g) override
    {
        // Draw image
        g.drawImage(image, getLocalBounds().toFloat());

        // Draw with opacity
        g.setOpacity(0.5f);
        g.drawImage(image, getLocalBounds().toFloat());

        // Draw transformed
        juce::AffineTransform transform;
        transform = transform.rotation(juce::MathConstants<float>::pi / 4,
                                      getWidth() / 2.0f,
                                      getHeight() / 2.0f);
        g.drawImageTransformed(image, transform);

        // Draw portion of image
        g.drawImage(image,
                   0, 0, 100, 100,  // destination
                   50, 50, 200, 200);  // source
    }

private:
    juce::Image image;
};
```

---

## Look and Feel Customization

### Basic LookAndFeel Override

```cpp
class CustomLookAndFeel : public juce::LookAndFeel_V4
{
public:
    CustomLookAndFeel()
    {
        // Set default colors
        setColour(juce::Slider::thumbColourId, juce::Colours::red);
        setColour(juce::Slider::trackColourId, juce::Colours::blue);
    }

    void drawRotarySlider(juce::Graphics& g,
                         int x, int y, int width, int height,
                         float sliderPos,
                         float rotaryStartAngle, float rotaryEndAngle,
                         juce::Slider& slider) override
    {
        auto bounds = juce::Rectangle<int>(x, y, width, height).toFloat().reduced(10);
        auto radius = juce::jmin(bounds.getWidth(), bounds.getHeight()) / 2.0f;
        auto toAngle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);
        auto arcRadius = radius - 4.0f;

        // Draw arc background
        g.setColour(juce::Colours::darkgrey);
        juce::Path backgroundArc;
        backgroundArc.addCentredArc(bounds.getCentreX(), bounds.getCentreY(),
                                   arcRadius, arcRadius,
                                   0.0f,
                                   rotaryStartAngle, rotaryEndAngle,
                                   true);
        g.strokePath(backgroundArc, juce::PathStrokeType(4.0f));

        // Draw arc value
        g.setColour(juce::Colours::orange);
        juce::Path valueArc;
        valueArc.addCentredArc(bounds.getCentreX(), bounds.getCentreY(),
                              arcRadius, arcRadius,
                              0.0f,
                              rotaryStartAngle, toAngle,
                              true);
        g.strokePath(valueArc, juce::PathStrokeType(4.0f));

        // Draw pointer
        juce::Path pointer;
        auto pointerLength = radius * 0.6f;
        auto pointerThickness = 3.0f;
        pointer.addRectangle(-pointerThickness * 0.5f, -radius,
                            pointerThickness, pointerLength);
        pointer.applyTransform(juce::AffineTransform::rotation(toAngle)
                              .translated(bounds.getCentreX(), bounds.getCentreY()));
        g.setColour(juce::Colours::white);
        g.fillPath(pointer);
    }
};

// Usage in component:
class MyComponent : public juce::Component
{
public:
    MyComponent()
    {
        setLookAndFeel(&customLF);
        // ...
    }

    ~MyComponent()
    {
        setLookAndFeel(nullptr);  // IMPORTANT: Clean up
    }

private:
    CustomLookAndFeel customLF;
};
```

---

## Animation and Timers

### Timer for Periodic Updates

```cpp
class AnimatedComponent : public juce::Component,
                         private juce::Timer
{
public:
    AnimatedComponent()
    {
        startTimerHz(60);  // 60 FPS
    }

    void timerCallback() override
    {
        // Update animation state
        angle += 0.05f;
        if (angle > juce::MathConstants<float>::twoPi)
            angle -= juce::MathConstants<float>::twoPi;

        repaint();
    }

    void paint(juce::Graphics& g) override
    {
        // Draw animated content
        g.fillAll(juce::Colours::black);

        auto bounds = getLocalBounds().toFloat();
        auto transform = juce::AffineTransform::rotation(angle,
                                                        bounds.getCentreX(),
                                                        bounds.getCentreY());

        juce::Path shape;
        shape.addRectangle(bounds.reduced(50));
        shape.applyTransform(transform);

        g.setColour(juce::Colours::white);
        g.fillPath(shape);
    }

private:
    float angle = 0.0f;
};
```

### Value Smoothing (Animated Values)

```cpp
class SmoothedComponent : public juce::Component,
                         private juce::Timer
{
public:
    SmoothedComponent()
    {
        targetValue = 0.0f;
        currentValue.reset(44100.0, 0.05);  // Sample rate, ramp time
        currentValue.setCurrentAndTargetValue(0.0f);

        startTimerHz(60);
    }

    void setValue(float newValue)
    {
        targetValue = newValue;
        currentValue.setTargetValue(newValue);
    }

    void timerCallback() override
    {
        if (currentValue.isSmoothing())
        {
            currentValue.skip(44100 / 60);  // Advance smoothing
            repaint();
        }
    }

private:
    float targetValue;
    juce::LinearSmoothedValue<float> currentValue;
};
```

---

## Plugin Editor Patterns

### Basic Plugin Editor

```cpp
class MyPluginEditor : public juce::AudioProcessorEditor
{
public:
    MyPluginEditor(MyAudioProcessor& p)
        : AudioProcessorEditor(&p),
          audioProcessor(p),
          gainAttachment(p.apvts, "gain", gainSlider),
          frequencyAttachment(p.apvts, "frequency", frequencySlider)
    {
        // Configure slider
        addAndMakeVisible(gainSlider);
        gainSlider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        gainSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 80, 20);

        addAndMakeVisible(frequencySlider);
        frequencySlider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        frequencySlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 80, 20);

        // Set size
        setSize(400, 300);
        setResizable(true, true);
        setResizeLimits(300, 200, 800, 600);
    }

    void paint(juce::Graphics& g) override
    {
        g.fillAll(getLookAndFeel().findColour(
            juce::ResizableWindow::backgroundColourId));
    }

    void resized() override
    {
        auto bounds = getLocalBounds().reduced(20);

        juce::FlexBox fb;
        fb.flexDirection = juce::FlexBox::Direction::row;
        fb.justifyContent = juce::FlexBox::JustifyContent::spaceAround;

        fb.items.add(juce::FlexItem(gainSlider).withMinWidth(100).withMinHeight(100));
        fb.items.add(juce::FlexItem(frequencySlider).withMinWidth(100).withMinHeight(100));

        fb.performLayout(bounds);
    }

private:
    MyAudioProcessor& audioProcessor;

    juce::Slider gainSlider;
    juce::Slider frequencySlider;

    juce::AudioProcessorValueTreeState::SliderAttachment gainAttachment;
    juce::AudioProcessorValueTreeState::SliderAttachment frequencyAttachment;
};
```

---

## Advanced Patterns

### Custom Component with Mouse Interaction

```cpp
class DraggableComponent : public juce::Component
{
public:
    void mouseDown(const juce::MouseEvent& event) override
    {
        dragger.startDraggingComponent(this, event);
    }

    void mouseDrag(const juce::MouseEvent& event) override
    {
        dragger.dragComponent(this, event, nullptr);
    }

    void mouseEnter(const juce::MouseEvent&) override
    {
        isHovered = true;
        repaint();
    }

    void mouseExit(const juce::MouseEvent&) override
    {
        isHovered = false;
        repaint();
    }

    void paint(juce::Graphics& g) override
    {
        g.fillAll(isHovered ? juce::Colours::lightblue : juce::Colours::blue);
    }

private:
    juce::ComponentDragger dragger;
    bool isHovered = false;
};
```

### Level Meter

```cpp
class LevelMeter : public juce::Component,
                   private juce::Timer
{
public:
    LevelMeter()
    {
        startTimerHz(30);  // 30 FPS
    }

    void setLevel(float newLevel)
    {
        level = juce::jlimit(0.0f, 1.0f, newLevel);
    }

    void timerCallback() override
    {
        // Decay
        level *= 0.95f;
        repaint();
    }

    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();

        // Background
        g.setColour(juce::Colours::darkgrey);
        g.fillRect(bounds);

        // Level bar
        auto levelBounds = bounds.withWidth(bounds.getWidth() * level);

        // Color based on level
        if (level > 0.9f)
            g.setColour(juce::Colours::red);
        else if (level > 0.7f)
            g.setColour(juce::Colours::yellow);
        else
            g.setColour(juce::Colours::green);

        g.fillRect(levelBounds);
    }

private:
    float level = 0.0f;
};
```

---

## Performance Tips

1. **Minimize repaints**: Use `repaint(area)` for partial repaints
2. **Cache images**: Use `juce::Image` for complex drawings, redraw only when needed
3. **Avoid painting in Timer**: Set a flag and call `repaint()` instead
4. **Use Component::setBufferedToImage()**: For expensive painting
5. **Reduce Component hierarchy**: Fewer nested components = better performance
6. **Profile with Graphics Performance Tools**: Use Instruments (macOS) or similar

---

## Accessibility

```cpp
class AccessibleButton : public juce::TextButton
{
public:
    AccessibleButton()
    {
        // Set accessibility properties
        setTitle("Play Button");
        setDescription("Starts playback of the audio");
        setHelpText("Press this button or use spacebar to play");
    }

    std::unique_ptr<juce::AccessibilityHandler> createAccessibilityHandler() override
    {
        return std::make_unique<juce::AccessibilityHandler>(
            *this,
            juce::AccessibilityRole::button,
            juce::AccessibilityActions().addAction(
                juce::AccessibilityActionType::press,
                [this] { triggerClick(); }));
    }
};
```
