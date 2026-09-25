// SPDX-License-Identifier: GPL-3.0-or-later
#include "presentation/LinkLayer.hpp"

#include <QLineF>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <algorithm>
#include <cmath>

namespace wirerunner {
namespace {
QPointF bezier(QPointF a, QPointF b, double t) {
  const double handle = std::max(70.0, std::abs(b.x() - a.x()) * 0.42);
  const QPointF c1(a.x() + handle, a.y());
  const QPointF c2(b.x() - handle, b.y());
  const double u = 1.0 - t;
  return a * (u*u*u) + c1 * (3*u*u*t) + c2 * (3*u*t*t) + b * (t*t*t);
}
QPainterPath linkPath(QPointF a, QPointF b) {
  const double handle = std::max(70.0, std::abs(b.x() - a.x()) * 0.42);
  QPainterPath path(a);
  path.cubicTo({a.x() + handle, a.y()}, {b.x() - handle, b.y()}, b);
  return path;
}
QColor linkColor(const QString &media, bool focused) {
  QColor color = media == QStringLiteral("video") ? QColor("#e6b765")
    : media == QStringLiteral("midi") ? QColor("#a893f5") : QColor("#68d18b");
  color.setAlphaF(focused ? 0.86F : 0.12F);
  return color;
}
double segmentDistance(QPointF point, QPointF a, QPointF b) {
  const QPointF segment = b-a;
  const double length = QPointF::dotProduct(segment,segment);
  if (length == 0.0) return QLineF(point,a).length();
  const double position = std::clamp(QPointF::dotProduct(point-a,segment)/length,0.0,1.0);
  return QLineF(point,a+segment*position).length();
}
QHash<QString, QPointF> anchorPoints(const QVariantList &anchors) {
  QHash<QString,QPointF> points;
  for(const auto &value:anchors){const auto anchor=value.toMap();points.insert(anchor.value("key").toString(),{anchor.value("x").toDouble(),anchor.value("y").toDouble()});}
  return points;
}
}

LinkLayer::LinkLayer(QQuickItem *parent) : QQuickPaintedItem(parent) {
  setAntialiasing(true);
  setAcceptedMouseButtons(Qt::LeftButton);
}
void LinkLayer::setPortAnchors(QVariantList value) { portAnchors_=std::move(value); emit portAnchorsChanged(); update(); }
void LinkLayer::setLinks(QVariantList value) { links_=std::move(value); emit linksChanged(); update(); }
void LinkLayer::setBlockers(QVariantList value) { blockers_=std::move(value); emit blockersChanged(); update(); }
void LinkLayer::setViewScale(double value) { if(qFuzzyCompare(viewScale_,value))return;viewScale_=value;emit viewTransformChanged();update(); }
void LinkLayer::setContentX(double value) { if(qFuzzyCompare(contentX_,value))return;contentX_=value;emit viewTransformChanged();update(); }
void LinkLayer::setContentY(double value) { if(qFuzzyCompare(contentY_,value))return;contentY_=value;emit viewTransformChanged();update(); }
void LinkLayer::setMediaFilter(QString value) { if(mediaFilter_==value)return; mediaFilter_=std::move(value); emit mediaFilterChanged(); update(); }
void LinkLayer::setSelectedKey(QString value) { if(selectedKey_==value)return; selectedKey_=std::move(value); emit selectedKeyChanged(); update(); }

void LinkLayer::paint(QPainter *painter) {
  const auto graphPoints = anchorPoints(portAnchors_);
  QHash<QString,QPointF> points;
  for (auto point = graphPoints.cbegin(); point != graphPoints.cend(); ++point)
    points.insert(point.key(), {point->x()*viewScale_-contentX_, point->y()*viewScale_-contentY_});
  painter->setRenderHint(QPainter::Antialiasing, true);
  QRegion visibleRegion(QRect(0, 0, static_cast<int>(width()), static_cast<int>(height())));
  for (const auto &value : blockers_) {
    const auto blocker = value.toMap();
    const QRect rect(static_cast<int>(blocker.value("x").toDouble()*viewScale_-contentX_),
      static_cast<int>(blocker.value("y").toDouble()*viewScale_-contentY_),
      static_cast<int>(blocker.value("width").toDouble()*viewScale_),
      static_cast<int>(blocker.value("height").toDouble()*viewScale_));
    visibleRegion -= rect.adjusted(1, 1, -1, -1);
  }
  painter->setClipRegion(visibleRegion);
  for (const auto &value : links_) {
    const auto link=value.toMap();
    const auto media=link.value("media").toString();
    if(mediaFilter_!="all" && mediaFilter_!=media) continue;
    const auto output=link.value("outputAnchorKey").toString();
    const auto input=link.value("inputAnchorKey").toString();
    if(!points.contains(output)||!points.contains(input)) continue;
    const QRectF bounds(points.value(output),points.value(input));
    if (!bounds.normalized().adjusted(-100.0,-100.0,100.0,100.0).intersects({0.0,0.0,width(),height()})) continue;
    QPen pen(linkColor(media,link.value("focused").toBool()),
      link.value("key").toString()==selectedKey_?4.0:2.0,Qt::SolidLine,Qt::RoundCap,Qt::RoundJoin);
    painter->setPen(pen);
    painter->drawPath(linkPath(points.value(output),points.value(input)));
  }
}

void LinkLayer::mousePressEvent(QMouseEvent *event) {
  const auto graphPoints = anchorPoints(portAnchors_);
  QHash<QString,QPointF> points;
  for (auto point = graphPoints.cbegin(); point != graphPoints.cend(); ++point)
    points.insert(point.key(), {point->x()*viewScale_-contentX_, point->y()*viewScale_-contentY_});
  double best=10.0; QString chosen;
  for(const auto &value:links_){
    const auto link=value.toMap();const auto media=link.value("media").toString();if(mediaFilter_!="all"&&mediaFilter_!=media)continue;
    const auto output=link.value("outputAnchorKey").toString(),input=link.value("inputAnchorKey").toString();if(!points.contains(output)||!points.contains(input))continue;
    const auto a=points.value(output),b=points.value(input);QPointF previous=a;
    for(int index=1;index<=32;++index){const auto next=bezier(a,b,static_cast<double>(index)/32.0);const auto distance=segmentDistance(event->position(),previous,next);if(distance<best){best=distance;chosen=link.value("key").toString();}previous=next;}
  }
  if(!chosen.isEmpty()){setSelectedKey(chosen);emit linkActivated(chosen);event->accept();return;}
  event->ignore();
}

} // namespace wirerunner
