#include "presentation/gui/MainWindow.h"

#include "application/BinaryPatcher.h"
#include "presentation/gui/CallGraphView.h"

#include <QAction>
#include <QDockWidget>
#include <QFileDialog>
#include <QFontDatabase>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSplitter>
#include <QStatusBar>
#include <QToolBar>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <QWidget>

namespace mykisah::gui {
namespace {

constexpr int ADDRESS_ROLE = Qt::UserRole;
constexpr int CALL_TARGET_ROLE = Qt::UserRole + 1;
constexpr int INSTRUCTION_SIZE_ROLE = Qt::UserRole + 2;

QString hex_address(uint64_t address) {
    return QStringLiteral("0x%1").arg(address, 16, 16, QLatin1Char('0'));
}

} // namespace

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent) {
    build_ui();
    setWindowTitle(QStringLiteral("My Kisah Decompiler"));
    resize(1500, 850);
}

void MainWindow::build_ui() {
    auto* toolbar = addToolBar(QStringLiteral("Main"));
    toolbar->setMovable(false);
    open_action_ = toolbar->addAction(QStringLiteral("Open Binary"));
    patch_action_ = toolbar->addAction(QStringLiteral("Patch Instruction"));

    auto* central = new QSplitter(Qt::Horizontal, this);

    auto* function_panel = new QWidget(central);
    auto* function_layout = new QVBoxLayout(function_panel);
    function_layout->setContentsMargins(6, 6, 6, 6);
    search_ = new QLineEdit(function_panel);
    search_->setPlaceholderText(QStringLiteral("Search functions..."));
    functions_ = new QListWidget(function_panel);
    function_layout->addWidget(new QLabel(QStringLiteral("Functions"), function_panel));
    function_layout->addWidget(search_);
    function_layout->addWidget(functions_);

    auto* source_panel = new QWidget(central);
    auto* source_layout = new QVBoxLayout(source_panel);
    source_layout->setContentsMargins(6, 6, 6, 6);
    address_ = new QLabel(QStringLiteral("Address: —"), source_panel);
    decompiled_ = new QPlainTextEdit(source_panel);
    decompiled_->setReadOnly(true);
    decompiled_->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    source_layout->addWidget(new QLabel(QStringLiteral("Decompiled C++"), source_panel));
    source_layout->addWidget(address_);
    source_layout->addWidget(decompiled_);

    auto* assembly_panel = new QWidget(central);
    auto* assembly_layout = new QVBoxLayout(assembly_panel);
    assembly_layout->setContentsMargins(6, 6, 6, 6);
    assembly_ = new QTreeWidget(assembly_panel);
    assembly_->setColumnCount(3);
    assembly_->setHeaderLabels({QStringLiteral("Address"), QStringLiteral("Bytes"), QStringLiteral("Assembly / Opcode")});
    assembly_->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    assembly_->setRootIsDecorated(false);
    assembly_->setAlternatingRowColors(true);
    assembly_->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    assembly_->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    assembly_->header()->setSectionResizeMode(2, QHeaderView::Stretch);
    assembly_layout->addWidget(new QLabel(QStringLiteral("Assembly / Opcode"), assembly_panel));
    assembly_layout->addWidget(assembly_);

    central->addWidget(function_panel);
    central->addWidget(source_panel);
    central->addWidget(assembly_panel);
    central->setStretchFactor(0, 1);
    central->setStretchFactor(1, 3);
    central->setStretchFactor(2, 3);
    setCentralWidget(central);

    auto* graph_dock = new QDockWidget(QStringLiteral("Call Graph"), this);
    auto* graph_panel = new QWidget(graph_dock);
    auto* graph_layout = new QVBoxLayout(graph_panel);
    graph_layout->setContentsMargins(4, 4, 4, 4);
    auto* graph_controls = new QHBoxLayout();
    auto* zoom_in = new QPushButton(QStringLiteral("Zoom In"), graph_panel);
    auto* zoom_out = new QPushButton(QStringLiteral("Zoom Out"), graph_panel);
    auto* reset_zoom = new QPushButton(QStringLiteral("Fit"), graph_panel);
    graph_controls->addWidget(zoom_in);
    graph_controls->addWidget(zoom_out);
    graph_controls->addWidget(reset_zoom);
    graph_controls->addStretch();
    call_graph_ = new CallGraphView(graph_panel);
    graph_layout->addLayout(graph_controls);
    graph_layout->addWidget(call_graph_);
    graph_dock->setWidget(graph_panel);
    addDockWidget(Qt::BottomDockWidgetArea, graph_dock);

    call_graph_->set_navigate_callback([this](uint64_t address) { navigate_to_address(address); });
    connect(zoom_in, &QPushButton::clicked, call_graph_, &CallGraphView::zoom_in);
    connect(zoom_out, &QPushButton::clicked, call_graph_, &CallGraphView::zoom_out);
    connect(reset_zoom, &QPushButton::clicked, call_graph_, &CallGraphView::reset_zoom);

    statusBar()->showMessage(QStringLiteral("Open an ELF64 x86-64 binary"));

    connect(open_action_, &QAction::triggered, this, [this] { choose_binary(); });
    connect(patch_action_, &QAction::triggered, this, [this] { patch_selected_instruction(); });
    connect(search_, &QLineEdit::textChanged, this, [this](const QString& query) { filter_functions(query); });
    connect(functions_, &QListWidget::itemSelectionChanged, this, [this] { select_function_item(); });
    connect(assembly_, &QTreeWidget::itemDoubleClicked, this, [this](QTreeWidgetItem* item, int) {
        activate_assembly_item(item);
    });
}

void MainWindow::choose_binary() {
    const auto path = QFileDialog::getOpenFileName(this, QStringLiteral("Open ELF Binary"));
    if (!path.isEmpty()) {
        open_binary(path);
    }
}

void MainWindow::open_binary(const QString& path) {
    try {
        session_.load(path.toStdString());
        populate_functions();
        call_graph_->set_call_graph(session_.call_graph());
        statusBar()->showMessage(QStringLiteral("Loaded %1 functions from %2")
                                     .arg(session_.functions().size())
                                     .arg(path));
        setWindowTitle(QStringLiteral("My Kisah Decompiler — %1").arg(path));
    } catch (const std::exception& exception) {
        QMessageBox::critical(this, QStringLiteral("Cannot Open Binary"), QString::fromUtf8(exception.what()));
    }
}

void MainWindow::populate_functions() {
    functions_->clear();
    for (const auto& function : session_.functions()) {
        auto* item = new QListWidgetItem(QString::fromStdString(function.function.name), functions_);
        item->setData(ADDRESS_ROLE, QVariant::fromValue<qulonglong>(function.function.address));
        item->setToolTip(hex_address(function.function.address));
    }

    if (functions_->count() > 0) {
        functions_->setCurrentRow(0);
    }
}

void MainWindow::filter_functions(const QString& query) {
    for (int i = 0; i < functions_->count(); ++i) {
        auto* item = functions_->item(i);
        item->setHidden(!item->text().contains(query, Qt::CaseInsensitive));
    }
}

void MainWindow::select_function_item() {
    const auto items = functions_->selectedItems();
    if (items.empty()) {
        return;
    }

    const auto address = items.front()->data(ADDRESS_ROLE).toULongLong();
    const auto* function = session_.find_function(address);
    if (function != nullptr) {
        show_function(*function);
    }
}

void MainWindow::show_function(const app::FunctionResult& function) {
    address_->setText(QStringLiteral("ELF VA: %1    Image-relative: %2    Size: %3 bytes")
                          .arg(hex_address(function.function.location.virtual_address))
                          .arg(hex_address(function.function.location.image_relative))
                          .arg(function.function.size));
    decompiled_->setPlainText(QString::fromStdString(function.decompiled_cpp));

    assembly_->clear();
    for (const auto& line : function.assembly) {
        auto* item = new QTreeWidgetItem(assembly_);
        item->setText(0, hex_address(line.address));
        item->setData(0, ADDRESS_ROLE, QVariant::fromValue<qulonglong>(line.address));
        item->setData(0, INSTRUCTION_SIZE_ROLE, QVariant::fromValue<qulonglong>(line.size));
        item->setText(1, QString::fromStdString(line.bytes));
        item->setText(2, QString::fromStdString(line.text));
        if (line.has_call_target) {
            item->setData(0, CALL_TARGET_ROLE, QVariant::fromValue<qulonglong>(line.call_target));
            if (session_.find_function_containing(line.call_target) != nullptr) {
                item->setToolTip(2, QStringLiteral("Double-click to navigate to %1").arg(hex_address(line.call_target)));
            }
        }
    }
}

void MainWindow::navigate_to_address(uint64_t address) {
    const auto* function = session_.find_function_containing(address);
    if (function == nullptr) {
        statusBar()->showMessage(QStringLiteral("No discovered function at %1").arg(hex_address(address)), 4000);
        return;
    }

    for (int i = 0; i < functions_->count(); ++i) {
        auto* item = functions_->item(i);
        if (item->data(ADDRESS_ROLE).toULongLong() == function->function.address) {
            item->setHidden(false);
            functions_->setCurrentItem(item);
            functions_->scrollToItem(item);
            return;
        }
    }
}

void MainWindow::activate_assembly_item(QTreeWidgetItem* item) {
    if (item == nullptr || !item->data(0, CALL_TARGET_ROLE).isValid()) {
        return;
    }
    navigate_to_address(item->data(0, CALL_TARGET_ROLE).toULongLong());
}

void MainWindow::patch_selected_instruction() {
    auto* item = assembly_->currentItem();
    if (item == nullptr || !item->data(0, ADDRESS_ROLE).isValid() || session_.path().empty()) {
        QMessageBox::information(this, QStringLiteral("Patch Instruction"), QStringLiteral("Select an assembly instruction first."));
        return;
    }

    bool accepted = false;
    const auto replacement_text = QInputDialog::getText(
        this,
        QStringLiteral("Replacement Bytes"),
        QStringLiteral("Enter same-size or shorter hexadecimal bytes. Shorter replacements are padded with 0x90 NOPs."),
        QLineEdit::Normal,
        QString(),
        &accepted);
    if (!accepted || replacement_text.trimmed().isEmpty()) {
        return;
    }

    const auto suggested = QString::fromStdString(session_.path()) + QStringLiteral(".patched");
    const auto output_path = QFileDialog::getSaveFileName(
        this,
        QStringLiteral("Save Patched ELF Copy"),
        suggested);
    if (output_path.isEmpty()) {
        return;
    }

    const uint64_t instruction_address = item->data(0, ADDRESS_ROLE).toULongLong();
    const uint64_t instruction_size = item->data(0, INSTRUCTION_SIZE_ROLE).toULongLong();

    try {
        app::BinaryPatcher patcher;
        app::PatchRequest request;
        request.source_path = session_.path();
        request.output_path = output_path.toStdString();
        request.virtual_address = instruction_address;
        request.original_size = instruction_size;
        request.replacement_bytes = app::BinaryPatcher::parse_hex_bytes(replacement_text.toStdString());

        const auto result = patcher.patch_instruction(request);
        session_.load(request.output_path);
        populate_functions();
        call_graph_->set_call_graph(session_.call_graph());
        navigate_to_address(instruction_address);

        statusBar()->showMessage(
            QStringLiteral("Patched copy loaded: %1 (file offset 0x%2)")
                .arg(output_path)
                .arg(result.file_offset, 0, 16),
            8000);
    } catch (const std::exception& exception) {
        QMessageBox::critical(this, QStringLiteral("Patch Failed"), QString::fromUtf8(exception.what()));
    }
}

} // namespace mykisah::gui
