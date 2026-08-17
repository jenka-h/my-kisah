#pragma once

#include "decompiler/graph/CallGraph.h"

#include <QGraphicsView>

#include <cstdint>
#include <functional>

namespace mykisah::gui {

class CallGraphView : public QGraphicsView {
public:
    explicit CallGraphView(QWidget* parent = nullptr);

    void set_call_graph(const core::CallGraph& graph);
    void set_navigate_callback(std::function<void(uint64_t)> callback);
    void zoom_in();
    void zoom_out();
    void reset_zoom();

protected:
    void mouseDoubleClickEvent(QMouseEvent* event) override;

private:
    std::function<void(uint64_t)> navigate_callback_;
};

} // namespace mykisah::gui
