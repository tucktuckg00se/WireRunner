// SPDX-License-Identifier: GPL-3.0-or-later
#include "presentation/LinkLayer.hpp"

#include <QLineF>
#include <QMouseEvent>
#include <QSGFlatColorMaterial>
#include <QSGGeometryNode>
#include <algorithm>
#include <cmath>
#include <unordered_map>

namespace wirerunner {
namespace {
QPointF bezier(QPointF a, QPointF b, double t) {
  const double handle = std::max(70.0, std::abs(b.x() - a.x()) * 0.42);
  const QPointF c1(a.x() + handle, a.y());
  const QPointF c2(b.x() - handle, b.y());
  const double u = 1.0 - t;
  return a * (u*u*u) + c1 * (3*u*u*t) + c2 * (3*u*t*t) + b * (t*t*t);
}
QColor linkColor(const QString &media) {
  if (media == "video") return QColor("#e9bb69");
  if (media == "midi") return QColor("#b49cff");
  return QColor("#83dc9a");
}
double segmentDistance(QPointF p, QPointF a, QPointF b) {
  const QPointF ab = b-a;
  const double length = QPointF::dotProduct(ab,ab);
  if (length == 0.0) return QLineF(p,a).length();
  const double t = std::clamp(QPointF::dotProduct(p-a,ab)/length,0.0,1.0);
  return QLineF(p,a+ab*t).length();
}
}

LinkLayer::LinkLayer(QQuickItem *parent) : QQuickItem(parent) {
  setFlag(ItemHasContents, true);
  setAcceptedMouseButtons(Qt::LeftButton);
}
void LinkLayer::setNodes(QVariantList value) { nodes_=std::move(value); emit nodesChanged(); update(); }
void LinkLayer::setLinks(QVariantList value) { links_=std::move(value); emit linksChanged(); update(); }
void LinkLayer::setMediaFilter(QString value) { if(mediaFilter_==value)return; mediaFilter_=std::move(value); emit mediaFilterChanged(); update(); }
void LinkLayer::setSelectedLinkId(quint32 value) { if(selectedLinkId_==value)return; selectedLinkId_=value; emit selectedLinkIdChanged(); update(); }

QSGNode *LinkLayer::updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData *) {
  delete oldNode;
  auto *root = new QSGNode;
  std::unordered_map<quint32,QPointF> endpoints;
  for (const auto &value : nodes_) {
    const auto node=value.toMap();
    const auto id=node.value("id").toUInt();
    endpoints[id]=QPointF(node.value("x").toDouble(),node.value("y").toDouble()+56.0);
  }
  for (const auto &value : links_) {
    const auto link=value.toMap();
    const auto media=link.value("media").toString();
    if(mediaFilter_!="all" && mediaFilter_!=media) continue;
    const auto out=link.value("outputNodeId").toUInt(), in=link.value("inputNodeId").toUInt();
    if(!endpoints.contains(out)||!endpoints.contains(in)) continue;
    QPointF a=endpoints[out]+QPointF(220.0,0), b=endpoints[in];
    constexpr int segments=32;
    auto *geometry=new QSGGeometry(QSGGeometry::defaultAttributes_Point2D(),segments+1);
    geometry->setDrawingMode(QSGGeometry::DrawLineStrip);
    geometry->setLineWidth(link.value("id").toUInt()==selectedLinkId_?4.0F:2.0F);
    auto *vertices=geometry->vertexDataAsPoint2D();
    for(int i=0;i<=segments;++i){const auto p=bezier(a,b,static_cast<double>(i)/segments);vertices[i].set(static_cast<float>(p.x()),static_cast<float>(p.y()));}
    auto *node=new QSGGeometryNode; node->setGeometry(geometry); node->setFlag(QSGNode::OwnsGeometry);
    auto *material=new QSGFlatColorMaterial; material->setColor(linkColor(media));
    node->setMaterial(material); node->setFlag(QSGNode::OwnsMaterial); root->appendChildNode(node);
  }
  return root;
}

void LinkLayer::mousePressEvent(QMouseEvent *event) {
  std::unordered_map<quint32,QPointF> endpoints;
  for(const auto &value:nodes_){const auto n=value.toMap();endpoints[n.value("id").toUInt()]=QPointF(n.value("x").toDouble(),n.value("y").toDouble()+56.0);}
  double best=10.0; QVariantMap chosen;
  for(const auto &value:links_){const auto l=value.toMap();const auto media=l.value("media").toString();if(mediaFilter_!="all"&&mediaFilter_!=media)continue;
    const auto out=l.value("outputNodeId").toUInt(),in=l.value("inputNodeId").toUInt();if(!endpoints.contains(out)||!endpoints.contains(in))continue;
    const QPointF a=endpoints[out]+QPointF(220,0),b=endpoints[in];QPointF prev=a;
    for(int i=1;i<=32;++i){const auto next=bezier(a,b,static_cast<double>(i)/32.0);const auto distance=segmentDistance(event->position(),prev,next);if(distance<best){best=distance;chosen=l;}prev=next;}
  }
  if(!chosen.isEmpty()){setSelectedLinkId(chosen.value("id").toUInt());emit linkActivated(selectedLinkId_,chosen.value("fromName").toString(),chosen.value("toName").toString(),chosen.value("media").toString(),chosen.value("state").toString());event->accept();return;}
  event->ignore();
}
} // namespace wirerunner
