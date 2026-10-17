#include "graph_coloring.h"

#include <QApplication>
#include <QColor>
#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QPaintEvent>
#include <QPalette>
#include <QPen>
#include <QPushButton>
#include <QSizePolicy>
#include <QSpinBox>
#include <QVBoxLayout>
#include <QWidget>

#include <algorithm>
#include <array>
#include <cmath>
#include <functional>
#include <utility>
#include <vector>

namespace {

const std::array<QColor, 12> COLOR_PALETTE = {
    QColor("#ef4444"), QColor("#3b82f6"), QColor("#22c55e"),
    QColor("#f59e0b"), QColor("#a855f7"), QColor("#06b6d4"),
    QColor("#ec4899"), QColor("#84cc16"), QColor("#f97316"),
    QColor("#6366f1"), QColor("#14b8a6"), QColor("#8b5cf6")
};

class GraphCanvas : public QWidget {
public:
    explicit GraphCanvas(QWidget* parent = nullptr) : QWidget(parent) {
        setMinimumSize(520, 360);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        setAutoFillBackground(true);
        QPalette canvasPalette = palette();
        canvasPalette.setColor(QPalette::Window, QColor("#f8fafc"));
        setPalette(canvasPalette);
    }

    const std::vector<std::pair<int, int>>& edges() const {
        return edges_;
    }

    void setVertexCount(int count) {
        vertex_count_ = count;
        edges_.erase(
            std::remove_if(edges_.begin(), edges_.end(),
                           [count](const std::pair<int, int>& edge) {
                               return edge.first > count || edge.second > count;
                           }),
            edges_.end());
        selected_vertex_ = -1;
        coloring_.clear();
        update();
    }

    void clearEdges() {
        edges_.clear();
        selected_vertex_ = -1;
        coloring_.clear();
        update();
    }

    void clearColoring() {
        coloring_.clear();
        update();
    }

    void setColoring(std::vector<int> coloring) {
        coloring_ = std::move(coloring);
        update();
    }

    void setGraphChangedHandler(std::function<void()> handler) {
        graph_changed_handler_ = std::move(handler);
    }

protected:
    void paintEvent(QPaintEvent*) override {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.fillRect(rect(), QColor("#f8fafc"));

        const std::vector<QPointF> points = vertexPoints();
        painter.setPen(QPen(QColor("#64748b"), 2));
        for (const auto& edge : edges_) {
            painter.drawLine(points[edge.first - 1], points[edge.second - 1]);
        }

        for (int vertex = 0; vertex < vertex_count_; ++vertex) {
            const QPointF center = points[vertex];
            QColor fill("#dbeafe");
            if (vertex < static_cast<int>(coloring_.size()) &&
                coloring_[vertex] > 0 &&
                coloring_[vertex] <= static_cast<int>(COLOR_PALETTE.size())) {
                fill = COLOR_PALETTE[coloring_[vertex] - 1];
            }

            painter.setBrush(fill);
            painter.setPen(QPen(
                vertex + 1 == selected_vertex_ ? QColor("#f59e0b")
                                              : QColor("#334155"),
                vertex + 1 == selected_vertex_ ? 4 : 2));
            painter.drawEllipse(center, NODE_RADIUS, NODE_RADIUS);

            painter.setPen(QColor("#0f172a"));
            painter.drawText(
                QRectF(center.x() - NODE_RADIUS, center.y() - NODE_RADIUS,
                       NODE_RADIUS * 2, NODE_RADIUS * 2),
                Qt::AlignCenter, QString::number(vertex + 1));
        }
    }

    void mousePressEvent(QMouseEvent* event) override {
        if (event->button() == Qt::RightButton) {
            selected_vertex_ = -1;
            update();
            return;
        }
        if (event->button() != Qt::LeftButton) {
            return;
        }

        const int vertex = vertexAt(event->pos());
        if (vertex == -1) {
            return;
        }
        if (selected_vertex_ == -1) {
            selected_vertex_ = vertex;
            update();
            return;
        }

        const int first = selected_vertex_;
        selected_vertex_ = -1;
        if (first == vertex) {
            update();
            return;
        }

        const bool already_exists = std::any_of(
            edges_.begin(), edges_.end(),
            [first, vertex](const std::pair<int, int>& edge) {
                return (edge.first == first && edge.second == vertex) ||
                       (edge.first == vertex && edge.second == first);
            });
        if (!already_exists) {
            edges_.emplace_back(first, vertex);
            coloring_.clear();
            if (graph_changed_handler_) {
                graph_changed_handler_();
            }
        }
        update();
    }

private:
    static constexpr qreal NODE_RADIUS = 19.0;
    int vertex_count_ = 6;
    int selected_vertex_ = -1;
    std::vector<std::pair<int, int>> edges_;
    std::vector<int> coloring_;
    std::function<void()> graph_changed_handler_;

    std::vector<QPointF> vertexPoints() const {
        std::vector<QPointF> points;
        points.reserve(vertex_count_);
        const QPointF center(width() / 2.0, height() / 2.0);
        const qreal orbit =
            std::max<qreal>(0.0, std::min(width(), height()) / 2.0 - 42.0);
        const qreal pi = std::acos(-1.0);

        for (int vertex = 0; vertex < vertex_count_; ++vertex) {
            const qreal angle =
                -pi / 2.0 + 2.0 * pi * vertex / vertex_count_;
            points.emplace_back(center.x() + orbit * std::cos(angle),
                                center.y() + orbit * std::sin(angle));
        }
        return points;
    }

    int vertexAt(const QPoint& position) const {
        const std::vector<QPointF> points = vertexPoints();
        const qreal hit_radius = NODE_RADIUS + 7.0;
        for (int vertex = 0; vertex < vertex_count_; ++vertex) {
            const qreal dx = position.x() - points[vertex].x();
            const qreal dy = position.y() - points[vertex].y();
            if (dx * dx + dy * dy <= hit_radius * hit_radius) {
                return vertex + 1;
            }
        }
        return -1;
    }
};

class GraphColoringWindow : public QWidget {
public:
    GraphColoringWindow() {
        setWindowTitle("SAT Solver — Graph Coloring");
        resize(800, 620);

        auto* title = new QLabel("Graph Coloring Solver");
        QFont title_font = title->font();
        title_font.setPointSize(title_font.pointSize() + 5);
        title_font.setBold(true);
        title->setFont(title_font);

        auto* instructions = new QLabel(
            "Choose the number of vertices and colours, then click two vertices "
            "to add an edge. Right-click to cancel a selection.");
        instructions->setWordWrap(true);

        auto* vertex_label = new QLabel("Vertices:");
        vertex_count_ = new QSpinBox;
        vertex_count_->setRange(1, 60);
        vertex_count_->setValue(6);

        auto* color_label = new QLabel("Colours:");
        color_count_ = new QSpinBox;
        color_count_->setRange(1, static_cast<int>(COLOR_PALETTE.size()));
        color_count_->setValue(3);

        auto* solve_button = new QPushButton("Solve coloring");
        auto* clear_button = new QPushButton("Clear edges");
        auto* toolbar = new QHBoxLayout;
        toolbar->addWidget(vertex_label);
        toolbar->addWidget(vertex_count_);
        toolbar->addSpacing(16);
        toolbar->addWidget(color_label);
        toolbar->addWidget(color_count_);
        toolbar->addStretch();
        toolbar->addWidget(clear_button);
        toolbar->addWidget(solve_button);

        canvas_ = new GraphCanvas;
        canvas_->setVertexCount(vertex_count_->value());

        status_ = new QLabel("Click two vertices to add an edge, then solve.");
        status_->setWordWrap(true);
        status_->setTextInteractionFlags(Qt::TextSelectableByMouse);

        auto* layout = new QVBoxLayout(this);
        layout->addWidget(title);
        layout->addWidget(instructions);
        layout->addLayout(toolbar);
        layout->addWidget(canvas_, 1);
        layout->addWidget(status_);

        canvas_->setGraphChangedHandler([this]() {
            clearSolution();
            status_->setText(QString("Graph updated: %1 edge(s).")
                                 .arg(static_cast<qulonglong>(
                                     canvas_->edges().size())));
        });
        QObject::connect(vertex_count_, qOverload<int>(&QSpinBox::valueChanged),
                         this, [this](int count) {
            canvas_->setVertexCount(count);
            clearSolution();
            status_->setText(
                QString("Graph has %1 vertices and %2 edge(s).")
                    .arg(count)
                    .arg(static_cast<qulonglong>(canvas_->edges().size())));
        });
        QObject::connect(color_count_, qOverload<int>(&QSpinBox::valueChanged),
                         this, [this](int) {
            clearSolution();
            status_->setText("Colour count changed; solve the graph again.");
        });
        QObject::connect(clear_button, &QPushButton::clicked, this, [this]() {
            canvas_->clearEdges();
            clearSolution();
            status_->setText("All edges cleared.");
        });
        QObject::connect(solve_button, &QPushButton::clicked, this, [this]() {
            solveGraph();
        });
    }

private:
    QSpinBox* vertex_count_ = nullptr;
    QSpinBox* color_count_ = nullptr;
    GraphCanvas* canvas_ = nullptr;
    QLabel* status_ = nullptr;

    void clearSolution() {
        canvas_->clearColoring();
        status_->setStyleSheet(QString());
    }

    void solveGraph() {
        try {
            const GraphColoringResult result = solve_graph_coloring(
                vertex_count_->value(), color_count_->value(), canvas_->edges());
            if (!result.is_satisfiable) {
                canvas_->clearColoring();
                status_->setStyleSheet("QLabel { color: #b42318; }");
                status_->setText(
                    QString("No valid coloring exists with %1 colour(s).")
                        .arg(color_count_->value()));
                return;
            }

            canvas_->setColoring(result.vertex_colors);
            status_->setStyleSheet("QLabel { color: #207245; }");
            status_->setText(
                QString("Valid coloring found. Decisions: %1  |  Backtracks: %2"
                        "  |  Solver time: %3 ms")
                    .arg(result.decision_count)
                    .arg(result.backtrack_count)
                    .arg(result.solver_time_ms, 0, 'f', 3));
        } catch (const std::exception& error) {
            canvas_->clearColoring();
            status_->setStyleSheet("QLabel { color: #b42318; }");
            status_->setText(
                QString("Unable to solve graph: %1").arg(error.what()));
        }
    }
};

} // namespace

int main(int argc, char* argv[]) {
    qunsetenv("GTK_PATH");
    qunsetenv("GIO_MODULE_DIR");

    QApplication app(argc, argv);
    GraphColoringWindow window;
    window.show();
    return app.exec();
}
