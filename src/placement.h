#pragma once
#include <QList>
#include <QPoint>
#include <QRect>
#include <QSize>
#include <algorithm>
#include <optional>

// Preserve a pin on its own monitor. If that monitor has disappeared, use the
// foreground application's monitor and keep the overlay reachable.
inline QPoint overlayPosition(const QSize &size, const QRect &fallback,
                              const QList<QRect> &screens,
                              const std::optional<QPoint> &pin) {
    QRect area=fallback;
    QPoint position(fallback.x()+fallback.width()-size.width()-12,
                    fallback.y()+fallback.height()-size.height()-12);
    if(pin) {
        for(const QRect &screen:screens) {
            if(screen.contains(*pin)){area=screen;position=*pin;break;}
        }
    }
    const int right=area.x()+std::max(0,area.width()-size.width());
    const int bottom=area.y()+std::max(0,area.height()-size.height());
    return QPoint(std::clamp(position.x(),area.x(),right),
                  std::clamp(position.y(),area.y(),bottom));
}
