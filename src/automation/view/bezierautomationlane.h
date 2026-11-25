/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

#include <QQuickPaintedItem>
#include <QColor>
#include <QVariantList>
#include <QSet>
#include "au3-components/BezierAutomation.h"

namespace au::automation {
// UI-only item. The owning clip model applies editCommitted as ONE undo step.
// Painting reads the GUI snapshot synchronized by QQuickPaintedItem; it never
// reads a WaveClip or plugin on the scene-graph/audio threads.
class BezierAutomationLane : public QQuickPaintedItem
{
    Q_OBJECT
    Q_PROPERTY(QVariantList segments READ segments WRITE setSegments NOTIFY segmentsChanged)
    Q_PROPERTY(double viewStart READ viewStart WRITE setViewStart NOTIFY viewChanged)
    Q_PROPERTY(double viewEnd READ viewEnd WRITE setViewEnd NOTIFY viewChanged)
    Q_PROPERTY(double selectionStart MEMBER m_selectionStart)
    Q_PROPERTY(double selectionEnd MEMBER m_selectionEnd)
    Q_PROPERTY(QColor lineColor MEMBER m_color NOTIFY lineColorChanged)
    //! Ableton: a control that isn't automated yet shows a dashed line at its
    //! current value, without points; the first click on it starts the envelope.
    Q_PROPERTY(bool dashed MEMBER m_dashed NOTIFY lineColorChanged)
    //! Switches: each value holds until the next point, then jumps (Ableton's on/off steps).
    Q_PROPERTY(bool stepped MEMBER m_stepped NOTIFY lineColorChanged)
    //! GlideMix: the lowest value sits this far above the bottom, clear of the phrase ribbon.
    Q_PROPERTY(double bottomInset MEMBER m_bottomInset NOTIFY lineColorChanged)
    //! GlideMix: the transition pill's glow, filled from the line towards glowBase
    //! (a value: 0 = the bottom; 0.5 = the centre, for the Color line).
    Q_PROPERTY(bool glow MEMBER m_glow NOTIFY lineColorChanged)
    //! GlideMix 1.3.9: among sibling lanes that also set this, a press goes to the
    //! line nearest to it, whichever is drawn on top (volume vs the band lines).
    Q_PROPERTY(bool nearestWins MEMBER m_nearestWins NOTIFY lineColorChanged)
    //! GlideMix 1.3.9: the band lines' look -- a dark ring round the dots keeps them
    //! clear on a song of any colour (the line itself stays thin, as the volume's).
    Q_PROPERTY(double lineWidth MEMBER m_lineWidth NOTIFY lineColorChanged)
    Q_PROPERTY(bool outline MEMBER m_outline NOTIFY lineColorChanged)
    //! GlideMix 1.3.10 (MixMeister's band markers): press and drag on the line moves that stretch
    //! between its two points up or down; it never adds a point (keys B / M / T / C do).
    Q_PROPERTY(bool segmentDrag MEMBER m_segmentDrag NOTIFY lineColorChanged)
    //! GlideMix 1.3.10: every point drawn and grabbable, also on a flat stretch (MixMeister's markers).
    Q_PROPERTY(bool allPoints MEMBER m_allPoints NOTIFY lineColorChanged)
    //! GlideMix 1.3.10 (Chris; MixMeister's Control-click > Add ... Marker): right-click on the line
    //! itself (not a point) asks the owner for the line's menu (lineMenuRequested).
    Q_PROPERTY(bool lineMenu MEMBER m_lineMenu NOTIFY lineColorChanged)
    //! InTheMix: points land on the beat grid -- snapStep seconds apart, one of
    //! them at snapOrigin (lane time). 0: free. Shift held: free.
    Q_PROPERTY(double snapStep MEMBER m_snapStep NOTIFY lineColorChanged)
    Q_PROPERTY(double snapOrigin MEMBER m_snapOrigin NOTIFY lineColorChanged)
    Q_PROPERTY(double glowBase MEMBER m_glowBase NOTIFY lineColorChanged)
    //! GlideMix: Logic's look -- each drawn point shows its value beside it.
    //! "db": a volume line; labelGainMin/Max are the gain range of the owner's
    //! 0..1 mapping (ClipGainModel::gainFromNormalized, split at 0.7 = unity).
    //! "percent": an effect line, 0.8 = 100 % as in the Set value… box.
    //! Empty: no labels.
    Q_PROPERTY(QString labelKind MEMBER m_labelKind NOTIFY lineColorChanged)
    Q_PROPERTY(double labelGainMin MEMBER m_labelGainMin NOTIFY lineColorChanged)
    Q_PROPERTY(double labelGainMax MEMBER m_labelGainMax NOTIFY lineColorChanged)
    // Live drag feedback -- e.g. a floating dB readout while a point is being
    // dragged, the same job PolylinePlot's activePoint* properties did before
    // this component replaced it. Normalized 0..1, same as the values in
    // `segments`; a consumer that needs a real unit (dB, Hz, ...) converts it
    // itself, since only the consumer's own model knows that mapping.
    Q_PROPERTY(bool hasActivePoint READ hasActivePoint NOTIFY activePointChanged)
    Q_PROPERTY(double activePointX READ activePointX NOTIFY activePointChanged)
    Q_PROPERTY(double activePointY READ activePointY NOTIFY activePointChanged)
    Q_PROPERTY(double activePointValue READ activePointValue NOTIFY activePointChanged)
public:
    explicit BezierAutomationLane(QQuickItem* parent = nullptr);
    ~BezierAutomationLane() override;
    QVariantList segments() const;
    void setSegments(const QVariantList& data);
    double viewStart() const { return m_start; }
    double viewEnd() const { return m_end; }
    void setViewStart(double value);
    void setViewEnd(double value);
    bool hasActivePoint() const { return m_hasActivePoint; }
    double activePointX() const { return m_activePoint.x(); }
    double activePointY() const { return m_activePoint.y(); }
    double activePointValue() const { return m_activePointValue; }
    void paint(QPainter* painter) override;
    /// Removes the point, joining its two segments (one undoable edit). The
    /// clip's first and last points can't be removed.
    Q_INVOKABLE bool deletePoint(int node);
    /// GlideMix: a point's value (0..1, as in `segments`), and typing one in
    /// (one undoable edit). -1 for no such point.
    Q_INVOKABLE double pointValue(int node) const;
    Q_INVOKABLE bool setPointValue(int node, double value);
    /// GlideMix: the line from point `node` to the next -- S-curve or straight.
    Q_INVOKABLE bool isSCurve(int node) const;
    Q_INVOKABLE bool setSCurve(int node, bool sCurve);
    /// The point stays highlighted while its menu is open.
    Q_INVOKABLE void clearPointHighlight();
    /// GlideMix: what a click at x, y would grab: a point (its index), else -1.
    Q_INVOKABLE int nodeAt(double x, double y) const { return hitNode(QPointF(x, y)); }
    /// True when x, y is on a point or on the line.
    Q_INVOKABLE bool grabsAt(double x, double y) const { return hitNode(QPointF(x, y)) >= 0 || hitSegment(QPointF(x, y)) >= 0; }
    /// GlideMix: whether any editable lane on screen (a clip's volume or FX
    /// line) has a point at this scene position. The transition pill's lane
    /// is drawn over the clips and lets such a press through to the point.
    Q_INVOKABLE bool anyLaneNodeAtScene(double sceneX, double sceneY) const;
    /// GlideMix 1.3.10 (MixMeister's Add ... Marker): a point on the line at x, at the line's
    /// own height there (one undoable edit, editCommitted). False on an existing point / off the line.
    Q_INVOKABLE bool addPointAt(double x);
    //! InTheMix: a point at x with `value` (0..1), for a double-click away from the line.
    Q_INVOKABLE bool addPointAtValue(double x, double value);
    //! InTheMix: a point at x, at the height y (both in this lane) -- the lane's right-click menu.
    Q_INVOKABLE bool addPointAtXY(double x, double y) { return addPointAtValue(x, valueAt(y)); }
    /// GlideMix 1.3.10: whether the mouse pointer is over this lane now, and where (x), read from
    /// the pointer itself -- hover events go to the topmost line only.
    Q_INVOKABLE bool cursorInside() const;
    Q_INVOKABLE double cursorX() const;
    Q_INVOKABLE int nodeCount() const { return m_curve.segments.empty() ? 0 : static_cast<int>(m_curve.segments.size()) + 1; }
signals:
    void segmentsChanged();
    void viewChanged();
    void lineColorChanged();
    void editCommitted(const QVariantList& segments);
    /// Every mouse move of a drag, before editCommitted -- lets the owner apply
    /// the curve live (stock Audacity's waveform follows a dragged gain point).
    void editChanged(const QVariantList& segments);
    void editCancelled();
    void activePointChanged();
    /// GlideMix: right-click on a point -- the owner shows its menu
    /// ("Delete point") at x, y and calls deletePoint(node).
    void pointMenuRequested(int node, double x, double y);
    /// GlideMix 1.3.10: right-click on the line between points (lineMenu on) -- the owner shows
    /// "Add point here" and calls addPointAt(x).
    void lineMenuRequested(double x, double y);
protected:
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void mouseUngrabEvent() override;
    void hoverMoveEvent(QHoverEvent* event) override;
    void hoverLeaveEvent(QHoverEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    bool event(QEvent* event) override;
    void focusOutEvent(QFocusEvent* event) override;
private:

    // Keep extrema inside the clip instead of letting a 0 or 1 value
    // disappear into its boundary or selection outline.
    double drawableHeight() const;
    double valueAt(double y) const;
    QPointF map(clipautomation::Point point) const;
    /// Where a point's dot is drawn and grabbed: map(), with the clip's first
    /// and last points moved a little inside the clip.
    QPointF nodePosition(int node) const;
    double timeAt(double x) const;
    int hitSegment(QPointF position) const;
    int hitNode(QPointF position) const;
    //! How far `position` is from this line, if it would grab it (else infinity).
    double grabDistance(QPointF position) const;
    //! lineMenusOnly: compare with sibling lanes that have a line menu only (the band lines).
    bool nearerSiblingAt(QPointF position, bool lineMenusOnly = false) const;
    //! Right-click: whether a sibling lane has a point nearer than this lane's `node`.
    bool nearerSiblingNodeAt(QPointF position, int node) const;
    double nodeValue(int node) const;
    //! GlideMix: false for points that only repeat the flat line around them.
    bool nodeShown(int node) const;
    void cancel();
    //! The point Delete / Backspace would remove (just clicked or added; never the clip's edges), else -1.
    int keyTargetNode() const;
    //! GlideMix: the Shift-picked points, moved together from m_before.
    void movePicked(double dt, double dv);
    void setActivePoint(QPointF position, double value);
    void clearActivePoint();
    clipautomation::Curve m_curve;
    clipautomation::Curve m_before;
    double m_start{}, m_end{1};
    double m_selectionStart{}, m_selectionEnd{};
    QColor m_color{QStringLiteral("#F5BB42")};
    double m_bottomInset{10.0};
    bool m_glow = false;
    bool m_nearestWins = false;
    double m_lineWidth = 1.5;
    bool m_outline = false;
    bool m_segmentDrag = false;
    bool m_allPoints = false;
    bool m_lineMenu = false;
    double m_snapStep = 0.0;
    double m_snapOrigin = 0.0;
    bool m_freeSnap = false;   ///< Shift held in this gesture
    double snapTime(double t) const;
    bool m_segmentMoved = false;
    bool m_dragStarted = false;   ///< a line/shelf gesture went past the 4 px click threshold
    double m_glowBase = 0.0;
    QString m_labelKind;
    double m_labelGainMin = 0.0;
    double m_labelGainMax = 2.0;
    QString valueLabel(double value) const;
    bool m_dashed = false;
    bool m_stepped = false;
    QPointF m_press;
    QPointF m_hover;
    int m_segment{-1};
    int m_node{-1};
    bool m_bending{}, m_shelf{}, m_inserting{}, m_dragging{}, m_hoverPoint{};
    bool m_hasActivePoint{};
    int m_hoverNode{-1};
    int m_selectedNode{-1};   ///< the point whose menu is open
    QSet<int> m_picked;       ///< GlideMix: Shift-clicked points that move together
    int m_lastClickedNode{-1};
    bool m_groupDrag{};
    QPointF m_activePoint;
    double m_activePointValue{};
};
}
