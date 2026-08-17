#include "presentation/gui/CallGraphView.h"

#include <QGraphicsItem>
#include <QGraphicsScene>
#include <QMouseEvent>
#include <QPainter>
#include <QPen>

#include <algorithm>
#include <map>
#include <queue>
#include <set>
#include <vector>

namespace mykisah::gui {
namespace {

constexpr int FUNCTION_ADDRESS_ROLE = 1;
constexpr qreal NODE_WIDTH = 190.0;
constexpr qreal NODE_HEIGHT = 58.0;
constexpr qreal HORIZONTAL_GAP = 45.0;
constexpr qreal VERTICAL_GAP = 75.0;

QString node_label(const core::CallGraphNode& node) {
    return QStringLiteral("%1\n0x%2")
        .arg(QString::fromStdString(node.function_name))
        .arg(node.function_address, 0, 16);
}

} // namespace

CallGraphView::CallGraphView(QWidget* parent)
    : QGraphicsView(parent) {
    setScene(new QGraphicsScene(this));
    setRenderHint(QPainter::Antialiasing, true);
    setDragMode(QGraphicsView::ScrollHandDrag);
    setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
}

void CallGraphView::set_navigate_callback(std::function<void(uint64_t)> callback) {
    navigate_callback_ = std::move(callback);
}

void CallGraphView::set_call_graph(const core::CallGraph& graph) {
    scene()->clear();

    std::map<uint64_t, unsigned> indegree;
    std::map<uint64_t, std::vector<uint64_t>> outgoing;
    for (const auto& [address, node] : graph.nodes) {
        (void)node;
        indegree[address] = 0;
    }
    for (const auto& edge : graph.edges) {
        if (graph.nodes.count(edge.caller) == 0 || graph.nodes.count(edge.callee) == 0) {
            continue;
        }
        outgoing[edge.caller].push_back(edge.callee);
        ++indegree[edge.callee];
    }

    std::map<uint64_t, unsigned> level;
    std::queue<uint64_t> queue;
    for (const auto& [address, degree] : indegree) {
        if (degree == 0) {
            queue.push(address);
            level[address] = 0;
        }
    }
    if (queue.empty() && !graph.nodes.empty()) {
        queue.push(graph.nodes.begin()->first);
        level[graph.nodes.begin()->first] = 0;
    }

    std::set<uint64_t> visited;
    while (!queue.empty()) {
        const auto caller = queue.front();
        queue.pop();
        if (!visited.insert(caller).second) {
            continue;
        }

        for (const auto callee : outgoing[caller]) {
            level[callee] = std::max(level[callee], level[caller] + 1U);
            queue.push(callee);
        }
    }

    for (const auto& [address, node] : graph.nodes) {
        (void)node;
        if (level.count(address) == 0) {
            level[address] = 0;
        }
    }

    std::map<unsigned, std::vector<uint64_t>> nodes_by_level;
    for (const auto& [address, node_level] : level) {
        nodes_by_level[node_level].push_back(address);
    }

    std::map<uint64_t, QRectF> node_rects;
    for (auto& [node_level, addresses] : nodes_by_level) {
        std::sort(addresses.begin(), addresses.end());
        for (std::size_t column = 0; column < addresses.size(); ++column) {
            const qreal x = static_cast<qreal>(column) * (NODE_WIDTH + HORIZONTAL_GAP);
            const qreal y = static_cast<qreal>(node_level) * (NODE_HEIGHT + VERTICAL_GAP);
            node_rects[addresses[column]] = QRectF(x, y, NODE_WIDTH, NODE_HEIGHT);
        }
    }

    QPen edge_pen(palette().color(QPalette::Mid));
    edge_pen.setWidthF(1.5);
    for (const auto& edge : graph.edges) {
        if (node_rects.count(edge.caller) == 0 || node_rects.count(edge.callee) == 0) {
            continue;
        }
        const auto from = node_rects[edge.caller].center();
        const auto to = node_rects[edge.callee].center();
        scene()->addLine(QLineF(from, to), edge_pen);
    }

    for (const auto& [address, rect] : node_rects) {
        const auto node_iterator = graph.nodes.find(address);
        if (node_iterator == graph.nodes.end()) {
            continue;
        }

        auto* box = scene()->addRect(rect, QPen(palette().color(QPalette::Highlight)), palette().brush(QPalette::Base));
        box->setData(FUNCTION_ADDRESS_ROLE, QVariant::fromValue<qulonglong>(address));
        box->setToolTip(QStringLiteral("Double-click to open function"));
        box->setZValue(1.0);

        auto* text = scene()->addText(node_label(node_iterator->second));
        text->setDefaultTextColor(palette().color(QPalette::Text));
        text->setTextWidth(NODE_WIDTH - 12.0);
        text->setPos(rect.left() + 6.0, rect.top() + 5.0);
        text->setData(FUNCTION_ADDRESS_ROLE, QVariant::fromValue<qulonglong>(address));
        text->setZValue(2.0);
    }

    scene()->setSceneRect(scene()->itemsBoundingRect().adjusted(-30.0, -30.0, 30.0, 30.0));
    reset_zoom();
}

void CallGraphView::zoom_in() {
    scale(1.2, 1.2);
}

void CallGraphView::zoom_out() {
    scale(1.0 / 1.2, 1.0 / 1.2);
}

void CallGraphView::reset_zoom() {
    resetTransform();
    if (!scene()->items().isEmpty()) {
        fitInView(scene()->sceneRect(), Qt::KeepAspectRatio);
    }
}

void CallGraphView::mouseDoubleClickEvent(QMouseEvent* event) {
    auto* item = itemAt(event->position().toPoint());
    while (item != nullptr) {
        const auto value = item->data(FUNCTION_ADDRESS_ROLE);
        if (value.isValid()) {
            if (navigate_callback_) {
                navigate_callback_(value.toULongLong());
            }
            event->accept();
            return;
        }
        item = item->parentItem();
    }

    QGraphicsView::mouseDoubleClickEvent(event);
}

} // namespace mykisah::gui
